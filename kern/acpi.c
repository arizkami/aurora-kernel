/* ACPI discovery & table parsing */
#include "../aurora.h"
#include "../include/acpi.h"
#include "../include/kern.h"
#include "../include/mem.h"

static BOOL g_AcpiInitialized = FALSE;
static PACPI_RSDP g_Rsdp = NULL;
static const ACPI_TABLE_HEADER* g_Madt = NULL;
static const ACPI_TABLE_HEADER* g_Hpet = NULL;
static UINT64 g_LapicBase = 0;
static UINT64 g_IoApicBase = 0;
static UINT8  g_LapicIds[256];
static UINT32 g_LapicCount = 0;
static UINT32 g_LapicFlags[256];
static UINT64 g_HpetBase = 0;

/* Simple phys->virt mapping stub (architecture should override) */
__attribute__((weak)) void* AcpiMapPhysical(UINT64 phys, UINT32 length){
    UINT64 aligned = MemAlignDown(phys, AURORA_PAGE_SIZE);
    UINT64 end = phys + length;
    UINT64 endAligned = MemAlignUp(end, AURORA_PAGE_SIZE);
    SIZE_T mapLen = (SIZE_T)(endAligned - aligned);
    /* Map physical into a temporary virtual region: for now reuse physical==virtual if already valid */
    if(MemIsPhysicalAddressValid(phys)){
        /* Attempt to map to same address to keep early simplicity */
        if(NT_SUCCESS(MemMapPhysicalMemory(aligned, (PVOID)(UINT_PTR)aligned, mapLen, MEM_PROTECT_READ)))
            return (void*)(UINT_PTR)phys;
    }
    return NULL;
}

static BOOL AcpiChecksumOk(const UINT8* data, UINT32 length){
    UINT8 sum=0; for(UINT32 i=0;i<length;i++) sum = (UINT8)(sum + data[i]); return sum==0; }

/* Scan for RSDP in EBDA (0x40E gives EBDA segment) and then high BIOS area */
static PACPI_RSDP AcpiFindRsdp(void){
    /* EBDA segment pointer at 0x40E (word) */
    UINT32 ebdaPhys = 0;
    if(MemIsPhysicalAddressValid(0x40E)){
        UINT16* ebdaSegPtr = (UINT16*)(UINT_PTR)0x40E;
        UINT16 ebdaSeg = *ebdaSegPtr;
        ebdaPhys = ((UINT32)ebdaSeg) << 4; /* segment to phys */
    }
    const CHAR sig[] = { 'R','S','D',' ','P','T','R',' ' };
    if(ebdaPhys && MemIsPhysicalAddressValid(ebdaPhys)){
        for(UINT32 p = ebdaPhys; p < ebdaPhys + 1024; p += 16){
            PACPI_RSDP rsdp = (PACPI_RSDP)(UINT_PTR)p;
            if(memcmp(rsdp->Signature, sig, 8)==0){
                UINT32 len = (rsdp->Revision >= 2) ? rsdp->Length : 20;
                if(AcpiChecksumOk((const UINT8*)rsdp, len)) return rsdp;
            }
        }
    }
    /* Fallback area 0xE0000 - 0xFFFFF */
    for(UINT32 p = 0xE0000; p < 0x100000; p += 16){
        PACPI_RSDP rsdp = (PACPI_RSDP)(UINT_PTR)p;
        if(memcmp(rsdp->Signature, sig, 8)==0){
            UINT32 len = (rsdp->Revision >= 2) ? rsdp->Length : 20;
            if(AcpiChecksumOk((const UINT8*)rsdp, len)) return rsdp;
        }
    }
    return NULL;
}

static void AcpiEnumerateTables(void){
    if(!g_Rsdp) return;
    BOOL useXsdt = (g_Rsdp->Revision >= 2 && g_Rsdp->XsdtPhysical != 0);
    if(useXsdt){
        PACPI_XSDT xsdt = (PACPI_XSDT)AcpiMapPhysical(g_Rsdp->XsdtPhysical, sizeof(ACPI_TABLE_HEADER));
        if(!xsdt) return;
        UINT32 totalLen = xsdt->Header.Length;
        UINT32 entries = (totalLen - sizeof(ACPI_TABLE_HEADER)) / 8;
        UINT64* ptrs = (UINT64*)(((UINT8*)xsdt) + sizeof(ACPI_TABLE_HEADER));
        for(UINT32 i=0;i<entries;i++){
            UINT64 phys = ptrs[i];
            ACPI_TABLE_HEADER* h = (ACPI_TABLE_HEADER*)AcpiMapPhysical(phys, sizeof(ACPI_TABLE_HEADER));
            if(!h) continue;
            if(memcmp(h->Signature, "APIC",4)==0){ g_Madt = h; }
            else if(memcmp(h->Signature,"HPET",4)==0){ g_Hpet = h; }
        }
    } else {
        PACPI_RSDT rsdt = (PACPI_RSDT)AcpiMapPhysical(g_Rsdp->RsdtPhysical, sizeof(ACPI_TABLE_HEADER));
        if(!rsdt) return;
        UINT32 totalLen = rsdt->Header.Length;
        UINT32 entries = (totalLen - sizeof(ACPI_TABLE_HEADER)) / 4;
        UINT32* ptrs = (UINT32*)(((UINT8*)rsdt) + sizeof(ACPI_TABLE_HEADER));
        for(UINT32 i=0;i<entries;i++){
            UINT64 phys = ptrs[i];
            ACPI_TABLE_HEADER* h = (ACPI_TABLE_HEADER*)AcpiMapPhysical(phys, sizeof(ACPI_TABLE_HEADER));
            if(!h) continue;
            if(memcmp(h->Signature, "APIC",4)==0){ g_Madt = h; }
            else if(memcmp(h->Signature,"HPET",4)==0){ g_Hpet = h; }
        }
    }
}

static void AcpiParseMadt(void){
    if(!g_Madt) return;
    PACPI_MADT madt = (PACPI_MADT)g_Madt;
    g_LapicBase = madt->LocalApicAddress;
    UINT8* end = ((UINT8*)madt) + madt->Header.Length;
    UINT8* cur = madt->Records;
    while(cur + sizeof(ACPI_MADT_ENTRY_HEADER) <= end){
        PACPI_MADT_ENTRY_HEADER h = (PACPI_MADT_ENTRY_HEADER)cur;
        if(h->Length == 0) break;
        if(cur + h->Length > end) break;
        switch(h->Type){
            case ACPI_MADT_LAPIC: {
                PACPI_MADT_LAPIC_ENTRY e = (PACPI_MADT_LAPIC_ENTRY)cur;
                if(e->Flags & 1){ /* enabled */
                    if(g_LapicCount < 256){
                        g_LapicIds[g_LapicCount] = e->ApicId;
                        g_LapicFlags[g_LapicCount] = e->Flags;
                        g_LapicCount++;
                    }
                }
                break; }
            case ACPI_MADT_IOAPIC: {
                PACPI_MADT_IOAPIC_ENTRY e = (PACPI_MADT_IOAPIC_ENTRY)cur;
                if(g_IoApicBase == 0) g_IoApicBase = e->IoApicAddress;
                break; }
            case ACPI_MADT_INT_OVERRIDE: {
                /* Interrupt Source Override structure layout:
                   Type(1) Length(1) Bus(1) Source(1) GSI(4) Flags(2)
                   We may store or log for future IRQ routing; minimal parse now. */
                if(h->Length >= 10){
                    UINT8 bus = cur[2];
                    UINT8 src = cur[3];
                    UINT32 gsi = *(UINT32*)(cur+4);
                    UINT16 flags = *(UINT16*)(cur+8);
                    (void)bus; (void)src; (void)gsi; (void)flags; /* TODO: store mapping structure */
                }
                break; }
            default: break;
        }
        cur += h->Length;
    }
}

static void AcpiParseHpet(void){
    if(!g_Hpet) return;
    PACPI_HPET hpet = (PACPI_HPET)g_Hpet;
    if(hpet->Address.AddressSpaceId != 0){
        KernDebugPrint("ACPI: HPET GAS unexpected space %u (expected 0=SystemMemory)\n", hpet->Address.AddressSpaceId);
        return;
    }
    g_HpetBase = hpet->Address.Address;
}

BOOL AcpiInitialize(void){
    if(g_AcpiInitialized) return TRUE;
    g_Rsdp = AcpiFindRsdp();
    if(!g_Rsdp){
        KernDebugPrint("ACPI: RSDP not found\n");
        return FALSE;
    }
    AcpiEnumerateTables();
    AcpiParseMadt();
    AcpiParseHpet();
    g_AcpiInitialized = TRUE;
    KernDebugPrint("ACPI: initialized (rev %u LAPIC=0x%llX IOAPIC=0x%llX HPET=0x%llX CPUs=%u)\n",
        g_Rsdp->Revision, (unsigned long long)g_LapicBase, (unsigned long long)g_IoApicBase, (unsigned long long)g_HpetBase, g_LapicCount);
    return TRUE;
}

const ACPI_TABLE_HEADER* AcpiFindTable(const CHAR Sig[4]){
    if(!g_AcpiInitialized) return NULL;
    if(memcmp(Sig,"APIC",4)==0) return g_Madt;
    if(memcmp(Sig,"HPET",4)==0) return g_Hpet;
    return NULL;
}

UINT64 AcpiGetLapicBase(void){ return g_LapicBase; }
UINT64 AcpiGetIoApicBase(void){ return g_IoApicBase; }
UINT64 AcpiGetHpetBase(void){ return g_HpetBase; }
BOOL AcpiEnumerateLapics(UINT8* ids, UINT32 capacity, UINT32* countOut){
    if(countOut) *countOut = g_LapicCount;
    if(!ids) return TRUE;
    UINT32 n = (g_LapicCount < capacity)? g_LapicCount : capacity;
    for(UINT32 i=0;i<n;i++) ids[i] = g_LapicIds[i];
    return TRUE;
}

BOOL AcpiGetLapicFlags(UINT8 apicId, UINT32* flagsOut){
    if(!flagsOut) return FALSE;
    for(UINT32 i=0;i<g_LapicCount;i++) if(g_LapicIds[i]==apicId){ *flagsOut = g_LapicFlags[i]; return TRUE; }
    return FALSE;
}

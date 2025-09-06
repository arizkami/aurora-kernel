#include "../aurora.h"
#include "../include/acpi.h"

static BOOL g_AcpiInitialized = FALSE;

static const ACPI_TABLE_HEADER* g_DummyTables[4];
static UINT32 g_DummyTableCount = 0;

/* Very small checksum helper */
static BOOL AcpiChecksumOk(const UINT8* data, UINT32 length){
    UINT8 sum=0; for(UINT32 i=0;i<length;i++) sum = (UINT8)(sum + data[i]); return sum==0; }

BOOL AcpiInitialize(void){
    if(g_AcpiInitialized) return TRUE;
    /* TODO: real RSDP scan (EBDA / BIOS ROM range). For now, stub success. */
    g_AcpiInitialized = TRUE; return TRUE; }

const ACPI_TABLE_HEADER* AcpiFindTable(const CHAR Sig[4]){
    if(!g_AcpiInitialized) return NULL;
    for(UINT32 i=0;i<g_DummyTableCount;i++){
        const ACPI_TABLE_HEADER* h = g_DummyTables[i];
        if(h && memcmp(h->Signature,Sig,4)==0){
            if(AcpiChecksumOk((const UINT8*)h,h->Length)) return h; else return NULL;
        }
    }
    return NULL; }

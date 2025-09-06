/* Minimal ACPI shim derived conceptually from external Fiasco ACPI structures */
#ifndef _AURORA_ACPI_H_
#define _AURORA_ACPI_H_
#include "../aurora.h"

typedef struct _ACPI_TABLE_HEADER {
    CHAR     Signature[4];
    UINT32   Length;
    UINT8    Revision;
    UINT8    Checksum;
    CHAR     OemId[6];
    CHAR     OemTableId[8];
    UINT32   OemRevision;
    UINT32   CreatorId;
    UINT32   CreatorRevision;
} ACPI_TABLE_HEADER, *PACPI_TABLE_HEADER;

typedef struct _ACPI_RSDP {
    CHAR     Signature[8];
    UINT8    Checksum;
    CHAR     OemId[6];
    UINT8    Revision;
    UINT32   RsdtPhysical;
    UINT32   Length;
    UINT64   XsdtPhysical;
    UINT8    ExtendedChecksum;
    UINT8    Reserved[3];
} ACPI_RSDP, *PACPI_RSDP;

typedef struct _ACPI_RSDT {
    ACPI_TABLE_HEADER Header;
    UINT32 TablePointers[]; /* variable */
} ACPI_RSDT, *PACPI_RSDT;

typedef struct _ACPI_XSDT {
    ACPI_TABLE_HEADER Header;
    UINT64 TablePointers[]; /* variable */
} ACPI_XSDT, *PACPI_XSDT;

/* MADT (APIC) */
typedef struct _ACPI_MADT {
    ACPI_TABLE_HEADER Header;
    UINT32 LocalApicAddress;
    UINT32 Flags;
    UINT8  Records[]; /* variable length entries */
} ACPI_MADT, *PACPI_MADT;

/* MADT entry headers */
typedef struct _ACPI_MADT_ENTRY_HEADER {
    UINT8 Type;
    UINT8 Length;
} ACPI_MADT_ENTRY_HEADER, *PACPI_MADT_ENTRY_HEADER;

/* Common MADT entry types we care about */
#define ACPI_MADT_LAPIC          0
#define ACPI_MADT_IOAPIC         1
#define ACPI_MADT_INT_OVERRIDE   2

typedef struct _ACPI_MADT_LAPIC_ENTRY {
    ACPI_MADT_ENTRY_HEADER H;
    UINT8  ProcessorId;
    UINT8  ApicId;
    UINT32 Flags;
} ACPI_MADT_LAPIC_ENTRY, *PACPI_MADT_LAPIC_ENTRY;

typedef struct _ACPI_MADT_IOAPIC_ENTRY {
    ACPI_MADT_ENTRY_HEADER H;
    UINT8  IoApicId;
    UINT8  Reserved;
    UINT32 IoApicAddress;
    UINT32 GlobalSystemInterruptBase;
} ACPI_MADT_IOAPIC_ENTRY, *PACPI_MADT_IOAPIC_ENTRY;

/* HPET table */
typedef struct _ACPI_HPET_DESCRIPTOR {
    UINT8  AddressSpaceId;
    UINT8  RegisterBitWidth;
    UINT8  RegisterBitOffset;
    UINT8  Reserved;
    UINT64 Address;
} ACPI_HPET_DESCRIPTOR, *PACPI_HPET_DESCRIPTOR;

typedef struct _ACPI_HPET {
    ACPI_TABLE_HEADER Header;
    UINT32 Id;
    ACPI_HPET_DESCRIPTOR Address;
    UINT8  Number;
    UINT16 MinimumTick;
    UINT8  Attributes;
} ACPI_HPET, *PACPI_HPET;

/* Public query helpers */
UINT64 AcpiGetLapicBase(void);
UINT64 AcpiGetIoApicBase(void);
UINT64 AcpiGetHpetBase(void);
BOOL   AcpiEnumerateLapics(UINT8* ids, UINT32 capacity, UINT32* countOut);
BOOL   AcpiGetLapicFlags(UINT8 apicId, UINT32* flagsOut);

/* Internal use: physical mapping helper (arch must provide) */
void* AcpiMapPhysical(UINT64 phys, UINT32 length);

BOOL AcpiInitialize(void);
const ACPI_TABLE_HEADER* AcpiFindTable(const CHAR Sig[4]);

#endif
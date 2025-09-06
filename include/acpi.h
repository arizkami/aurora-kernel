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

BOOL AcpiInitialize(void);
const ACPI_TABLE_HEADER* AcpiFindTable(const CHAR Sig[4]);

#endif
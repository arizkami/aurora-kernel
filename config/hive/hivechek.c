/*
 * Aurora Kernel - Hive Integrity Checking
 * Copyright (c) 2024 NTCore Project
 */

#include "hive.h"

/* Define offsetof if not available */
#ifndef offsetof
#define offsetof(type, member) ((SIZE_T) &((type *)0)->member)
#endif

/* Define abs if not available */
#ifndef abs
#define abs(x) ((x) < 0 ? -(x) : (x))
#endif

/* Hive integrity checking and validation functions */

/*
 * Verify hive header integrity
 */
BOOL HiveCheckHeader(IN PHIVE Hive)
{
    if (!Hive || !Hive->BaseAddress) {
        return FALSE;
    }

    PHIVE_HEADER Header = Hive->Header;
    
    /* Check signature */
    if (Header->Signature != HIVE_SIGNATURE) {
        return FALSE;
    }

    /* Check version */
    if (Header->MajorVersion == 0 || Header->MajorVersion > 10) {
        return FALSE;
    }

    /* Check length */
    if (Header->Length == 0 || Header->Length > Hive->Size) {
        return FALSE;
    }

    /* Check root cell offset */
    if (Header->RootCell == 0 || Header->RootCell >= Header->Length) {
        return FALSE;
    }

    return TRUE;
}

/*
 * Verify hive checksum
 */
/* Verification helpers are provided in hivesum.c */

/*
 * Check cell structure integrity
 */
BOOL HiveCheckCellStructure(IN PHIVE Hive, IN UINT32 CellOffset)
{
    if (!HiveValidateCell(Hive, CellOffset)) {
        return FALSE;
    }

    PCELL_HEADER Header = (PCELL_HEADER)((UINT8*)Hive->BaseAddress + CellOffset);
    
    /* Check cell type specific structure */
    switch (Header->Signature) {
        case CellTypeKey:
            return HiveCheckKeyCell(Hive, CellOffset);
            
        case CellTypeValue:
            return HiveCheckValueCell(Hive, CellOffset);
            
        case CellTypeSubkeys:
        case CellTypeData:
        case CellTypeSecurity:
            /* Basic validation already done in HiveValidateCell */
            return TRUE;
            
        default:
            return FALSE;
    }
}

/*
 * Check key cell specific structure
 */
BOOL HiveCheckKeyCell(IN PHIVE Hive, IN UINT32 CellOffset)
{
    PKEY_CELL KeyCell = HiveGetKeyCell(Hive, CellOffset);
    if (!KeyCell) {
        return FALSE;
    }

    /* Check name length */
    if (KeyCell->NameLength > HIVE_MAX_NAME_LENGTH) {
        return FALSE;
    }

    /* Check parent cell if specified */
    if (KeyCell->Parent != 0 && !HiveValidateCell(Hive, KeyCell->Parent)) {
        return FALSE;
    }

    /* Check subkeys list if specified */
    if (KeyCell->SubKeysList != 0 && !HiveValidateCell(Hive, KeyCell->SubKeysList)) {
        return FALSE;
    }

    /* Check values list if specified */
    if (KeyCell->ValuesList != 0 && !HiveValidateCell(Hive, KeyCell->ValuesList)) {
        return FALSE;
    }

    /* Check security cell if specified */
    if (KeyCell->Security != 0 && !HiveValidateCell(Hive, KeyCell->Security)) {
        return FALSE;
    }

    return TRUE;
}

/*
 * Check value cell specific structure
 */
BOOL HiveCheckValueCell(IN PHIVE Hive, IN UINT32 CellOffset)
{
    PVALUE_CELL ValueCell = HiveGetValueCell(Hive, CellOffset);
    if (!ValueCell) {
        return FALSE;
    }

    /* Check name length */
    if (ValueCell->NameLength > HIVE_MAX_NAME_LENGTH) {
        return FALSE;
    }

    /* Check data size */
    if (ValueCell->DataLength > HIVE_MAX_VALUE_SIZE) {
        return FALSE;
    }

    /* Check data cell if data is stored separately */
    if (ValueCell->DataLength > 4 && ValueCell->DataOffset != 0) {
        if (!HiveValidateCell(Hive, ValueCell->DataOffset)) {
            return FALSE;
        }
    }

    return TRUE;
}

/*
 * Perform comprehensive hive integrity check
 */
NTSTATUS HivePerformIntegrityCheck(IN PHIVE Hive, OUT PUINT32 ErrorCount)
{
    if (!Hive || !ErrorCount) {
        return STATUS_INVALID_PARAMETER;
    }

    *ErrorCount = 0;

    /* Check header */
    if (!HiveCheckHeader(Hive)) {
        (*ErrorCount)++;
    }

    /* Check checksum */
    if (!HiveVerifyChecksum(Hive->Header)) {
        (*ErrorCount)++;
    }

    /* Check root cell */
    if (!Hive->Header || !HiveCheckCellStructure(Hive, Hive->Header->RootCell)) {
        (*ErrorCount)++;
    }

    /* Walk through all cells */
    UINT32 Offset = sizeof(HIVE_HEADER);
    while (Hive->Header && Offset < Hive->Header->Length) {
        PCELL_HEADER Cell = (PCELL_HEADER)((UINT8*)Hive->BaseAddress + Offset);
        
        /* Check basic cell structure */
        if (!HiveBinValidateBlock(Hive, Offset)) {
            (*ErrorCount)++;
            break; /* Cannot continue if basic structure is corrupt */
        }

        /* Check allocated cells */
        if (Cell->Size < 0) {
            if (!HiveCheckCellStructure(Hive, Offset)) {
                (*ErrorCount)++;
            }
        }
        
        Offset += abs(Cell->Size);
    }

    return (*ErrorCount == 0) ? STATUS_SUCCESS : STATUS_UNSUCCESSFUL;
}

/*
 * Check for circular references in key hierarchy
 */
BOOL HiveCheckCircularReferences(IN PHIVE Hive, IN UINT32 StartCell, IN UINT32 MaxDepth)
{
    if (!Hive || StartCell == 0 || MaxDepth == 0) {
        return FALSE;
    }

    UINT32 VisitedCells[256]; /* Track visited cells */
    UINT32 VisitedCount = 0;
    UINT32 CurrentCell = StartCell;
    UINT32 Depth = 0;

    while (CurrentCell != 0 && Depth < MaxDepth) {
        /* Check if we've seen this cell before */
        for (UINT32 i = 0; i < VisitedCount; i++) {
            if (VisitedCells[i] == CurrentCell) {
                return TRUE; /* Circular reference detected */
            }
        }

        /* Add to visited list */
        if (VisitedCount < 256) {
            VisitedCells[VisitedCount++] = CurrentCell;
        }

        /* Get parent cell */
        PKEY_CELL KeyCell = HiveGetKeyCell(Hive, CurrentCell);
        if (!KeyCell) {
            break;
        }

        CurrentCell = KeyCell->Parent;
        Depth++;
    }

    return FALSE; /* No circular reference found */
}

/*
 * Repair hive checksum
 */
NTSTATUS HiveRepairChecksum(IN PHIVE Hive)
{
    if (!Hive) {
        return STATUS_INVALID_PARAMETER;
    }

    HiveAcquireLock(Hive);
    
    if (Hive->Header) {
        Hive->Header->Checksum = HiveCalculateChecksum(Hive->Header);
    }
    Hive->Dirty = TRUE;
    
    HiveReleaseLock(Hive);
    
    return STATUS_SUCCESS;
}

/*
 * Basic integrity check entry point
 */
NTSTATUS HiveCheckIntegrity(IN PHIVE Hive, IN PVOID BaseAddress, IN SIZE_T Size)
{
    if (!Hive || !BaseAddress || Size < sizeof(HIVE_HEADER)) {
        return STATUS_INVALID_PARAMETER;
    }

    PHIVE_HEADER Header = (PHIVE_HEADER)BaseAddress;
    if (Header->Signature != HIVE_SIGNATURE) {
        return STATUS_INVALID_HIVE_SIGNATURE;
    }

    if (!HiveVerifyChecksum(Header)) {
        return STATUS_HIVE_CHECKSUM_MISMATCH;
    }

    /* Minimal structural pass */
    UINT32 Offset = sizeof(HIVE_HEADER);
    while (Offset + sizeof(CELL_HEADER) <= Size) {
        PCELL_HEADER Cell = (PCELL_HEADER)((UINT8*)BaseAddress + Offset);
        UINT32 step = (UINT32)abs(Cell->Size);
        if (step == 0) break;
        if (Offset + step > Size) {
            return STATUS_HIVE_SIZE_MISMATCH;
        }
        Offset += step;
    }

    return STATUS_SUCCESS;
}

/*
 * Validate hive file format
 */
BOOL HiveValidateFileFormat(IN PVOID FileData, IN SIZE_T FileSize)
{
    if (!FileData || FileSize < sizeof(HIVE_HEADER)) {
        return FALSE;
    }

    PHIVE_HEADER Header = (PHIVE_HEADER)FileData;
    
    /* Check signature */
    if (Header->Signature != HIVE_SIGNATURE) {
        return FALSE;
    }

    /* Check file size consistency */
    if (Header->Length > FileSize) {
        return FALSE;
    }

    /* Check basic header fields */
    if (Header->MajorVersion == 0 || Header->RootCell == 0) {
        return FALSE;
    }

    return TRUE;
}

/*
 * Get hive health status
 */
NTSTATUS HiveGetHealthStatus(IN PHIVE Hive, OUT PUINT32 HealthScore, OUT PUINT32 IssueCount)
{
    if (!Hive || !HealthScore || !IssueCount) {
        return STATUS_INVALID_PARAMETER;
    }

    UINT32 ErrorCount = 0;
    NTSTATUS Status = HivePerformIntegrityCheck(Hive, &ErrorCount);
    
    *IssueCount = ErrorCount;
    
    /* Calculate health score (0-100) */
    if (ErrorCount == 0) {
        *HealthScore = 100;
    } else if (ErrorCount < 5) {
        *HealthScore = 80;
    } else if (ErrorCount < 20) {
        *HealthScore = 60;
    } else if (ErrorCount < 50) {
        *HealthScore = 40;
    } else {
        *HealthScore = 0;
    }

    return Status;
}
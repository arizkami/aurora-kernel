/*
 * Aurora Kernel - Registry Hive Cell Management
 * Copyright (c) 2024 NTCore Project
 */

#include "hive.h"

/* Cell management functions for registry hive */

/*
 * Allocate a new cell in the hive
 */
UINT32 HiveAllocateCell(IN PHIVE Hive, IN SIZE_T Size)
{
    if (!Hive || Size == 0) {
        return 0;
    }

    /* Add space for cell header */
    SIZE_T TotalSize = Size + sizeof(CELL_HEADER);
    
    /* Align to 8-byte boundary */
    TotalSize = (TotalSize + 7) & ~7;
    
    return HiveBinAllocateBlock(Hive, TotalSize);
}

/*
 * Free a cell in the hive
 */
VOID HiveFreeCell(IN PHIVE Hive, IN UINT32 CellOffset)
{
    if (!Hive || CellOffset == 0) {
        return;
    }

    HiveBinFreeBlock(Hive, CellOffset);
}

/*
 * Get cell data pointer
 */
PVOID HiveGetCell(IN PHIVE Hive, IN UINT32 CellOffset)
{
    if (!Hive || CellOffset == 0 || CellOffset >= Hive->Size) {
        return NULL;
    }

    PCELL_HEADER Cell = (PCELL_HEADER)((UINT8*)Hive->BaseAddress + CellOffset);
    
    /* Verify cell is allocated */
    if (Cell->Size >= 0) {
        return NULL;
    }

    return (UINT8*)Cell + sizeof(CELL_HEADER);
}

/*
 * Validate cell structure and integrity
 */
BOOL HiveValidateCell(IN PHIVE Hive, IN UINT32 CellOffset)
{
    if (!Hive || CellOffset == 0 || CellOffset >= Hive->Size) {
        return FALSE;
    }

    PCELL_HEADER Cell = (PCELL_HEADER)((UINT8*)Hive->BaseAddress + CellOffset);
    
    /* Check if cell is allocated */
    if (Cell->Size >= 0) {
        return FALSE;
    }

    /* Check bounds */
    if (CellOffset + (-Cell->Size) > Hive->Size) {
        return FALSE;
    }

    /* Check minimum size */
    if (-Cell->Size < sizeof(CELL_HEADER)) {
        return FALSE;
    }

    return TRUE;
}

/*
 * Create a new key cell
 */
UINT32 HiveCreateKeyCell(IN PHIVE Hive, IN PCSTR KeyName, IN UINT32 ParentCell)
{
    if (!Hive || !KeyName) {
        return 0;
    }

    SIZE_T NameLength = strlen(KeyName);
    if (NameLength > HIVE_MAX_NAME_LENGTH) {
        return 0;
    }

    SIZE_T CellSize = sizeof(KEY_CELL) + NameLength;
    UINT32 CellOffset = HiveAllocateCell(Hive, CellSize);
    if (CellOffset == 0) {
        return 0;
    }

    PKEY_CELL KeyCell = (PKEY_CELL)HiveGetCell(Hive, CellOffset);
    if (!KeyCell) {
        HiveFreeCell(Hive, CellOffset);
        return 0;
    }

    /* Initialize key cell */
    PCELL_HEADER Header = (PCELL_HEADER)((UINT8*)KeyCell - sizeof(CELL_HEADER));
    Header->Signature = CellTypeKey;
    Header->Flags = 0;

    KeyCell->Flags = 0;
    KeyCell->LastWriteTime = 0; /* TODO: Get current time */
    KeyCell->Spare = 0;
    KeyCell->Parent = ParentCell;
    KeyCell->SubKeysCount = 0;
    KeyCell->VolatileSubKeysCount = 0;
    KeyCell->SubKeysList = 0;
    KeyCell->VolatileSubKeysList = 0;
    KeyCell->ValuesCount = 0;
    KeyCell->ValuesList = 0;
    KeyCell->Security = 0;
    KeyCell->Class = 0;
    KeyCell->MaxNameLen = 0;
    KeyCell->MaxClassLen = 0;
    KeyCell->MaxValueNameLen = 0;
    KeyCell->MaxValueDataLen = 0;
    KeyCell->WorkVar = 0;
    KeyCell->NameLength = (UINT16)NameLength;
    KeyCell->ClassLength = 0;
    
    /* Copy key name */
    memcpy(KeyCell->Name, KeyName, NameLength);

    Hive->Dirty = TRUE;
    return CellOffset;
}

/*
 * Create a new value cell
 */
UINT32 HiveCreateValueCell(IN PHIVE Hive, IN PCSTR ValueName, IN UINT32 Type, IN PVOID Data, IN UINT32 DataSize)
{
    if (!Hive || !ValueName) {
        return 0;
    }

    SIZE_T NameLength = strlen(ValueName);
    if (NameLength > HIVE_MAX_NAME_LENGTH) {
        return 0;
    }

    SIZE_T CellSize = sizeof(VALUE_CELL) + NameLength;
    UINT32 CellOffset = HiveAllocateCell(Hive, CellSize);
    if (CellOffset == 0) {
        return 0;
    }

    PVALUE_CELL ValueCell = (PVALUE_CELL)HiveGetCell(Hive, CellOffset);
    if (!ValueCell) {
        HiveFreeCell(Hive, CellOffset);
        return 0;
    }

    /* Initialize value cell */
    PCELL_HEADER Header = (PCELL_HEADER)((UINT8*)ValueCell - sizeof(CELL_HEADER));
    Header->Signature = CellTypeValue;
    Header->Flags = 0;

    ValueCell->NameLength = (UINT16)NameLength;
    ValueCell->DataLength = DataSize;
    ValueCell->Type = Type;
    ValueCell->Flags = 0;
    ValueCell->Spare = 0;
    
    /* Handle data storage */
    if (Data && DataSize > 0) {
        if (DataSize <= 4) {
            /* Store small data inline */
            ValueCell->DataOffset = 0;
            memcpy(&ValueCell->DataOffset, Data, DataSize);
        } else {
            /* Allocate separate data cell */
            UINT32 DataOffset = HiveAllocateCell(Hive, DataSize);
            if (DataOffset == 0) {
                HiveFreeCell(Hive, CellOffset);
                return 0;
            }
            
            PVOID DataCell = HiveGetCell(Hive, DataOffset);
            if (!DataCell) {
                HiveFreeCell(Hive, DataOffset);
                HiveFreeCell(Hive, CellOffset);
                return 0;
            }
            
            memcpy(DataCell, Data, DataSize);
            ValueCell->DataOffset = DataOffset;
        }
    } else {
        ValueCell->DataOffset = 0;
    }
    
    /* Copy value name */
    memcpy(ValueCell->Name, ValueName, NameLength);

    Hive->Dirty = TRUE;
    return CellOffset;
}

/*
 * Get key cell by offset
 */
PKEY_CELL HiveGetKeyCell(IN PHIVE Hive, IN UINT32 CellOffset)
{
    if (!HiveValidateCell(Hive, CellOffset)) {
        return NULL;
    }

    PCELL_HEADER Header = (PCELL_HEADER)((UINT8*)Hive->BaseAddress + CellOffset);
    if (Header->Signature != CellTypeKey) {
        return NULL;
    }

    return (PKEY_CELL)HiveGetCell(Hive, CellOffset);
}

/*
 * Get value cell by offset
 */
PVALUE_CELL HiveGetValueCell(IN PHIVE Hive, IN UINT32 CellOffset)
{
    if (!HiveValidateCell(Hive, CellOffset)) {
        return NULL;
    }

    PCELL_HEADER Header = (PCELL_HEADER)((UINT8*)Hive->BaseAddress + CellOffset);
    if (Header->Signature != CellTypeValue) {
        return NULL;
    }

    return (PVALUE_CELL)HiveGetCell(Hive, CellOffset);
}

/*
 * Delete a cell and its associated data
 */
NTSTATUS HiveDeleteCell(IN PHIVE Hive, IN UINT32 CellOffset)
{
    if (!HiveValidateCell(Hive, CellOffset)) {
        return STATUS_INVALID_PARAMETER;
    }

    PCELL_HEADER Header = (PCELL_HEADER)((UINT8*)Hive->BaseAddress + CellOffset);
    
    /* Handle different cell types */
    if (Header->Signature == CellTypeValue) {
        PVALUE_CELL ValueCell = (PVALUE_CELL)HiveGetCell(Hive, CellOffset);
        if (ValueCell && ValueCell->DataLength > 4 && ValueCell->DataOffset != 0) {
            /* Free associated data cell */
            HiveFreeCell(Hive, ValueCell->DataOffset);
        }
    }
    
    /* Free the cell itself */
    HiveFreeCell(Hive, CellOffset);
    Hive->Dirty = TRUE;
    
    return STATUS_SUCCESS;
}

/*
 * Get cell size
 */
SIZE_T HiveGetCellSize(IN PHIVE Hive, IN UINT32 CellOffset)
{
    if (!HiveValidateCell(Hive, CellOffset)) {
        return 0;
    }

    PCELL_HEADER Header = (PCELL_HEADER)((UINT8*)Hive->BaseAddress + CellOffset);
    return -Header->Size - sizeof(CELL_HEADER);
}

/*
 * Resize a cell (allocate new cell and copy data)
 */
UINT32 HiveResizeCell(IN PHIVE Hive, IN UINT32 OldCellOffset, IN SIZE_T NewSize)
{
    if (!HiveValidateCell(Hive, OldCellOffset) || NewSize == 0) {
        return 0;
    }

    SIZE_T OldSize = HiveGetCellSize(Hive, OldCellOffset);
    if (OldSize == 0) {
        return 0;
    }

    /* Allocate new cell */
    UINT32 NewCellOffset = HiveAllocateCell(Hive, NewSize);
    if (NewCellOffset == 0) {
        return 0;
    }

    /* Copy old data */
    PVOID OldData = HiveGetCell(Hive, OldCellOffset);
    PVOID NewData = HiveGetCell(Hive, NewCellOffset);
    
    if (OldData && NewData) {
        SIZE_T CopySize = (OldSize < NewSize) ? OldSize : NewSize;
        memcpy(NewData, OldData, CopySize);
        
        /* Copy cell header signature and flags */
        PCELL_HEADER OldHeader = (PCELL_HEADER)((UINT8*)OldData - sizeof(CELL_HEADER));
        PCELL_HEADER NewHeader = (PCELL_HEADER)((UINT8*)NewData - sizeof(CELL_HEADER));
        NewHeader->Signature = OldHeader->Signature;
        NewHeader->Flags = OldHeader->Flags;
    }

    /* Free old cell */
    HiveFreeCell(Hive, OldCellOffset);
    Hive->Dirty = TRUE;
    
    return NewCellOffset;
}
/*
 * Aurora Kernel - Registry Hive Binary Operations
 * Copyright (c) 2024 NTCore Project
 */

#include "hive.h"

/* Binary block management for hive operations */

/*
 * Allocate a new binary block in the hive
 */
UINT32 HiveBinAllocateBlock(IN PHIVE Hive, IN SIZE_T Size)
{
    if (!Hive || Size == 0) {
        return 0;
    }

    HiveAcquireLock(Hive);

    /* Align size to 8-byte boundary */
    SIZE_T AlignedSize = (Size + 7) & ~7;
    
    /* Search for free block */
    UINT32 Offset = sizeof(HIVE_HEADER);
    while (Offset < Hive->Size) {
        PCELL_HEADER Cell = (PCELL_HEADER)((UINT8*)Hive->BaseAddress + Offset);
        
        if (Cell->Size > 0 && (UINT32)Cell->Size >= AlignedSize) {
            /* Found suitable free block */
            if ((UINT32)Cell->Size > AlignedSize + sizeof(CELL_HEADER)) {
                /* Split the block */
                PCELL_HEADER NewCell = (PCELL_HEADER)((UINT8*)Cell + AlignedSize);
                NewCell->Size = Cell->Size - (INT32)AlignedSize;
                NewCell->Signature = 0;
                NewCell->Flags = 0;
            }
            
            Cell->Size = -(INT32)AlignedSize;
            Cell->Signature = 0;
            Cell->Flags = 0;
            
            Hive->Dirty = TRUE;
            HiveReleaseLock(Hive);
            return Offset;
        }
        
        Offset += abs(Cell->Size);
    }

    HiveReleaseLock(Hive);
    return 0; /* No suitable block found */
}

/*
 * Free a binary block in the hive
 */
VOID HiveBinFreeBlock(IN PHIVE Hive, IN UINT32 Offset)
{
    if (!Hive || Offset == 0 || Offset >= Hive->Size) {
        return;
    }

    HiveAcquireLock(Hive);

    PCELL_HEADER Cell = (PCELL_HEADER)((UINT8*)Hive->BaseAddress + Offset);
    if (Cell->Size < 0) {
        /* Mark as free */
        Cell->Size = -Cell->Size;
        Cell->Signature = 0;
        Cell->Flags = 0;
        
        /* Try to coalesce with next block */
        UINT32 NextOffset = Offset + Cell->Size;
        if (NextOffset < Hive->Size) {
            PCELL_HEADER NextCell = (PCELL_HEADER)((UINT8*)Hive->BaseAddress + NextOffset);
            if (NextCell->Size > 0) {
                Cell->Size += NextCell->Size;
            }
        }
        
        /* Try to coalesce with previous block */
        UINT32 PrevOffset = sizeof(HIVE_HEADER);
        while (PrevOffset < Offset) {
            PCELL_HEADER PrevCell = (PCELL_HEADER)((UINT8*)Hive->BaseAddress + PrevOffset);
            UINT32 PrevEnd = PrevOffset + abs(PrevCell->Size);
            
            if (PrevEnd == Offset && PrevCell->Size > 0) {
                PrevCell->Size += Cell->Size;
                break;
            }
            
            PrevOffset = PrevEnd;
        }
        
        Hive->Dirty = TRUE;
    }

    HiveReleaseLock(Hive);
}

/*
 * Get binary data from hive block
 */
PVOID HiveBinGetData(IN PHIVE Hive, IN UINT32 Offset, IN SIZE_T Size)
{
    if (!Hive || Offset == 0 || Offset >= Hive->Size) {
        return NULL;
    }

    PCELL_HEADER Cell = (PCELL_HEADER)((UINT8*)Hive->BaseAddress + Offset);
    if (Cell->Size >= 0 || (SIZE_T)(-Cell->Size) < Size + sizeof(CELL_HEADER)) {
        return NULL;
    }

    return (UINT8*)Cell + sizeof(CELL_HEADER);
}

/*
 * Write binary data to hive block
 */
NTSTATUS HiveBinWriteData(IN PHIVE Hive, IN UINT32 Offset, IN PVOID Data, IN SIZE_T Size)
{
    if (!Hive || Offset == 0 || !Data || Size == 0) {
        return STATUS_INVALID_PARAMETER;
    }

    PVOID BlockData = HiveBinGetData(Hive, Offset, Size);
    if (!BlockData) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    HiveAcquireLock(Hive);
    memcpy(BlockData, Data, Size);
    Hive->Dirty = TRUE;
    HiveReleaseLock(Hive);

    return STATUS_SUCCESS;
}

/*
 * Read binary data from hive block
 */
NTSTATUS HiveBinReadData(IN PHIVE Hive, IN UINT32 Offset, OUT PVOID Data, IN SIZE_T Size)
{
    if (!Hive || Offset == 0 || !Data || Size == 0) {
        return STATUS_INVALID_PARAMETER;
    }

    PVOID BlockData = HiveBinGetData(Hive, Offset, Size);
    if (!BlockData) {
        return STATUS_NOT_FOUND;
    }

    memcpy(Data, BlockData, Size);
    return STATUS_SUCCESS;
}

/*
 * Validate binary block structure
 */
BOOL HiveBinValidateBlock(IN PHIVE Hive, IN UINT32 Offset)
{
    if (!Hive || Offset == 0 || Offset >= Hive->Size) {
        return FALSE;
    }

    PCELL_HEADER Cell = (PCELL_HEADER)((UINT8*)Hive->BaseAddress + Offset);
    
    /* Check if offset + size is within hive bounds */
    if (Offset + abs(Cell->Size) > Hive->Size) {
        return FALSE;
    }

    /* Check minimum size */
    if (abs(Cell->Size) < sizeof(CELL_HEADER)) {
        return FALSE;
    }

    return TRUE;
}

/*
 * Compact hive binary data
 */
NTSTATUS HiveBinCompact(IN PHIVE Hive)
{
    if (!Hive) {
        return STATUS_INVALID_PARAMETER;
    }

    HiveAcquireLock(Hive);

    UINT32 ReadOffset = sizeof(HIVE_HEADER);
    UINT32 WriteOffset = sizeof(HIVE_HEADER);
    
    while (ReadOffset < Hive->Size) {
        PCELL_HEADER ReadCell = (PCELL_HEADER)((UINT8*)Hive->BaseAddress + ReadOffset);
        
        if (ReadCell->Size < 0) {
            /* Allocated block - move it */
            SIZE_T BlockSize = -ReadCell->Size;
            
            if (ReadOffset != WriteOffset) {
                memmove((UINT8*)Hive->BaseAddress + WriteOffset,
                       (UINT8*)Hive->BaseAddress + ReadOffset,
                       BlockSize);
            }
            
            WriteOffset += BlockSize;
        }
        
        ReadOffset += abs(ReadCell->Size);
    }
    
    /* Create one large free block at the end */
    if (WriteOffset < Hive->Size) {
        PCELL_HEADER FreeCell = (PCELL_HEADER)((UINT8*)Hive->BaseAddress + WriteOffset);
        FreeCell->Size = Hive->Size - WriteOffset;
        FreeCell->Signature = 0;
        FreeCell->Flags = 0;
    }
    
    Hive->Dirty = TRUE;
    HiveReleaseLock(Hive);

    return STATUS_SUCCESS;
}

/*
 * Get binary block statistics
 */
NTSTATUS HiveBinGetStatistics(IN PHIVE Hive, OUT PUINT32 AllocatedBlocks, OUT PUINT32 FreeBlocks, OUT PUINT32 TotalSize, OUT PUINT32 FreeSize)
{
    if (!Hive || !AllocatedBlocks || !FreeBlocks || !TotalSize || !FreeSize) {
        return STATUS_INVALID_PARAMETER;
    }

    *AllocatedBlocks = 0;
    *FreeBlocks = 0;
    *TotalSize = 0;
    *FreeSize = 0;

    UINT32 Offset = sizeof(HIVE_HEADER);
    while (Offset < Hive->Size) {
        PCELL_HEADER Cell = (PCELL_HEADER)((UINT8*)Hive->BaseAddress + Offset);
        
        if (Cell->Size < 0) {
            (*AllocatedBlocks)++;
            *TotalSize += -Cell->Size;
        } else {
            (*FreeBlocks)++;
            *FreeSize += Cell->Size;
        }
        
        Offset += abs(Cell->Size);
    }

    return STATUS_SUCCESS;
}
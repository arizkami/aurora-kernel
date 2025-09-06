/*
 * Aurora Kernel - Hive Free Space Management
 * Copyright (c) 2024 NTCore Project
 */

#include "hive.h"

/* Define abs if not available */
#ifndef abs
#define abs(x) ((x) < 0 ? -(x) : (x))
#endif

/* Forward declarations */
static NTSTATUS HiveCoalesceFreeSpace(IN PHIVE Hive, IN UINT32 Offset);

/* API helpers declared in header but not yet implemented elsewhere */
VOID HiveFreeSpaceClear(IN PHIVE Hive)
{
    UNREFERENCED_PARAMETER(Hive);
    /* In a full implementation, this would clear per-hive tracking structures. */
}

VOID HiveFreeSpaceAdd(IN PHIVE Hive, IN UINT32 Offset, IN UINT32 Size)
{
    UNREFERENCED_PARAMETER(Hive);
    UNREFERENCED_PARAMETER(Offset);
    UNREFERENCED_PARAMETER(Size);
    /* In a full implementation, this would record a free extent for the hive. */
}

/* Free space management for registry hive */

/* Free space tracking structure */
typedef struct _FREE_SPACE_ENTRY {
    UINT32 Offset;
    UINT32 Size;
    struct _FREE_SPACE_ENTRY* Next;
} FREE_SPACE_ENTRY, *PFREE_SPACE_ENTRY;

/* Global free space list */
static PFREE_SPACE_ENTRY g_FreeSpaceList = NULL;
static AURORA_SPINLOCK g_FreeSpaceLock;

/*
 * Initialize free space management
 */
NTSTATUS HiveFreeSpaceInitialize(void)
{
    g_FreeSpaceList = NULL;
    /* Initialize spinlock - simplified for kernel */
    g_FreeSpaceLock = 0;
    return STATUS_SUCCESS;
}

/*
 * Shutdown free space management
 */
VOID HiveFreeSpaceShutdown(void)
{
    /* Clean up free space list */
    while (g_FreeSpaceList) {
        PFREE_SPACE_ENTRY Entry = g_FreeSpaceList;
        g_FreeSpaceList = g_FreeSpaceList->Next;
        /* Free entry memory - would use kernel allocator */
    }
}

/*
 * Find free space in hive
 */
UINT32 HiveFindFreeSpace(IN PHIVE Hive, IN SIZE_T RequiredSize)
{
    if (!Hive || RequiredSize == 0) {
        return 0;
    }

    /* Align size to 8-byte boundary */
    SIZE_T AlignedSize = (RequiredSize + 7) & ~7;
    
    UINT32 Offset = sizeof(HIVE_HEADER);
    while (Offset < Hive->Size) {
        PCELL_HEADER Cell = (PCELL_HEADER)((UINT8*)Hive->BaseAddress + Offset);
        
        /* Check if this is a free block */
        if (Cell->Size > 0 && (UINT32)Cell->Size >= AlignedSize) {
            return Offset;
        }
        
        Offset += abs(Cell->Size);
    }

    return 0; /* No suitable free space found */
}

/*
 * Mark space as free
 */
NTSTATUS HiveMarkSpaceAsFree(IN PHIVE Hive, IN UINT32 Offset, IN SIZE_T Size)
{
    if (!Hive || Offset == 0 || Size == 0) {
        return STATUS_INVALID_PARAMETER;
    }

    if (Offset + Size > Hive->Size) {
        return STATUS_INVALID_PARAMETER;
    }

    HiveAcquireLock(Hive);

    PCELL_HEADER Cell = (PCELL_HEADER)((UINT8*)Hive->BaseAddress + Offset);
    Cell->Size = (INT32)Size;
    Cell->Signature = 0;
    Cell->Flags = 0;
    
    /* Try to coalesce with adjacent free blocks */
    HiveCoalesceFreeSpace(Hive, Offset);
    
    Hive->Dirty = TRUE;
    HiveReleaseLock(Hive);

    return STATUS_SUCCESS;
}

/*
 * Coalesce adjacent free spaces
 */
static NTSTATUS HiveCoalesceFreeSpace(IN PHIVE Hive, IN UINT32 Offset)
{
    if (!Hive || Offset == 0) {
        return STATUS_INVALID_PARAMETER;
    }

    PCELL_HEADER Cell = (PCELL_HEADER)((UINT8*)Hive->BaseAddress + Offset);
    if (Cell->Size <= 0) {
        return STATUS_INVALID_PARAMETER; /* Not a free block */
    }

    /* Coalesce with next block */
    UINT32 NextOffset = Offset + Cell->Size;
    if (NextOffset < Hive->Size) {
        PCELL_HEADER NextCell = (PCELL_HEADER)((UINT8*)Hive->BaseAddress + NextOffset);
        if (NextCell->Size > 0) {
            Cell->Size += NextCell->Size;
        }
    }

    /* Coalesce with previous block */
    UINT32 PrevOffset = sizeof(HIVE_HEADER);
    while (PrevOffset < Offset) {
        PCELL_HEADER PrevCell = (PCELL_HEADER)((UINT8*)Hive->BaseAddress + PrevOffset);
        UINT32 PrevEnd = PrevOffset + abs(PrevCell->Size);
        
        if (PrevEnd == Offset && PrevCell->Size > 0) {
            /* Previous block is free and adjacent */
            PrevCell->Size += Cell->Size;
            break;
        }
        
        PrevOffset = PrevEnd;
    }

    return STATUS_SUCCESS;
}

/*
 * Get free space statistics
 */
NTSTATUS HiveGetFreeSpaceStatistics(IN PHIVE Hive, OUT PUINT32 FreeBlocks, OUT PUINT32 TotalFreeSize, OUT PUINT32 LargestFreeBlock)
{
    if (!Hive || !FreeBlocks || !TotalFreeSize || !LargestFreeBlock) {
        return STATUS_INVALID_PARAMETER;
    }

    *FreeBlocks = 0;
    *TotalFreeSize = 0;
    *LargestFreeBlock = 0;

    UINT32 Offset = sizeof(HIVE_HEADER);
    while (Offset < Hive->Size) {
        PCELL_HEADER Cell = (PCELL_HEADER)((UINT8*)Hive->BaseAddress + Offset);
        
        if (Cell->Size > 0) {
            (*FreeBlocks)++;
            *TotalFreeSize += Cell->Size;
            
            if ((UINT32)Cell->Size > *LargestFreeBlock) {
                *LargestFreeBlock = Cell->Size;
            }
        }
        
        Offset += abs(Cell->Size);
    }

    return STATUS_SUCCESS;
}

/*
 * Defragment hive free space
 */
NTSTATUS HiveDefragmentFreeSpace(IN PHIVE Hive)
{
    if (!Hive) {
        return STATUS_INVALID_PARAMETER;
    }

    HiveAcquireLock(Hive);

    BOOL Changed = TRUE;
    while (Changed) {
        Changed = FALSE;
        
        UINT32 Offset = sizeof(HIVE_HEADER);
        while (Offset < Hive->Size) {
            PCELL_HEADER Cell = (PCELL_HEADER)((UINT8*)Hive->BaseAddress + Offset);
            
            if (Cell->Size > 0) {
                /* Try to coalesce this free block */
                UINT32 OriginalSize = Cell->Size;
                HiveCoalesceFreeSpace(Hive, Offset);
                
                if (Cell->Size != OriginalSize) {
                    Changed = TRUE;
                }
            }
            
            Offset += abs(Cell->Size);
        }
    }
    
    Hive->Dirty = TRUE;
    HiveReleaseLock(Hive);

    return STATUS_SUCCESS;
}

/*
 * Allocate from free space
 */
UINT32 HiveAllocateFromFreeSpace(IN PHIVE Hive, IN SIZE_T Size)
{
    if (!Hive || Size == 0) {
        return 0;
    }

    /* Align size to 8-byte boundary */
    SIZE_T AlignedSize = (Size + 7) & ~7;
    
    UINT32 FreeOffset = HiveFindFreeSpace(Hive, AlignedSize);
    if (FreeOffset == 0) {
        return 0;
    }

    HiveAcquireLock(Hive);

    PCELL_HEADER FreeCell = (PCELL_HEADER)((UINT8*)Hive->BaseAddress + FreeOffset);
    
    /* Check if we need to split the free block */
    if ((UINT32)FreeCell->Size > AlignedSize + sizeof(CELL_HEADER)) {
        /* Split the block */
        UINT32 RemainingSize = FreeCell->Size - AlignedSize;
        PCELL_HEADER NewFreeCell = (PCELL_HEADER)((UINT8*)FreeCell + AlignedSize);
        
        NewFreeCell->Size = RemainingSize;
        NewFreeCell->Signature = 0;
        NewFreeCell->Flags = 0;
    }
    
    /* Mark the allocated block */
    FreeCell->Size = -(INT32)AlignedSize;
    FreeCell->Signature = 0;
    FreeCell->Flags = 0;
    
    Hive->Dirty = TRUE;
    HiveReleaseLock(Hive);

    return FreeOffset;
}

/*
 * Check free space fragmentation level
 */
UINT32 HiveGetFragmentationLevel(IN PHIVE Hive)
{
    if (!Hive) {
        return 100; /* Maximum fragmentation */
    }

    UINT32 FreeBlocks = 0;
    UINT32 TotalFreeSize = 0;
    UINT32 LargestFreeBlock = 0;
    
    NTSTATUS Status = HiveGetFreeSpaceStatistics(Hive, &FreeBlocks, &TotalFreeSize, &LargestFreeBlock);
    if (!NT_SUCCESS(Status) || TotalFreeSize == 0) {
        return 0; /* No fragmentation if no free space */
    }

    /* Calculate fragmentation percentage */
    /* Higher number of blocks relative to total size indicates more fragmentation */
    UINT32 FragmentationScore = (FreeBlocks * 100) / (TotalFreeSize / 1024 + 1);
    
    /* Also consider largest block ratio */
    UINT32 LargestBlockRatio = (LargestFreeBlock * 100) / TotalFreeSize;
    
    /* Combine metrics (lower largest block ratio = higher fragmentation) */
    UINT32 FinalScore = FragmentationScore + (100 - LargestBlockRatio);
    
    return (FinalScore > 100) ? 100 : FinalScore;
}

/*
 * Optimize free space layout
 */
NTSTATUS HiveOptimizeFreeSpace(IN PHIVE Hive)
{
    if (!Hive) {
        return STATUS_INVALID_PARAMETER;
    }

    /* First defragment to coalesce adjacent blocks */
    NTSTATUS Status = HiveDefragmentFreeSpace(Hive);
    if (!NT_SUCCESS(Status)) {
        return Status;
    }

    /* Then compact the hive to move all free space to the end */
    Status = HiveBinCompact(Hive);
    if (!NT_SUCCESS(Status)) {
        return Status;
    }

    return STATUS_SUCCESS;
}

/*
 * Get free space map
 */
NTSTATUS HiveGetFreeSpaceMap(IN PHIVE Hive, OUT PUINT32 FreeSpaceMap, IN UINT32 MapSize, OUT PUINT32 EntriesReturned)
{
    if (!Hive || !FreeSpaceMap || MapSize == 0 || !EntriesReturned) {
        return STATUS_INVALID_PARAMETER;
    }

    *EntriesReturned = 0;
    UINT32 MapIndex = 0;

    UINT32 Offset = sizeof(HIVE_HEADER);
    while (Offset < Hive->Size && MapIndex < MapSize - 1) {
        PCELL_HEADER Cell = (PCELL_HEADER)((UINT8*)Hive->BaseAddress + Offset);
        
        if (Cell->Size > 0) {
            /* Store offset and size as pairs */
            FreeSpaceMap[MapIndex++] = Offset;
            FreeSpaceMap[MapIndex++] = Cell->Size;
            (*EntriesReturned)++;
        }
        
        Offset += abs(Cell->Size);
    }

    return STATUS_SUCCESS;
}
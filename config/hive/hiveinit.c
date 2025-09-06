/*
 * Aurora Kernel - Registry Hive Initialization
 * Copyright (c) 2024 NTCore Project
 */

#include "hive.h"

#ifndef abs
#define abs(x) ((x) < 0 ? -(x) : (x))
#endif

/* Global hive list */
static PHIVE g_HiveList = NULL;
static AURORA_SPINLOCK g_HiveListLock = 0;
static UINT32 g_HiveCount = 0;

/* Default hive names */
static const CHAR* g_DefaultHiveNames[] = {
    "SYSTEM",
    "SOFTWARE",
    "SECURITY",
    "SAM",
    "DEFAULT",
    "USERS",
    "HARDWARE"
};

/*
 * Initialize the hive management system
 */
NTSTATUS HiveInitializeSystem(void)
{
    g_HiveList = NULL;
    g_HiveListLock = 0;
    g_HiveCount = 0;
    
    /* Initialize hint system */
    NTSTATUS Status = HiveHintInitialize();
    if (!NT_SUCCESS(Status)) {
        return Status;
    }
    
    /* Initialize free space management */
    Status = HiveFreeSpaceInitialize();
    if (!NT_SUCCESS(Status)) {
        HiveHintShutdown();
        return Status;
    }
    
    return STATUS_SUCCESS;
}

/*
 * Shutdown the hive management system
 */
VOID HiveShutdownSystem(void)
{
    /* Shutdown all hives */
    while (g_HiveList) {
        PHIVE Hive = g_HiveList;
        g_HiveList = g_HiveList->Next;
        HiveClose(Hive);
    }
    
    g_HiveCount = 0;
    
    /* Shutdown subsystems */
    HiveFreeSpaceShutdown();
    HiveHintShutdown();
}

/*
 * Initialize a new hive structure
 */
NTSTATUS HiveInitialize(OUT PHIVE* Hive, IN PCSTR Name, IN HIVE_TYPE Type)
{
    if (!Hive || !Name) {
        return STATUS_INVALID_PARAMETER;
    }

    /* Allocate hive structure */
    /* In real implementation, would use kernel allocator */
    PHIVE NewHive = NULL; /* KernAllocateMemory(sizeof(HIVE)); */
    if (!NewHive) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    /* Initialize hive structure */
    memset(NewHive, 0, sizeof(HIVE));
    
    /* Copy name */
    strncpy(NewHive->Name, Name, sizeof(NewHive->Name) - 1);
    NewHive->Name[sizeof(NewHive->Name) - 1] = '\0';
    
    NewHive->Type = Type;
    NewHive->Signature = HIVE_SIGNATURE;
    NewHive->Version = HIVE_VERSION;
    NewHive->Flags = 0;
    NewHive->Size = 0;
    NewHive->BaseAddress = NULL;
    NewHive->RootKeyOffset = 0;
    NewHive->DirtyFlag = FALSE;
    NewHive->ReadOnly = FALSE;
    NewHive->Lock = 0;
    NewHive->RefCount = 1;
    NewHive->Next = NULL;
    
    /* Initialize header */
    NewHive->Header = NULL; /* Will be allocated when hive is created/loaded */
    
    *Hive = NewHive;
    return STATUS_SUCCESS;
}

/*
 * Create a new empty hive
 */
NTSTATUS HiveCreate(IN PHIVE Hive, IN UINT32 InitialSize)
{
    if (!Hive || InitialSize < sizeof(HIVE_HEADER)) {
        return STATUS_INVALID_PARAMETER;
    }

    /* Allocate hive memory */
    /* In real implementation, would use kernel allocator */
    PVOID HiveMemory = NULL; /* KernAllocateMemory(InitialSize); */
    if (!HiveMemory) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    memset(HiveMemory, 0, InitialSize);
    
    Hive->BaseAddress = HiveMemory;
    Hive->Size = InitialSize;
    
    /* Initialize hive header */
    PHIVE_HEADER Header = (PHIVE_HEADER)HiveMemory;
    Header->Signature = HIVE_HEADER_SIGNATURE;
    Header->Version = HIVE_VERSION;
    Header->Size = InitialSize;
    Header->RootKeyOffset = 0; /* Will be set when root key is created */
    Header->Checksum = 0;
    Header->Timestamp = 0; /* TODO: Get current time */
    Header->Flags = 0;
    
    /* Calculate and set checksum */
    Header->Checksum = HiveCalculateChecksum(Header);
    
    Hive->Header = Header;
    
    /* Create initial free space entry */
    UINT32 FreeSpaceOffset = sizeof(HIVE_HEADER);
    UINT32 FreeSpaceSize = InitialSize - sizeof(HIVE_HEADER);
    
    PCELL_HEADER FreeCell = (PCELL_HEADER)((UINT8*)HiveMemory + FreeSpaceOffset);
    FreeCell->Size = (INT32)FreeSpaceSize; /* Positive size indicates free cell */
    FreeCell->Signature = CellTypeFree;
    
    /* Initialize free space management for this hive */
    HiveFreeSpaceAdd(Hive, FreeSpaceOffset, FreeSpaceSize);
    
    return STATUS_SUCCESS;
}

/*
 * Load an existing hive from memory
 */
NTSTATUS HiveLoad(IN PHIVE Hive, IN PVOID HiveData, IN UINT32 HiveSize)
{
    if (!Hive || !HiveData || HiveSize < sizeof(HIVE_HEADER)) {
        return STATUS_INVALID_PARAMETER;
    }

    /* Validate hive header */
    PHIVE_HEADER Header = (PHIVE_HEADER)HiveData;
    if (Header->Signature != HIVE_HEADER_SIGNATURE) {
        return STATUS_INVALID_HIVE_SIGNATURE;
    }
    
    if (Header->Size != HiveSize) {
        return STATUS_HIVE_SIZE_MISMATCH;
    }
    
    /* Verify checksum */
    UINT32 StoredChecksum = Header->Checksum;
    Header->Checksum = 0;
    UINT32 CalculatedChecksum = HiveCalculateChecksum(Header);
    Header->Checksum = StoredChecksum;
    
    if (StoredChecksum != CalculatedChecksum) {
        return STATUS_HIVE_CHECKSUM_MISMATCH;
    }
    
    /* Perform integrity check */
    NTSTATUS Status = HiveCheckIntegrity(Hive, HiveData, HiveSize);
    if (!NT_SUCCESS(Status)) {
        return Status;
    }
    
    /* Set hive properties */
    Hive->BaseAddress = HiveData;
    Hive->Size = HiveSize;
    Hive->Header = Header;
    Hive->RootKeyOffset = Header->RootKeyOffset;
    
    /* Rebuild free space map */
    Status = HiveRebuildFreeSpaceMap(Hive);
    if (!NT_SUCCESS(Status)) {
        return Status;
    }
    
    return STATUS_SUCCESS;
}

/*
 * Close and cleanup a hive
 */
NTSTATUS HiveClose(IN PHIVE Hive)
{
    if (!Hive) {
        return STATUS_INVALID_PARAMETER;
    }

    /* Decrement reference count */
    if (--Hive->RefCount > 0) {
        return STATUS_SUCCESS;
    }
    
    /* Flush any pending changes */
    if (Hive->DirtyFlag && !Hive->ReadOnly) {
        HiveFlush(Hive);
    }
    
    /* Remove from global hive list */
    HiveRemoveFromList(Hive);
    
    /* Clear hints for this hive */
    HiveClearHints(Hive);
    
    /* Free hive memory */
    if (Hive->BaseAddress) {
        /* In real implementation, would use kernel allocator */
        /* KernFreeMemory(Hive->BaseAddress); */
    }
    
    /* Free hive structure */
    /* KernFreeMemory(Hive); */
    
    return STATUS_SUCCESS;
}

/*
 * Add hive to global list
 */
NTSTATUS HiveAddToList(IN PHIVE Hive)
{
    if (!Hive) {
        return STATUS_INVALID_PARAMETER;
    }

    /* Add to head of list */
    Hive->Next = g_HiveList;
    g_HiveList = Hive;
    g_HiveCount++;
    
    return STATUS_SUCCESS;
}

/*
 * Remove hive from global list
 */
NTSTATUS HiveRemoveFromList(IN PHIVE Hive)
{
    if (!Hive) {
        return STATUS_INVALID_PARAMETER;
    }

    if (g_HiveList == Hive) {
        g_HiveList = Hive->Next;
        g_HiveCount--;
        return STATUS_SUCCESS;
    }
    
    PHIVE Current = g_HiveList;
    while (Current && Current->Next != Hive) {
        Current = Current->Next;
    }
    
    if (Current) {
        Current->Next = Hive->Next;
        g_HiveCount--;
        return STATUS_SUCCESS;
    }
    
    return STATUS_NOT_FOUND;
}

/*
 * Find hive by name
 */
PHIVE HiveFindByName(IN PCSTR Name)
{
    if (!Name) {
        return NULL;
    }

    PHIVE Current = g_HiveList;
    while (Current) {
        if (strcmp(Current->Name, Name) == 0) {
            Current->RefCount++;
            return Current;
        }
        Current = Current->Next;
    }
    
    return NULL;
}

/*
 * Get hive count
 */
UINT32 HiveGetCount(void)
{
    return g_HiveCount;
}

/*
 * Enumerate all hives
 */
NTSTATUS HiveEnumerate(OUT PHIVE* HiveArray, IN UINT32 MaxHives, OUT PUINT32 HiveCount)
{
    if (!HiveArray || MaxHives == 0 || !HiveCount) {
        return STATUS_INVALID_PARAMETER;
    }

    *HiveCount = 0;
    PHIVE Current = g_HiveList;
    
    while (Current && *HiveCount < MaxHives) {
        HiveArray[*HiveCount] = Current;
        Current->RefCount++; /* Increment reference count */
        (*HiveCount)++;
        Current = Current->Next;
    }
    
    return STATUS_SUCCESS;
}

/*
 * Initialize default system hives
 */
NTSTATUS HiveInitializeDefaults(void)
{
    NTSTATUS Status = STATUS_SUCCESS;
    
    for (UINT32 i = 0; i < sizeof(g_DefaultHiveNames) / sizeof(g_DefaultHiveNames[0]); i++) {
        PHIVE Hive;
        Status = HiveInitialize(&Hive, g_DefaultHiveNames[i], HiveTypeSystem);
        if (!NT_SUCCESS(Status)) {
            break;
        }
        
        /* Create empty hive with default size */
        Status = HiveCreate(Hive, 64 * 1024); /* 64KB initial size */
        if (!NT_SUCCESS(Status)) {
            HiveClose(Hive);
            break;
        }
        
        /* Add to global list */
        Status = HiveAddToList(Hive);
        if (!NT_SUCCESS(Status)) {
            HiveClose(Hive);
            break;
        }
    }
    
    return Status;
}

/*
 * Flush hive changes to storage
 */
NTSTATUS HiveFlush(IN PHIVE Hive)
{
    if (!Hive) {
        return STATUS_INVALID_PARAMETER;
    }

    if (!Hive->DirtyFlag) {
        return STATUS_SUCCESS; /* Nothing to flush */
    }
    
    if (Hive->ReadOnly) {
        return STATUS_ACCESS_DENIED;
    }
    
    /* Update header timestamp */
    if (Hive->Header) {
        Hive->Header->Timestamp = 0; /* TODO: Get current time */
        
        /* Recalculate checksum */
        UINT32 OldChecksum = Hive->Header->Checksum;
        Hive->Header->Checksum = 0;
        Hive->Header->Checksum = HiveCalculateChecksum(Hive->Header);
    }
    
    /* In real implementation, would write to disk/storage */
    /* Status = StorageWriteHive(Hive->Name, Hive->BaseAddress, Hive->Size); */
    
    Hive->DirtyFlag = FALSE;
    return STATUS_SUCCESS;
}

/*
 * Flush all dirty hives
 */
NTSTATUS HiveFlushAll(void)
{
    NTSTATUS Status = STATUS_SUCCESS;
    PHIVE Current = g_HiveList;
    
    while (Current) {
        if (Current->DirtyFlag) {
            NTSTATUS FlushStatus = HiveFlush(Current);
            if (!NT_SUCCESS(FlushStatus) && NT_SUCCESS(Status)) {
                Status = FlushStatus; /* Remember first error */
            }
        }
        Current = Current->Next;
    }
    
    return Status;
}

/*
 * Rebuild free space map for a hive
 */
NTSTATUS HiveRebuildFreeSpaceMap(IN PHIVE Hive)
{
    if (!Hive || !Hive->BaseAddress) {
        return STATUS_INVALID_PARAMETER;
    }

    /* Clear existing free space map */
    HiveFreeSpaceClear(Hive);
    
    /* Scan hive and rebuild free space map */
    UINT32 Offset = sizeof(HIVE_HEADER);
    while (Offset < Hive->Size) {
        PCELL_HEADER Cell = (PCELL_HEADER)((UINT8*)Hive->BaseAddress + Offset);
        
        if (Cell->Size > 0) {
            /* Free cell */
            HiveFreeSpaceAdd(Hive, Offset, Cell->Size);
        }
        
        Offset += abs(Cell->Size);
    }
    
    return STATUS_SUCCESS;
}

/*
 * Get hive statistics
 */
NTSTATUS HiveGetStatistics(IN PHIVE Hive, OUT PHIVE_STATISTICS Statistics)
{
    if (!Hive || !Statistics) {
        return STATUS_INVALID_PARAMETER;
    }

    memset(Statistics, 0, sizeof(HIVE_STATISTICS));
    
    Statistics->TotalSize = Hive->Size;
    Statistics->HeaderSize = sizeof(HIVE_HEADER);
    
    /* Scan hive and collect statistics */
    UINT32 Offset = sizeof(HIVE_HEADER);
    while (Offset < Hive->Size) {
        PCELL_HEADER Cell = (PCELL_HEADER)((UINT8*)Hive->BaseAddress + Offset);
        
        if (Cell->Size > 0) {
            /* Free cell */
            Statistics->FreeSpace += Cell->Size;
            Statistics->FreeCells++;
        } else {
            /* Allocated cell */
            Statistics->UsedSpace += abs(Cell->Size);
            Statistics->AllocatedCells++;
            
            /* Count by type */
            switch (Cell->Signature) {
                case CellTypeKey:
                    Statistics->KeyCells++;
                    break;
                case CellTypeValue:
                    Statistics->ValueCells++;
                    break;
                case CellTypeSecurityDescriptor:
                    Statistics->SecurityCells++;
                    break;
                default:
                    Statistics->OtherCells++;
                    break;
            }
        }
        
        Offset += abs(Cell->Size);
    }
    
    /* Calculate fragmentation percentage */
    if (Statistics->TotalSize > 0) {
        Statistics->FragmentationPercent = 
            (Statistics->FreeCells * 100) / (Statistics->AllocatedCells + Statistics->FreeCells);
    }
    
    return STATUS_SUCCESS;
}

/*
 * Set hive as read-only
 */
NTSTATUS HiveSetReadOnly(IN PHIVE Hive, IN BOOLEAN ReadOnly)
{
    if (!Hive) {
        return STATUS_INVALID_PARAMETER;
    }

    Hive->ReadOnly = ReadOnly;
    return STATUS_SUCCESS;
}

/*
 * Check if hive is dirty
 */
BOOLEAN HiveIsDirty(IN PHIVE Hive)
{
    return Hive ? Hive->DirtyFlag : FALSE;
}

/*
 * Mark hive as dirty
 */
VOID HiveMarkDirty(IN PHIVE Hive)
{
    if (Hive && !Hive->ReadOnly) {
        Hive->DirtyFlag = TRUE;
    }
}
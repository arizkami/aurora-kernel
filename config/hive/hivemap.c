/*
 * Aurora Kernel - Registry Hive Memory Mapping
 * Copyright (c) 2024 NTCore Project
 */

#include "hive.h"

/* Memory mapping constants */
#define HIVE_MAP_GRANULARITY 4096      /* 4KB page size */
#define HIVE_MAX_MAPPED_VIEWS 64       /* Maximum mapped views per hive */
#define HIVE_VIEW_SIZE (64 * 1024)     /* 64KB view size */

/* Memory mapping structures */
typedef struct _HIVE_VIEW {
    PVOID VirtualAddress;       /* Mapped virtual address */
    UINT32 FileOffset;          /* Offset in hive file */
    UINT32 Size;                /* Size of mapped view */
    UINT32 RefCount;            /* Reference count */
    BOOLEAN Dirty;              /* View has been modified */
    struct _HIVE_VIEW* Next;    /* Next view in list */
} HIVE_VIEW, *PHIVE_VIEW;

typedef struct _HIVE_MAPPING {
    PHIVE Hive;                 /* Associated hive */
    PHIVE_VIEW Views;           /* List of mapped views */
    UINT32 ViewCount;           /* Number of active views */
    AURORA_SPINLOCK Lock;       /* Synchronization lock */
    PVOID BaseMapping;          /* Base mapping address */
    UINT32 MappingSize;         /* Total mapping size */
} HIVE_MAPPING, *PHIVE_MAPPING;

/* Global mapping list */
static PHIVE_MAPPING g_HiveMappings = NULL;
static AURORA_SPINLOCK g_MappingLock = 0;

/*
 * Initialize hive memory mapping system
 */
NTSTATUS HiveMapInitialize(void)
{
    g_HiveMappings = NULL;
    g_MappingLock = 0;
    
    return STATUS_SUCCESS;
}

/*
 * Shutdown hive memory mapping system
 */
VOID HiveMapShutdown(void)
{
    /* Unmap all hives */
    while (g_HiveMappings) {
        PHIVE_MAPPING Mapping = g_HiveMappings;
        g_HiveMappings = (PHIVE_MAPPING)Mapping->Views; /* Reuse Next pointer */
        HiveUnmapHive(Mapping->Hive);
    }
}

/*
 * Map hive into virtual memory
 */
NTSTATUS HiveMapHive(IN PHIVE Hive, OUT PVOID* BaseAddress)
{
    if (!Hive || !BaseAddress) {
        return STATUS_INVALID_PARAMETER;
    }

    *BaseAddress = NULL;
    
    /* Check if hive is already mapped */
    PHIVE_MAPPING ExistingMapping = HiveFindMapping(Hive);
    if (ExistingMapping) {
        *BaseAddress = ExistingMapping->BaseMapping;
        return STATUS_SUCCESS;
    }
    
    /* Create new mapping */
    PHIVE_MAPPING Mapping = NULL; /* KernAllocateMemory(sizeof(HIVE_MAPPING)); */
    if (!Mapping) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    
    memset(Mapping, 0, sizeof(HIVE_MAPPING));
    Mapping->Hive = Hive;
    Mapping->Views = NULL;
    Mapping->ViewCount = 0;
    Mapping->Lock = 0;
    
    /* Calculate mapping size (align to page boundary) */
    UINT32 MappingSize = (Hive->Size + HIVE_MAP_GRANULARITY - 1) & ~(HIVE_MAP_GRANULARITY - 1);
    
    /* Map hive data into virtual memory */
    /* In real implementation, would use memory manager */
    PVOID MappedAddress = NULL; /* MmMapViewOfSection(...); */
    if (!MappedAddress) {
        /* KernFreeMemory(Mapping); */
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    
    /* For simulation, use the hive's existing base address */
    MappedAddress = Hive->BaseAddress;
    
    Mapping->BaseMapping = MappedAddress;
    Mapping->MappingSize = MappingSize;
    
    /* Add to global mapping list */
    Mapping->Views = (PHIVE_VIEW)g_HiveMappings; /* Reuse Views pointer for list linkage */
    g_HiveMappings = Mapping;
    
    *BaseAddress = MappedAddress;
    return STATUS_SUCCESS;
}

/*
 * Unmap hive from virtual memory
 */
NTSTATUS HiveUnmapHive(IN PHIVE Hive)
{
    if (!Hive) {
        return STATUS_INVALID_PARAMETER;
    }

    PHIVE_MAPPING Mapping = HiveFindMapping(Hive);
    if (!Mapping) {
        return STATUS_NOT_FOUND;
    }
    
    /* Flush any dirty views */
    HiveFlushMappedViews(Mapping);
    
    /* Unmap all views */
    while (Mapping->Views) {
        PHIVE_VIEW View = Mapping->Views;
        Mapping->Views = View->Next;
        
        /* Unmap view */
        /* MmUnmapViewOfSection(View->VirtualAddress); */
        
        /* Free view structure */
        /* KernFreeMemory(View); */
        Mapping->ViewCount--;
    }
    
    /* Unmap base mapping */
    if (Mapping->BaseMapping && Mapping->BaseMapping != Hive->BaseAddress) {
        /* MmUnmapViewOfSection(Mapping->BaseMapping); */
    }
    
    /* Remove from global list */
    HiveRemoveMapping(Mapping);
    
    /* Free mapping structure */
    /* KernFreeMemory(Mapping); */
    
    return STATUS_SUCCESS;
}

/*
 * Map a view of hive data
 */
NTSTATUS HiveMapView(IN PHIVE Hive, IN UINT32 Offset, IN UINT32 Size, OUT PVOID* ViewAddress)
{
    if (!Hive || !ViewAddress || Size == 0) {
        return STATUS_INVALID_PARAMETER;
    }

    *ViewAddress = NULL;
    
    /* Find hive mapping */
    PHIVE_MAPPING Mapping = HiveFindMapping(Hive);
    if (!Mapping) {
        /* Map hive first */
        PVOID BaseAddress;
        NTSTATUS Status = HiveMapHive(Hive, &BaseAddress);
        if (!NT_SUCCESS(Status)) {
            return Status;
        }
        
        Mapping = HiveFindMapping(Hive);
        if (!Mapping) {
            return STATUS_INTERNAL_ERROR;
        }
    }
    
    /* Check if view already exists */
    PHIVE_VIEW ExistingView = HiveFindView(Mapping, Offset, Size);
    if (ExistingView) {
        ExistingView->RefCount++;
        *ViewAddress = ExistingView->VirtualAddress;
        return STATUS_SUCCESS;
    }
    
    /* Check view limit */
    if (Mapping->ViewCount >= HIVE_MAX_MAPPED_VIEWS) {
        return STATUS_TOO_MANY_VIEWS;
    }
    
    /* Create new view */
    PHIVE_VIEW View = NULL; /* KernAllocateMemory(sizeof(HIVE_VIEW)); */
    if (!View) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    
    /* Align offset and size to page boundaries */
    UINT32 AlignedOffset = Offset & ~(HIVE_MAP_GRANULARITY - 1);
    UINT32 AlignedSize = ((Offset + Size - AlignedOffset) + HIVE_MAP_GRANULARITY - 1) & ~(HIVE_MAP_GRANULARITY - 1);
    
    /* Map view */
    PVOID ViewAddr = (UINT8*)Mapping->BaseMapping + AlignedOffset;
    
    View->VirtualAddress = ViewAddr;
    View->FileOffset = AlignedOffset;
    View->Size = AlignedSize;
    View->RefCount = 1;
    View->Dirty = FALSE;
    View->Next = Mapping->Views;
    
    Mapping->Views = View;
    Mapping->ViewCount++;
    
    /* Calculate actual view address (accounting for alignment) */
    *ViewAddress = (UINT8*)ViewAddr + (Offset - AlignedOffset);
    
    return STATUS_SUCCESS;
}

/*
 * Unmap a view of hive data
 */
NTSTATUS HiveUnmapView(IN PHIVE Hive, IN PVOID ViewAddress)
{
    if (!Hive || !ViewAddress) {
        return STATUS_INVALID_PARAMETER;
    }

    PHIVE_MAPPING Mapping = HiveFindMapping(Hive);
    if (!Mapping) {
        return STATUS_NOT_FOUND;
    }
    
    /* Find view */
    PHIVE_VIEW* ViewPtr = &Mapping->Views;
    while (*ViewPtr) {
        PHIVE_VIEW View = *ViewPtr;
        
        /* Check if address is within this view */
        if (ViewAddress >= View->VirtualAddress && 
            ViewAddress < (UINT8*)View->VirtualAddress + View->Size) {
            
            /* Decrement reference count */
            if (--View->RefCount == 0) {
                /* Remove view from list */
                *ViewPtr = View->Next;
                Mapping->ViewCount--;
                
                /* Flush if dirty */
                if (View->Dirty) {
                    HiveFlushView(Mapping, View);
                }
                
                /* Unmap view */
                /* MmUnmapViewOfSection(View->VirtualAddress); */
                
                /* Free view structure */
                /* KernFreeMemory(View); */
            }
            
            return STATUS_SUCCESS;
        }
        
        ViewPtr = &View->Next;
    }
    
    return STATUS_NOT_FOUND;
}

/*
 * Mark view as dirty
 */
NTSTATUS HiveMarkViewDirty(IN PHIVE Hive, IN PVOID ViewAddress)
{
    if (!Hive || !ViewAddress) {
        return STATUS_INVALID_PARAMETER;
    }

    PHIVE_MAPPING Mapping = HiveFindMapping(Hive);
    if (!Mapping) {
        return STATUS_NOT_FOUND;
    }
    
    /* Find view containing the address */
    PHIVE_VIEW View = Mapping->Views;
    while (View) {
        if (ViewAddress >= View->VirtualAddress && 
            ViewAddress < (UINT8*)View->VirtualAddress + View->Size) {
            
            View->Dirty = TRUE;
            Hive->DirtyFlag = TRUE;
            return STATUS_SUCCESS;
        }
        
        View = View->Next;
    }
    
    return STATUS_NOT_FOUND;
}

/*
 * Flush all dirty views for a hive
 */
NTSTATUS HiveFlushMappedViews(IN PHIVE_MAPPING Mapping)
{
    if (!Mapping) {
        return STATUS_INVALID_PARAMETER;
    }

    NTSTATUS Status = STATUS_SUCCESS;
    PHIVE_VIEW View = Mapping->Views;
    
    while (View) {
        if (View->Dirty) {
            NTSTATUS FlushStatus = HiveFlushView(Mapping, View);
            if (!NT_SUCCESS(FlushStatus) && NT_SUCCESS(Status)) {
                Status = FlushStatus; /* Remember first error */
            }
        }
        
        View = View->Next;
    }
    
    return Status;
}

/*
 * Flush a single view
 */
NTSTATUS HiveFlushView(IN PHIVE_MAPPING Mapping, IN PHIVE_VIEW View)
{
    if (!Mapping || !View || !View->Dirty) {
        return STATUS_INVALID_PARAMETER;
    }

    /* In real implementation, would write view data back to storage */
    /* For now, just mark as clean */
    View->Dirty = FALSE;
    
    return STATUS_SUCCESS;
}

/*
 * Find mapping for hive
 */
PHIVE_MAPPING HiveFindMapping(IN PHIVE Hive)
{
    if (!Hive) {
        return NULL;
    }

    PHIVE_MAPPING Current = g_HiveMappings;
    while (Current) {
        if (Current->Hive == Hive) {
            return Current;
        }
        Current = (PHIVE_MAPPING)Current->Views; /* Reuse Views pointer for list linkage */
    }
    
    return NULL;
}

/*
 * Find view within mapping
 */
PHIVE_VIEW HiveFindView(IN PHIVE_MAPPING Mapping, IN UINT32 Offset, IN UINT32 Size)
{
    if (!Mapping) {
        return NULL;
    }

    PHIVE_VIEW View = Mapping->Views;
    while (View) {
        /* Check if requested range overlaps with existing view */
        if (Offset >= View->FileOffset && 
            Offset + Size <= View->FileOffset + View->Size) {
            return View;
        }
        
        View = View->Next;
    }
    
    return NULL;
}

/*
 * Remove mapping from global list
 */
VOID HiveRemoveMapping(IN PHIVE_MAPPING Mapping)
{
    if (!Mapping) {
        return;
    }

    if (g_HiveMappings == Mapping) {
        g_HiveMappings = (PHIVE_MAPPING)Mapping->Views; /* Reuse Views pointer */
        return;
    }
    
    PHIVE_MAPPING Current = g_HiveMappings;
    while (Current) {
        PHIVE_MAPPING Next = (PHIVE_MAPPING)Current->Views;
        if (Next == Mapping) {
            Current->Views = (PHIVE_VIEW)Next->Views; /* Reuse Views pointer */
            return;
        }
        Current = Next;
    }
}

/*
 * Get mapping statistics
 */
NTSTATUS HiveGetMappingStatistics(IN PHIVE Hive, OUT PUINT32 ViewCount, OUT PUINT32 TotalMappedSize, OUT PUINT32 DirtyViews)
{
    if (!Hive || !ViewCount || !TotalMappedSize || !DirtyViews) {
        return STATUS_INVALID_PARAMETER;
    }

    *ViewCount = 0;
    *TotalMappedSize = 0;
    *DirtyViews = 0;
    
    PHIVE_MAPPING Mapping = HiveFindMapping(Hive);
    if (!Mapping) {
        return STATUS_NOT_FOUND;
    }
    
    PHIVE_VIEW View = Mapping->Views;
    while (View) {
        (*ViewCount)++;
        *TotalMappedSize += View->Size;
        
        if (View->Dirty) {
            (*DirtyViews)++;
        }
        
        View = View->Next;
    }
    
    return STATUS_SUCCESS;
}

/*
 * Prefault pages for better performance
 */
NTSTATUS HivePrefaultPages(IN PHIVE Hive, IN UINT32 Offset, IN UINT32 Size)
{
    if (!Hive || Size == 0) {
        return STATUS_INVALID_PARAMETER;
    }

    PHIVE_MAPPING Mapping = HiveFindMapping(Hive);
    if (!Mapping) {
        return STATUS_NOT_FOUND;
    }
    
    /* Touch pages to bring them into memory */
    UINT32 PageSize = HIVE_MAP_GRANULARITY;
    UINT32 StartPage = Offset / PageSize;
    UINT32 EndPage = (Offset + Size + PageSize - 1) / PageSize;
    
    for (UINT32 Page = StartPage; Page < EndPage; Page++) {
        UINT32 PageOffset = Page * PageSize;
        if (PageOffset < Mapping->MappingSize) {
            /* Touch page to fault it in */
            volatile UINT8* PageAddr = (UINT8*)Mapping->BaseMapping + PageOffset;
            UINT8 Dummy = *PageAddr;
            UNREFERENCED_PARAMETER(Dummy);
        }
    }
    
    return STATUS_SUCCESS;
}

/*
 * Lock pages in memory
 */
NTSTATUS HiveLockPages(IN PHIVE Hive, IN UINT32 Offset, IN UINT32 Size)
{
    if (!Hive || Size == 0) {
        return STATUS_INVALID_PARAMETER;
    }

    /* In real implementation, would lock pages using memory manager */
    /* For now, just prefault the pages */
    return HivePrefaultPages(Hive, Offset, Size);
}

/*
 * Unlock pages from memory
 */
NTSTATUS HiveUnlockPages(IN PHIVE Hive, IN UINT32 Offset, IN UINT32 Size)
{
    if (!Hive || Size == 0) {
        return STATUS_INVALID_PARAMETER;
    }

    /* In real implementation, would unlock pages using memory manager */
    UNREFERENCED_PARAMETER(Offset);
    return STATUS_SUCCESS;
}

/*
 * Map hive read-only
 */
NTSTATUS HiveMapReadOnly(IN PHIVE Hive, OUT PVOID* BaseAddress)
{
    if (!Hive || !BaseAddress) {
        return STATUS_INVALID_PARAMETER;
    }

    /* Map hive normally but mark as read-only */
    NTSTATUS Status = HiveMapHive(Hive, BaseAddress);
    if (NT_SUCCESS(Status)) {
        /* In real implementation, would set page protection to read-only */
        /* MmProtectVirtualMemory(*BaseAddress, Hive->Size, PAGE_READONLY); */
    }
    
    return Status;
}

/*
 * Remap hive with different protection
 */
NTSTATUS HiveRemapWithProtection(IN PHIVE Hive, IN UINT32 Protection)
{
    if (!Hive) {
        return STATUS_INVALID_PARAMETER;
    }

    PHIVE_MAPPING Mapping = HiveFindMapping(Hive);
    if (!Mapping) {
        return STATUS_NOT_FOUND;
    }
    
    /* In real implementation, would change page protection */
    /* MmProtectVirtualMemory(Mapping->BaseMapping, Mapping->MappingSize, Protection); */
    
    UNREFERENCED_PARAMETER(Protection);
    return STATUS_SUCCESS;
}

/*
 * Get virtual address for hive offset
 */
PVOID HiveGetVirtualAddress(IN PHIVE Hive, IN UINT32 Offset)
{
    if (!Hive) {
        return NULL;
    }

    PHIVE_MAPPING Mapping = HiveFindMapping(Hive);
    if (!Mapping || Offset >= Mapping->MappingSize) {
        return NULL;
    }
    
    return (UINT8*)Mapping->BaseMapping + Offset;
}

/*
 * Get hive offset for virtual address
 */
UINT32 HiveGetOffsetFromAddress(IN PHIVE Hive, IN PVOID Address)
{
    if (!Hive || !Address) {
        return 0;
    }

    PHIVE_MAPPING Mapping = HiveFindMapping(Hive);
    if (!Mapping) {
        return 0;
    }
    
    if (Address < Mapping->BaseMapping || 
        Address >= (UINT8*)Mapping->BaseMapping + Mapping->MappingSize) {
        return 0;
    }
    
    return (UINT32)((UINT8*)Address - (UINT8*)Mapping->BaseMapping);
}
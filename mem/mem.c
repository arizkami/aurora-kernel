/*
 * Aurora Kernel - Enhanced Memory Management
 * Copyright (c) 2024 Aurora Project
 */

#include "../include/mem.h"
#include "../include/kern.h"
#include "../aurora.h"

/* Runtime library functions */
#define RtlZeroMemory(Destination, Length) memset((Destination), 0, (Length))
#define RtlFillMemory(Destination, Length, Fill) memset((Destination), (Fill), (Length))
#define RtlCopyMemory(Destination, Source, Length) memcpy((Destination), (Source), (Length))

/* Forward declarations */
static PVOID MemAllocInternal(IN SIZE_T Size);
static VOID MemFreeInternal(IN PVOID Ptr);

/* Physical Memory Manager */
static UINT64 g_TotalPhysicalPages = 0;
static UINT64 g_AvailablePhysicalPages = 0;
static UINT64 g_AllocatedPhysicalPages = 0;
static UINT32* g_PhysicalBitmap = NULL;
static UINT64 g_BitmapSize = 0;
static UINT64 g_PhysicalMemoryBase = 0;

/* Virtual Memory Manager */
static PVIRTUAL_ADDRESS_DESCRIPTOR g_VirtualAddressHead = NULL;
static UINT64 g_KernelVirtualBase = 0xFFFF800000000000ULL; /* Kernel space start */
static UINT64 g_UserVirtualBase = 0x0000000000400000ULL;   /* User space start */

/* Heap Manager */
static UINT8 g_KernelHeap[4 * 1024 * 1024]; /* 4MB heap */
static SIZE_T g_HeapOffset = 0;
static MEMORY_STATISTICS g_MemoryStats = {0};

/* Memory Pool Headers */
typedef struct _POOL_HEADER {
    SIZE_T Size;
    MEMORY_POOL_TYPE PoolType;
    UINT32 Tag;
    struct _POOL_HEADER* Next;
} POOL_HEADER, *PPOOL_HEADER;

static PPOOL_HEADER g_PoolHeaders[4] = {NULL}; /* One for each pool type */

/*
 * Initialize kernel memory manager
 */
NTSTATUS MemInitialize(void)
{
    NTSTATUS Status;
    
    /* Initialize heap */
    g_HeapOffset = 0;
    RtlZeroMemory(&g_MemoryStats, sizeof(MEMORY_STATISTICS));
    
    /* Initialize pool headers */
    for (int i = 0; i < 4; i++) {
        g_PoolHeaders[i] = NULL;
    }
    
    /* Initialize virtual memory manager */
    Status = MemInitializeVirtualManager();
    if (!NT_SUCCESS(Status)) {
        return Status;
    }
    
    /* Initialize heap manager */
    Status = MemInitializeHeapManager();
    if (!NT_SUCCESS(Status)) {
        return Status;
    }
    
    return STATUS_SUCCESS;
}

/*
 * Initialize physical memory manager
 */
NTSTATUS MemInitializePhysicalManager(IN PPHYSICAL_MEMORY_DESCRIPTOR MemoryMap, IN UINT32 DescriptorCount)
{
    UINT64 MaxPhysicalAddress = 0;
    UINT64 TotalMemory = 0;
    
    /* Calculate total physical memory and find highest address */
    for (UINT32 i = 0; i < DescriptorCount; i++) {
        UINT64 EndAddress = (MemoryMap[i].BasePage + MemoryMap[i].PageCount) * AURORA_PAGE_SIZE;
        if (EndAddress > MaxPhysicalAddress) {
            MaxPhysicalAddress = EndAddress;
        }
        TotalMemory += MemoryMap[i].PageCount * AURORA_PAGE_SIZE;
    }
    
    g_TotalPhysicalPages = MaxPhysicalAddress / AURORA_PAGE_SIZE;
    g_BitmapSize = (g_TotalPhysicalPages + 31) / 32; /* 32 bits per UINT32 */
    
    /* Allocate bitmap from heap */
    g_PhysicalBitmap = (UINT32*)MemAllocInternal(g_BitmapSize * sizeof(UINT32));
    if (!g_PhysicalBitmap) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    
    /* Initialize bitmap - mark all pages as allocated initially */
    RtlFillMemory(g_PhysicalBitmap, g_BitmapSize * sizeof(UINT32), 0xFF);
    
    /* Mark available pages as free */
    for (UINT32 i = 0; i < DescriptorCount; i++) {
        if (MemoryMap[i].MemoryType == 1) { /* Available memory */
            for (UINT64 page = MemoryMap[i].BasePage; page < MemoryMap[i].BasePage + MemoryMap[i].PageCount; page++) {
                UINT32 dwordIndex = (UINT32)(page / 32);
                UINT32 bitIndex = (UINT32)(page % 32);
                g_PhysicalBitmap[dwordIndex] &= ~(1U << bitIndex);
                g_AvailablePhysicalPages++;
            }
        }
    }
    
    g_MemoryStats.TotalPhysicalPages = g_TotalPhysicalPages;
    g_MemoryStats.AvailablePhysicalPages = g_AvailablePhysicalPages;
    
    return STATUS_SUCCESS;
}

/*
 * Initialize virtual memory manager
 */
NTSTATUS MemInitializeVirtualManager(void)
{
    g_VirtualAddressHead = NULL;
    g_MemoryStats.TotalVirtualPages = 0x1000000; /* 64TB virtual space */
    g_MemoryStats.AvailableVirtualPages = g_MemoryStats.TotalVirtualPages;
    return STATUS_SUCCESS;
}

/*
 * Initialize heap manager
 */
NTSTATUS MemInitializeHeapManager(void)
{
    g_HeapOffset = 0;
    return STATUS_SUCCESS;
}

/*
 * Allocate physical pages using bitmap allocator
 */
PVOID MemAllocatePhysicalPages(IN SIZE_T PageCount)
{
    if (!g_PhysicalBitmap || PageCount == 0) {
        return NULL;
    }
    
    UINT64 consecutivePages = 0;
    UINT64 startPage = 0;
    
    /* Find consecutive free pages */
    for (UINT64 page = 0; page < g_TotalPhysicalPages; page++) {
        UINT32 dwordIndex = (UINT32)(page / 32);
        UINT32 bitIndex = (UINT32)(page % 32);
        
        if ((g_PhysicalBitmap[dwordIndex] & (1U << bitIndex)) == 0) {
            if (consecutivePages == 0) {
                startPage = page;
            }
            consecutivePages++;
            
            if (consecutivePages >= PageCount) {
                /* Mark pages as allocated */
                for (UINT64 i = 0; i < PageCount; i++) {
                    UINT64 allocPage = startPage + i;
                    UINT32 allocDwordIndex = (UINT32)(allocPage / 32);
                    UINT32 allocBitIndex = (UINT32)(allocPage % 32);
                    g_PhysicalBitmap[allocDwordIndex] |= (1U << allocBitIndex);
                }
                
                g_AvailablePhysicalPages -= PageCount;
                g_AllocatedPhysicalPages += PageCount;
                g_MemoryStats.AvailablePhysicalPages = g_AvailablePhysicalPages;
                g_MemoryStats.AllocatedPhysicalPages = g_AllocatedPhysicalPages;
                
                return (PVOID)(startPage * AURORA_PAGE_SIZE);
            }
        } else {
            consecutivePages = 0;
        }
    }
    
    return NULL; /* No consecutive pages available */
}

/*
 * Free physical pages
 */
VOID MemFreePhysicalPages(IN PVOID PhysicalAddress, IN SIZE_T PageCount)
{
    if (!g_PhysicalBitmap || !PhysicalAddress || PageCount == 0) {
        return;
    }
    
    UINT64 startPage = (UINT64)PhysicalAddress / AURORA_PAGE_SIZE;
    
    /* Mark pages as free */
    for (SIZE_T i = 0; i < PageCount; i++) {
        UINT64 page = startPage + i;
        if (page < g_TotalPhysicalPages) {
            UINT32 dwordIndex = (UINT32)(page / 32);
            UINT32 bitIndex = (UINT32)(page % 32);
            g_PhysicalBitmap[dwordIndex] &= ~(1U << bitIndex);
        }
    }
    
    g_AvailablePhysicalPages += PageCount;
    g_AllocatedPhysicalPages -= PageCount;
    g_MemoryStats.AvailablePhysicalPages = g_AvailablePhysicalPages;
    g_MemoryStats.AllocatedPhysicalPages = g_AllocatedPhysicalPages;
}

/*
 * Internal heap allocator
 */
static PVOID MemAllocInternal(IN SIZE_T Size)
{
    if (Size == 0) {
        return NULL;
    }
    
    /* Align size to pointer boundary */
    Size = MemAlignUp(Size, sizeof(PVOID));
    
    if (g_HeapOffset + Size > sizeof(g_KernelHeap)) {
        return NULL; /* Out of memory */
    }
    
    PVOID Result = &g_KernelHeap[g_HeapOffset];
    g_HeapOffset += Size;
    
    g_MemoryStats.HeapAllocations++;
    g_MemoryStats.HeapBytesAllocated += Size;
    
    return Result;
}

/*
 * Public heap allocator with proper tracking
 */
PVOID MemAlloc(IN SIZE_T Size)
{
    return MemAllocInternal(Size);
}

/*
 * Public memory free
 */
VOID MemFree(IN PVOID Ptr)
{
    MemFreeInternal(Ptr);
}

/*
 * Allocate zero-initialized memory
 */
PVOID MemAllocZero(IN SIZE_T Size)
{
    PVOID Ptr = MemAllocInternal(Size);
    if (Ptr) {
        RtlZeroMemory(Ptr, Size);
    }
    return Ptr;
}

/*
 * Internal memory free
 */
static VOID MemFreeInternal(IN PVOID Ptr)
{
    if (Ptr) {
        g_MemoryStats.HeapDeallocations++;
        /* Note: Actual deallocation not implemented in bump allocator */
    }
}

/*
 * Allocate pages (wrapper for physical page allocation)
 */
PVOID MemAllocPages(IN SIZE_T PageCount)
{
    return MemAllocatePhysicalPages(PageCount);
}

/*
 * Free pages (wrapper for physical page deallocation)
 */
VOID MemFreePages(IN PVOID Base, IN SIZE_T PageCount)
{
    MemFreePhysicalPages(Base, PageCount);
}

/*
 * Get physical address from virtual address (stub)
 */
UINT64 MemGetPhysicalAddress(IN PVOID VirtualAddress)
{
    /* For now, assume identity mapping */
    return (UINT64)VirtualAddress;
}

/*
 * Check if physical address is valid
 */
BOOL MemIsPhysicalAddressValid(IN UINT64 PhysicalAddress)
{
    UINT64 page = PhysicalAddress / AURORA_PAGE_SIZE;
    return (page < g_TotalPhysicalPages);
}

/*
 * Allocate virtual memory
 */
PVOID MemAllocateVirtualMemory(IN SIZE_T Size, IN UINT32 Protection)
{
    /* Simple implementation - allocate from heap for now */
    PVOID Address = MemAlloc(Size);
    if (Address) {
        /* Create virtual address descriptor */
        PVIRTUAL_ADDRESS_DESCRIPTOR Descriptor = (PVIRTUAL_ADDRESS_DESCRIPTOR)MemAllocInternal(sizeof(VIRTUAL_ADDRESS_DESCRIPTOR));
        if (Descriptor) {
            Descriptor->BaseAddress = Address;
            Descriptor->Size = Size;
            Descriptor->Protection = Protection;
            Descriptor->Type = 1; /* Private */
            Descriptor->Next = g_VirtualAddressHead;
            g_VirtualAddressHead = Descriptor;
            
            g_MemoryStats.AllocatedVirtualPages += MemBytesToPages(Size);
            g_MemoryStats.AvailableVirtualPages -= MemBytesToPages(Size);
        }
    }
    return Address;
}

/*
 * Free virtual memory
 */
NTSTATUS MemFreeVirtualMemory(IN PVOID BaseAddress, IN SIZE_T Size)
{
    /* Find and remove virtual address descriptor */
    PVIRTUAL_ADDRESS_DESCRIPTOR* Current = &g_VirtualAddressHead;
    while (*Current) {
        if ((*Current)->BaseAddress == BaseAddress) {
            PVIRTUAL_ADDRESS_DESCRIPTOR ToRemove = *Current;
            *Current = (*Current)->Next;
            
            g_MemoryStats.AllocatedVirtualPages -= MemBytesToPages(ToRemove->Size);
            g_MemoryStats.AvailableVirtualPages += MemBytesToPages(ToRemove->Size);
            
            MemFreeInternal(ToRemove);
            return STATUS_SUCCESS;
        }
        Current = &(*Current)->Next;
    }
    return STATUS_INVALID_PARAMETER;
}

/*
 * Reallocate memory
 */
PVOID MemRealloc(IN PVOID Ptr, IN SIZE_T NewSize)
{
    if (!Ptr) {
        return MemAlloc(NewSize);
    }
    
    if (NewSize == 0) {
        MemFreeInternal(Ptr);
        return NULL;
    }
    
    /* Simple implementation - allocate new and copy */
    PVOID NewPtr = MemAllocInternal(NewSize);
    if (NewPtr && Ptr) {
        /* Note: We don't know the old size, so this is unsafe */
        /* In a real implementation, we'd store allocation sizes */
        RtlCopyMemory(NewPtr, Ptr, NewSize);
        MemFree(Ptr);
    }
    
    return NewPtr;
}

/*
 * Allocate from specific pool
 */
PVOID MemAllocFromPool(IN MEMORY_POOL_TYPE PoolType, IN SIZE_T Size)
{
    if (PoolType >= 4) {
        return NULL;
    }
    
    /* Allocate pool header + data */
    SIZE_T TotalSize = sizeof(POOL_HEADER) + Size;
    PPOOL_HEADER Header = (PPOOL_HEADER)MemAllocInternal(TotalSize);
    if (!Header) {
        return NULL;
    }
    
    Header->Size = Size;
    Header->PoolType = PoolType;
    Header->Tag = 0x504F4F4C; /* 'POOL' */
    Header->Next = g_PoolHeaders[PoolType];
    g_PoolHeaders[PoolType] = Header;
    
    return (PVOID)(Header + 1);
}

/*
 * Free to specific pool
 */
VOID MemFreeToPool(IN PVOID Ptr, IN MEMORY_POOL_TYPE PoolType)
{
    if (!Ptr || PoolType >= 4) {
        return;
    }
    
    PPOOL_HEADER Header = ((PPOOL_HEADER)Ptr) - 1;
    
    /* Remove from pool list */
    PPOOL_HEADER* Current = &g_PoolHeaders[PoolType];
    while (*Current) {
        if (*Current == Header) {
            *Current = (*Current)->Next;
            MemFreeInternal(Header);
            return;
        }
        Current = &(*Current)->Next;
    }
}

/*
 * Get memory statistics
 */
NTSTATUS MemGetStatistics(OUT PMEMORY_STATISTICS Statistics)
{
    if (!Statistics) {
        return STATUS_INVALID_PARAMETER;
    }
    
    RtlCopyMemory(Statistics, &g_MemoryStats, sizeof(MEMORY_STATISTICS));
    return STATUS_SUCCESS;
}

/*
 * Get allocation size (stub)
 */
SIZE_T MemGetAllocationSize(IN PVOID Ptr)
{
    /* Not implemented in bump allocator */
    UNREFERENCED_PARAMETER(Ptr);
    return 0;
}

/*
 * Check if address is valid
 */
BOOL MemIsAddressValid(IN PVOID Address)
{
    /* Simple check - within heap range */
    return (Address >= (PVOID)g_KernelHeap && 
            Address < (PVOID)(g_KernelHeap + sizeof(g_KernelHeap)));
}

/*
 * Stub implementations for remaining functions
 */
NTSTATUS MemMapPhysicalMemory(IN UINT64 PhysicalAddress, IN PVOID VirtualAddress, IN SIZE_T Size, IN UINT32 Protection)
{
    UNREFERENCED_PARAMETER(PhysicalAddress);
    UNREFERENCED_PARAMETER(VirtualAddress);
    UNREFERENCED_PARAMETER(Size);
    UNREFERENCED_PARAMETER(Protection);
    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS MemUnmapVirtualMemory(IN PVOID VirtualAddress, IN SIZE_T Size)
{
    UNREFERENCED_PARAMETER(VirtualAddress);
    UNREFERENCED_PARAMETER(Size);
    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS MemProtectVirtualMemory(IN PVOID BaseAddress, IN SIZE_T Size, IN UINT32 NewProtection, OUT PUINT32 OldProtection)
{
    UNREFERENCED_PARAMETER(BaseAddress);
    UNREFERENCED_PARAMETER(Size);
    UNREFERENCED_PARAMETER(NewProtection);
    if (OldProtection) *OldProtection = MEM_PROTECT_READ | MEM_PROTECT_WRITE;
    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS MemLockPages(IN PVOID BaseAddress, IN SIZE_T Size)
{
    UNREFERENCED_PARAMETER(BaseAddress);
    UNREFERENCED_PARAMETER(Size);
    return STATUS_SUCCESS; /* Always locked in kernel */
}

NTSTATUS MemUnlockPages(IN PVOID BaseAddress, IN SIZE_T Size)
{
    UNREFERENCED_PARAMETER(BaseAddress);
    UNREFERENCED_PARAMETER(Size);
    return STATUS_SUCCESS;
}

VOID MemFlushCache(IN PVOID BaseAddress, IN SIZE_T Size)
{
    UNREFERENCED_PARAMETER(BaseAddress);
    UNREFERENCED_PARAMETER(Size);
    /* Cache flushing would be architecture-specific */
}

VOID MemInvalidateCache(IN PVOID BaseAddress, IN SIZE_T Size)
{
    UNREFERENCED_PARAMETER(BaseAddress);
    UNREFERENCED_PARAMETER(Size);
    /* Cache invalidation would be architecture-specific */
}

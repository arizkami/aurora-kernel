/*
 * Aurora Kernel - Enhanced Memory Management Interface
 * Copyright (c) 2024 Aurora Project
 */

#ifndef _AURORA_MEM_H_
#define _AURORA_MEM_H_

#include "../aurora.h"
#include "kern.h"

/* Memory Constants */
#define AURORA_PAGE_SIZE 4096ULL
#define AURORA_PAGE_SHIFT 12
#define AURORA_PAGE_MASK (AURORA_PAGE_SIZE - 1)
#define AURORA_LARGE_PAGE_SIZE (2 * 1024 * 1024ULL) /* 2MB */
#define AURORA_HUGE_PAGE_SIZE (1024 * 1024 * 1024ULL) /* 1GB */

/* Memory Pool Types */
typedef enum _MEMORY_POOL_TYPE {
    NonPagedPool = 0,
    PagedPool = 1,
    NonPagedPoolExecute = 2,
    PagedPoolExecute = 3
} MEMORY_POOL_TYPE;

/* Memory Protection Flags */
#define MEM_PROTECT_READ     0x01
#define MEM_PROTECT_WRITE    0x02
#define MEM_PROTECT_EXECUTE  0x04
#define MEM_PROTECT_USER     0x08
#define MEM_PROTECT_NOCACHE  0x10
#define MEM_PROTECT_GUARD    0x20

/* Physical Memory Descriptor */
typedef struct _PHYSICAL_MEMORY_DESCRIPTOR {
    UINT64 BasePage;
    UINT64 PageCount;
    UINT32 MemoryType;
    UINT32 Reserved;
} PHYSICAL_MEMORY_DESCRIPTOR, *PPHYSICAL_MEMORY_DESCRIPTOR;

/* Memory Statistics */
typedef struct _MEMORY_STATISTICS {
    UINT64 TotalPhysicalPages;
    UINT64 AvailablePhysicalPages;
    UINT64 AllocatedPhysicalPages;
    UINT64 TotalVirtualPages;
    UINT64 AvailableVirtualPages;
    UINT64 AllocatedVirtualPages;
    UINT64 HeapAllocations;
    UINT64 HeapDeallocations;
    UINT64 HeapBytesAllocated;
    UINT64 HeapBytesFreed;
} MEMORY_STATISTICS, *PMEMORY_STATISTICS;

/* Virtual Address Descriptor */
typedef struct _VIRTUAL_ADDRESS_DESCRIPTOR {
    PVOID BaseAddress;
    SIZE_T Size;
    UINT32 Protection;
    UINT32 Type;
    struct _VIRTUAL_ADDRESS_DESCRIPTOR* Next;
} VIRTUAL_ADDRESS_DESCRIPTOR, *PVIRTUAL_ADDRESS_DESCRIPTOR;

/* Memory Manager Functions */

/* Initialization */
NTSTATUS MemInitialize(void);
NTSTATUS MemInitializePhysicalManager(IN PPHYSICAL_MEMORY_DESCRIPTOR MemoryMap, IN UINT32 DescriptorCount);
NTSTATUS MemInitializeVirtualManager(void);
NTSTATUS MemInitializeHeapManager(void);

/* Physical Memory Management */
PVOID MemAllocatePhysicalPages(IN SIZE_T PageCount);
VOID MemFreePhysicalPages(IN PVOID PhysicalAddress, IN SIZE_T PageCount);
UINT64 MemGetPhysicalAddress(IN PVOID VirtualAddress);
BOOL MemIsPhysicalAddressValid(IN UINT64 PhysicalAddress);

/* Virtual Memory Management */
PVOID MemAllocateVirtualMemory(IN SIZE_T Size, IN UINT32 Protection);
NTSTATUS MemFreeVirtualMemory(IN PVOID BaseAddress, IN SIZE_T Size);
NTSTATUS MemMapPhysicalMemory(IN UINT64 PhysicalAddress, IN PVOID VirtualAddress, IN SIZE_T Size, IN UINT32 Protection);
NTSTATUS MemUnmapVirtualMemory(IN PVOID VirtualAddress, IN SIZE_T Size);
NTSTATUS MemProtectVirtualMemory(IN PVOID BaseAddress, IN SIZE_T Size, IN UINT32 NewProtection, OUT PUINT32 OldProtection);

/* Heap Management */
PVOID MemAlloc(IN SIZE_T Size);
PVOID MemAllocZero(IN SIZE_T Size);
PVOID MemRealloc(IN PVOID Ptr, IN SIZE_T NewSize);
VOID MemFree(IN PVOID Ptr);
PVOID MemAllocFromPool(IN MEMORY_POOL_TYPE PoolType, IN SIZE_T Size);
VOID MemFreeToPool(IN PVOID Ptr, IN MEMORY_POOL_TYPE PoolType);

/* Page Management */
PVOID MemAllocPages(IN SIZE_T PageCount);
VOID MemFreePages(IN PVOID Base, IN SIZE_T PageCount);
NTSTATUS MemLockPages(IN PVOID BaseAddress, IN SIZE_T Size);
NTSTATUS MemUnlockPages(IN PVOID BaseAddress, IN SIZE_T Size);

/* Memory Information */
NTSTATUS MemGetStatistics(OUT PMEMORY_STATISTICS Statistics);
SIZE_T MemGetAllocationSize(IN PVOID Ptr);
BOOL MemIsAddressValid(IN PVOID Address);

/* Cache Management */
VOID MemFlushCache(IN PVOID BaseAddress, IN SIZE_T Size);
VOID MemInvalidateCache(IN PVOID BaseAddress, IN SIZE_T Size);

/* Utility Functions */
static inline UINT64 MemAlignUp(UINT64 Value, UINT64 Alignment) {
    return (Value + Alignment - 1) & ~(Alignment - 1);
}

static inline UINT64 MemAlignDown(UINT64 Value, UINT64 Alignment) {
    return Value & ~(Alignment - 1);
}

static inline BOOL MemIsAligned(UINT64 Value, UINT64 Alignment) {
    return (Value & (Alignment - 1)) == 0;
}

static inline UINT64 MemBytesToPages(UINT64 Bytes) {
    return (Bytes + AURORA_PAGE_SIZE - 1) >> AURORA_PAGE_SHIFT;
}

static inline UINT64 MemPagesToBytes(UINT64 Pages) {
    return Pages << AURORA_PAGE_SHIFT;
}

#endif /* _AURORA_MEM_H_ */

/* Minimal Aurora platform stubs to satisfy linkage */
#include "../aurora.h"

NTSTATUS AuroraInitializeSpinLock(OUT PAURORA_SPINLOCK SpinLock){ if(!SpinLock) return STATUS_INVALID_PARAMETER; *SpinLock=0; return STATUS_SUCCESS; }
void AuroraAcquireSpinLock(IN PAURORA_SPINLOCK SpinLock, OUT PAURORA_IRQL OldIrql){ (void)SpinLock; if(OldIrql) *OldIrql=0; }
void AuroraReleaseSpinLock(IN PAURORA_SPINLOCK SpinLock, IN AURORA_IRQL OldIrql){ (void)SpinLock; (void)OldIrql; }
NTSTATUS AuroraInitializeEvent(OUT PAURORA_EVENT Event, IN BOOL ManualReset, IN BOOL InitialState){ (void)ManualReset; if(!Event) return STATUS_INVALID_PARAMETER; *Event = InitialState ? 1u : 0u; return STATUS_SUCCESS; }

PVOID AuroraAllocateMemory(IN UINT64 Size){ return AuroraAllocatePool(Size); }
PVOID AuroraAllocatePool(IN UINT64 Size){ /* naive bump allocator placeholder: not persistent */
    static unsigned char pool[1<<18]; /* 256KB */
    static UINT64 off=0; if(Size==0) return NULL; if(off+Size>sizeof(pool)) return NULL; void* p=&pool[off]; off += (Size + 7) & ~7ULL; return p; }
void AuroraFreeMemory(IN PVOID Memory){ (void)Memory; /* no-op in stub */ }
void AuroraFreePool(IN PVOID Memory){ (void)Memory; /* no-op */ }
void AuroraMemoryZero(IN PVOID Memory, IN UINT64 Size){ memset(Memory, 0, (size_t)Size); }
void AuroraMemoryCopy(OUT PVOID Destination, IN PVOID Source, IN UINT64 Size){ memcpy(Destination, Source, (size_t)Size); }

UINT64 AuroraGetSystemTime(void){ static UINT64 t=0; return ++t; }
UINT32 AuroraGetCurrentProcessId(void){ return 1; }
UINT32 AuroraGetCurrentThreadId(void){ return 1; }

BOOL AuroraIsEqualGuid(IN PGUID a, IN PGUID b){ if(!a||!b) return FALSE; return memcmp(a,b,sizeof(GUID))==0; }

void AuroraDebugPrint(IN PCHAR Format, ...){ (void)Format; /* no-op stub */ }

NTSTATUS AuroraSetEvent(IN PVOID Event){ (void)Event; return STATUS_SUCCESS; }
NTSTATUS AuroraWaitForSingleObject(IN PVOID Object, IN UINT32 TimeoutMs){ (void)Object; (void)TimeoutMs; return STATUS_TIMEOUT; }

NTSTATUS AuroraMapMemory(IN PVOID VirtualAddress, IN UINT64 PhysicalAddress, IN UINT64 Size, IN UINT32 Protection){ (void)VirtualAddress; (void)PhysicalAddress; (void)Size; (void)Protection; return STATUS_NOT_IMPLEMENTED; }
NTSTATUS AuroraUnmapMemory(IN PVOID VirtualAddress, IN UINT64 Size){ (void)VirtualAddress; (void)Size; return STATUS_NOT_IMPLEMENTED; }

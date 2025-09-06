#include "../aurora.h"
#include "../include/raw.h"

/* NOTE: Placeholder; real implementation requires paging/ mapping */
NTSTATUS RawReadPhysical(IN UINT64 PhysAddr, OUT PVOID Buffer, IN UINT32 Length){
    (void)PhysAddr; (void)Buffer; (void)Length; return STATUS_NOT_IMPLEMENTED;
}
NTSTATUS RawWritePhysical(IN UINT64 PhysAddr, IN PVOID Buffer, IN UINT32 Length){
    (void)PhysAddr; (void)Buffer; (void)Length; return STATUS_NOT_IMPLEMENTED;
}

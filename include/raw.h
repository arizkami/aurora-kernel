/* Raw device/physical memory helper */
#ifndef _AURORA_RAW_H_
#define _AURORA_RAW_H_
#include "../aurora.h"

NTSTATUS RawReadPhysical(IN UINT64 PhysAddr, OUT PVOID Buffer, IN UINT32 Length);
NTSTATUS RawWritePhysical(IN UINT64 PhysAddr, IN PVOID Buffer, IN UINT32 Length);

#endif

/*
 * Aurora Kernel - Process Subsystem Interface
 */
#ifndef _AURORA_PROC_H_
#define _AURORA_PROC_H_

#include "../aurora.h"
#include "kern.h"

/* Initialize process subsystem (future use) */
NTSTATUS ProcInitialize(void);

/* Create architecture-specific address space (e.g., PML4 on x86_64) */
PVOID ProcArchCreateAddressSpace(void);

/* Set up a process address space and attach to PROCESS */
NTSTATUS ProcSetupAddressSpace(IN OUT PPROCESS Process);

/* Assembly entry to switch to user mode (stub for now) */
VOID ProcEnterUserMode(void);

#endif /* _AURORA_PROC_H_ */

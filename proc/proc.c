/*
 * Aurora Kernel - Process Subsystem (core)
 */
#include "../aurora.h"
#include "../include/kern.h"
#include "../include/mem.h"
#include "../include/proc.h"

NTSTATUS ProcInitialize(void)
{
    /* Nothing yet; reserved for handle tables, VADs, etc. */
    return STATUS_SUCCESS;
}

NTSTATUS ProcSetupAddressSpace(IN OUT PPROCESS Process)
{
    if (!Process) return STATUS_INVALID_PARAMETER;
    if (Process->VirtualAddressSpace) return STATUS_SUCCESS;

    PVOID as = ProcArchCreateAddressSpace();
    if (!as) return STATUS_INSUFFICIENT_RESOURCES;
    Process->VirtualAddressSpace = as;
    return STATUS_SUCCESS;
}

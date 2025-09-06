/*
 * Aurora Kernel - AMD64 Process Arch helpers
 */
#include "../../aurora.h"
#include "../../include/kern.h"
#include "../../include/mem.h"
#include "../../include/proc.h"

/* Create a bare-bones PML4 structure in kernel memory and return its base */
PVOID ProcArchCreateAddressSpace(void)
{
    /* For now, allocate one page as a placeholder PML4; actual mapping TBD */
    return MemAllocPages(1);
}

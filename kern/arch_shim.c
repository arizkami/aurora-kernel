/* Arch shim to provide generic Arch* symbols for current architecture */
#include "../aurora.h"
#include "../include/kern.h"
#include "amd64/kern_arch.h"

VOID ArchSaveContext(IN PTHREAD Thread) { Amd64SaveContext(Thread); }
VOID ArchRestoreContext(IN PTHREAD Thread) { Amd64RestoreContext(Thread); }
VOID ArchInitializeThreadContext(IN PTHREAD Thread, IN PVOID StartAddress, IN PVOID Parameter) {
    Amd64InitializeThreadContext(Thread, StartAddress, Parameter);
}

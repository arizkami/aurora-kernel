/* Fiasco subsystem public interface */
#ifndef _AURORA_FIASCO_H_
#define _AURORA_FIASCO_H_

#include "../aurora.h"
#include "kern.h"
#include "l4.h"

NTSTATUS FiascoInitialize(void);
NTSTATUS FiascoIpcFastpath(PTHREAD Current, L4_CAP DestCap, PL4_MSG Msg);

/* Configuration toggles */
extern BOOL g_FiascoFastpathEnabled;
VOID FiascoSetFastpath(BOOL Enable);

/* Pager stub interface */
typedef NTSTATUS (*FIASCO_PAGER_HANDLER)(PTHREAD FaultingThread, UINT64 Address, UINT32 Flags);
VOID FiascoRegisterPager(FIASCO_PAGER_HANDLER Handler);

/* Fault injection (simulated) */
NTSTATUS FiascoHandlePageFault(PTHREAD Thread, UINT64 Address, UINT32 Flags);

#endif
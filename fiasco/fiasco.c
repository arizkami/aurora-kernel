#include "../aurora.h"
#include "../include/kern.h"
#include "../include/l4.h"
#include "../include/fiasco.h"

/* Fiasco subsystem: higher-level policies built atop L4 adaptation.
 * This layer will house capability rights enforcement, IPC fastpath hooks,
 * and future pager/mapping abstractions.
 *
 * External reference: selected concepts mirrored from external/fiasco/src
 * without directly compiling its C++ sources yet. We progressively
 * introduce small C reimplementations to avoid pulling full toolchain.
 */
static BOOL g_FiascoInitialized = FALSE;
BOOL g_FiascoFastpathEnabled = TRUE; /* default enabled */
static FIASCO_PAGER_HANDLER g_PagerHandler = 0;

VOID FiascoSetFastpath(BOOL Enable){ g_FiascoFastpathEnabled = Enable; }
VOID FiascoRegisterPager(FIASCO_PAGER_HANDLER Handler){ g_PagerHandler = Handler; }

NTSTATUS FiascoHandlePageFault(PTHREAD Thread, UINT64 Address, UINT32 Flags){
    if(g_PagerHandler) return g_PagerHandler(Thread,Address,Flags);
    return STATUS_NOT_IMPLEMENTED; /* default: no pager */
}

NTSTATUS FiascoInitialize(void){
    if(g_FiascoInitialized) return STATUS_ALREADY_INITIALIZED;
    /* Currently nothing extra beyond L4Initialize() which is called earlier. */
    g_FiascoInitialized = TRUE;
    return STATUS_SUCCESS;
}

/* Placeholder for future fastpath dispatch (e.g., system call hook). */
static NTSTATUS _FiascoEnqueueSender(PTHREAD Dest, PTHREAD Sender){
    if(!Dest || !Sender) return STATUS_INVALID_PARAMETER;
    if(!Dest->IpcWaitHead){
        Dest->IpcWaitHead = Sender;
    } else {
        PTHREAD tail = Dest->IpcWaitHead; while(tail->IpcWaitNext) tail = tail->IpcWaitNext; tail->IpcWaitNext = Sender;
    }
    Dest->IpcWaitCount++;
    Sender->IpcWaitNext = NULL;
    return STATUS_SUCCESS;
}

static PTHREAD _FiascoDequeueSender(PTHREAD Dest){
    if(!Dest || !Dest->IpcWaitHead) return NULL;
    PTHREAD s = Dest->IpcWaitHead; Dest->IpcWaitHead = s->IpcWaitNext; s->IpcWaitNext = NULL; if(Dest->IpcWaitCount) Dest->IpcWaitCount--; return s; }

NTSTATUS FiascoIpcFastpath(PTHREAD Current, L4_CAP DestCap, PL4_MSG Msg){
    if(!g_FiascoFastpathEnabled) return STATUS_NOT_IMPLEMENTED;
    if(!Current || !Msg) return STATUS_INVALID_PARAMETER;
    PL4_TCB_EXTENSION ext = (PL4_TCB_EXTENSION)Current->Extension;
    if(!ext || !ext->CapTable) return STATUS_NOT_INITIALIZED;
    /* Enforce rights mask */
    PVOID obj = L4CapLookup(ext->CapTable, DestCap, L4_IPC_RIGHT_SEND);
    if(!obj) return STATUS_ACCESS_DENIED;
    PTHREAD dest = (PTHREAD)obj;
    PL4_TCB_EXTENSION dext = (PL4_TCB_EXTENSION)dest->Extension;
    if(!dext) return STATUS_INVALID_PARAMETER;
    /* Attempt send */
    NTSTATUS st = L4IpcSend(ext,dext,Msg);
    if(st==STATUS_BUFFER_TOO_SMALL){
        /* Mailbox full: enqueue sender for blocking semantics */
        _FiascoEnqueueSender(dest,Current);
        /* STATUS_PENDING not defined in our set: reuse STATUS_MORE_PROCESSING_REQUIRED if exists */
#ifndef STATUS_PENDING
#ifdef STATUS_MORE_PROCESSING_REQUIRED
#define STATUS_PENDING STATUS_MORE_PROCESSING_REQUIRED
#else
#define STATUS_PENDING ((NTSTATUS)0x00000103L)
#endif
#endif
        return STATUS_PENDING; /* indicate blocked */
    }
    if(NT_SUCCESS(st)){
        /* If there are queued senders and inbox emptied later, they will be retried externally */
    }
    return st;
}

/* Helper to be called after a receive to wake one waiting sender */
VOID FiascoIpcPostReceive(PTHREAD Receiver){
    if(!Receiver) return;
    if(Receiver->IpcWaitHead){
        PTHREAD snd = _FiascoDequeueSender(Receiver);
        if(snd){
            /* TODO: reattempt stored message; currently no per-sender queued message state. */
        }
    }
}

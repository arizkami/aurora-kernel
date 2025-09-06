#include "../aurora.h"
#include "../include/kern.h"
#include "../include/l4.h"
#include "../include/fiasco.h"
#include "../l4_sublayer/include/l4_types.h"
#include "../l4_sublayer/include/l4_ipc.h"
#include "../l4_sublayer/include/l4_msg_item.h"

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
    
    /* Initialize L4 sublayer if not already done */
    NTSTATUS status = L4Initialize();
    if (!NT_SUCCESS(status)) {
        return status;
    }
    
    /* Initialize Fiasco-specific components */
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
    
    /* Enforce rights mask using Fiasco policy */
    PVOID obj = L4CapLookup(ext->CapTable, DestCap, L4_IPC_RIGHT_SEND);
    if(!obj) return STATUS_ACCESS_DENIED;
    PTHREAD dest = (PTHREAD)obj;
    PL4_TCB_EXTENSION dext = (PL4_TCB_EXTENSION)dest->Extension;
    if(!dext) return STATUS_INVALID_PARAMETER;
    
    /* Use L4 sublayer for the actual IPC operation */
    L4_obj_ref dest_ref = L4ObjRefCreate(dest->ThreadId, L4_IPC_SEND);
    L4_timeout timeout = L4TimeoutNever();
    
    /* Convert Aurora message to L4 message registers */
    for (UINT32 i = 0; i < Msg->Length && i < 4; i++) {
        L4SetMR(i, Msg->MR[i]);
    }
    
    L4_msg_tag tag = L4MsgTagCreate(Msg->Length, 0, 0, L4_PROTO_NONE);
    L4_error error = L4_IpcSend(dest_ref, timeout, tag);
    
    if (L4ErrorIsOk(error)) {
        /* Fallback to legacy Aurora IPC for compatibility */
        NTSTATUS st = L4IpcSend(ext, dext, Msg);
        if(st == STATUS_BUFFER_TOO_SMALL){
            /* Mailbox full: enqueue sender for blocking semantics */
            _FiascoEnqueueSender(dest, Current);
#ifndef STATUS_PENDING
#ifdef STATUS_MORE_PROCESSING_REQUIRED
#define STATUS_PENDING STATUS_MORE_PROCESSING_REQUIRED
#else
#define STATUS_PENDING ((NTSTATUS)0x00000103L)
#endif
#endif
            return STATUS_PENDING; /* indicate blocked */
        }
        return st;
    }
    
    /* Convert L4 error to Aurora status */
    L4_error_code code = L4ErrorGetCode(error);
    switch (code) {
        case L4_EINVAL: return STATUS_INVALID_PARAMETER;
        case L4_EPERM: return STATUS_ACCESS_DENIED;
        case L4_ENOMEM: return STATUS_INSUFFICIENT_RESOURCES;
        case L4_EFAULT: return STATUS_ACCESS_VIOLATION;
        default: return STATUS_UNSUCCESSFUL;
    }
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

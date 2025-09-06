#include "../aurora.h"
#include "../include/l4.h"
#include "../include/ipc.h"
#include "../include/proc.h"
#include "../l4_sublayer/include/l4_types.h"
#include "../l4_sublayer/include/l4_ipc.h"
#include "../l4_sublayer/include/l4_msg_item.h"

static BOOL g_L4Initialized = FALSE;
L4_utcb* g_SystemUtcb = NULL;

/* TODO (Capability lifecycle):
 *  - Introduce global registry for reverse lookups during revoke
 *  - Implement L4CapRevokeAll(Object) to remove object from all tables
 *  - Implement L4CapDerive(ParentCap, ReducedRights)
 */

/* TODO (Message extensions):
 *  - Support extended message descriptors (memory grant/map, strings)
 *  - Keep small fixed MR fastpath while allowing optional heap-backed extension
 */

NTSTATUS L4Initialize(void){
    /* Initialize the L4 sublayer */
    g_SystemUtcb = (L4_utcb*)AuroraAllocateMemory(sizeof(L4_utcb));
    if (!g_SystemUtcb) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    
    L4UtcbInit(g_SystemUtcb);
    L4SetUtcb(g_SystemUtcb);
    
    g_L4Initialized = TRUE;
    return STATUS_SUCCESS;
}

PL4_TCB_EXTENSION L4GetOrCreateTcbExtension(PTHREAD Thread){
    if(!Thread) return NULL;
    if(Thread->Extension == NULL){
        PL4_TCB_EXTENSION ext = (PL4_TCB_EXTENSION)AuroraAllocateMemory(sizeof(L4_TCB_EXTENSION));
        if(!ext) return NULL;
        memset(ext,0,sizeof(*ext));
        ext->ThreadId = Thread->ThreadId;
        AuroraInitializeSpinLock(&ext->Lock);
        ext->CapTable = (L4_CAP_TABLE*)AuroraAllocateMemory(sizeof(L4_CAP_TABLE));
        if(!ext->CapTable){
            /* Failed, free ext and return */
            KernFreeMemory(ext);
            return NULL;
        }
        memset(ext->CapTable,0,sizeof(L4_CAP_TABLE));
        Thread->Extension = ext;
    }
    return (PL4_TCB_EXTENSION)Thread->Extension;
}

NTSTATUS L4CapInsert(PL4_CAP_TABLE Table, L4_CAP* OutCap, UINT32 Type, UINT32 Rights, PVOID Object){
    if(!Table || !OutCap) return STATUS_INVALID_PARAMETER;
    for(UINT32 i=0;i<L4_MAX_CAPS;i++){
        if(Table->Entries[i].Type==0){
            Table->Entries[i].Type = Type;
            Table->Entries[i].Rights = Rights;
            Table->Entries[i].Object = Object;
            *OutCap = i;
            return STATUS_SUCCESS;
        }
    }
    return STATUS_INSUFFICIENT_RESOURCES; /* no free slots */
}

PVOID L4CapLookup(PL4_CAP_TABLE Table, L4_CAP Cap, UINT32 RequiredRights){
    if(!Table || Cap>=L4_MAX_CAPS) return NULL;
    L4_CAP_ENTRY* e = &Table->Entries[Cap];
    if(e->Type==0) return NULL;
    if((e->Rights & RequiredRights) != RequiredRights) return NULL;
    return e->Object;
}

/* Stub: capability revoke (single cap) */
NTSTATUS L4CapRevoke(PL4_CAP_TABLE Table, L4_CAP Cap){
    if(!Table || Cap>=L4_MAX_CAPS) return STATUS_INVALID_PARAMETER;
    L4_CAP_ENTRY* e = &Table->Entries[Cap];
    if(e->Type==0) return STATUS_NOT_FOUND;
    memset(e,0,sizeof(*e));
    return STATUS_SUCCESS;
}

/* Stub: derive reduced-rights capability (same object, subset rights) */
NTSTATUS L4CapDerive(PL4_CAP_TABLE Table, L4_CAP Source, UINT32 NewRights, L4_CAP* Out){
    if(!Table || !Out) return STATUS_INVALID_PARAMETER;
    if(Source>=L4_MAX_CAPS) return STATUS_INVALID_PARAMETER;
    L4_CAP_ENTRY* e = &Table->Entries[Source];
    if(e->Type==0) return STATUS_NOT_FOUND;
    if((NewRights & e->Rights) != NewRights) return STATUS_ACCESS_DENIED; /* cannot add rights */
    return L4CapInsert(Table,Out,e->Type,NewRights,e->Object);
}

NTSTATUS L4IpcSend(PL4_TCB_EXTENSION Sender, PL4_TCB_EXTENSION Receiver, PL4_MSG Msg){
    if(!Sender || !Receiver) return STATUS_INVALID_PARAMETER;
    
    /* Set up L4 IPC using the sublayer */
    L4_obj_ref dest = L4ObjRefCreate(Receiver->ThreadId, L4_IPC_SEND);
    L4_timeout timeout = L4TimeoutNever();
    
    /* Convert Aurora message to L4 message registers */
    if (Msg && Msg->Length > 0) {
        for (UINT32 i = 0; i < Msg->Length && i < 4; i++) {
            L4SetMR(i, Msg->MR[i]);
        }
    }
    
    L4_msg_tag tag = L4MsgTagCreate(Msg ? Msg->Length : 0, 0, 0, L4_PROTO_NONE);
    L4_error error = L4_IpcSend(dest, timeout, tag);
    
    if (L4ErrorIsOk(error)) {
        /* Legacy compatibility: still update receiver's inbox */
        AURORA_IRQL old;
        AuroraAcquireSpinLock(&Receiver->Lock,&old);
        if(Receiver->Inbox.Length!=0){
            AuroraReleaseSpinLock(&Receiver->Lock,old);
            return STATUS_BUFFER_TOO_SMALL;
        }
        if(Msg){
            Receiver->Inbox = *Msg;
        }
        AuroraReleaseSpinLock(&Receiver->Lock,old);
        return STATUS_SUCCESS;
    }
    
    return STATUS_UNSUCCESSFUL;
}

NTSTATUS L4IpcReceive(PL4_TCB_EXTENSION Receiver, PL4_MSG MsgOut){
    if(!Receiver || !MsgOut) return STATUS_INVALID_PARAMETER;
    
    /* Set up L4 IPC receive using the sublayer */
    L4_obj_ref from_spec = L4ObjRefCreate(0, L4_IPC_OPEN_WAIT);
    L4_timeout timeout = L4TimeoutZero(); /* Non-blocking for compatibility */
    L4_obj_ref sender;
    
    L4_msg_tag result_tag = L4_IpcReceive(from_spec, timeout, &sender);
    
    if (!L4MsgTagHasError(result_tag)) {
        /* Convert L4 message registers back to Aurora message */
        UINT32 words = L4MsgTagGetWords(result_tag);
        MsgOut->Length = (words > 4) ? 4 : words;
        
        for (UINT32 i = 0; i < MsgOut->Length; i++) {
            MsgOut->MR[i] = L4GetMR(i);
        }
        
        /* Legacy compatibility: clear inbox */
        AURORA_IRQL old; 
        AuroraAcquireSpinLock(&Receiver->Lock,&old);
        Receiver->Inbox.Length = 0;
        AuroraReleaseSpinLock(&Receiver->Lock,old);
        
        return STATUS_SUCCESS;
    }
    
    /* Fallback to legacy implementation for compatibility */
    AURORA_IRQL old; 
    AuroraAcquireSpinLock(&Receiver->Lock,&old);
    if(Receiver->Inbox.Length==0){ 
        AuroraReleaseSpinLock(&Receiver->Lock,old); 
        return STATUS_NO_MORE_ENTRIES; 
    }
    *MsgOut = Receiver->Inbox;
    Receiver->Inbox.Length = 0;
    AuroraReleaseSpinLock(&Receiver->Lock,old);
    return STATUS_SUCCESS;
}

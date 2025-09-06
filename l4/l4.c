#include "../aurora.h"
#include "../include/l4.h"
#include "../include/ipc.h"
#include "../include/proc.h"

static BOOL g_L4Initialized = FALSE;

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
    g_L4Initialized = TRUE; return STATUS_SUCCESS;
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
    return STATUS_INSUFFICIENT_RESOURCES;
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
    AURORA_IRQL old;
    AuroraAcquireSpinLock(&Receiver->Lock,&old);
    if(Receiver->Inbox.Length!=0){
        AuroraReleaseSpinLock(&Receiver->Lock,old);
        return STATUS_BUFFER_TOO_SMALL; /* mailbox full */
    }
    if(Msg){
        Receiver->Inbox = *Msg; /* copy message registers */
    }
    AuroraReleaseSpinLock(&Receiver->Lock,old);
    return STATUS_SUCCESS;
}

NTSTATUS L4IpcReceive(PL4_TCB_EXTENSION Receiver, PL4_MSG MsgOut){
    if(!Receiver || !MsgOut) return STATUS_INVALID_PARAMETER;
    AURORA_IRQL old; AuroraAcquireSpinLock(&Receiver->Lock,&old);
    if(Receiver->Inbox.Length==0){ AuroraReleaseSpinLock(&Receiver->Lock,old); return STATUS_NO_MORE_ENTRIES; }
    *MsgOut = Receiver->Inbox;
    Receiver->Inbox.Length = 0;
    AuroraReleaseSpinLock(&Receiver->Lock,old);
    return STATUS_SUCCESS;
}

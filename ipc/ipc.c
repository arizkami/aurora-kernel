#include "../aurora.h"
#include "../include/ipc.h"

static IPC_CHANNEL g_Channels[IPC_MAX_CHANNELS];

NTSTATUS IpcInitialize(void){
    AuroraMemoryZero(g_Channels, sizeof(g_Channels));
    for(UINT32 i=0;i<IPC_MAX_CHANNELS;i++) g_Channels[i].Id = (UINT32)-1;
    return STATUS_SUCCESS;
}

NTSTATUS IpcCreateChannel(OUT PUINT32 ChannelId){
    if(!ChannelId) return STATUS_INVALID_PARAMETER;
    for(UINT32 i=0;i<IPC_MAX_CHANNELS;i++){
        if(g_Channels[i].Id == (UINT32)-1){
            g_Channels[i].Id = i;
            g_Channels[i].Queue.Size = 0;
            g_Channels[i].Lock = 0;
            *ChannelId = i; return STATUS_SUCCESS;
        }
    }
    return STATUS_INSUFFICIENT_RESOURCES;
}

NTSTATUS IpcSend(IN UINT32 ChannelId, IN PVOID Data, IN UINT32 Size){
    if(ChannelId>=IPC_MAX_CHANNELS || !Data || Size==0 || Size>IPC_MAX_MESSAGE) return STATUS_INVALID_PARAMETER;
    IPC_CHANNEL* ch = &g_Channels[ChannelId];
    if(ch->Id != ChannelId) return STATUS_INVALID_HANDLE;
    AURORA_IRQL old; AuroraAcquireSpinLock(&ch->Lock,&old);
    if(ch->Queue.Size!=0){ AuroraReleaseSpinLock(&ch->Lock,old); return STATUS_BUFFER_TOO_SMALL; }
    ch->Queue.Size = Size; AuroraMemoryCopy(ch->Queue.Data, Data, Size);
    AuroraReleaseSpinLock(&ch->Lock,old);
    return STATUS_SUCCESS;
}

NTSTATUS IpcReceive(IN UINT32 ChannelId, OUT PVOID Buffer, IN OUT PUINT32 Size){
    if(ChannelId>=IPC_MAX_CHANNELS || !Buffer || !Size || *Size==0) return STATUS_INVALID_PARAMETER;
    IPC_CHANNEL* ch = &g_Channels[ChannelId];
    if(ch->Id != ChannelId) return STATUS_INVALID_HANDLE;
    AURORA_IRQL old; AuroraAcquireSpinLock(&ch->Lock,&old);
    if(ch->Queue.Size==0){ AuroraReleaseSpinLock(&ch->Lock,old); return STATUS_NO_MORE_ENTRIES; }
    if(*Size < ch->Queue.Size){ *Size = ch->Queue.Size; AuroraReleaseSpinLock(&ch->Lock,old); return STATUS_BUFFER_TOO_SMALL; }
    AuroraMemoryCopy(Buffer, ch->Queue.Data, ch->Queue.Size);
    *Size = ch->Queue.Size; ch->Queue.Size = 0;
    AuroraReleaseSpinLock(&ch->Lock,old);
    return STATUS_SUCCESS;
}

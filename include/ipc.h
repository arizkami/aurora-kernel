/* Simple IPC channel API */
#ifndef _AURORA_IPC_H_
#define _AURORA_IPC_H_
#include "../aurora.h"

#define IPC_MAX_CHANNELS 64
#define IPC_MAX_MESSAGE 256

typedef struct _IPC_MESSAGE { UINT32 Size; CHAR Data[IPC_MAX_MESSAGE]; } IPC_MESSAGE, *PIPC_MESSAGE;

typedef struct _IPC_CHANNEL { UINT32 Id; IPC_MESSAGE Queue; AURORA_SPINLOCK Lock; } IPC_CHANNEL, *PIPC_CHANNEL;

NTSTATUS IpcInitialize(void);
NTSTATUS IpcCreateChannel(OUT PUINT32 ChannelId);
NTSTATUS IpcSend(IN UINT32 ChannelId, IN PVOID Data, IN UINT32 Size);
NTSTATUS IpcReceive(IN UINT32 ChannelId, OUT PVOID Buffer, IN OUT PUINT32 Size);

#endif

/*
 * Aurora Kernel - WMI Core Header
 * Windows Management Instrumentation Implementation
 * Copyright (c) 2024 NTCore Project
 */

#ifndef _WMI_H_
#define _WMI_H_

#include "../aurora.h"

/* WMI Provider Flags */
#define WMI_PROVIDER_FLAG_TRACE         0x00000001
#define WMI_PROVIDER_FLAG_EVENT         0x00000002
#define WMI_PROVIDER_FLAG_EXPENSIVE     0x00000004
#define WMI_PROVIDER_FLAG_ACTIVE        0x00000008
#define WMI_PROVIDER_FLAG_SINGLE_INST   0x00000010
#define WMI_PROVIDER_FLAG_REMOVE_OK     0x00000020
#define WMI_PROVIDER_FLAG_UNREGISTERED  0x00000040

/* WMI Query Types */
#define WMI_QUERY_ALL_DATA              0x00000001
#define WMI_QUERY_SINGLE_INSTANCE       0x00000002
#define WMI_QUERY_REGINFO               0x00000003
#define WMI_ENABLE_EVENTS               0x00000004
#define WMI_DISABLE_EVENTS              0x00000005

/* WMI Method IDs */
#define WMI_METHOD_EXECUTE              0x00000001
#define WMI_METHOD_SET_NOTIFICATION     0x00000002
#define WMI_METHOD_GET_NOTIFICATION     0x00000003

/* WMI Event Levels */
#define WMI_EVENT_LEVEL_CRITICAL        1
#define WMI_EVENT_LEVEL_ERROR           2
#define WMI_EVENT_LEVEL_WARNING         3
#define WMI_EVENT_LEVEL_INFORMATION     4
#define WMI_EVENT_LEVEL_VERBOSE         5

/* WMI Data Block Structure */
typedef struct _WMI_DATA_BLOCK {
    WMI_GUID Guid;
    UINT32 InstanceIndex;
    UINT32 DataSize;
    UINT32 Flags;
    PVOID Data;
    struct _WMI_DATA_BLOCK* Next;
} WMI_DATA_BLOCK, *PWMI_DATA_BLOCK;

/* WMI Instance Information */
typedef struct _WMI_INSTANCE_INFO {
    UINT32 InstanceIndex;
    UINT32 InstanceNameLength;
    PWCHAR InstanceName;
    PVOID InstanceData;
    UINT32 DataSize;
} WMI_INSTANCE_INFO, *PWMI_INSTANCE_INFO;

/* WMI Registration Information */
typedef struct _WMI_REGINFO {
    UINT32 BufferSize;
    UINT32 NextWmiRegInfo;
    UINT32 RegistryPath;
    UINT32 MofResourceName;
    UINT32 GuidCount;
    WMI_GUID Guids[1];
} WMI_REGINFO, *PWMI_REGINFO;

/* WMI Trace Event Structure */
typedef struct _WMI_TRACE_EVENT {
    UINT16 Size;
    UINT16 FieldTypeFlags;
    union {
        UINT32 Version;
        struct {
            UINT8 Type;
            UINT8 Level;
            UINT16 Version;
        } Class;
    };
    UINT32 ThreadId;
    UINT32 ProcessId;
    UINT64 TimeStamp;
    union {
        WMI_GUID Guid;
        UINT64 GuidPtr;
    };
    UINT32 ClientContext;
    UINT32 Flags;
} WMI_TRACE_EVENT, *PWMI_TRACE_EVENT;

/* WMI Context Structure */
typedef struct _WMI_CONTEXT {
    UINT32 Signature;
    UINT32 Size;
    PVOID DeviceObject;
    PVOID RegistrationContext;
    UINT32 ProviderCount;
    PWMI_PROVIDER_INFO Providers;
    UINT32 EventCount;
    PWMI_EVENT Events;
    PVOID Lock;
} WMI_CONTEXT, *PWMI_CONTEXT;

/* Global WMI Context */
extern WMI_CONTEXT g_WmiContext;

/* WMI Core Function Prototypes */

/* Initialization and Cleanup */
NTSTATUS WmiInitializeContext(OUT PWMI_CONTEXT Context);
NTSTATUS WmiCleanupContext(IN PWMI_CONTEXT Context);

/* Provider Management */
NTSTATUS WmiRegisterProviderInternal(IN PWMI_CONTEXT Context, IN PWMI_PROVIDER_REGISTRATION Registration);
NTSTATUS WmiUnregisterProviderInternal(IN PWMI_CONTEXT Context, IN PWMI_GUID Guid);
NTSTATUS WmiFindProvider(IN PGUID ProviderGuid, OUT PHANDLE ProviderHandle);
NTSTATUS WmiEnumerateProviders(OUT PGUID ProviderGuids, IN OUT PUINT32 ProviderCount);

/* Event Management */
NTSTATUS WmiCreateEvent(IN PWMI_GUID ProviderId, IN WMI_EVENT_TYPE EventType, IN UINT32 EventId, IN PVOID EventData, IN UINT32 DataSize, OUT PWMI_EVENT* Event);
NTSTATUS WmiFireEventInternal(IN PWMI_CONTEXT Context, IN PWMI_EVENT Event);
NTSTATUS WmiProcessEvent(IN PWMI_CONTEXT Context, IN PWMI_EVENT Event);
NTSTATUS WmiFlushEvents(IN PGUID ProviderGuid OPTIONAL);

/* Data Block Management */
NTSTATUS WmiCreateDataBlock(IN PWMI_GUID Guid, IN UINT32 InstanceIndex, IN PVOID Data, IN UINT32 DataSize, OUT PWMI_DATA_BLOCK* DataBlock);
NTSTATUS WmiUpdateDataBlock(IN PWMI_DATA_BLOCK DataBlock, IN PVOID Data, IN UINT32 DataSize);
NTSTATUS WmiDeleteDataBlock(IN PWMI_DATA_BLOCK DataBlock);
PWMI_DATA_BLOCK WmiFindDataBlock(IN PWMI_CONTEXT Context, IN PWMI_GUID Guid, IN UINT32 InstanceIndex);

/* Query and Set Operations */
NTSTATUS WmiQueryAllData(IN PWMI_CONTEXT Context, IN PWMI_GUID Guid, OUT PVOID Buffer, IN OUT PUINT32 BufferSize);
NTSTATUS WmiQuerySingleInstance(IN PWMI_CONTEXT Context, IN PWMI_GUID Guid, IN UINT32 InstanceIndex, OUT PVOID Buffer, IN OUT PUINT32 BufferSize);
NTSTATUS WmiSetSingleInstance(IN PWMI_CONTEXT Context, IN PWMI_GUID Guid, IN UINT32 InstanceIndex, IN PVOID Buffer, IN UINT32 BufferSize);
NTSTATUS WmiSetSingleItem(IN PWMI_CONTEXT Context, IN PWMI_GUID Guid, IN UINT32 InstanceIndex, IN UINT32 DataItemId, IN PVOID Buffer, IN UINT32 BufferSize);

/* Method Execution */
NTSTATUS WmiExecuteMethod(IN PWMI_CONTEXT Context, IN PWMI_GUID Guid, IN UINT32 InstanceIndex, IN UINT32 MethodId, IN PVOID InputBuffer, IN UINT32 InputSize, OUT PVOID OutputBuffer, IN OUT PUINT32 OutputSize);

/* Trace Functions */
NTSTATUS WmiTraceEvent(IN PWMI_GUID ProviderId, IN UINT8 Level, IN UINT32 Flags, IN PVOID EventData, IN UINT32 DataSize);
NTSTATUS WmiTraceMessage(IN PWMI_GUID ProviderId, IN UINT8 Level, IN PCHAR Format, ...);

/* Utility Functions */
NTSTATUS WmiGenerateGuid(OUT PWMI_GUID Guid);
BOOL WmiCompareGuids(IN PWMI_GUID Guid1, IN PWMI_GUID Guid2);
NTSTATUS WmiGuidToString(IN PWMI_GUID Guid, OUT PCHAR Buffer, IN UINT32 BufferSize);
NTSTATUS WmiStringToGuid(IN PCHAR GuidString, OUT PWMI_GUID Guid);
UINT64 WmiGetSystemTime(void);
UINT32 WmiGetCurrentThreadId(void);
UINT32 WmiGetCurrentProcessId(void);

/* Lock Management */
NTSTATUS WmiAcquireLock(IN PVOID Lock);
NTSTATUS WmiReleaseLock(IN PVOID Lock);
NTSTATUS WmiCreateLock(OUT PVOID* Lock);
NTSTATUS WmiDestroyLock(IN PVOID Lock);

/* Memory Management for WMI */
PVOID WmiAllocateMemory(IN UINT32 Size);
void WmiFreeMemory(IN PVOID Memory);
PVOID WmiReallocateMemory(IN PVOID Memory, IN UINT32 NewSize);

/* Error Handling */
PCHAR WmiGetErrorString(IN NTSTATUS Status);
void WmiLogError(IN NTSTATUS Status, IN PCHAR Function, IN UINT32 Line);

/* Debugging Macros */
#ifdef DEBUG
    #define WMI_DEBUG_PRINT(Format, ...) AuroraDebugPrint("[WMI] " Format, ##__VA_ARGS__)
    #define WMI_LOG_ERROR(Status, Function) WmiLogError(Status, Function, __LINE__)
#else
    #define WMI_DEBUG_PRINT(Format, ...)
    #define WMI_LOG_ERROR(Status, Function)
#endif

/* Helper Macros */
#define WMI_IS_VALID_GUID(Guid) ((Guid) != NULL && (Guid)->Data1 != 0)
#define WMI_IS_VALID_PROVIDER(Provider) ((Provider) != NULL && WMI_IS_VALID_GUID(&(Provider)->Guid))
#define WMI_IS_VALID_EVENT(Event) ((Event) != NULL && WMI_IS_VALID_GUID(&(Event)->ProviderId))
#define WMI_ROUND_UP_SIZE(Size) AURORA_ALIGN_UP(Size, sizeof(PVOID))

/* Standard WMI GUIDs */
extern const WMI_GUID WMI_GUID_SYSTEM_TRACE;
extern const WMI_GUID WMI_GUID_KERNEL_PROCESS;
extern const WMI_GUID WMI_GUID_KERNEL_THREAD;
extern const WMI_GUID WMI_GUID_KERNEL_MEMORY;
extern const WMI_GUID WMI_GUID_KERNEL_IO;

#endif /* _WMI_H_ */
/*
 * Aurora Kernel - WMI Provider Registration and Event Handling
 * Windows Management Instrumentation - Provider Management
 * Copyright (c) 2024 NTCore Project
 */

#include "../aurora.h"
#include "../include/wmi.h"

/* Global Provider Registry */
static WMI_PROVIDER_REGISTRATION g_ProviderRegistry[WMI_MAX_PROVIDERS];
static UINT32 g_ProviderCount = 0;
static AURORA_SPINLOCK g_ProviderLock;
static BOOL g_ProviderSystemInitialized = FALSE;

/* Event Queue Management */
static WMI_EVENT_QUEUE g_EventQueue;
static AURORA_SPINLOCK g_EventLock;
static AURORA_EVENT g_EventAvailable;

/* Provider Registration Functions */
NTSTATUS WmiRegisterProvider(
    IN PGUID ProviderGuid,
    IN PWMI_PROVIDER_CALLBACKS Callbacks,
    IN PVOID Context,
    OUT PHANDLE ProviderHandle
)
{
    if (!ProviderGuid || !Callbacks || !ProviderHandle) {
        return STATUS_INVALID_PARAMETER;
    }

    if (!g_ProviderSystemInitialized) {
        return STATUS_NOT_INITIALIZED;
    }

    AURORA_IRQL oldIrql;
    AuroraAcquireSpinLock(&g_ProviderLock, &oldIrql);

    /* Check if provider already registered */
    for (UINT32 i = 0; i < g_ProviderCount; i++) {
        if (AuroraIsEqualGuid((PGUID)&g_ProviderRegistry[i].ProviderGuid, (PGUID)ProviderGuid)) {
            AuroraReleaseSpinLock(&g_ProviderLock, oldIrql);
            return STATUS_OBJECT_NAME_COLLISION;
        }
    }

    /* Check if we have space for new provider */
    if (g_ProviderCount >= WMI_MAX_PROVIDERS) {
        AuroraReleaseSpinLock(&g_ProviderLock, oldIrql);
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    /* Register new provider */
    UINT32 index = g_ProviderCount;
    WMI_PROVIDER_REGISTRATION* provider = &g_ProviderRegistry[index];

    AuroraMemoryCopy(&provider->ProviderGuid, ProviderGuid, sizeof(GUID));
    AuroraMemoryCopy(&provider->Callbacks, Callbacks, sizeof(WMI_PROVIDER_CALLBACKS));
    provider->Context = Context;
    provider->Handle = (HANDLE)(ULONG_PTR)(index + 1); /* 1-based handle */
    provider->Flags = WMI_PROVIDER_FLAG_ACTIVE;
    provider->RegistrationTime = AuroraGetSystemTime();
    provider->QueryCount = 0;
    provider->EventCount = 0;
    provider->ErrorCount = 0;

    g_ProviderCount++;
    *ProviderHandle = provider->Handle;

    AuroraReleaseSpinLock(&g_ProviderLock, oldIrql);

    WMI_DEBUG_PRINT("Provider registered: GUID=%08X-%04X-%04X, Handle=%p\n",
                   ProviderGuid->Data1, ProviderGuid->Data2, ProviderGuid->Data3, *ProviderHandle);

    return STATUS_SUCCESS;
}

NTSTATUS WmiUnregisterProvider(IN HANDLE ProviderHandle)
{
    if (!ProviderHandle) {
        return STATUS_INVALID_PARAMETER;
    }

    if (!g_ProviderSystemInitialized) {
        return STATUS_NOT_INITIALIZED;
    }

    AURORA_IRQL oldIrql;
    AuroraAcquireSpinLock(&g_ProviderLock, &oldIrql);

    UINT32 index = (UINT32)(ULONG_PTR)ProviderHandle - 1; /* Convert to 0-based */
    if (index >= g_ProviderCount) {
        AuroraReleaseSpinLock(&g_ProviderLock, oldIrql);
        return STATUS_INVALID_HANDLE;
    }

    WMI_PROVIDER_REGISTRATION* provider = &g_ProviderRegistry[index];
    if (provider->Handle != ProviderHandle) {
        AuroraReleaseSpinLock(&g_ProviderLock, oldIrql);
        return STATUS_INVALID_HANDLE;
    }

    /* Mark provider as inactive */
    provider->Flags &= ~WMI_PROVIDER_FLAG_ACTIVE;
    provider->Flags |= WMI_PROVIDER_FLAG_UNREGISTERED;

    /* Compact the registry if this is the last provider */
    if (index == g_ProviderCount - 1) {
        g_ProviderCount--;
    }

    AuroraReleaseSpinLock(&g_ProviderLock, oldIrql);

    WMI_DEBUG_PRINT("Provider unregistered: Handle=%p\n", ProviderHandle);

    return STATUS_SUCCESS;
}

NTSTATUS WmiFindProvider(IN PGUID ProviderGuid, OUT PHANDLE ProviderHandle)
{
    if (!ProviderGuid || !ProviderHandle) {
        return STATUS_INVALID_PARAMETER;
    }

    if (!g_ProviderSystemInitialized) {
        return STATUS_NOT_INITIALIZED;
    }

    AURORA_IRQL oldIrql;
    AuroraAcquireSpinLock(&g_ProviderLock, &oldIrql);

    for (UINT32 i = 0; i < g_ProviderCount; i++) {
        WMI_PROVIDER_REGISTRATION* provider = &g_ProviderRegistry[i];
        if ((provider->Flags & WMI_PROVIDER_FLAG_ACTIVE) &&
            AuroraIsEqualGuid((PGUID)&provider->ProviderGuid, (PGUID)ProviderGuid)) {
            *ProviderHandle = provider->Handle;
            AuroraReleaseSpinLock(&g_ProviderLock, oldIrql);
            return STATUS_SUCCESS;
        }
    }

    AuroraReleaseSpinLock(&g_ProviderLock, oldIrql);
    return STATUS_NOT_FOUND;
}

NTSTATUS WmiEnumerateProviders(
    OUT PGUID ProviderGuids,
    IN OUT PUINT32 ProviderCount
)
{
    if (!ProviderGuids || !ProviderCount) {
        return STATUS_INVALID_PARAMETER;
    }

    if (!g_ProviderSystemInitialized) {
        return STATUS_NOT_INITIALIZED;
    }

    AURORA_IRQL oldIrql;
    AuroraAcquireSpinLock(&g_ProviderLock, &oldIrql);

    UINT32 maxCount = *ProviderCount;
    UINT32 actualCount = 0;

    for (UINT32 i = 0; i < g_ProviderCount && actualCount < maxCount; i++) {
        WMI_PROVIDER_REGISTRATION* provider = &g_ProviderRegistry[i];
        if (provider->Flags & WMI_PROVIDER_FLAG_ACTIVE) {
            AuroraMemoryCopy(&ProviderGuids[actualCount], &provider->ProviderGuid, sizeof(GUID));
            actualCount++;
        }
    }

    *ProviderCount = actualCount;
    AuroraReleaseSpinLock(&g_ProviderLock, oldIrql);

    return STATUS_SUCCESS;
}

/* Event Handling Functions */
NTSTATUS WmiFireEvent(
    IN HANDLE ProviderHandle,
    IN PGUID EventGuid,
    IN PVOID EventData,
    IN UINT32 EventDataSize
)
{
    if (!ProviderHandle || !EventGuid) {
        return STATUS_INVALID_PARAMETER;
    }

    if (!g_ProviderSystemInitialized) {
        return STATUS_NOT_INITIALIZED;
    }

    /* Validate provider handle */
    AURORA_IRQL oldIrql;
    AuroraAcquireSpinLock(&g_ProviderLock, &oldIrql);

    UINT32 index = (UINT32)(ULONG_PTR)ProviderHandle - 1;
    if (index >= g_ProviderCount) {
        AuroraReleaseSpinLock(&g_ProviderLock, oldIrql);
        return STATUS_INVALID_HANDLE;
    }

    WMI_PROVIDER_REGISTRATION* provider = &g_ProviderRegistry[index];
    if (provider->Handle != ProviderHandle || !(provider->Flags & WMI_PROVIDER_FLAG_ACTIVE)) {
        AuroraReleaseSpinLock(&g_ProviderLock, oldIrql);
        return STATUS_INVALID_HANDLE;
    }

    provider->EventCount++;
    AuroraReleaseSpinLock(&g_ProviderLock, oldIrql);

    /* Create event entry */
    WMI_EVENT_ENTRY* eventEntry = (WMI_EVENT_ENTRY*)AuroraAllocatePool(sizeof(WMI_EVENT_ENTRY) + EventDataSize);
    if (!eventEntry) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    AuroraMemoryCopy(&eventEntry->ProviderGuid, &provider->ProviderGuid, sizeof(GUID));
    AuroraMemoryCopy(&eventEntry->EventGuid, EventGuid, sizeof(GUID));
    eventEntry->TimeStamp = AuroraGetSystemTime();
    eventEntry->ProcessId = AuroraGetCurrentProcessId();
    eventEntry->ThreadId = AuroraGetCurrentThreadId();
    eventEntry->DataSize = EventDataSize;
    eventEntry->Flags = WMI_EVENT_FLAG_ACTIVE;

    if (EventData && EventDataSize > 0) {
        AuroraMemoryCopy(eventEntry + 1, EventData, EventDataSize);
    }

    /* Add to event queue */
    AuroraAcquireSpinLock(&g_EventLock, &oldIrql);

    if (g_EventQueue.Count >= WMI_MAX_EVENTS) {
        /* Queue is full, remove oldest event */
        WMI_EVENT_ENTRY* oldestEvent = g_EventQueue.Head;
        g_EventQueue.Head = oldestEvent->Next;
        if (g_EventQueue.Head) {
            g_EventQueue.Head->Previous = NULL;
        } else {
            g_EventQueue.Tail = NULL;
        }
        g_EventQueue.Count--;
        AuroraFreePool(oldestEvent);
    }

    /* Add new event to tail */
    eventEntry->Next = NULL;
    eventEntry->Previous = g_EventQueue.Tail;
    if (g_EventQueue.Tail) {
        g_EventQueue.Tail->Next = eventEntry;
    } else {
        g_EventQueue.Head = eventEntry;
    }
    g_EventQueue.Tail = eventEntry;
    g_EventQueue.Count++;

    AuroraReleaseSpinLock(&g_EventLock, oldIrql);

    /* Signal event availability */
    AuroraSetEvent(&g_EventAvailable);

    WMI_DEBUG_PRINT("Event fired: Provider=%p, Event=%08X-%04X-%04X\n",
                   ProviderHandle, EventGuid->Data1, EventGuid->Data2, EventGuid->Data3);

    return STATUS_SUCCESS;
}

NTSTATUS WmiGetNextEvent(
    OUT PWMI_EVENT_ENTRY EventEntry,
    IN OUT PUINT32 BufferSize,
    IN UINT32 TimeoutMs
)
{
    if (!EventEntry || !BufferSize) {
        return STATUS_INVALID_PARAMETER;
    }

    if (!g_ProviderSystemInitialized) {
        return STATUS_NOT_INITIALIZED;
    }

    /* Wait for event availability */
    NTSTATUS status = AuroraWaitForSingleObject(&g_EventAvailable, TimeoutMs);
    if (status == STATUS_TIMEOUT) {
        return STATUS_TIMEOUT;
    }
    if (!NT_SUCCESS(status)) {
        return status;
    }

    AURORA_IRQL oldIrql;
    AuroraAcquireSpinLock(&g_EventLock, &oldIrql);

    if (g_EventQueue.Count == 0) {
        AuroraReleaseSpinLock(&g_EventLock, oldIrql);
        return STATUS_NO_MORE_ENTRIES;
    }

    /* Get event from head */
    WMI_EVENT_ENTRY* event = g_EventQueue.Head;
    UINT32 requiredSize = sizeof(WMI_EVENT_ENTRY) + event->DataSize;

    if (*BufferSize < requiredSize) {
        *BufferSize = requiredSize;
        AuroraReleaseSpinLock(&g_EventLock, oldIrql);
        return STATUS_BUFFER_TOO_SMALL;
    }

    /* Remove from queue */
    g_EventQueue.Head = event->Next;
    if (g_EventQueue.Head) {
        g_EventQueue.Head->Previous = NULL;
    } else {
        g_EventQueue.Tail = NULL;
    }
    g_EventQueue.Count--;

    AuroraReleaseSpinLock(&g_EventLock, oldIrql);

    /* Copy event data */
    AuroraMemoryCopy(EventEntry, event, requiredSize);
    *BufferSize = requiredSize;

    /* Clean up */
    AuroraFreePool(event);

    return STATUS_SUCCESS;
}

/* Query Handling Functions */
NTSTATUS WmiQueryProvider(
    IN HANDLE ProviderHandle,
    IN PGUID DataBlockGuid,
    IN UINT32 InstanceIndex,
    OUT PVOID Buffer,
    IN OUT PUINT32 BufferSize
)
{
    if (!ProviderHandle || !DataBlockGuid || !Buffer || !BufferSize) {
        return STATUS_INVALID_PARAMETER;
    }

    if (!g_ProviderSystemInitialized) {
        return STATUS_NOT_INITIALIZED;
    }

    /* Find provider */
    AURORA_IRQL oldIrql;
    AuroraAcquireSpinLock(&g_ProviderLock, &oldIrql);

    UINT32 index = (UINT32)(ULONG_PTR)ProviderHandle - 1;
    if (index >= g_ProviderCount) {
        AuroraReleaseSpinLock(&g_ProviderLock, oldIrql);
        return STATUS_INVALID_HANDLE;
    }

    WMI_PROVIDER_REGISTRATION* provider = &g_ProviderRegistry[index];
    if (provider->Handle != ProviderHandle || !(provider->Flags & WMI_PROVIDER_FLAG_ACTIVE)) {
        AuroraReleaseSpinLock(&g_ProviderLock, oldIrql);
        return STATUS_INVALID_HANDLE;
    }

    /* Check if provider supports queries */
    if (!provider->Callbacks.QueryCallback) {
        AuroraReleaseSpinLock(&g_ProviderLock, oldIrql);
        return STATUS_NOT_SUPPORTED;
    }

    provider->QueryCount++;
    WMI_PROVIDER_CALLBACKS callbacks = provider->Callbacks;
    PVOID context = provider->Context;

    AuroraReleaseSpinLock(&g_ProviderLock, oldIrql);

    /* Call provider's query callback */
    NTSTATUS status = callbacks.QueryCallback(
        DataBlockGuid,
        InstanceIndex,
        Buffer,
        BufferSize,
        context
    );

    if (!NT_SUCCESS(status)) {
        AuroraAcquireSpinLock(&g_ProviderLock, &oldIrql);
        if (index < g_ProviderCount && g_ProviderRegistry[index].Handle == ProviderHandle) {
            g_ProviderRegistry[index].ErrorCount++;
        }
        AuroraReleaseSpinLock(&g_ProviderLock, oldIrql);
    }

    return status;
}

NTSTATUS WmiSetProvider(
    IN HANDLE ProviderHandle,
    IN PGUID DataBlockGuid,
    IN UINT32 InstanceIndex,
    IN PVOID Buffer,
    IN UINT32 BufferSize
)
{
    if (!ProviderHandle || !DataBlockGuid || !Buffer) {
        return STATUS_INVALID_PARAMETER;
    }

    if (!g_ProviderSystemInitialized) {
        return STATUS_NOT_INITIALIZED;
    }

    /* Find provider */
    AURORA_IRQL oldIrql;
    AuroraAcquireSpinLock(&g_ProviderLock, &oldIrql);

    UINT32 index = (UINT32)(ULONG_PTR)ProviderHandle - 1;
    if (index >= g_ProviderCount) {
        AuroraReleaseSpinLock(&g_ProviderLock, oldIrql);
        return STATUS_INVALID_HANDLE;
    }

    WMI_PROVIDER_REGISTRATION* provider = &g_ProviderRegistry[index];
    if (provider->Handle != ProviderHandle || !(provider->Flags & WMI_PROVIDER_FLAG_ACTIVE)) {
        AuroraReleaseSpinLock(&g_ProviderLock, oldIrql);
        return STATUS_INVALID_HANDLE;
    }

    /* Check if provider supports set operations */
    if (!provider->Callbacks.SetCallback) {
        AuroraReleaseSpinLock(&g_ProviderLock, oldIrql);
        return STATUS_NOT_SUPPORTED;
    }

    WMI_PROVIDER_CALLBACKS callbacks = provider->Callbacks;
    PVOID context = provider->Context;

    AuroraReleaseSpinLock(&g_ProviderLock, oldIrql);

    /* Call provider's set callback */
    NTSTATUS status = callbacks.SetCallback(
        DataBlockGuid,
        InstanceIndex,
        Buffer,
        BufferSize,
        context
    );

    if (!NT_SUCCESS(status)) {
        AuroraAcquireSpinLock(&g_ProviderLock, &oldIrql);
        if (index < g_ProviderCount && g_ProviderRegistry[index].Handle == ProviderHandle) {
            g_ProviderRegistry[index].ErrorCount++;
        }
        AuroraReleaseSpinLock(&g_ProviderLock, oldIrql);
    }

    return status;
}

/* Provider System Management */
NTSTATUS WmiInitializeProviderSystem(void)
{
    if (g_ProviderSystemInitialized) {
        return STATUS_SUCCESS;
    }

    /* Initialize provider registry */
    AuroraMemoryZero(g_ProviderRegistry, sizeof(g_ProviderRegistry));
    g_ProviderCount = 0;
    AuroraInitializeSpinLock(&g_ProviderLock);

    /* Initialize event queue */
    AuroraMemoryZero(&g_EventQueue, sizeof(g_EventQueue));
    AuroraInitializeSpinLock(&g_EventLock);
    AuroraInitializeEvent(&g_EventAvailable, FALSE, FALSE);

    g_ProviderSystemInitialized = TRUE;

    WMI_DEBUG_PRINT("WMI Provider System initialized\n");

    return STATUS_SUCCESS;
}

NTSTATUS WmiShutdownProviderSystem(void)
{
    if (!g_ProviderSystemInitialized) {
        return STATUS_SUCCESS;
    }

    /* Unregister all providers */
    AURORA_IRQL oldIrql;
    AuroraAcquireSpinLock(&g_ProviderLock, &oldIrql);

    for (UINT32 i = 0; i < g_ProviderCount; i++) {
        WMI_PROVIDER_REGISTRATION* provider = &g_ProviderRegistry[i];
        if (provider->Flags & WMI_PROVIDER_FLAG_ACTIVE) {
            provider->Flags &= ~WMI_PROVIDER_FLAG_ACTIVE;
            provider->Flags |= WMI_PROVIDER_FLAG_UNREGISTERED;
        }
    }

    g_ProviderCount = 0;
    AuroraReleaseSpinLock(&g_ProviderLock, oldIrql);

    /* Flush all events */
    WmiFlushEvents(NULL);

    g_ProviderSystemInitialized = FALSE;

    WMI_DEBUG_PRINT("WMI Provider System shutdown\n");

    return STATUS_SUCCESS;
}

NTSTATUS WmiGetProviderStatistics(
    IN HANDLE ProviderHandle,
    OUT PWMI_PROVIDER_STATISTICS Statistics
)
{
    if (!ProviderHandle || !Statistics) {
        return STATUS_INVALID_PARAMETER;
    }

    if (!g_ProviderSystemInitialized) {
        return STATUS_NOT_INITIALIZED;
    }

    AURORA_IRQL oldIrql;
    AuroraAcquireSpinLock(&g_ProviderLock, &oldIrql);

    UINT32 index = (UINT32)(ULONG_PTR)ProviderHandle - 1;
    if (index >= g_ProviderCount) {
        AuroraReleaseSpinLock(&g_ProviderLock, oldIrql);
        return STATUS_INVALID_HANDLE;
    }

    WMI_PROVIDER_REGISTRATION* provider = &g_ProviderRegistry[index];
    if (provider->Handle != ProviderHandle) {
        AuroraReleaseSpinLock(&g_ProviderLock, oldIrql);
        return STATUS_INVALID_HANDLE;
    }

    /* Copy statistics */
    AuroraMemoryCopy(&Statistics->ProviderGuid, &provider->ProviderGuid, sizeof(GUID));
    Statistics->RegistrationTime = provider->RegistrationTime;
    Statistics->QueryCount = provider->QueryCount;
    Statistics->EventCount = provider->EventCount;
    Statistics->ErrorCount = provider->ErrorCount;
    Statistics->Flags = provider->Flags;

    AuroraReleaseSpinLock(&g_ProviderLock, oldIrql);

    return STATUS_SUCCESS;
}
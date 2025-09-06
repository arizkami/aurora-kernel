/*
 * Aurora Kernel - Main Header File
 * NTCore Kernel with WMI Support
 * Copyright (c) 2024 NTCore Project
 */

#ifndef _AURORA_H_
#define _AURORA_H_

/* Kernel-specific type definitions */
typedef unsigned char       uint8_t;
typedef unsigned short      uint16_t;
typedef unsigned int        uint32_t;
typedef unsigned long long  uint64_t;
typedef signed char         int8_t;
typedef signed short        int16_t;
typedef signed int          int32_t;
typedef signed long long    int64_t;
typedef uint64_t            size_t;
typedef uint64_t            uintptr_t;
typedef int64_t             ptrdiff_t;
typedef int64_t             ssize_t; /* added for driver read/write */
typedef unsigned short      wchar_t;

/* Boolean type */
typedef uint8_t             bool;
#define true                1
#define false               0

/* Windows-style boolean */
typedef int                 BOOL;
#define TRUE                1
#define FALSE               0

/* NULL definition */
#ifndef NULL
#define NULL                ((void*)0)
#endif

/* Parameter direction macros */
#define IN
#define OUT
#define OPTIONAL

/* Variadic arguments support */
typedef __builtin_va_list va_list;
#define va_start(v,l) __builtin_va_start(v,l)
#define va_end(v) __builtin_va_end(v)
#define va_arg(v,l) __builtin_va_arg(v,l)

/* Basic string functions */
size_t strlen(const char* str);
int strcmp(const char* str1, const char* str2);
int strncmp(const char* str1, const char* str2, size_t n);
char* strcpy(char* dest, const char* src);
char* strncpy(char* dest, const char* src, size_t n);
void* memset(void* ptr, int value, size_t num);
void* memcpy(void* dest, const void* src, size_t num);
int memcmp(const void* ptr1, const void* ptr2, size_t num);

/* Kernel Version Information */
#define AURORA_VERSION_MAJOR    1
#define AURORA_VERSION_MINOR    0
#define AURORA_VERSION_BUILD    0
#define AURORA_KERNEL_NAME      "Aurora"

/* Architecture Detection */
#if defined(_M_X64) || defined(__x86_64__)
    #define AURORA_ARCH_AMD64
    #define AURORA_ARCH_NAME "amd64"
#elif defined(_M_IX86) || defined(__i386__)
    #define AURORA_ARCH_X86
    #define AURORA_ARCH_NAME "x86"
#elif defined(_M_ARM64) || defined(__aarch64__)
    #define AURORA_ARCH_AARCH64
    #define AURORA_ARCH_NAME "aarch64"
#else
    #error "Unsupported architecture"
#endif

/* Common Types */
typedef uint8_t     UINT8;
typedef uint16_t    UINT16;
typedef uint32_t    UINT32;
typedef uint64_t    UINT64;
typedef int8_t      INT8;
typedef int16_t     INT16;
typedef int32_t     INT32;
typedef int64_t     INT64;
typedef void*       PVOID;
typedef char        CHAR;
typedef char*       PCHAR;
typedef const char* PCSTR; /* Added for VFS & driver interfaces */
typedef wchar_t*    PWCHAR;
typedef UINT32*     PUINT32;
typedef UINT64*     PUINT64;
typedef void*       HANDLE;
typedef HANDLE*     PHANDLE;
typedef UINT64      ULONG_PTR;
typedef struct {
    UINT32 Data1;
    UINT16 Data2;
    UINT16 Data3;
    UINT8  Data4[8];
} GUID;
typedef GUID*       PGUID;
typedef UINT32      AURORA_IRQL;
typedef UINT32      AURORA_SPINLOCK;
typedef UINT32      AURORA_EVENT;
typedef AURORA_SPINLOCK* PAURORA_SPINLOCK;
typedef AURORA_IRQL* PAURORA_IRQL;
typedef AURORA_EVENT* PAURORA_EVENT;

/* Status Codes */
typedef UINT32 NTSTATUS;
#define STATUS_SUCCESS                  0x00000000
#define STATUS_UNSUCCESSFUL             0xC0000001
#define STATUS_NOT_IMPLEMENTED          0xC0000002
#define STATUS_INVALID_PARAMETER        0xC000000D
#define STATUS_INSUFFICIENT_RESOURCES   0xC000009A
#define STATUS_ACCESS_DENIED            0xC0000022
#define STATUS_NOT_INITIALIZED          0xC0000007
#define STATUS_INVALID_HANDLE           0xC0000008
#define STATUS_NOT_SUPPORTED            0xC00000BB
#define STATUS_TIMEOUT                  0x00000102
#define STATUS_NO_MORE_ENTRIES          0x8000001A
#define STATUS_BUFFER_TOO_SMALL         0xC0000023
#define STATUS_OBJECT_NAME_COLLISION    0xC0000035
#define STATUS_NOT_FOUND                0xC0000225
#define STATUS_INVALID_IMAGE_FORMAT     0xC000007B

#define NT_SUCCESS(Status) ((NTSTATUS)(Status) == STATUS_SUCCESS)

/* WMI Related Definitions */
#define WMI_MAX_PROVIDERS               256
#define WMI_MAX_EVENTS                  1024
#define WMI_MAX_GUID_LENGTH             16
#define WMI_MAX_NAME_LENGTH             256
#define WMI_MAX_DATA_SIZE               65536

/* WMI Event Flags */
#define WMI_EVENT_FLAG_ACTIVE           0x00000001

/* WMI GUID Structure */
typedef struct _WMI_GUID {
    UINT32 Data1;
    UINT16 Data2;
    UINT16 Data3;
    UINT8  Data4[8];
} WMI_GUID, *PWMI_GUID;

/* WMI Event Types */
typedef enum _WMI_EVENT_TYPE {
    WmiEventTypeTrace = 0,
    WmiEventTypeNotification,
    WmiEventTypeDataBlock,
    WmiEventTypeMethod,
    WmiEventTypeMax
} WMI_EVENT_TYPE;

/* WMI Provider Information */
typedef struct _WMI_PROVIDER_INFO {
    WMI_GUID Guid;
    CHAR Name[WMI_MAX_NAME_LENGTH];
    UINT32 Flags;
    UINT32 EventCount;
    PVOID Context;
    struct _WMI_PROVIDER_INFO* Next;
} WMI_PROVIDER_INFO, *PWMI_PROVIDER_INFO;

/* WMI Event Structure */
typedef struct _WMI_EVENT {
    WMI_GUID ProviderId;
    WMI_EVENT_TYPE EventType;
    UINT32 EventId;
    UINT32 DataSize;
    UINT64 TimeStamp;
    PVOID EventData;
    struct _WMI_EVENT* Next;
} WMI_EVENT, *PWMI_EVENT;

/* WMI Callback Functions */
typedef NTSTATUS (*PWMI_QUERY_CALLBACK)(
    IN PGUID DataBlockGuid,
    IN UINT32 InstanceIndex,
    OUT PVOID Buffer,
    IN OUT PUINT32 BufferSize,
    IN PVOID Context
);

typedef NTSTATUS (*PWMI_SET_CALLBACK)(
    IN PGUID DataBlockGuid,
    IN UINT32 InstanceIndex,
    IN PVOID Buffer,
    IN UINT32 BufferSize,
    IN PVOID Context
);

typedef NTSTATUS (*PWMI_METHOD_CALLBACK)(
    IN PGUID DataBlockGuid,
    IN UINT32 MethodId,
    IN PVOID InputBuffer,
    IN UINT32 InputSize,
    OUT PVOID OutputBuffer,
    IN OUT PUINT32 OutputSize,
    IN PVOID Context
);

/* WMI Provider Callbacks */
typedef struct _WMI_PROVIDER_CALLBACKS {
    PWMI_QUERY_CALLBACK QueryCallback;
    PWMI_SET_CALLBACK SetCallback;
    PWMI_METHOD_CALLBACK MethodCallback;
} WMI_PROVIDER_CALLBACKS, *PWMI_PROVIDER_CALLBACKS;

/* WMI Provider Registration Structure */
typedef struct _WMI_PROVIDER_REGISTRATION {
    WMI_GUID Guid;
    WMI_GUID ProviderGuid;
    PCHAR Name;
    UINT32 Flags;
    HANDLE Handle;
    WMI_PROVIDER_CALLBACKS Callbacks;
    PWMI_QUERY_CALLBACK QueryCallback;
    PWMI_SET_CALLBACK SetCallback;
    PWMI_METHOD_CALLBACK MethodCallback;
    PVOID Context;
    UINT64 RegistrationTime;
    UINT32 QueryCount;
    UINT32 EventCount;
    UINT32 ErrorCount;
} WMI_PROVIDER_REGISTRATION, *PWMI_PROVIDER_REGISTRATION;

/* WMI Event Queue */
typedef struct _WMI_EVENT_ENTRY {
    WMI_GUID ProviderGuid;
    WMI_GUID EventGuid;
    UINT32 DataSize;
    UINT64 TimeStamp;
    UINT32 ProcessId;
    UINT32 ThreadId;
    UINT32 Flags;
    PVOID EventData;
    struct _WMI_EVENT_ENTRY* Next;
    struct _WMI_EVENT_ENTRY* Previous;
} WMI_EVENT_ENTRY, *PWMI_EVENT_ENTRY;

typedef struct _WMI_EVENT_QUEUE {
    PWMI_EVENT_ENTRY Head;
    PWMI_EVENT_ENTRY Tail;
    UINT32 Count;
    UINT32 MaxCount;
} WMI_EVENT_QUEUE, *PWMI_EVENT_QUEUE;

/* WMI Provider Statistics */
typedef struct _WMI_PROVIDER_STATISTICS {
    WMI_GUID ProviderGuid;
    UINT64 RegistrationTime;
    UINT32 QueryCount;
    UINT32 SetCount;
    UINT32 EventCount;
    UINT32 ErrorCount;
    UINT32 Flags;
    UINT64 TotalDataSize;
    UINT64 LastAccessTime;
} WMI_PROVIDER_STATISTICS, *PWMI_PROVIDER_STATISTICS;

/* Kernel Memory Management */
typedef struct _MEMORY_DESCRIPTOR {
    PVOID BaseAddress;
    UINT64 Size;
    UINT32 Protection;
    UINT32 Type;
} MEMORY_DESCRIPTOR, *PMEMORY_DESCRIPTOR;

/* Process Structure */
typedef struct _AURORA_PROCESS {
    UINT32 ProcessId;
    CHAR ImageName[256];
    PVOID ImageBase;
    UINT64 ImageSize;
    UINT32 ThreadCount;
    UINT64 CreateTime;
    struct _AURORA_PROCESS* Next;
} AURORA_PROCESS, *PAURORA_PROCESS;

/* Thread Structure */
typedef struct _AURORA_THREAD {
    UINT32 ThreadId;
    UINT32 ProcessId;
    PVOID StackBase;
    UINT64 StackSize;
    UINT32 Priority;
    UINT64 CreateTime;
    struct _AURORA_THREAD* Next;
} AURORA_THREAD, *PAURORA_THREAD;

/* Kernel Global Variables */
extern PWMI_PROVIDER_INFO g_WmiProviders;
extern PWMI_EVENT g_WmiEvents;
extern UINT32 g_WmiProviderCount;
extern UINT32 g_WmiEventCount;

/* Function Declarations */

/* WMI Core Functions */
NTSTATUS WmiInitialize(void);
NTSTATUS WmiShutdown(void);
NTSTATUS WmiRegisterProvider(IN PGUID ProviderGuid, IN PWMI_PROVIDER_CALLBACKS Callbacks, IN PVOID Context, OUT PHANDLE ProviderHandle);
NTSTATUS WmiUnregisterProvider(IN HANDLE ProviderHandle);
NTSTATUS WmiFireEvent(IN HANDLE ProviderHandle, IN PGUID EventGuid, IN PVOID EventData, IN UINT32 EventDataSize);
NTSTATUS WmiQueryProvider(IN HANDLE ProviderHandle, IN PGUID DataBlockGuid, IN UINT32 InstanceIndex, OUT PVOID Buffer, IN OUT PUINT32 BufferSize);

/* Memory Management */
PVOID AuroraAllocateMemory(IN UINT64 Size);
PVOID AuroraAllocatePool(IN UINT64 Size);
void AuroraFreeMemory(IN PVOID Memory);
void AuroraMemoryZero(IN PVOID Memory, IN UINT64 Size);
void AuroraMemoryCopy(OUT PVOID Destination, IN PVOID Source, IN UINT64 Size);
NTSTATUS AuroraMapMemory(IN PVOID VirtualAddress, IN UINT64 PhysicalAddress, IN UINT64 Size, IN UINT32 Protection);
NTSTATUS AuroraUnmapMemory(IN PVOID VirtualAddress, IN UINT64 Size);

/* Process Management */
NTSTATUS AuroraCreateProcess(IN PCHAR ImagePath, OUT PUINT32 ProcessId);
NTSTATUS AuroraTerminateProcess(IN UINT32 ProcessId);
PAURORA_PROCESS AuroraGetProcessById(IN UINT32 ProcessId);
NTSTATUS AuroraEnumerateProcesses(OUT PAURORA_PROCESS* ProcessList, OUT PUINT32 ProcessCount);

/* Thread Management */
NTSTATUS AuroraCreateThread(IN UINT32 ProcessId, IN PVOID StartAddress, IN PVOID Parameter, OUT PUINT32 ThreadId);
NTSTATUS AuroraTerminateThread(IN UINT32 ThreadId);
PAURORA_THREAD AuroraGetThreadById(IN UINT32 ThreadId);
NTSTATUS AuroraEnumerateThreads(IN UINT32 ProcessId, OUT PAURORA_THREAD* ThreadList, OUT PUINT32 ThreadCount);

/* Kernel Initialization */
NTSTATUS AuroraKernelInitialize(void);
void AuroraKernelShutdown(void);

/* Debug and Logging */
void AuroraDebugPrint(IN PCHAR Format, ...);
void AuroraLogEvent(IN UINT32 Level, IN PCHAR Message);
BOOL AuroraIsEqualGuid(IN PGUID Guid1, IN PGUID Guid2);
NTSTATUS AuroraSetEvent(IN PVOID Event);
NTSTATUS AuroraWaitForSingleObject(IN PVOID Object, IN UINT32 TimeoutMs);
void AuroraFreePool(IN PVOID Memory);
UINT64 AuroraGetSystemTime(void);
UINT32 AuroraGetCurrentProcessId(void);
UINT32 AuroraGetCurrentThreadId(void);

/* Aurora Synchronization Functions */
void AuroraAcquireSpinLock(IN PAURORA_SPINLOCK SpinLock, OUT PAURORA_IRQL OldIrql);
void AuroraReleaseSpinLock(IN PAURORA_SPINLOCK SpinLock, IN AURORA_IRQL OldIrql);
NTSTATUS AuroraInitializeSpinLock(OUT PAURORA_SPINLOCK SpinLock);
NTSTATUS AuroraInitializeEvent(OUT PAURORA_EVENT Event, IN BOOL ManualReset, IN BOOL InitialState);

/* I/O Manager */
NTSTATUS IoInitialize(void);


/* Utility Macros */
#define AURORA_SUCCESS(Status) ((Status) == STATUS_SUCCESS)
#define AURORA_FAILED(Status) ((Status) != STATUS_SUCCESS)
#define AURORA_ALIGN_UP(Value, Alignment) (((Value) + (Alignment) - 1) & ~((Alignment) - 1))
#define AURORA_ALIGN_DOWN(Value, Alignment) ((Value) & ~((Alignment) - 1))
#define AURORA_ARRAY_SIZE(Array) (sizeof(Array) / sizeof((Array)[0]))

/* Architecture-specific includes */
#ifdef AURORA_ARCH_AMD64
    #include "wmi/amd64/wmi_arch.h"
#elif defined(AURORA_ARCH_X86)
    #include "wmi/x86/wmi_arch.h"
#elif defined(AURORA_ARCH_AARCH64)
    #include "wmi/aarch64/wmi_arch.h"
#endif

#ifdef __has_include
#if __has_include(<stdarg.h>)
#include <stdarg.h>
#endif
#endif

#endif /* _AURORA_H_ */
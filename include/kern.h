/*
 * Aurora Kernel - Core Kernel Definitions
 * Copyright (c) 2024 Aurora Project
 */

#ifndef _KERN_H_
#define _KERN_H_

#include "../aurora.h"

/* Additional type definitions */
#ifndef VOID
#define VOID void
#endif

#ifndef PCSTR
typedef const char* PCSTR;
#endif

#ifndef SIZE_T
#ifdef _WIN64
typedef unsigned long long SIZE_T;
#else
typedef unsigned int SIZE_T;
#endif
#endif

#ifndef UINT_PTR
#ifdef _WIN64
typedef unsigned long long UINT_PTR;
#else
typedef unsigned int UINT_PTR;
#endif
#endif

/* Process and Thread ID types */
typedef UINT32 PROCESS_ID;
typedef UINT32 THREAD_ID;

#ifndef PPROCESS_ID
typedef PROCESS_ID* PPROCESS_ID;
#endif

#ifndef PTHREAD_ID
typedef THREAD_ID* PTHREAD_ID;
#endif

/* Additional status codes */
#ifndef STATUS_ALREADY_INITIALIZED
#define STATUS_ALREADY_INITIALIZED ((NTSTATUS)0xC0000021L)
#endif

#ifndef STATUS_ACCESS_VIOLATION
#define STATUS_ACCESS_VIOLATION ((NTSTATUS)0xC0000005L)
#endif

/* Utility macros */
#ifndef UNREFERENCED_PARAMETER
#define UNREFERENCED_PARAMETER(P) ((void)(P))
#endif

/* Kernel Version Information */
#define AURORA_KERNEL_VERSION_MAJOR    1
#define AURORA_KERNEL_VERSION_MINOR    0
#define AURORA_KERNEL_VERSION_BUILD    0
#define AURORA_KERNEL_VERSION_REVISION 1

/* Kernel Constants */
#define KERNEL_STACK_SIZE       0x4000      /* 16KB kernel stack */
#define MAX_PROCESSES           1024        /* Maximum number of processes */
#define MAX_THREADS_PER_PROCESS 256         /* Maximum threads per process */
#define PROCESS_NAME_MAX        64          /* Maximum process name length */
#define THREAD_NAME_MAX         32          /* Maximum thread name length */

/* Process States */
typedef enum _PROCESS_STATE {
    ProcessStateCreated = 0,
    ProcessStateReady,
    ProcessStateRunning,
    ProcessStateWaiting,
    ProcessStateSuspended,
    ProcessStateTerminated
} PROCESS_STATE, *PPROCESS_STATE;

/* Thread States */
typedef enum _THREAD_STATE {
    ThreadStateInitialized = 0,
    ThreadStateReady,
    ThreadStateRunning,
    ThreadStateWaiting,
    ThreadStateTerminated
} THREAD_STATE, *PTHREAD_STATE;

/* Priority Levels */
typedef enum _THREAD_PRIORITY {
    PriorityIdle = 0,
    PriorityLow = 1,
    PriorityNormal = 2,
    PriorityHigh = 3,
    PriorityRealtime = 4
} THREAD_PRIORITY, *PTHREAD_PRIORITY;

/* Process ID and Thread ID types */
typedef UINT32 PROCESS_ID;
typedef UINT32 THREAD_ID;

/* Forward declarations */
typedef struct _PROCESS PROCESS, *PPROCESS;
typedef struct _THREAD THREAD, *PTHREAD;
typedef struct _SCHEDULER_CONTEXT SCHEDULER_CONTEXT, *PSCHEDULER_CONTEXT;

/* CPU Context Structure (architecture-specific) */
typedef struct _CPU_CONTEXT {
    /* Architecture-specific register context */
    /* This will be defined in architecture-specific headers */
    UINT_PTR ContextData[32];  /* Generic placeholder */
} CPU_CONTEXT, *PCPU_CONTEXT;

/* Thread Control Block */
typedef struct _THREAD {
    /* Thread identification */
    THREAD_ID ThreadId;
    PROCESS_ID ProcessId;
    CHAR ThreadName[THREAD_NAME_MAX];
    
    /* Thread state */
    THREAD_STATE State;
    THREAD_PRIORITY Priority;
    UINT32 TimeSlice;
    UINT64 CreationTime;
    UINT64 KernelTime;
    UINT64 UserTime;
    
    /* CPU context */
    CPU_CONTEXT Context;
    PVOID KernelStack;
    PVOID UserStack;
    UINT_PTR StackSize;
    
    /* Synchronization */
    AURORA_SPINLOCK ThreadLock;
    PVOID WaitObject;
    UINT32 WaitReason;
    
    /* Linked list pointers */
    struct _THREAD* NextThread;
    struct _THREAD* PreviousThread;
    
    /* Parent process */
    PPROCESS ParentProcess;
    
    /* Thread-local storage */
    PVOID TlsData;
} THREAD, *PTHREAD;

/* Process Control Block */
typedef struct _PROCESS {
    /* Process identification */
    PROCESS_ID ProcessId;
    PROCESS_ID ParentProcessId;
    CHAR ProcessName[PROCESS_NAME_MAX];
    
    /* Process state */
    PROCESS_STATE State;
    UINT32 ThreadCount;
    UINT64 CreationTime;
    
    /* Memory management */
    PVOID VirtualAddressSpace;
    UINT_PTR ImageBase;
    UINT_PTR ImageSize;
    
    /* Thread management */
    PTHREAD ThreadList;
    PTHREAD MainThread;
    AURORA_SPINLOCK ProcessLock;
    
    /* Handle table */
    PVOID HandleTable;
    
    /* Security context */
    PVOID SecurityContext;
    
    /* Linked list pointers */
    struct _PROCESS* NextProcess;
    struct _PROCESS* PreviousProcess;
    
    /* Exit code */
    UINT32 ExitCode;
} PROCESS, *PPROCESS;

/* Scheduler Context */
typedef struct _SCHEDULER_CONTEXT {
    /* Current running thread */
    PTHREAD CurrentThread;
    
    /* Ready queues for each priority level */
    PTHREAD ReadyQueues[5];  /* One for each priority level */
    
    /* Scheduler statistics */
    UINT64 ContextSwitches;
    UINT64 SchedulerTicks;
    
    /* Scheduler lock */
    AURORA_SPINLOCK SchedulerLock;
    
    /* Idle thread */
    PTHREAD IdleThread;
} SCHEDULER_CONTEXT, *PSCHEDULER_CONTEXT;

/* System Call Numbers */
#define SYSCALL_EXIT            0x01
#define SYSCALL_CREATE_PROCESS  0x02
#define SYSCALL_CREATE_THREAD   0x03
#define SYSCALL_TERMINATE_THREAD 0x04
#define SYSCALL_SLEEP           0x05
#define SYSCALL_YIELD           0x06
#define SYSCALL_GET_PROCESS_ID  0x07
#define SYSCALL_GET_THREAD_ID   0x08
#define SYSCALL_WAIT_FOR_OBJECT 0x09
#define SYSCALL_SIGNAL_OBJECT   0x0A

/* Kernel Function Declarations */

/* Process Management */
NTSTATUS KernCreateProcess(
    IN PCSTR ProcessName,
    IN PVOID ImageBase,
    IN UINT_PTR ImageSize,
    OUT PPROCESS_ID ProcessId
);

NTSTATUS KernTerminateProcess(
    IN PROCESS_ID ProcessId,
    IN UINT32 ExitCode
);

PPROCESS KernGetProcessById(IN PROCESS_ID ProcessId);
PPROCESS KernGetCurrentProcess(void);

/* Thread Management */
NTSTATUS KernCreateThread(
    IN PROCESS_ID ProcessId,
    IN PVOID StartAddress,
    IN PVOID Parameter,
    IN THREAD_PRIORITY Priority,
    OUT PTHREAD_ID ThreadId
);

NTSTATUS KernTerminateThread(
    IN THREAD_ID ThreadId,
    IN UINT32 ExitCode
);

PTHREAD KernGetThreadById(IN THREAD_ID ThreadId);
PTHREAD KernGetCurrentThread(void);

/* Scheduler Functions */
NTSTATUS KernInitializeScheduler(void);
VOID KernSchedule(void);
VOID KernYieldProcessor(void);
NTSTATUS KernSleep(IN UINT32 Milliseconds);

/* System Call Interface */
UINT_PTR KernSystemCallHandler(
    IN UINT32 SystemCallNumber,
    IN UINT_PTR Parameter1,
    IN UINT_PTR Parameter2,
    IN UINT_PTR Parameter3,
    IN UINT_PTR Parameter4
);

/* Kernel Initialization */
NTSTATUS KernInitialize(void);
VOID KernShutdown(void);

/* Memory management functions */
PVOID KernAllocateMemory(IN SIZE_T Size);
VOID KernFreeMemory(IN PVOID Memory);

/* Debug functions */
VOID KernDebugPrint(IN PCSTR Format, ...);
VOID KernPanic(IN PCSTR Message);

/* Exports for arch */
extern SCHEDULER_CONTEXT g_SchedulerContext;
extern PPROCESS g_CurrentProcess;
extern PTHREAD g_CurrentThread;
VOID KernSetCurrentThread(PTHREAD Thread);

#endif /* _KERN_H_ */
/*
 * Aurora Kernel - Core Kernel Implementation
 * Copyright (c) 2024 Aurora Project
 */

#include "../aurora.h"
#include "../include/kern.h"

/* Global kernel state */
static BOOL g_KernelInitialized = FALSE;
static PROCESS g_ProcessTable[MAX_PROCESSES];
static THREAD g_ThreadTable[MAX_PROCESSES * MAX_THREADS_PER_PROCESS];
static SCHEDULER_CONTEXT g_SchedulerContext;
static AURORA_SPINLOCK g_ProcessTableLock;
static AURORA_SPINLOCK g_ThreadTableLock;
static PROCESS_ID g_NextProcessId = 1;
static THREAD_ID g_NextThreadId = 1;

/* Current process and thread (per-CPU) */
static PPROCESS g_CurrentProcess = NULL;
static PTHREAD g_CurrentThread = NULL;

/*
 * Initialize the kernel subsystem
 */
NTSTATUS KernInitialize(void)
{
    if (g_KernelInitialized) {
        return STATUS_ALREADY_INITIALIZED;
    }

    /* Initialize locks */
    AuroraInitializeSpinLock(&g_ProcessTableLock);
    AuroraInitializeSpinLock(&g_ThreadTableLock);
    AuroraInitializeSpinLock(&g_SchedulerContext.SchedulerLock);

    /* Clear process and thread tables */
    memset(g_ProcessTable, 0, sizeof(g_ProcessTable));
    memset(g_ThreadTable, 0, sizeof(g_ThreadTable));
    memset(&g_SchedulerContext, 0, sizeof(g_SchedulerContext));

    /* Initialize scheduler */
    NTSTATUS status = KernInitializeScheduler();
    if (!NT_SUCCESS(status)) {
        return status;
    }

    g_KernelInitialized = TRUE;
    KernDebugPrint("Aurora Kernel initialized successfully\n");
    
    return STATUS_SUCCESS;
}

/*
 * Shutdown the kernel subsystem
 */
VOID KernShutdown(void)
{
    if (!g_KernelInitialized) {
        return;
    }

    /* Terminate all processes */
    AURORA_IRQL oldIrql;
    AuroraAcquireSpinLock(&g_ProcessTableLock, &oldIrql);
    
    for (UINT32 i = 0; i < MAX_PROCESSES; i++) {
        if (g_ProcessTable[i].ProcessId != 0) {
            KernTerminateProcess(g_ProcessTable[i].ProcessId, 0);
        }
    }
    
    AuroraReleaseSpinLock(&g_ProcessTableLock, oldIrql);

    g_KernelInitialized = FALSE;
    KernDebugPrint("Aurora Kernel shutdown completed\n");
}

/*
 * Create a new process
 */
NTSTATUS KernCreateProcess(
    IN PCSTR ProcessName,
    IN PVOID ImageBase,
    IN UINT_PTR ImageSize,
    OUT PPROCESS_ID ProcessId
)
{
    if (!g_KernelInitialized || !ProcessName || !ProcessId) {
        return STATUS_INVALID_PARAMETER;
    }

    AURORA_IRQL oldIrql;
    AuroraAcquireSpinLock(&g_ProcessTableLock, &oldIrql);

    /* Find free process slot */
    PPROCESS process = NULL;
    for (UINT32 i = 0; i < MAX_PROCESSES; i++) {
        if (g_ProcessTable[i].ProcessId == 0) {
            process = &g_ProcessTable[i];
            break;
        }
    }

    if (!process) {
        AuroraReleaseSpinLock(&g_ProcessTableLock, oldIrql);
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    /* Initialize process */
    memset(process, 0, sizeof(PROCESS));
    process->ProcessId = g_NextProcessId++;
    process->ParentProcessId = g_CurrentProcess ? g_CurrentProcess->ProcessId : 0;
    strncpy(process->ProcessName, ProcessName, PROCESS_NAME_MAX - 1);
    process->State = ProcessStateCreated;
    process->ImageBase = (UINT_PTR)ImageBase;
    process->ImageSize = ImageSize;
    process->CreationTime = AuroraGetSystemTime();
    
    AuroraInitializeSpinLock(&process->ProcessLock);

    *ProcessId = process->ProcessId;
    
    AuroraReleaseSpinLock(&g_ProcessTableLock, oldIrql);
    
    KernDebugPrint("Created process '%s' with ID %u\n", ProcessName, *ProcessId);
    return STATUS_SUCCESS;
}

/*
 * Terminate a process
 */
NTSTATUS KernTerminateProcess(
    IN PROCESS_ID ProcessId,
    IN UINT32 ExitCode
)
{
    if (!g_KernelInitialized) {
        return STATUS_NOT_INITIALIZED;
    }

    PPROCESS process = KernGetProcessById(ProcessId);
    if (!process) {
        return STATUS_INVALID_PARAMETER;
    }

    AURORA_IRQL oldIrql;
    AuroraAcquireSpinLock(&process->ProcessLock, &oldIrql);

    /* Terminate all threads in the process */
    PTHREAD thread = process->ThreadList;
    while (thread) {
        PTHREAD nextThread = thread->NextThread;
        KernTerminateThread(thread->ThreadId, ExitCode);
        thread = nextThread;
    }

    /* Update process state */
    process->State = ProcessStateTerminated;
    process->ExitCode = ExitCode;

    AuroraReleaseSpinLock(&process->ProcessLock, oldIrql);
    
    KernDebugPrint("Terminated process ID %u with exit code %u\n", ProcessId, ExitCode);
    return STATUS_SUCCESS;
}

/*
 * Get process by ID
 */
PPROCESS KernGetProcessById(IN PROCESS_ID ProcessId)
{
    if (!g_KernelInitialized || ProcessId == 0) {
        return NULL;
    }

    for (UINT32 i = 0; i < MAX_PROCESSES; i++) {
        if (g_ProcessTable[i].ProcessId == ProcessId) {
            return &g_ProcessTable[i];
        }
    }

    return NULL;
}

/*
 * Get current process
 */
PPROCESS KernGetCurrentProcess(void)
{
    return g_CurrentProcess;
}

/*
 * Create a new thread
 */
NTSTATUS KernCreateThread(
    IN PROCESS_ID ProcessId,
    IN PVOID StartAddress,
    IN PVOID Parameter,
    IN THREAD_PRIORITY Priority,
    OUT PTHREAD_ID ThreadId
)
{
    if (!g_KernelInitialized || !StartAddress || !ThreadId) {
        return STATUS_INVALID_PARAMETER;
    }

    PPROCESS process = KernGetProcessById(ProcessId);
    if (!process) {
        return STATUS_INVALID_PARAMETER;
    }

    AURORA_IRQL oldIrql;
    AuroraAcquireSpinLock(&g_ThreadTableLock, &oldIrql);

    /* Find free thread slot */
    PTHREAD thread = NULL;
    for (UINT32 i = 0; i < MAX_PROCESSES * MAX_THREADS_PER_PROCESS; i++) {
        if (g_ThreadTable[i].ThreadId == 0) {
            thread = &g_ThreadTable[i];
            break;
        }
    }

    if (!thread) {
        AuroraReleaseSpinLock(&g_ThreadTableLock, oldIrql);
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    /* Initialize thread */
    memset(thread, 0, sizeof(THREAD));
    thread->ThreadId = g_NextThreadId++;
    thread->ProcessId = ProcessId;
    thread->State = ThreadStateInitialized;
    thread->Priority = Priority;
    thread->TimeSlice = 10; /* Default time slice */
    thread->CreationTime = AuroraGetSystemTime();
    thread->ParentProcess = process;
    
    /* Allocate kernel stack */
    thread->KernelStack = AuroraAllocatePool(KERNEL_STACK_SIZE);
    if (!thread->KernelStack) {
        AuroraReleaseSpinLock(&g_ThreadTableLock, oldIrql);
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    thread->StackSize = KERNEL_STACK_SIZE;
    
    AuroraInitializeSpinLock(&thread->ThreadLock);

    /* Add thread to process thread list */
    AuroraAcquireSpinLock(&process->ProcessLock, &oldIrql);
    thread->NextThread = process->ThreadList;
    if (process->ThreadList) {
        process->ThreadList->PreviousThread = thread;
    }
    process->ThreadList = thread;
    process->ThreadCount++;
    
    if (!process->MainThread) {
        process->MainThread = thread;
    }
    
    AuroraReleaseSpinLock(&process->ProcessLock, oldIrql);

    *ThreadId = thread->ThreadId;
    
    AuroraReleaseSpinLock(&g_ThreadTableLock, oldIrql);
    
    KernDebugPrint("Created thread ID %u in process ID %u\n", *ThreadId, ProcessId);
    return STATUS_SUCCESS;
}

/*
 * Terminate a thread
 */
NTSTATUS KernTerminateThread(
    IN THREAD_ID ThreadId,
    IN UINT32 ExitCode
)
{
    if (!g_KernelInitialized) {
        return STATUS_NOT_INITIALIZED;
    }

    PTHREAD thread = KernGetThreadById(ThreadId);
    if (!thread) {
        return STATUS_INVALID_PARAMETER;
    }

    AURORA_IRQL oldIrql;
    AuroraAcquireSpinLock(&thread->ThreadLock, &oldIrql);

    /* Update thread state */
    thread->State = ThreadStateTerminated;

    /* Free kernel stack */
    if (thread->KernelStack) {
        AuroraFreePool(thread->KernelStack);
        thread->KernelStack = NULL;
    }

    AuroraReleaseSpinLock(&thread->ThreadLock, oldIrql);
    
    KernDebugPrint("Terminated thread ID %u with exit code %u\n", ThreadId, ExitCode);
    return STATUS_SUCCESS;
}

/*
 * Get thread by ID
 */
PTHREAD KernGetThreadById(IN THREAD_ID ThreadId)
{
    if (!g_KernelInitialized || ThreadId == 0) {
        return NULL;
    }

    for (UINT32 i = 0; i < MAX_PROCESSES * MAX_THREADS_PER_PROCESS; i++) {
        if (g_ThreadTable[i].ThreadId == ThreadId) {
            return &g_ThreadTable[i];
        }
    }

    return NULL;
}

/*
 * Get current thread
 */
PTHREAD KernGetCurrentThread(void)
{
    return g_CurrentThread;
}

/*
 * Memory Management Functions
 */
PVOID KernAllocateMemory(IN SIZE_T Size)
{
    /* Simple memory allocation implementation */
    /* In a real kernel, this would use a proper memory allocator */
    
    /* For now, return NULL to indicate allocation failure */
    /* This should be replaced with actual memory allocation */
    return NULL;
}

VOID KernFreeMemory(IN PVOID Memory)
{
    /* Simple memory deallocation implementation */
    /* In a real kernel, this would free the allocated memory */
    
    if (Memory) {
        /* Placeholder - actual deallocation would happen here */
    }
}

/*
 * Debug print function
 */
VOID KernDebugPrint(IN PCSTR Format, ...)
{
    /* TODO: Implement debug output */
    /* For now, this is a placeholder */
    UNREFERENCED_PARAMETER(Format);
}

/*
 * Kernel panic function
 */
VOID KernPanic(IN PCSTR Message)
{
    /* TODO: Implement kernel panic handling */
    KernDebugPrint("KERNEL PANIC: %s\n", Message);
    
    /* Halt the system */
    while (TRUE) {
        /* Infinite loop */
    }
}
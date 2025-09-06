/*
 * Aurora Kernel - Core Kernel Implementation
 * Copyright (c) 2024 Aurora Project
 */

#include "../aurora.h"
#include "../include/kern.h"
#include "../include/mem.h"
#include "../include/proc.h"
#include "../include/perf.h"
#include "../include/ipc.h"
#include "../include/l4.h"
#include "../include/fiasco.h"
#include "../include/acpi.h"
#include "../include/io.h"

/* Global kernel state */
static BOOL g_KernelInitialized = FALSE;
static PROCESS g_ProcessTable[MAX_PROCESSES];
static THREAD g_ThreadTable[MAX_PROCESSES * MAX_THREADS_PER_PROCESS];
SCHEDULER_CONTEXT g_SchedulerContext; /* exported */
static AURORA_SPINLOCK g_ProcessTableLock;
static AURORA_SPINLOCK g_ThreadTableLock;
static PROCESS_ID g_NextProcessId = 1;
static THREAD_ID g_NextThreadId = 1;

/* Current process and thread (per-CPU) */
PPROCESS g_CurrentProcess = NULL; /* exported */
PTHREAD g_CurrentThread = NULL;   /* exported */

/* Setter used by arch layer */
VOID KernSetCurrentThread(PTHREAD Thread)
{
    g_CurrentThread = Thread;
    g_CurrentProcess = Thread ? Thread->ParentProcess : NULL;
}

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

    /* Initialize memory manager */
    NTSTATUS status = MemInitialize();
    if (!NT_SUCCESS(status)) {
        return status;
    }

    /* Initialize ACPI early to discover LAPIC/IOAPIC/Timers */
    AcpiInitialize(); /* ignore failure for now, system can still run with legacy PIC later */

    /* Initialize process subsystem */
    status = ProcInitialize();
    if (!NT_SUCCESS(status)) {
        return status;
    }

    /* Initialize I/O manager */
    status = IoInitialize();
    if (!NT_SUCCESS(status)) {
        return status;
    }
    /* Initialize block storage subsystem (ATA/NVMe stubs) */
    BlockSubsystemInitialize();

    /* Initialize scheduler */
    status = KernInitializeScheduler();
    if (!NT_SUCCESS(status)) {
        return status;
    }

    /* Initialize performance counters */
    PerfInitialize();
    /* Initialize IPC */
    IpcInitialize();
    /* Initialize L4 adaptation */
    L4Initialize();
    /* Initialize Fiasco subsystem (policies/fastpaths) */
    FiascoInitialize();

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

    /* Setup address space for the process */
    NTSTATUS asStatus = ProcSetupAddressSpace(process);
    if (!NT_SUCCESS(asStatus)) {
        AuroraReleaseSpinLock(&g_ProcessTableLock, oldIrql);
        return asStatus;
    }

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
    thread->KernelStack = KernAllocateMemory(KERNEL_STACK_SIZE);
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
    /* L4: create TCB extension and self capability */
    do {
        PL4_TCB_EXTENSION ext = L4GetOrCreateTcbExtension(thread);
        if(ext && ext->CapTable){
            L4_CAP selfCap; /* ignore status for now */
            L4CapInsert(ext->CapTable,&selfCap,L4_THREAD_CAP_TYPE, L4_IPC_RIGHT_SEND|L4_IPC_RIGHT_RECV, thread);
        }
    } while(0);
    
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
        KernFreeMemory(thread->KernelStack);
        thread->KernelStack = NULL;
    }
    /* Free L4 extension if present */
    if(thread->Extension){
        PL4_TCB_EXTENSION ext = (PL4_TCB_EXTENSION)thread->Extension;
        if(ext->CapTable){
            KernFreeMemory(ext->CapTable);
        }
        KernFreeMemory(ext);
        thread->Extension = NULL;
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
 * Switch context between threads
 */
VOID KernSwitchContext(IN PTHREAD OldThread, IN PTHREAD NewThread)
{
#if defined(_M_X64) || defined(__x86_64__)
    extern VOID Amd64SwitchContext(IN PTHREAD OldThread, IN PTHREAD NewThread);
    Amd64SwitchContext(OldThread, NewThread);
#elif defined(_M_ARM64) || defined(__aarch64__)
    extern VOID Aarch64SwitchContext(IN PTHREAD OldThread, IN PTHREAD NewThread);
    Aarch64SwitchContext(OldThread, NewThread);
#elif defined(_M_IX86) || defined(__i386__)
    extern VOID X86SwitchContext(IN PTHREAD OldThread, IN PTHREAD NewThread);
    X86SwitchContext(OldThread, NewThread);
#else
#error "Unsupported architecture"
#endif
}

/*
 * Memory Management Functions
 */
PVOID KernAllocateMemory(IN SIZE_T Size)
{
    return MemAlloc((size_t)Size);
}

VOID KernFreeMemory(IN PVOID Memory)
{
    if (Memory) MemFree(Memory);
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
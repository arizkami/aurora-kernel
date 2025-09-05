/*
 * Aurora Kernel - Scheduler Implementation
 * Copyright (c) 2024 Aurora Project
 */

#include "../aurora.h"
#include "../include/kern.h"

/* Forward declarations */
static VOID KernIdleThreadProc(IN PVOID Parameter);
static NTSTATUS KernCreateIdleThread(void);

/* External references */
extern SCHEDULER_CONTEXT g_SchedulerContext;
extern PROCESS g_ProcessTable[MAX_PROCESSES];
extern THREAD g_ThreadTable[];
extern PTHREAD g_CurrentThread;
extern PPROCESS g_CurrentProcess;

/* Scheduler statistics */
static UINT64 g_TotalContextSwitches = 0;
static UINT64 g_SchedulerTicks = 0;
static BOOL g_SchedulerEnabled = FALSE;

/* Architecture-specific context switching functions */
/* These will be implemented in architecture-specific files */
extern VOID ArchSaveContext(IN PTHREAD Thread);
extern VOID ArchRestoreContext(IN PTHREAD Thread);
extern VOID ArchInitializeThreadContext(
    IN PTHREAD Thread,
    IN PVOID StartAddress,
    IN PVOID Parameter
);

/*
 * Initialize the scheduler
 */
NTSTATUS KernInitializeScheduler(void)
{
    /* Initialize scheduler context */
    memset(&g_SchedulerContext, 0, sizeof(SCHEDULER_CONTEXT));
    AuroraInitializeSpinLock(&g_SchedulerContext.SchedulerLock);
    
    /* Initialize ready queues */
    for (INT32 i = 0; i < 5; i++) {
        g_SchedulerContext.ReadyQueues[i] = NULL;
    }
    
    g_SchedulerContext.CurrentThread = NULL;
    g_SchedulerContext.IdleThread = NULL;
    g_SchedulerContext.ContextSwitches = 0;
    g_SchedulerContext.SchedulerTicks = 0;
    
    /* Create idle thread */
    NTSTATUS status = KernCreateIdleThread();
    if (!NT_SUCCESS(status)) {
        return status;
    }
    
    g_SchedulerEnabled = TRUE;
    return STATUS_SUCCESS;
}

/*
 * Create the idle thread
 */
NTSTATUS KernCreateIdleThread(void)
{
    /* Allocate idle thread structure */
    PTHREAD idleThread = (PTHREAD)AuroraAllocatePool(sizeof(THREAD));
    if (!idleThread) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    
    /* Initialize idle thread */
    memset(idleThread, 0, sizeof(THREAD));
    idleThread->ThreadId = 0; /* Special ID for idle thread */
    idleThread->ProcessId = 0; /* System process */
    strncpy(idleThread->ThreadName, "Idle", THREAD_NAME_MAX - 1);
    idleThread->State = ThreadStateReady;
    idleThread->Priority = PriorityIdle;
    idleThread->TimeSlice = 1;
    idleThread->CreationTime = AuroraGetSystemTime();
    
    /* Allocate kernel stack for idle thread */
    idleThread->KernelStack = AuroraAllocatePool(KERNEL_STACK_SIZE);
    if (!idleThread->KernelStack) {
        AuroraFreePool(idleThread);
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    idleThread->StackSize = KERNEL_STACK_SIZE;
    
    AuroraInitializeSpinLock(&idleThread->ThreadLock);
    
    /* Initialize context for idle thread */
    ArchInitializeThreadContext(idleThread, (PVOID)KernIdleThreadProc, NULL);
    
    g_SchedulerContext.IdleThread = idleThread;
    return STATUS_SUCCESS;
}

/*
 * Idle thread procedure
 */
VOID KernIdleThreadProc(PVOID Parameter)
{
    UNREFERENCED_PARAMETER(Parameter);
    
    while (TRUE) {
        /* Halt processor until next interrupt */
        /* Architecture-specific halt - placeholder for now */
        /* ArchHaltProcessor(); */
    }
}

/*
 * Add thread to ready queue
 */
VOID KernAddThreadToReadyQueue(IN PTHREAD Thread)
{
    if (!Thread || !g_SchedulerEnabled) {
        return;
    }
    
    AURORA_IRQL oldIrql;
    AuroraAcquireSpinLock(&g_SchedulerContext.SchedulerLock, &oldIrql);
    
    /* Set thread state to ready */
    Thread->State = ThreadStateReady;
    
    /* Add to appropriate priority queue */
    INT32 priority = (INT32)Thread->Priority;
    if (priority < 0 || priority >= 5) {
        priority = PriorityNormal;
    }
    
    /* Insert at end of queue (FIFO within priority) */
    Thread->NextThread = NULL;
    Thread->PreviousThread = NULL;
    
    if (!g_SchedulerContext.ReadyQueues[priority]) {
        /* First thread in this priority queue */
        g_SchedulerContext.ReadyQueues[priority] = Thread;
    } else {
        /* Find end of queue and append */
        PTHREAD current = g_SchedulerContext.ReadyQueues[priority];
        while (current->NextThread) {
            current = current->NextThread;
        }
        current->NextThread = Thread;
        Thread->PreviousThread = current;
    }
    
    AuroraReleaseSpinLock(&g_SchedulerContext.SchedulerLock, oldIrql);
}

/*
 * Remove thread from ready queue
 */
VOID KernRemoveThreadFromReadyQueue(IN PTHREAD Thread)
{
    if (!Thread || !g_SchedulerEnabled) {
        return;
    }
    
    AURORA_IRQL oldIrql;
    AuroraAcquireSpinLock(&g_SchedulerContext.SchedulerLock, &oldIrql);
    
    INT32 priority = (INT32)Thread->Priority;
    if (priority < 0 || priority >= 5) {
        AuroraReleaseSpinLock(&g_SchedulerContext.SchedulerLock, oldIrql);
        return;
    }
    
    /* Remove from linked list */
    if (Thread->PreviousThread) {
        Thread->PreviousThread->NextThread = Thread->NextThread;
    } else {
        /* This was the first thread in the queue */
        g_SchedulerContext.ReadyQueues[priority] = Thread->NextThread;
    }
    
    if (Thread->NextThread) {
        Thread->NextThread->PreviousThread = Thread->PreviousThread;
    }
    
    Thread->NextThread = NULL;
    Thread->PreviousThread = NULL;
    
    AuroraReleaseSpinLock(&g_SchedulerContext.SchedulerLock, oldIrql);
}

/*
 * Select next thread to run
 */
PTHREAD KernSelectNextThread(void)
{
    if (!g_SchedulerEnabled) {
        return g_SchedulerContext.IdleThread;
    }
    
    /* Search ready queues from highest to lowest priority */
    for (INT32 priority = PriorityRealtime; priority >= PriorityIdle; priority--) {
        if (g_SchedulerContext.ReadyQueues[priority]) {
            PTHREAD thread = g_SchedulerContext.ReadyQueues[priority];
            KernRemoveThreadFromReadyQueue(thread);
            return thread;
        }
    }
    
    /* No ready threads, return idle thread */
    return g_SchedulerContext.IdleThread;
}

/*
 * Main scheduler function
 */
VOID KernSchedule(void)
{
    if (!g_SchedulerEnabled) {
        return;
    }
    
    AURORA_IRQL oldIrql;
    AuroraAcquireSpinLock(&g_SchedulerContext.SchedulerLock, &oldIrql);
    
    PTHREAD currentThread = g_SchedulerContext.CurrentThread;
    PTHREAD nextThread = KernSelectNextThread();
    
    /* If no thread change needed */
    if (currentThread == nextThread) {
        AuroraReleaseSpinLock(&g_SchedulerContext.SchedulerLock, oldIrql);
        return;
    }
    
    /* Save current thread context if it exists and is still runnable */
    if (currentThread && currentThread->State == ThreadStateRunning) {
        ArchSaveContext(currentThread);
        
        /* Put current thread back in ready queue if it's not terminated */
        if (currentThread->State != ThreadStateTerminated) {
            currentThread->State = ThreadStateReady;
            KernAddThreadToReadyQueue(currentThread);
        }
    }
    
    /* Switch to next thread */
    g_SchedulerContext.CurrentThread = nextThread;
    g_CurrentThread = nextThread;
    
    if (nextThread) {
        nextThread->State = ThreadStateRunning;
        g_CurrentProcess = nextThread->ParentProcess;
        
        /* Update statistics */
        g_SchedulerContext.ContextSwitches++;
        g_TotalContextSwitches++;
        
        /* Restore new thread context */
        ArchRestoreContext(nextThread);
    }
    
    AuroraReleaseSpinLock(&g_SchedulerContext.SchedulerLock, oldIrql);
}

/*
 * Yield processor to next thread
 */
VOID KernYieldProcessor(void)
{
    if (!g_SchedulerEnabled) {
        return;
    }
    
    PTHREAD currentThread = g_SchedulerContext.CurrentThread;
    if (currentThread && currentThread->State == ThreadStateRunning) {
        /* Put current thread at end of its priority queue */
        currentThread->State = ThreadStateReady;
        KernAddThreadToReadyQueue(currentThread);
    }
    
    /* Schedule next thread */
    KernSchedule();
}

/*
 * Sleep for specified milliseconds
 */
NTSTATUS KernSleep(IN UINT32 Milliseconds)
{
    if (!g_SchedulerEnabled) {
        return STATUS_NOT_INITIALIZED;
    }
    
    PTHREAD currentThread = g_SchedulerContext.CurrentThread;
    if (!currentThread) {
        return STATUS_INVALID_PARAMETER;
    }
    
    /* Set thread to waiting state */
    currentThread->State = ThreadStateWaiting;
    currentThread->WaitReason = Milliseconds; /* Store sleep time */
    
    /* TODO: Implement timer-based wakeup */
    /* For now, just yield to next thread */
    KernSchedule();
    
    return STATUS_SUCCESS;
}

/*
 * Timer tick handler (called from timer interrupt)
 */
VOID KernSchedulerTimerTick(void)
{
    if (!g_SchedulerEnabled) {
        return;
    }
    
    g_SchedulerContext.SchedulerTicks++;
    g_SchedulerTicks++;
    
    PTHREAD currentThread = g_SchedulerContext.CurrentThread;
    if (currentThread && currentThread->State == ThreadStateRunning) {
        /* Decrement time slice */
        if (currentThread->TimeSlice > 0) {
            currentThread->TimeSlice--;
        }
        
        /* If time slice expired, schedule next thread */
        if (currentThread->TimeSlice == 0) {
            currentThread->TimeSlice = 10; /* Reset time slice */
            KernSchedule();
        }
    }
}

/*
 * Get scheduler statistics
 */
VOID KernGetSchedulerStatistics(
    OUT PUINT64 ContextSwitches,
    OUT PUINT64 SchedulerTicks
)
{
    if (ContextSwitches) {
        *ContextSwitches = g_TotalContextSwitches;
    }
    
    if (SchedulerTicks) {
        *SchedulerTicks = g_SchedulerTicks;
    }
}
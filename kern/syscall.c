/*
 * Aurora Kernel - System Call Interface
 * Copyright (c) 2024 Aurora Project
 */

#include "../aurora.h"
#include "../include/kern.h"

/* System call statistics */
static UINT64 g_SystemCallCount = 0;
static UINT64 g_SystemCallErrors = 0;
static BOOL g_SystemCallsEnabled = FALSE;

/* System call function prototypes */
static UINT_PTR SysExit(UINT_PTR ExitCode);
static UINT_PTR SysCreateProcess(UINT_PTR ProcessName, UINT_PTR ImageBase, UINT_PTR ImageSize);
static UINT_PTR SysCreateThread(UINT_PTR ProcessId, UINT_PTR StartAddress, UINT_PTR Parameter);
static UINT_PTR SysTerminateThread(UINT_PTR ThreadId, UINT_PTR ExitCode);
static UINT_PTR SysSleep(UINT_PTR Milliseconds);
static UINT_PTR SysYield(void);
static UINT_PTR SysGetProcessId(void);
static UINT_PTR SysGetThreadId(void);
static UINT_PTR SysWaitForObject(UINT_PTR ObjectHandle, UINT_PTR TimeoutMs);
static UINT_PTR SysSignalObject(UINT_PTR ObjectHandle);

/* System call dispatch table */
typedef UINT_PTR (*PSYSTEM_CALL_HANDLER)(UINT_PTR, UINT_PTR, UINT_PTR, UINT_PTR);

static PSYSTEM_CALL_HANDLER g_SystemCallTable[] = {
    NULL,                                           /* 0x00 - Reserved */
    (PSYSTEM_CALL_HANDLER)SysExit,                 /* 0x01 - Exit */
    (PSYSTEM_CALL_HANDLER)SysCreateProcess,        /* 0x02 - Create Process */
    (PSYSTEM_CALL_HANDLER)SysCreateThread,         /* 0x03 - Create Thread */
    (PSYSTEM_CALL_HANDLER)SysTerminateThread,      /* 0x04 - Terminate Thread */
    (PSYSTEM_CALL_HANDLER)SysSleep,                /* 0x05 - Sleep */
    (PSYSTEM_CALL_HANDLER)SysYield,                /* 0x06 - Yield */
    (PSYSTEM_CALL_HANDLER)SysGetProcessId,         /* 0x07 - Get Process ID */
    (PSYSTEM_CALL_HANDLER)SysGetThreadId,          /* 0x08 - Get Thread ID */
    (PSYSTEM_CALL_HANDLER)SysWaitForObject,        /* 0x09 - Wait For Object */
    (PSYSTEM_CALL_HANDLER)SysSignalObject,         /* 0x0A - Signal Object */
};

#define SYSTEM_CALL_COUNT (sizeof(g_SystemCallTable) / sizeof(g_SystemCallTable[0]))

/*
 * Initialize system call interface
 */
NTSTATUS KernInitializeSystemCalls(void)
{
    g_SystemCallCount = 0;
    g_SystemCallErrors = 0;
    g_SystemCallsEnabled = TRUE;
    
    KernDebugPrint("System call interface initialized with %u system calls\n", 
                   (UINT32)SYSTEM_CALL_COUNT);
    
    return STATUS_SUCCESS;
}

/*
 * Main system call handler
 */
UINT_PTR KernSystemCallHandler(
    IN UINT32 SystemCallNumber,
    IN UINT_PTR Parameter1,
    IN UINT_PTR Parameter2,
    IN UINT_PTR Parameter3,
    IN UINT_PTR Parameter4
)
{
    if (!g_SystemCallsEnabled) {
        g_SystemCallErrors++;
        return (UINT_PTR)STATUS_NOT_INITIALIZED;
    }
    
    /* Validate system call number */
    if (SystemCallNumber >= SYSTEM_CALL_COUNT || !g_SystemCallTable[SystemCallNumber]) {
        g_SystemCallErrors++;
        KernDebugPrint("Invalid system call number: 0x%X\n", SystemCallNumber);
        return (UINT_PTR)STATUS_INVALID_PARAMETER;
    }
    
    /* Update statistics */
    g_SystemCallCount++;
    
    /* Dispatch to appropriate handler */
    PSYSTEM_CALL_HANDLER handler = g_SystemCallTable[SystemCallNumber];
    UINT_PTR result = handler(Parameter1, Parameter2, Parameter3, Parameter4);
    
    return result;
}

/*
 * System Call Implementations
 */

/*
 * SysExit - Terminate current thread
 */
static UINT_PTR SysExit(UINT_PTR ExitCode)
{
    PTHREAD currentThread = KernGetCurrentThread();
    if (!currentThread) {
        return (UINT_PTR)STATUS_INVALID_PARAMETER;
    }
    
    NTSTATUS status = KernTerminateThread(currentThread->ThreadId, (UINT32)ExitCode);
    
    /* This should not return */
    KernSchedule();
    
    return (UINT_PTR)status;
}

/*
 * SysCreateProcess - Create a new process
 */
static UINT_PTR SysCreateProcess(UINT_PTR ProcessName, UINT_PTR ImageBase, UINT_PTR ImageSize)
{
    if (!ProcessName) {
        return (UINT_PTR)STATUS_INVALID_PARAMETER;
    }
    
    PROCESS_ID processId;
    NTSTATUS status = KernCreateProcess(
        (PCSTR)ProcessName,
        (PVOID)ImageBase,
        ImageSize,
        &processId
    );
    
    if (NT_SUCCESS(status)) {
        return (UINT_PTR)processId;
    }
    
    return (UINT_PTR)status;
}

/*
 * SysCreateThread - Create a new thread
 */
static UINT_PTR SysCreateThread(UINT_PTR ProcessId, UINT_PTR StartAddress, UINT_PTR Parameter)
{
    if (!StartAddress) {
        return (UINT_PTR)STATUS_INVALID_PARAMETER;
    }
    
    THREAD_ID threadId;
    NTSTATUS status = KernCreateThread(
        (PROCESS_ID)ProcessId,
        (PVOID)StartAddress,
        (PVOID)Parameter,
        PriorityNormal,
        &threadId
    );
    
    if (NT_SUCCESS(status)) {
        return (UINT_PTR)threadId;
    }
    
    return (UINT_PTR)status;
}

/*
 * SysTerminateThread - Terminate a thread
 */
static UINT_PTR SysTerminateThread(UINT_PTR ThreadId, UINT_PTR ExitCode)
{
    NTSTATUS status = KernTerminateThread((THREAD_ID)ThreadId, (UINT32)ExitCode);
    return (UINT_PTR)status;
}

/*
 * SysSleep - Sleep for specified milliseconds
 */
static UINT_PTR SysSleep(UINT_PTR Milliseconds)
{
    NTSTATUS status = KernSleep((UINT32)Milliseconds);
    return (UINT_PTR)status;
}

/*
 * SysYield - Yield processor to next thread
 */
static UINT_PTR SysYield(void)
{
    KernYieldProcessor();
    return (UINT_PTR)STATUS_SUCCESS;
}

/*
 * SysGetProcessId - Get current process ID
 */
static UINT_PTR SysGetProcessId(void)
{
    PPROCESS currentProcess = KernGetCurrentProcess();
    if (currentProcess) {
        return (UINT_PTR)currentProcess->ProcessId;
    }
    
    return 0;
}

/*
 * SysGetThreadId - Get current thread ID
 */
static UINT_PTR SysGetThreadId(void)
{
    PTHREAD currentThread = KernGetCurrentThread();
    if (currentThread) {
        return (UINT_PTR)currentThread->ThreadId;
    }
    
    return 0;
}

/*
 * SysWaitForObject - Wait for synchronization object
 */
static UINT_PTR SysWaitForObject(UINT_PTR ObjectHandle, UINT_PTR TimeoutMs)
{
    /* TODO: Implement synchronization object waiting */
    UNREFERENCED_PARAMETER(ObjectHandle);
    UNREFERENCED_PARAMETER(TimeoutMs);
    
    return (UINT_PTR)STATUS_NOT_IMPLEMENTED;
}

/*
 * SysSignalObject - Signal synchronization object
 */
static UINT_PTR SysSignalObject(UINT_PTR ObjectHandle)
{
    /* TODO: Implement synchronization object signaling */
    UNREFERENCED_PARAMETER(ObjectHandle);
    
    return (UINT_PTR)STATUS_NOT_IMPLEMENTED;
}

/*
 * Get system call statistics
 */
VOID KernGetSystemCallStatistics(
    OUT PUINT64 SystemCallCount,
    OUT PUINT64 SystemCallErrors
)
{
    if (SystemCallCount) {
        *SystemCallCount = g_SystemCallCount;
    }
    
    if (SystemCallErrors) {
        *SystemCallErrors = g_SystemCallErrors;
    }
}

/*
 * Validate user mode pointer
 */
BOOL KernValidateUserPointer(IN PVOID Pointer, IN UINT_PTR Size)
{
    /* TODO: Implement proper user mode pointer validation */
    /* For now, just check for NULL */
    if (!Pointer || Size == 0) {
        return FALSE;
    }
    
    /* TODO: Check if pointer is in user mode address space */
    /* TODO: Check if memory is accessible */
    
    return TRUE;
}

/*
 * Copy data from user mode to kernel mode
 */
NTSTATUS KernCopyFromUser(
    OUT PVOID KernelBuffer,
    IN PVOID UserBuffer,
    IN UINT_PTR Size
)
{
    if (!KernelBuffer || !UserBuffer || Size == 0) {
        return STATUS_INVALID_PARAMETER;
    }
    
    /* Validate user pointer */
    if (!KernValidateUserPointer(UserBuffer, Size)) {
        return STATUS_ACCESS_VIOLATION;
    }
    
    /* TODO: Implement safe copy with exception handling */
    AuroraMemoryCopy(KernelBuffer, UserBuffer, Size);
    
    return STATUS_SUCCESS;
}

/*
 * Copy data from kernel mode to user mode
 */
NTSTATUS KernCopyToUser(
    OUT PVOID UserBuffer,
    IN PVOID KernelBuffer,
    IN UINT_PTR Size
)
{
    if (!UserBuffer || !KernelBuffer || Size == 0) {
        return STATUS_INVALID_PARAMETER;
    }
    
    /* Validate user pointer */
    if (!KernValidateUserPointer(UserBuffer, Size)) {
        return STATUS_ACCESS_VIOLATION;
    }
    
    /* TODO: Implement safe copy with exception handling */
    AuroraMemoryCopy(UserBuffer, KernelBuffer, Size);
    
    return STATUS_SUCCESS;
}
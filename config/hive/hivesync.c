/*
 * Aurora Kernel - Registry Hive Synchronization
 * Copyright (c) 2024 NTCore Project
 */

#include "hive.h"

/* Synchronization constants */
#define HIVE_MAX_READERS 64
#define HIVE_LOCK_TIMEOUT 5000  /* 5 seconds in milliseconds */
#define HIVE_SYNC_MAGIC 0x53594E43  /* 'SYNC' */

/* Lock upgrade type (additional to header definitions) */
#define HiveLockUpgrade 3     /* Upgrade from shared to exclusive */

/* Lock request structure */
typedef struct _HIVE_LOCK_REQUEST {
    HIVE_LOCK_TYPE Type;
    UINT32 ThreadId;
    UINT64 RequestTime;
    UINT32 Timeout;
    struct _HIVE_LOCK_REQUEST* Next;
} HIVE_LOCK_REQUEST, *PHIVE_LOCK_REQUEST;

/* Reader-Writer lock structure */
typedef struct _HIVE_RWLOCK {
    UINT32 Magic;
    volatile UINT32 ReaderCount;
    volatile UINT32 WriterCount;
    volatile UINT32 WaitingReaders;
    volatile UINT32 WaitingWriters;
    AURORA_SPINLOCK SpinLock;
    PHIVE_LOCK_REQUEST WaitQueue;
    UINT32 OwnerThread;
    UINT32 RecursionCount;
} HIVE_RWLOCK, *PHIVE_RWLOCK;

/* Transaction structure */
typedef struct _HIVE_TRANSACTION {
    UINT32 TransactionId;
    UINT32 ThreadId;
    PHIVE Hive;
    UINT64 StartTime;
    UINT32 LockCount;
    BOOLEAN ReadOnly;
    struct _HIVE_TRANSACTION* Next;
} HIVE_TRANSACTION, *PHIVE_TRANSACTION;

/* Global transaction list */
static PHIVE_TRANSACTION g_ActiveTransactions = NULL;
static AURORA_SPINLOCK g_TransactionLock = 0;
static UINT32 g_NextTransactionId = 1;

/*
 * Initialize hive synchronization system
 */
NTSTATUS HiveSyncInitialize(void)
{
    g_ActiveTransactions = NULL;
    g_TransactionLock = 0;
    g_NextTransactionId = 1;
    
    return STATUS_SUCCESS;
}

/*
 * Shutdown hive synchronization system
 */
VOID HiveSyncShutdown(void)
{
    /* Abort all active transactions */
    while (g_ActiveTransactions) {
        PHIVE_TRANSACTION Transaction = g_ActiveTransactions;
        g_ActiveTransactions = Transaction->Next;
        HiveAbortTransaction(Transaction->TransactionId);
    }
}

/*
 * Initialize reader-writer lock
 */
NTSTATUS HiveInitializeRWLock(OUT PHIVE_RWLOCK RWLock)
{
    if (!RWLock) {
        return STATUS_INVALID_PARAMETER;
    }

    memset(RWLock, 0, sizeof(HIVE_RWLOCK));
    RWLock->Magic = HIVE_SYNC_MAGIC;
    RWLock->ReaderCount = 0;
    RWLock->WriterCount = 0;
    RWLock->WaitingReaders = 0;
    RWLock->WaitingWriters = 0;
    RWLock->SpinLock = 0;
    RWLock->WaitQueue = NULL;
    RWLock->OwnerThread = 0;
    RWLock->RecursionCount = 0;
    
    return STATUS_SUCCESS;
}

/*
 * Destroy reader-writer lock
 */
VOID HiveDestroyRWLock(IN PHIVE_RWLOCK RWLock)
{
    if (!RWLock || RWLock->Magic != HIVE_SYNC_MAGIC) {
        return;
    }

    /* Clear wait queue */
    while (RWLock->WaitQueue) {
        PHIVE_LOCK_REQUEST Request = RWLock->WaitQueue;
        RWLock->WaitQueue = Request->Next;
        /* Free request - would use kernel allocator */
    }
    
    RWLock->Magic = 0;
}

/*
 * Acquire shared (read) lock
 */
NTSTATUS HiveAcquireSharedLock(IN PHIVE Hive, IN UINT32 Timeout)
{
    if (!Hive) {
        return STATUS_INVALID_PARAMETER;
    }

    PHIVE_RWLOCK RWLock = (PHIVE_RWLOCK)&Hive->Lock;
    UINT32 CurrentThread = 0; /* TODO: Get current thread ID */
    
    /* Check for recursive lock by same thread */
    if (RWLock->OwnerThread == CurrentThread && RWLock->WriterCount > 0) {
        RWLock->RecursionCount++;
        return STATUS_SUCCESS;
    }
    
    /* Try to acquire lock immediately */
    if (RWLock->WriterCount == 0 && RWLock->WaitingWriters == 0) {
        RWLock->ReaderCount++;
        return STATUS_SUCCESS;
    }
    
    /* Need to wait - add to wait queue */
    PHIVE_LOCK_REQUEST Request = NULL; /* KernAllocateMemory(sizeof(HIVE_LOCK_REQUEST)); */
    if (!Request) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    
    Request->Type = HiveLockShared;
    Request->ThreadId = CurrentThread;
    Request->RequestTime = 0; /* TODO: Get current time */
    Request->Timeout = Timeout;
    Request->Next = RWLock->WaitQueue;
    
    RWLock->WaitQueue = Request;
    RWLock->WaitingReaders++;
    
    /* Wait for lock to become available */
    /* In real implementation, would use kernel synchronization primitives */
    /* For now, simulate immediate success */
    RWLock->WaitingReaders--;
    RWLock->ReaderCount++;
    
    /* Remove from wait queue */
    if (RWLock->WaitQueue == Request) {
        RWLock->WaitQueue = Request->Next;
    }
    
    /* Free request */
    /* KernFreeMemory(Request); */
    
    return STATUS_SUCCESS;
}

/*
 * Acquire exclusive (write) lock
 */
NTSTATUS HiveAcquireExclusiveLock(IN PHIVE Hive, IN UINT32 Timeout)
{
    if (!Hive) {
        return STATUS_INVALID_PARAMETER;
    }

    PHIVE_RWLOCK RWLock = (PHIVE_RWLOCK)&Hive->Lock;
    UINT32 CurrentThread = 0; /* TODO: Get current thread ID */
    
    /* Check for recursive lock by same thread */
    if (RWLock->OwnerThread == CurrentThread && RWLock->WriterCount > 0) {
        RWLock->RecursionCount++;
        return STATUS_SUCCESS;
    }
    
    /* Try to acquire lock immediately */
    if (RWLock->ReaderCount == 0 && RWLock->WriterCount == 0) {
        RWLock->WriterCount = 1;
        RWLock->OwnerThread = CurrentThread;
        RWLock->RecursionCount = 1;
        return STATUS_SUCCESS;
    }
    
    /* Need to wait - add to wait queue */
    PHIVE_LOCK_REQUEST Request = NULL; /* KernAllocateMemory(sizeof(HIVE_LOCK_REQUEST)); */
    if (!Request) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    
    Request->Type = HiveLockExclusive;
    Request->ThreadId = CurrentThread;
    Request->RequestTime = 0; /* TODO: Get current time */
    Request->Timeout = Timeout;
    Request->Next = RWLock->WaitQueue;
    
    RWLock->WaitQueue = Request;
    RWLock->WaitingWriters++;
    
    /* Wait for lock to become available */
    /* In real implementation, would use kernel synchronization primitives */
    /* For now, simulate immediate success */
    RWLock->WaitingWriters--;
    RWLock->WriterCount = 1;
    RWLock->OwnerThread = CurrentThread;
    RWLock->RecursionCount = 1;
    
    /* Remove from wait queue */
    if (RWLock->WaitQueue == Request) {
        RWLock->WaitQueue = Request->Next;
    }
    
    /* Free request */
    /* KernFreeMemory(Request); */
    
    return STATUS_SUCCESS;
}

/*
 * Release shared (read) lock
 */
NTSTATUS HiveReleaseSharedLock(IN PHIVE Hive)
{
    if (!Hive) {
        return STATUS_INVALID_PARAMETER;
    }

    PHIVE_RWLOCK RWLock = (PHIVE_RWLOCK)&Hive->Lock;
    
    if (RWLock->ReaderCount == 0) {
        return STATUS_INVALID_LOCK_STATE;
    }
    
    RWLock->ReaderCount--;
    
    /* Wake up waiting writers if no more readers */
    if (RWLock->ReaderCount == 0 && RWLock->WaitingWriters > 0) {
        HiveWakeupWaiters(RWLock, HiveLockExclusive);
    }
    
    return STATUS_SUCCESS;
}

/*
 * Release exclusive (write) lock
 */
NTSTATUS HiveReleaseExclusiveLock(IN PHIVE Hive)
{
    if (!Hive) {
        return STATUS_INVALID_PARAMETER;
    }

    PHIVE_RWLOCK RWLock = (PHIVE_RWLOCK)&Hive->Lock;
    UINT32 CurrentThread = 0; /* TODO: Get current thread ID */
    
    if (RWLock->WriterCount == 0 || RWLock->OwnerThread != CurrentThread) {
        return STATUS_INVALID_LOCK_STATE;
    }
    
    /* Handle recursive locks */
    if (--RWLock->RecursionCount > 0) {
        return STATUS_SUCCESS;
    }
    
    RWLock->WriterCount = 0;
    RWLock->OwnerThread = 0;
    
    /* Wake up waiting readers or writers */
    if (RWLock->WaitingReaders > 0) {
        HiveWakeupWaiters(RWLock, HiveLockShared);
    } else if (RWLock->WaitingWriters > 0) {
        HiveWakeupWaiters(RWLock, HiveLockExclusive);
    }
    
    return STATUS_SUCCESS;
}

/*
 * Wake up waiting threads
 */
VOID HiveWakeupWaiters(IN PHIVE_RWLOCK RWLock, IN HIVE_LOCK_TYPE LockType)
{
    if (!RWLock) {
        return;
    }

    /* In real implementation, would signal waiting threads */
    /* For now, just update counters */
    
    if (LockType == HiveLockShared && RWLock->WaitingReaders > 0) {
        /* Wake up all waiting readers */
        RWLock->ReaderCount += RWLock->WaitingReaders;
        RWLock->WaitingReaders = 0;
    } else if (LockType == HiveLockExclusive && RWLock->WaitingWriters > 0) {
        /* Wake up one waiting writer */
        RWLock->WriterCount = 1;
        RWLock->WaitingWriters--;
    }
}

/*
 * Begin transaction
 */
NTSTATUS HiveBeginTransaction(IN PHIVE Hive, IN BOOLEAN ReadOnly, OUT PUINT32 TransactionId)
{
    if (!Hive || !TransactionId) {
        return STATUS_INVALID_PARAMETER;
    }

    /* Allocate transaction structure */
    PHIVE_TRANSACTION Transaction = NULL; /* KernAllocateMemory(sizeof(HIVE_TRANSACTION)); */
    if (!Transaction) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    
    /* Initialize transaction */
    Transaction->TransactionId = g_NextTransactionId++;
    Transaction->ThreadId = 0; /* TODO: Get current thread ID */
    Transaction->Hive = Hive;
    Transaction->StartTime = 0; /* TODO: Get current time */
    Transaction->LockCount = 0;
    Transaction->ReadOnly = ReadOnly;
    Transaction->Next = g_ActiveTransactions;
    
    /* Add to active transaction list */
    g_ActiveTransactions = Transaction;
    
    /* Acquire appropriate lock */
    NTSTATUS Status;
    if (ReadOnly) {
        Status = HiveAcquireSharedLock(Hive, HIVE_LOCK_TIMEOUT);
    } else {
        Status = HiveAcquireExclusiveLock(Hive, HIVE_LOCK_TIMEOUT);
    }
    
    if (!NT_SUCCESS(Status)) {
        /* Remove from transaction list */
        if (g_ActiveTransactions == Transaction) {
            g_ActiveTransactions = Transaction->Next;
        }
        /* KernFreeMemory(Transaction); */
        return Status;
    }
    
    Transaction->LockCount = 1;
    *TransactionId = Transaction->TransactionId;
    
    return STATUS_SUCCESS;
}

/*
 * Commit transaction
 */
NTSTATUS HiveCommitTransaction(IN UINT32 TransactionId)
{
    PHIVE_TRANSACTION Transaction = HiveFindTransaction(TransactionId);
    if (!Transaction) {
        return STATUS_INVALID_TRANSACTION_ID;
    }

    NTSTATUS Status = STATUS_SUCCESS;
    
    /* Flush changes if not read-only */
    if (!Transaction->ReadOnly && Transaction->Hive->DirtyFlag) {
        Status = HiveFlush(Transaction->Hive);
    }
    
    /* Release locks */
    for (UINT32 i = 0; i < Transaction->LockCount; i++) {
        if (Transaction->ReadOnly) {
            HiveReleaseSharedLock(Transaction->Hive);
        } else {
            HiveReleaseExclusiveLock(Transaction->Hive);
        }
    }
    
    /* Remove from active transaction list */
    HiveRemoveTransaction(Transaction);
    
    return Status;
}

/*
 * Abort transaction
 */
NTSTATUS HiveAbortTransaction(IN UINT32 TransactionId)
{
    PHIVE_TRANSACTION Transaction = HiveFindTransaction(TransactionId);
    if (!Transaction) {
        return STATUS_INVALID_TRANSACTION_ID;
    }

    /* For read-only transactions, just release locks */
    /* For write transactions, would need to rollback changes */
    
    /* Release locks */
    for (UINT32 i = 0; i < Transaction->LockCount; i++) {
        if (Transaction->ReadOnly) {
            HiveReleaseSharedLock(Transaction->Hive);
        } else {
            HiveReleaseExclusiveLock(Transaction->Hive);
        }
    }
    
    /* Remove from active transaction list */
    HiveRemoveTransaction(Transaction);
    
    return STATUS_SUCCESS;
}

/*
 * Find transaction by ID
 */
PHIVE_TRANSACTION HiveFindTransaction(IN UINT32 TransactionId)
{
    PHIVE_TRANSACTION Current = g_ActiveTransactions;
    
    while (Current) {
        if (Current->TransactionId == TransactionId) {
            return Current;
        }
        Current = Current->Next;
    }
    
    return NULL;
}

/*
 * Remove transaction from active list
 */
VOID HiveRemoveTransaction(IN PHIVE_TRANSACTION Transaction)
{
    if (!Transaction) {
        return;
    }

    if (g_ActiveTransactions == Transaction) {
        g_ActiveTransactions = Transaction->Next;
    } else {
        PHIVE_TRANSACTION Current = g_ActiveTransactions;
        while (Current && Current->Next != Transaction) {
            Current = Current->Next;
        }
        
        if (Current) {
            Current->Next = Transaction->Next;
        }
    }
    
    /* Free transaction structure */
    /* KernFreeMemory(Transaction); */
}

/*
 * Check if hive is locked
 */
BOOLEAN HiveIsLocked(IN PHIVE Hive)
{
    if (!Hive) {
        return FALSE;
    }

    PHIVE_RWLOCK RWLock = (PHIVE_RWLOCK)&Hive->Lock;
    return (RWLock->ReaderCount > 0 || RWLock->WriterCount > 0);
}

/*
 * Get lock statistics
 */
NTSTATUS HiveGetLockStatistics(IN PHIVE Hive, OUT PUINT32 ReaderCount, OUT PUINT32 WriterCount, 
                              OUT PUINT32 WaitingReaders, OUT PUINT32 WaitingWriters)
{
    if (!Hive || !ReaderCount || !WriterCount || !WaitingReaders || !WaitingWriters) {
        return STATUS_INVALID_PARAMETER;
    }

    PHIVE_RWLOCK RWLock = (PHIVE_RWLOCK)&Hive->Lock;
    
    *ReaderCount = RWLock->ReaderCount;
    *WriterCount = RWLock->WriterCount;
    *WaitingReaders = RWLock->WaitingReaders;
    *WaitingWriters = RWLock->WaitingWriters;
    
    return STATUS_SUCCESS;
}

/*
 * Synchronize hive with storage
 */
NTSTATUS HiveSynchronize(IN PHIVE Hive)
{
    if (!Hive) {
        return STATUS_INVALID_PARAMETER;
    }

    /* Acquire exclusive lock for synchronization */
    NTSTATUS Status = HiveAcquireExclusiveLock(Hive, HIVE_LOCK_TIMEOUT);
    if (!NT_SUCCESS(Status)) {
        return Status;
    }
    
    /* Flush hive to storage */
    Status = HiveFlush(Hive);
    
    /* Release lock */
    HiveReleaseExclusiveLock(Hive);
    
    return Status;
}

/*
 * Force unlock hive (emergency use only)
 */
NTSTATUS HiveForceUnlock(IN PHIVE Hive)
{
    if (!Hive) {
        return STATUS_INVALID_PARAMETER;
    }

    PHIVE_RWLOCK RWLock = (PHIVE_RWLOCK)&Hive->Lock;
    
    /* Clear all lock counts */
    RWLock->ReaderCount = 0;
    RWLock->WriterCount = 0;
    RWLock->WaitingReaders = 0;
    RWLock->WaitingWriters = 0;
    RWLock->OwnerThread = 0;
    RWLock->RecursionCount = 0;
    
    /* Clear wait queue */
    while (RWLock->WaitQueue) {
        PHIVE_LOCK_REQUEST Request = RWLock->WaitQueue;
        RWLock->WaitQueue = Request->Next;
        /* Free request */
    }
    
    return STATUS_SUCCESS;
}

/*
 * Check for deadlock conditions
 */
BOOLEAN HiveCheckDeadlock(IN PHIVE Hive, IN UINT32 ThreadId)
{
    if (!Hive) {
        return FALSE;
    }

    /* Simple deadlock detection - check if thread already holds a lock */
    PHIVE_RWLOCK RWLock = (PHIVE_RWLOCK)&Hive->Lock;
    
    if (RWLock->OwnerThread == ThreadId && RWLock->WriterCount > 0) {
        /* Thread already holds exclusive lock */
        return FALSE; /* Not a deadlock, just recursive */
    }
    
    /* More sophisticated deadlock detection would be implemented here */
    /* For now, assume no deadlock */
    return FALSE;
}

/*
 * Set lock timeout
 */
NTSTATUS HiveSetLockTimeout(IN PHIVE Hive, IN UINT32 Timeout)
{
    if (!Hive) {
        return STATUS_INVALID_PARAMETER;
    }

    /* Store timeout in hive structure */
    /* For now, just return success */
    UNREFERENCED_PARAMETER(Timeout);
    return STATUS_SUCCESS;
}

/*
 * Get active transaction count
 */
UINT32 HiveGetActiveTransactionCount(void)
{
    UINT32 Count = 0;
    PHIVE_TRANSACTION Current = g_ActiveTransactions;
    
    while (Current) {
        Count++;
        Current = Current->Next;
    }
    
    return Count;
}

/*
 * Enumerate active transactions
 */
NTSTATUS HiveEnumerateTransactions(OUT PUINT32 TransactionIds, IN UINT32 MaxCount, OUT PUINT32 ActualCount)
{
    if (!TransactionIds || MaxCount == 0 || !ActualCount) {
        return STATUS_INVALID_PARAMETER;
    }

    *ActualCount = 0;
    PHIVE_TRANSACTION Current = g_ActiveTransactions;
    
    while (Current && *ActualCount < MaxCount) {
        TransactionIds[*ActualCount] = Current->TransactionId;
        (*ActualCount)++;
        Current = Current->Next;
    }
    
    return STATUS_SUCCESS;
}

/*
 * Wait for all transactions to complete
 */
NTSTATUS HiveWaitForTransactions(IN UINT32 Timeout)
{
    UINT64 StartTime = 0; /* TODO: Get current time */
    
    while (g_ActiveTransactions) {
        /* Check timeout */
        UINT64 CurrentTime = 0; /* TODO: Get current time */
        if (CurrentTime - StartTime > Timeout) {
            return STATUS_TIMEOUT;
        }
        
        /* Sleep briefly */
        /* KernSleep(10); */
    }
    
    return STATUS_SUCCESS;
}

/*
 * Cleanup orphaned locks
 */
NTSTATUS HiveCleanupOrphanedLocks(void)
{
    /* This would identify and clean up locks held by terminated threads */
    /* For now, just return success */
    return STATUS_SUCCESS;
}
/*
 * Aurora Kernel - Hive Lock Wrappers
 */

#include "hive.h"

VOID HiveAcquireLock(IN PHIVE Hive)
{
    if (!Hive) return;
    AURORA_IRQL oldIrql;
    AuroraAcquireSpinLock(&Hive->Lock, &oldIrql);
    /* We ignore oldIrql in this simplified environment */
}

VOID HiveReleaseLock(IN PHIVE Hive)
{
    if (!Hive) return;
    /* Release with dummy IRQL value as we don't track it */
    AuroraReleaseSpinLock(&Hive->Lock, 0);
}

/*
 * Aurora Kernel - File System Core (VFS scaffold)
 */

#include "../aurora.h"
#include "../include/kern.h"
#include "../include/fs.h"

static BOOL g_FsInitialized = FALSE;
static FS_MOUNT g_Mounts[FS_MAX_MOUNTS];
static FS_DRIVER g_Drivers[FS_MAX_DRIVERS];
static UINT32 g_DriverCount = 0;
static AURORA_SPINLOCK g_FsLock;

/* Forward decls for built-in adapters */
void FsAdapterRegisterFat32(void);
void FsAdapterRegisterExfat(void);
void FsAdapterRegisterNtfs(void);

NTSTATUS FsInitialize(void)
{
    if (g_FsInitialized) return STATUS_ALREADY_INITIALIZED;
    AuroraInitializeSpinLock(&g_FsLock);
    memset(g_Mounts, 0, sizeof(g_Mounts));
    memset(g_Drivers, 0, sizeof(g_Drivers));
    g_DriverCount = 0;

    /* Register built-in adapters (stubs) */
    FsRegisterBuiltInDrivers();

    g_FsInitialized = TRUE;
#ifdef DEBUG
    AuroraDebugPrint("[FS] Initialized file system core\n");
#endif
    return STATUS_SUCCESS;
}

NTSTATUS FsShutdown(void)
{
    if (!g_FsInitialized) return STATUS_SUCCESS;
    g_FsInitialized = FALSE;
#ifdef DEBUG
    AuroraDebugPrint("[FS] Shutdown file system core\n");
#endif
    return STATUS_SUCCESS;
}

NTSTATUS FsRegisterDriver(IN PFS_DRIVER Driver)
{
    if (!Driver || !Driver->Name) return STATUS_INVALID_PARAMETER;
    if (g_DriverCount >= FS_MAX_DRIVERS) return STATUS_INSUFFICIENT_RESOURCES;

    AURORA_IRQL irql;
    AuroraAcquireSpinLock(&g_FsLock, &irql);
    /* Simple duplicate check by name */
    for (UINT32 i = 0; i < g_DriverCount; ++i) {
        if (strcmp(g_Drivers[i].Name, Driver->Name) == 0) {
            AuroraReleaseSpinLock(&g_FsLock, irql);
            return STATUS_OBJECT_NAME_COLLISION;
        }
    }
    g_Drivers[g_DriverCount++] = *Driver; /* shallow copy of ops */
    AuroraReleaseSpinLock(&g_FsLock, irql);
    return STATUS_SUCCESS;
}

static PFS_DRIVER FsFindDriverByName(IN PCSTR Name)
{
    for (UINT32 i = 0; i < g_DriverCount; ++i) {
        if (strcmp(g_Drivers[i].Name, Name) == 0) {
            return &g_Drivers[i];
        }
    }
    return NULL;
}

NTSTATUS FsMount(IN PCSTR Device, IN PCSTR FsType, IN PCSTR MountName, IN PCSTR Options)
{
    if (!Device || !FsType || !MountName) return STATUS_INVALID_PARAMETER;
    PFS_DRIVER drv = FsFindDriverByName(FsType);
    if (!drv || !drv->Ops.Mount) return STATUS_NOT_SUPPORTED;

    AURORA_IRQL irql;
    AuroraAcquireSpinLock(&g_FsLock, &irql);

    /* find free mount slot and ensure unique name */
    INT32 freeIdx = -1;
    for (UINT32 i = 0; i < FS_MAX_MOUNTS; ++i) {
        if (g_Mounts[i].Name[0] == '\0' && freeIdx == -1) freeIdx = (INT32)i;
        if (g_Mounts[i].Name[0] != '\0' && strcmp(g_Mounts[i].Name, MountName) == 0) {
            AuroraReleaseSpinLock(&g_FsLock, irql);
            return STATUS_OBJECT_NAME_COLLISION;
        }
    }
    if (freeIdx < 0) {
        AuroraReleaseSpinLock(&g_FsLock, irql);
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    AuroraReleaseSpinLock(&g_FsLock, irql);

    /* perform driver mount outside lock */
    PVOID volCtx = NULL;
    NTSTATUS st = drv->Ops.Mount(Device, Options, &volCtx);
    if (!NT_SUCCESS(st)) return st;

    AuroraAcquireSpinLock(&g_FsLock, &irql);
    strncpy(g_Mounts[freeIdx].Name, MountName, sizeof(g_Mounts[freeIdx].Name) - 1);
    g_Mounts[freeIdx].FsDriver = drv;
    g_Mounts[freeIdx].VolumeData = volCtx;
    AuroraReleaseSpinLock(&g_FsLock, irql);
    return STATUS_SUCCESS;
}

NTSTATUS FsUnmount(IN PCSTR MountName)
{
    if (!MountName) return STATUS_INVALID_PARAMETER;
    AURORA_IRQL irql;
    AuroraAcquireSpinLock(&g_FsLock, &irql);
    for (UINT32 i = 0; i < FS_MAX_MOUNTS; ++i) {
        if (g_Mounts[i].Name[0] != '\0' && strcmp(g_Mounts[i].Name, MountName) == 0) {
            PFS_DRIVER drv = (PFS_DRIVER)g_Mounts[i].FsDriver;
            PVOID ctx = g_Mounts[i].VolumeData;
            g_Mounts[i].Name[0] = '\0';
            g_Mounts[i].FsDriver = NULL;
            g_Mounts[i].VolumeData = NULL;
            AuroraReleaseSpinLock(&g_FsLock, irql);
            if (drv && drv->Ops.Unmount) return drv->Ops.Unmount(ctx);
            return STATUS_SUCCESS;
        }
    }
    AuroraReleaseSpinLock(&g_FsLock, irql);
    return STATUS_NOT_FOUND;
}

void FsRegisterBuiltInDrivers(void)
{
#if AURORA_FS_ENABLE_FAT32
    FsAdapterRegisterFat32();
#endif
#if AURORA_FS_ENABLE_EXFAT
    FsAdapterRegisterExfat();
#endif
#if AURORA_FS_ENABLE_NTFS
    FsAdapterRegisterNtfs();
#endif
}

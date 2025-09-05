/*
 * Aurora Kernel - File System Interface (VFS scaffold)
 */
#ifndef _FS_H_
#define _FS_H_

#include "../aurora.h"
#include "fs_config.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Basic FS limits */
#ifndef FS_MAX_MOUNTS
#define FS_MAX_MOUNTS 16
#endif
#ifndef FS_MAX_DRIVERS
#define FS_MAX_DRIVERS 8
#endif

/* Forward decls */
typedef struct _FS_DRIVER FS_DRIVER, *PFS_DRIVER;
typedef struct _FS_MOUNT FS_MOUNT, *PFS_MOUNT;

/* File handle (opaque) */
typedef PVOID FS_FILE;
typedef PVOID FS_DIR;

typedef struct _FS_MOUNT {
    CHAR Name[32];
    PVOID FsDriver;   /* placeholder */
    PVOID VolumeData; /* placeholder */
} FS_MOUNT, *PFS_MOUNT;

NTSTATUS FsInitialize(void);
NTSTATUS FsShutdown(void);

/* VFS driver ops (minimal) */
typedef struct _FS_DRIVER_OPS {
    NTSTATUS (*Mount)(IN PCSTR Device, IN PCSTR Options, OUT PVOID* VolumeCtx);
    NTSTATUS (*Unmount)(IN PVOID VolumeCtx);
    /* Optional basic file ops (stubs by default) */
    NTSTATUS (*Open)(IN PVOID VolumeCtx, IN PCSTR Path, OUT FS_FILE* File);
    NTSTATUS (*Close)(IN FS_FILE File);
    NTSTATUS (*Read)(IN FS_FILE File, OUT PVOID Buffer, IN UINT32 Size, OUT PUINT32 BytesRead);
    NTSTATUS (*Write)(IN FS_FILE File, IN PVOID Buffer, IN UINT32 Size, OUT PUINT32 BytesWritten);
} FS_DRIVER_OPS, *PFS_DRIVER_OPS;

/* Driver descriptor */
struct _FS_DRIVER {
    PCSTR Name;          /* e.g., "fat32", "exfat", "ntfs" */
    FS_DRIVER_OPS Ops;
};

/* VFS API */
NTSTATUS FsRegisterDriver(IN PFS_DRIVER Driver);
NTSTATUS FsMount(IN PCSTR Device, IN PCSTR FsType, IN PCSTR MountName, IN PCSTR Options OPTIONAL);
NTSTATUS FsUnmount(IN PCSTR MountName);

/* Built-in adapter registration helpers */
void FsRegisterBuiltInDrivers(void);

#ifdef __cplusplus
}
#endif

#endif /* _FS_H_ */

/* NTFS driver enhanced stub */
#include "../../aurora.h"
#include "../../include/kern.h"
#include "../../include/fs.h"

typedef struct _NTFS_VOLUME { UINT32 Dummy; } NTFS_VOLUME, *PNTFS_VOLUME;

static NTSTATUS ntfs_mount(IN PCSTR Device, IN PCSTR Options, OUT PVOID* VolumeCtx)
{
    UNREFERENCED_PARAMETER(Device);
    UNREFERENCED_PARAMETER(Options);
    static NTFS_VOLUME vol = {0};
    *VolumeCtx = &vol;
    return STATUS_SUCCESS;
}
static NTSTATUS ntfs_unmount(IN PVOID VolumeCtx)
{
    UNREFERENCED_PARAMETER(VolumeCtx);
    return STATUS_SUCCESS;
}

void FsAdapterRegisterNtfs(void)
{
    static FS_DRIVER drv = {0};
    drv.Name = "ntfs";
    drv.Ops.Mount = ntfs_mount;
    drv.Ops.Unmount = ntfs_unmount;
    FsRegisterDriver(&drv);
}

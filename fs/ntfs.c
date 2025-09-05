/* NTFS adapter (stub) */
#include "../aurora.h"
#include "../include/kern.h"
#include "../include/fs.h"

static NTSTATUS ntfs_mount(IN PCSTR Device, IN PCSTR Options, OUT PVOID* VolumeCtx)
{
    UNREFERENCED_PARAMETER(Device);
    UNREFERENCED_PARAMETER(Options);
    *VolumeCtx = (PVOID)0x3; /* dummy */
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

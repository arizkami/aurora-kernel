/* exFAT adapter (stub) */
#include "../aurora.h"
#include "../include/kern.h"
#include "../include/fs.h"

static NTSTATUS exfat_mount(IN PCSTR Device, IN PCSTR Options, OUT PVOID* VolumeCtx)
{
    UNREFERENCED_PARAMETER(Device);
    UNREFERENCED_PARAMETER(Options);
    *VolumeCtx = (PVOID)0x2; /* dummy */
    return STATUS_SUCCESS;
}
static NTSTATUS exfat_unmount(IN PVOID VolumeCtx)
{
    UNREFERENCED_PARAMETER(VolumeCtx);
    return STATUS_SUCCESS;
}

void FsAdapterRegisterExfat(void)
{
    static FS_DRIVER drv = {0};
    drv.Name = "exfat";
    drv.Ops.Mount = exfat_mount;
    drv.Ops.Unmount = exfat_unmount;
    FsRegisterDriver(&drv);
}

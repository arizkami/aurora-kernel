/* FAT32 adapter (stub) */
#include "../aurora.h"
#include "../include/kern.h"
#include "../include/fs.h"

static NTSTATUS fat32_mount(IN PCSTR Device, IN PCSTR Options, OUT PVOID* VolumeCtx)
{
    UNREFERENCED_PARAMETER(Device);
    UNREFERENCED_PARAMETER(Options);
    *VolumeCtx = (PVOID)0x1; /* dummy */
    return STATUS_SUCCESS;
}
static NTSTATUS fat32_unmount(IN PVOID VolumeCtx)
{
    UNREFERENCED_PARAMETER(VolumeCtx);
    return STATUS_SUCCESS;
}

void FsAdapterRegisterFat32(void)
{
    static FS_DRIVER drv = {0};
    drv.Name = "fat32";
    drv.Ops.Mount = fat32_mount;
    drv.Ops.Unmount = fat32_unmount;
    FsRegisterDriver(&drv);
}

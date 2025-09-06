/* FAT32 driver enhanced stub */
#include "../../aurora.h"
#include "../../include/kern.h"
#include "../../include/fs.h"

typedef struct _FAT32_VOLUME {
    UINT32 Dummy;
} FAT32_VOLUME, *PFAT32_VOLUME;

static NTSTATUS fat32_mount(IN PCSTR Device, IN PCSTR Options, OUT PVOID* VolumeCtx)
{
    UNREFERENCED_PARAMETER(Device);
    UNREFERENCED_PARAMETER(Options);
    static FAT32_VOLUME vol = {0};
    *VolumeCtx = &vol;
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

/* Aurora I/O Manager core */
#include "../aurora.h"
#include "../include/io.h"

static PAIO_DRIVER_OBJECT g_DriverList = NULL;
static PAIO_DEVICE_OBJECT g_DeviceList = NULL;
static AURORA_SPINLOCK g_IoLock;

static void IoListInsertDriver(PAIO_DRIVER_OBJECT drv){
    drv->Next = g_DriverList;
    g_DriverList = drv;
}
static void IoListInsertDevice(PAIO_DEVICE_OBJECT dev){
    dev->Next = g_DeviceList;
    g_DeviceList = dev;
}

NTSTATUS IoInitialize(void){
    NTSTATUS st = AuroraInitializeSpinLock(&g_IoLock);
    if(!NT_SUCCESS(st)) return st;
    g_DriverList = NULL;
    g_DeviceList = NULL;
    AuroraDebugPrint("[io] initialized");
    /* Auto-register core pseudo/layer drivers (POSIX VFS layer first) */
    IoRegisterPosixVfsLayer();
    return STATUS_SUCCESS;
}

NTSTATUS IoRegisterDriver(IN PAIO_DRIVER_OBJECT Driver){
    AURORA_IRQL old;
    if(!Driver) return STATUS_INVALID_PARAMETER;
    AuroraAcquireSpinLock(&g_IoLock, &old);
    IoListInsertDriver(Driver);
    AuroraReleaseSpinLock(&g_IoLock, old);
    AuroraDebugPrint("[io] driver registered %s", Driver->Name);
    return STATUS_SUCCESS;
}

NTSTATUS IoCreateDevice(IN PAIO_DRIVER_OBJECT Driver, IN PCHAR Name, IN UINT32 Type, OUT PAIO_DEVICE_OBJECT* DeviceOut){
    AIO_DEVICE_OBJECT* dev;
    AURORA_IRQL old;
    if(!Driver || !Name || !DeviceOut) return STATUS_INVALID_PARAMETER;
    dev = (AIO_DEVICE_OBJECT*)AuroraAllocateMemory(sizeof(AIO_DEVICE_OBJECT));
    if(!dev) return STATUS_INSUFFICIENT_RESOURCES;
    memset(dev,0,sizeof(*dev));
    strncpy(dev->Name, Name, IO_MAX_NAME-1);
    dev->DeviceType = Type;
    dev->Driver = Driver;
    AuroraAcquireSpinLock(&g_IoLock, &old);
    IoListInsertDevice(dev);
    AuroraReleaseSpinLock(&g_IoLock, old);
    *DeviceOut = dev;
    AuroraDebugPrint("[io] device created %s", dev->Name);
    return STATUS_SUCCESS;
}

NTSTATUS IoDeleteDevice(IN PAIO_DEVICE_OBJECT Device){
    (void)Device; /* TODO: unlink lists */
    return STATUS_NOT_IMPLEMENTED;
}

PAIO_IRP IoAllocateIrp(IN AIO_IRP_MAJOR Major, IN UINT32 Length){
    AIO_IRP* irp;
    irp = (AIO_IRP*)AuroraAllocateMemory(sizeof(AIO_IRP));
    if(!irp) return NULL;
    memset(irp,0,sizeof(*irp));
    irp->Major = Major;
    if(Length){
        irp->Buffer = AuroraAllocateMemory(Length);
        if(!irp->Buffer) return NULL;
        irp->Length = Length;
    }
    return irp;
}

void IoFreeIrp(IN PAIO_IRP Irp){ (void)Irp; /* TODO pool mgmt */ }

NTSTATUS IoSubmitIrp(IN PAIO_DEVICE_OBJECT Device, IN PAIO_IRP Irp){
    PAIO_DRIVER_OBJECT drv;
    if(!Device || !Irp) return STATUS_INVALID_PARAMETER;
    drv = Device->Driver;
    if(!drv || Irp->Major >= AioIrpMax) return STATUS_INVALID_PARAMETER;
    if(!drv->Dispatch[Irp->Major]) return STATUS_NOT_IMPLEMENTED;
    Irp->Device = Device;
    Irp->Status = drv->Dispatch[Irp->Major](Device, Irp);
    return Irp->Status;
}

/* --- Driver category stub registrations --- */
static AIO_DRIVER_OBJECT g_StorageDriver, g_HidDriver, g_DisplayDriver, g_AudioDriver, g_PosixLayer;

NTSTATUS IoRegisterStorageDriver(void){
    IoDriverInitialize(&g_StorageDriver, "storage");
    return IoRegisterDriver(&g_StorageDriver);
}
NTSTATUS IoRegisterHidDriver(void){
    IoDriverInitialize(&g_HidDriver, "hid");
    return IoRegisterDriver(&g_HidDriver);
}
NTSTATUS IoRegisterDisplayDriver(void){
    IoDriverInitialize(&g_DisplayDriver, "display");
    return IoRegisterDriver(&g_DisplayDriver);
}
NTSTATUS IoRegisterAudioDriver(void){
    IoDriverInitialize(&g_AudioDriver, "audio");
    return IoRegisterDriver(&g_AudioDriver);
}
NTSTATUS IoRegisterPosixVfsLayer(void){
    IoDriverInitialize(&g_PosixLayer, "posixvfs");
    /* This layer will later provide path translation /dev,/proc, mount table */
    return IoRegisterDriver(&g_PosixLayer);
}


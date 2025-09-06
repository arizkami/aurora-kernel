/* Aurora Kernel I/O Manager Public Interfaces */
#ifndef _AURORA_IO_H_
#define _AURORA_IO_H_
#include "../aurora.h"

/* Forward types */
struct _AIO_DEVICE_OBJECT;
struct _AIO_DRIVER_OBJECT;
struct _AIO_IRP;

/* Status / Major function codes */
#define IO_MAX_NAME               64
#define IO_MAX_DRIVERS            128
#define IO_MAX_DEVICES            512

typedef enum _AIO_IRP_MAJOR {
    AioIrpCreate = 0,
    AioIrpClose,
    AioIrpRead,
    AioIrpWrite,
    AioIrpDeviceControl,
    AioIrpPnp,
    AioIrpMax
} AIO_IRP_MAJOR;

/* IRP structure (simplified) */
typedef struct _AIO_IRP {
    AIO_IRP_MAJOR Major;
    UINT32 Minor;
    PVOID  Buffer;
    UINT32 Length;
    UINT32 Information; /* bytes processed */
    NTSTATUS Status;
    struct _AIO_DEVICE_OBJECT* Device;
    struct _AIO_IRP* Next;
} AIO_IRP, *PAIO_IRP;

/* Driver dispatch signature */
typedef NTSTATUS (*PAIO_DRIVER_DISPATCH)(IN struct _AIO_DEVICE_OBJECT* Device, IN PAIO_IRP Irp);

/* Driver object */
typedef struct _AIO_DRIVER_OBJECT {
    CHAR Name[IO_MAX_NAME];
    UINT32 Flags;
    PAIO_DRIVER_DISPATCH Dispatch[AioIrpMax];
    struct _AIO_DRIVER_OBJECT* Next;
} AIO_DRIVER_OBJECT, *PAIO_DRIVER_OBJECT;

/* Device object */
typedef struct _AIO_DEVICE_OBJECT {
    CHAR Name[IO_MAX_NAME];
    UINT32 DeviceType;
    UINT32 Characteristics;
    PVOID  DeviceExtension;
    PAIO_DRIVER_OBJECT Driver;
    struct _AIO_DEVICE_OBJECT* Next;
} AIO_DEVICE_OBJECT, *PAIO_DEVICE_OBJECT;

/* Device type classes (coarse) */
enum {
    IO_DEVICE_CLASS_UNSPEC = 0,
    IO_DEVICE_CLASS_BLOCK,      /* ATA/SATA/SCSI/NVMe/SDCard/virtio-blk */
    IO_DEVICE_CLASS_CHAR,       /* Serial, basic input */
    IO_DEVICE_CLASS_HID,        /* Keyboard / mouse / pointer */
    IO_DEVICE_CLASS_DISPLAY,    /* Framebuffer / GOP */
    IO_DEVICE_CLASS_AUDIO,      /* Simple PCM output */
    IO_DEVICE_CLASS_MAX
};

/* Standard minor types for block */
enum {
    IO_BLOCK_TYPE_ATA = 1,
    IO_BLOCK_TYPE_SATA,
    IO_BLOCK_TYPE_SCSI,
    IO_BLOCK_TYPE_NVME,
    IO_BLOCK_TYPE_SDCARD,
    IO_BLOCK_TYPE_VIRTIO_BLK
};

/* HID minor types */
enum {
    IO_HID_TYPE_KEYBOARD = 1,
    IO_HID_TYPE_MOUSE,
    IO_HID_TYPE_TOUCHPAD
};

/* Display minor types */
enum { IO_DISPLAY_TYPE_FB = 1 }; /* raw linear framebuffer */

/* Audio minor types */
enum { IO_AUDIO_TYPE_PCM = 1 };

/* Registration helpers (stubs) */
NTSTATUS IoRegisterStorageDriver(void);
NTSTATUS IoRegisterHidDriver(void);
NTSTATUS IoRegisterDisplayDriver(void);
NTSTATUS IoRegisterAudioDriver(void);
NTSTATUS IoRegisterPosixVfsLayer(void); /* pseudo-driver for POSIX path translation */

/* Block layer API */
NTSTATUS BlockSubsystemInitialize(void);
NTSTATUS BlockRead(PAIO_DEVICE_OBJECT Dev, UINT64 Lba, UINT32 Count, PVOID Buffer);
NTSTATUS BlockWrite(PAIO_DEVICE_OBJECT Dev, UINT64 Lba, UINT32 Count, PVOID Buffer);

/* Registration */
NTSTATUS IoRegisterDriver(IN PAIO_DRIVER_OBJECT Driver);
NTSTATUS IoCreateDevice(IN PAIO_DRIVER_OBJECT Driver, IN PCHAR Name, IN UINT32 Type, OUT PAIO_DEVICE_OBJECT* DeviceOut);
NTSTATUS IoDeleteDevice(IN PAIO_DEVICE_OBJECT Device);
NTSTATUS IoDriverInitialize(PAIO_DRIVER_OBJECT Driver, const char* Name);

/* IRP lifecycle */
PAIO_IRP IoAllocateIrp(IN AIO_IRP_MAJOR Major, IN UINT32 Length);
void IoFreeIrp(IN PAIO_IRP Irp);
NTSTATUS IoSubmitIrp(IN PAIO_DEVICE_OBJECT Device, IN PAIO_IRP Irp);

/* Initialization */
NTSTATUS IoInitialize(void);

#endif /* _AURORA_IO_H_ */

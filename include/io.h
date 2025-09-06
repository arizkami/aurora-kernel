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

/* Registration */
NTSTATUS IoRegisterDriver(IN PAIO_DRIVER_OBJECT Driver);
NTSTATUS IoCreateDevice(IN PAIO_DRIVER_OBJECT Driver, IN PCHAR Name, IN UINT32 Type, OUT PAIO_DEVICE_OBJECT* DeviceOut);
NTSTATUS IoDeleteDevice(IN PAIO_DEVICE_OBJECT Device);

/* IRP lifecycle */
PAIO_IRP IoAllocateIrp(IN AIO_IRP_MAJOR Major, IN UINT32 Length);
void IoFreeIrp(IN PAIO_IRP Irp);
NTSTATUS IoSubmitIrp(IN PAIO_DEVICE_OBJECT Device, IN PAIO_IRP Irp);

/* Initialization */
NTSTATUS IoInitialize(void);

#endif /* _AURORA_IO_H_ */

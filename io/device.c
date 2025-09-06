/* Device specific helpers */
#include "../aurora.h"
#include "../include/io.h"

NTSTATUS IoDeviceSetExtension(IN PAIO_DEVICE_OBJECT Device, IN PVOID Extension){
    if(!Device) return STATUS_INVALID_PARAMETER;
    Device->DeviceExtension = Extension;
    return STATUS_SUCCESS;
}

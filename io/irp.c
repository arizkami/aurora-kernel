/* IRP helpers */
#include "../aurora.h"
#include "../include/io.h"

NTSTATUS IoCompleteIrp(IN PAIO_IRP Irp, IN NTSTATUS Status, IN UINT32 Information){
    if(!Irp) return STATUS_INVALID_PARAMETER;
    Irp->Status = Status;
    Irp->Information = Information;
    return Status;
}

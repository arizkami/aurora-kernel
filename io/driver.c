/* Driver helper creation */
#include "../aurora.h"
#include "../include/io.h"

NTSTATUS IoDriverInitialize(PAIO_DRIVER_OBJECT Driver, const char* Name){
    if(!Driver || !Name) return STATUS_INVALID_PARAMETER;
    memset(Driver,0,sizeof(*Driver));
    strncpy(Driver->Name, Name, IO_MAX_NAME-1);
    return STATUS_SUCCESS;
}

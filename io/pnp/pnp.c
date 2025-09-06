/* Plug and Play manager stub */
#include "../../aurora.h"
#include "../../include/io.h"

NTSTATUS PnpInitialize(void){
    AuroraDebugPrint("[pnp] init");
    return STATUS_SUCCESS;
}

/* Core driver registry for new driver framework */
#include "../aurora.h"
#include "../include/kern/driver.h"

#define MAX_AUR_DRIVERS 32
#define MAX_AUR_DEVICES 64

static aur_driver_t* g_drivers[MAX_AUR_DRIVERS];
static aur_device_t* g_devices[MAX_AUR_DEVICES];
static UINT32 g_driver_count=0, g_device_count=0;

aur_status_t aur_driver_register(aur_driver_t* drv){
    if(!drv || !drv->name) return AUR_ERR_INVAL;
    if(g_driver_count>=MAX_AUR_DRIVERS) return AUR_ERR_NOMEM;
    g_drivers[g_driver_count++] = drv;
    return AUR_OK;
}

aur_status_t aur_device_add(aur_device_t* dev){
    if(!dev || !dev->name[0]) return AUR_ERR_INVAL;
    if(g_device_count>=MAX_AUR_DEVICES) return AUR_ERR_NOMEM;
    g_devices[g_device_count++] = dev;
    return AUR_OK;
}

aur_device_t* aur_device_find(const CHAR* name){
    for(UINT32 i=0;i<g_device_count;i++) if(strcmp(g_devices[i]->name,name)==0) return g_devices[i];
    return NULL;
}

/* IRQ registry stub */
aur_status_t aur_register_irq(UINT32 irq, aur_irq_handler_t h, PVOID ctx){ (void)irq; (void)h; (void)ctx; return AUR_OK; }

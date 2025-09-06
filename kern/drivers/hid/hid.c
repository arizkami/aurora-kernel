/* Aurora HID Driver Skeleton */
#include "../../../aurora.h"
#include "../../../include/kern/driver.h"

typedef struct _hid_ring { UINT8 data[256]; UINT32 head, tail; } hid_ring_t;
static hid_ring_t g_hid_ring;
static aur_driver_t g_hid_driver = { "aurora-hid", NULL, NULL };
static aur_device_t g_hid_device;

static INT64 hid_read(aur_device_t* dev, void* buf, size_t size, UINT64 off){
    (void)off; hid_ring_t* r=(hid_ring_t*)dev->drvdata; UINT8* out=(UINT8*)buf; UINT32 n=0;
    while(n<size && r->tail!=r->head){ out[n++]=r->data[r->tail]; r->tail=(r->tail+1)%sizeof(r->data); }
    return (INT64)n;
}

void HidDriverInject(UINT8 code){ hid_ring_t* r=&g_hid_ring; r->data[r->head]=code; r->head=(r->head+1)%sizeof(r->data); if(r->head==r->tail) r->tail=(r->tail+1)%sizeof(r->data); }

void HidDriverInitialize(void){
    aur_driver_register(&g_hid_driver);
    memset(&g_hid_device,0,sizeof(g_hid_device));
    strncpy(g_hid_device.name, "aurhid0", sizeof(g_hid_device.name)-1);
    g_hid_device.driver=&g_hid_driver; g_hid_device.read=hid_read; g_hid_device.drvdata=&g_hid_ring; aur_device_add(&g_hid_device);
    HidDriverInject(0x1C); /* example 'A' scancode */
}

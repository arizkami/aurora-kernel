/* Aurora Display Driver Skeleton */
#include "../../../aurora.h"
#include "../../../include/kern/driver.h"
#include "../../../include/fb.h"

typedef struct _disp_priv { aur_display_info_t info; } disp_priv_t;

static aur_driver_t g_display_driver = { "aurora-display", NULL, NULL };
static aur_device_t g_display_device;
static disp_priv_t g_priv;

static aur_status_t display_ioctl(aur_device_t* dev, UINT32 code, void* inout){
    (void)dev; if(code==AUR_IOCTL_GET_FB_INFO){ *((aur_display_info_t*)inout)=g_priv.info; return AUR_OK; }
    return AUR_ERR_UNSUPPORTED;
}

void DisplayDriverInitialize(void){
    aur_driver_register(&g_display_driver);
    memset(&g_display_device,0,sizeof(g_display_device));
    strncpy(g_display_device.name, "aurfb0", sizeof(g_display_device.name)-1);
    g_display_device.driver=&g_display_driver;
    g_display_device.ioctl=display_ioctl;
    if(g_FramebufferMode.Base){
        g_priv.info.width = g_FramebufferMode.Width;
        g_priv.info.height= g_FramebufferMode.Height;
        g_priv.info.pitch = g_FramebufferMode.Pitch;
        g_priv.info.bpp   = g_FramebufferMode.Bpp;
        g_display_device.drvdata = &g_priv;
        aur_device_add(&g_display_device);
    }
}

/* Aurora Audio Driver Skeleton */
#include "../../../aurora.h"
#include "../../../include/kern/driver.h"

typedef struct _audio_priv {
    aur_audio_params_t params;
    UINT8 ring[8192];
    UINT32 head, tail;
} audio_priv_t;

static aur_driver_t g_audio_driver = { "aurora-audio", NULL, NULL };
static aur_device_t g_audio_device; static audio_priv_t g_audio_priv;

static INT64 audio_write(aur_device_t* dev, const void* buf, size_t size, UINT64 off){
    (void)off; audio_priv_t* p=(audio_priv_t*)dev->drvdata; const UINT8* b=(const UINT8*)buf; UINT32 w=0;
    while(w<size){ p->ring[p->head]=b[w]; p->head=(p->head+1)%sizeof(p->ring); if(p->head==p->tail) p->tail=(p->tail+1)%sizeof(p->ring); w++; }
    return (INT64)w;
}
static aur_status_t audio_ioctl(aur_device_t* dev, UINT32 code, void* inout){
    if(code==AUR_IOCTL_GET_AUDIO_PARAMS){ *((aur_audio_params_t*)inout)=((audio_priv_t*)dev->drvdata)->params; return AUR_OK; }
    return AUR_ERR_UNSUPPORTED;
}

void AudioDriverInitialize(void){
    aur_driver_register(&g_audio_driver);
    memset(&g_audio_device,0,sizeof(g_audio_device));
    strncpy(g_audio_device.name, "auraud0", sizeof(g_audio_device.name)-1);
    g_audio_device.driver=&g_audio_driver;
    g_audio_device.write=audio_write;
    g_audio_device.ioctl=audio_ioctl;
    g_audio_priv.params.sample_rate=48000; g_audio_priv.params.channels=2; g_audio_priv.params.bits=16;
    g_audio_device.drvdata=&g_audio_priv;
    aur_device_add(&g_audio_device);
}

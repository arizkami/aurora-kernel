/* Aurora Driver Framework (lightweight) */
#ifndef _AURORA_DRIVER_H_
#define _AURORA_DRIVER_H_

#include "../../aurora.h"
#include "../io.h"

typedef struct _aur_device aur_device_t;
typedef struct _aur_driver aur_driver_t;

typedef UINT32 aur_status_t;
#define AUR_OK              0
#define AUR_ERR_INVAL       1
#define AUR_ERR_NOMEM       2
#define AUR_ERR_IO          3
#define AUR_ERR_UNSUPPORTED 4

typedef INT64 (*aur_dev_read_t)(aur_device_t*, void*, size_t, UINT64 offset);
typedef INT64 (*aur_dev_write_t)(aur_device_t*, const void*, size_t, UINT64 offset);
typedef aur_status_t (*aur_dev_ioctl_t)(aur_device_t*, UINT32 code, void* inout);
typedef aur_status_t (*aur_dev_probe_t)(aur_device_t*);
typedef void (*aur_dev_remove_t)(aur_device_t*);

struct _aur_device {
    CHAR name[64];
    UINT32 type; /* class specific */
    PVOID  drvdata; /* driver private */
    aur_dev_read_t  read;
    aur_dev_write_t write;
    aur_dev_ioctl_t ioctl;
    aur_driver_t*   driver;
};

struct _aur_driver {
    const CHAR* name;
    aur_dev_probe_t  probe;
    aur_dev_remove_t remove;
};

/* Simple IRQ registration placeholder */
typedef void (*aur_irq_handler_t)(UINT32 irq, PVOID ctx);
aur_status_t aur_register_irq(UINT32 irq, aur_irq_handler_t h, PVOID ctx);

/* Device registration (very small directory) */
aur_status_t aur_driver_register(aur_driver_t* drv);
aur_status_t aur_device_add(aur_device_t* dev);
aur_device_t* aur_device_find(const CHAR* name);

/* Utility for block drivers */
typedef struct _aur_block_geometry {
    UINT64 total_bytes;
    UINT32 block_size; /* 512 or 4096 typical */
} aur_block_geometry_t;

typedef struct _aur_display_info {
    UINT32 width;
    UINT32 height;
    UINT32 pitch;
    UINT32 bpp;
} aur_display_info_t;

typedef struct _aur_audio_params {
    UINT32 sample_rate; /* Hz */
    UINT32 channels;    /* 1 mono 2 stereo */
    UINT32 bits;        /* bits per sample (8/16/32) */
} aur_audio_params_t;

#define AUR_IOCTL_GET_BLOCK_SIZE  0x1001
#define AUR_IOCTL_GET_DISK_SIZE   0x1002
#define AUR_IOCTL_GET_FB_INFO     0x2001
#define AUR_IOCTL_GET_AUDIO_PARAMS 0x3001

#endif

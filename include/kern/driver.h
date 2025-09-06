/* Aurora Driver Framework (Enhanced) */
#ifndef _AURORA_DRIVER_H_
#define _AURORA_DRIVER_H_

#include "../../aurora.h"
#include "../io.h"

typedef struct _aur_device aur_device_t;
typedef struct _aur_driver aur_driver_t;
typedef struct _aur_bus aur_bus_t;
typedef struct _aur_device_node aur_device_node_t;

typedef UINT32 aur_status_t;
#define AUR_OK              0
#define AUR_ERR_INVAL       1
#define AUR_ERR_NOMEM       2
#define AUR_ERR_IO          3
#define AUR_ERR_UNSUPPORTED 4
#define AUR_ERR_BUSY        5
#define AUR_ERR_TIMEOUT     6
#define AUR_ERR_NOT_FOUND   7
#define AUR_ERR_ACCESS      8

/* Device Classes */
#define AUR_CLASS_UNKNOWN   0x00
#define AUR_CLASS_STORAGE   0x01
#define AUR_CLASS_NETWORK   0x02
#define AUR_CLASS_DISPLAY   0x03
#define AUR_CLASS_AUDIO     0x04
#define AUR_CLASS_INPUT     0x05
#define AUR_CLASS_USB       0x06
#define AUR_CLASS_PCI       0x07
#define AUR_CLASS_POWER     0x08
#define AUR_CLASS_THERMAL   0x09

/* Device States */
#define AUR_DEV_STATE_UNKNOWN    0
#define AUR_DEV_STATE_PRESENT    1
#define AUR_DEV_STATE_STARTED    2
#define AUR_DEV_STATE_STOPPED    3
#define AUR_DEV_STATE_REMOVED    4
#define AUR_DEV_STATE_ERROR      5

typedef INT64 (*aur_dev_read_t)(aur_device_t*, void*, size_t, UINT64 offset);
typedef INT64 (*aur_dev_write_t)(aur_device_t*, const void*, size_t, UINT64 offset);
typedef aur_status_t (*aur_dev_ioctl_t)(aur_device_t*, UINT32 code, void* inout);
typedef aur_status_t (*aur_dev_probe_t)(aur_device_t*);
typedef void (*aur_dev_remove_t)(aur_device_t*);
typedef aur_status_t (*aur_dev_suspend_t)(aur_device_t*);
typedef aur_status_t (*aur_dev_resume_t)(aur_device_t*);
typedef aur_status_t (*aur_dev_reset_t)(aur_device_t*);
typedef void (*aur_dev_shutdown_t)(aur_device_t*);

/* Bus operations */
typedef aur_status_t (*aur_bus_match_t)(aur_device_t*, aur_driver_t*);
typedef aur_status_t (*aur_bus_probe_t)(aur_device_t*);
typedef void (*aur_bus_remove_t)(aur_device_t*);

/* Device tree node */
struct _aur_device_node {
    CHAR name[64];
    CHAR compatible[128];
    UINT32 reg[8];  /* Register addresses */
    UINT32 reg_count;
    UINT32 interrupts[4];
    UINT32 irq_count;
    struct _aur_device_node* parent;
    struct _aur_device_node* child;
    struct _aur_device_node* sibling;
    PVOID properties;
};

/* Bus structure */
struct _aur_bus {
    CHAR name[64];
    UINT32 type;
    aur_bus_match_t match;
    aur_bus_probe_t probe;
    aur_bus_remove_t remove;
    struct _aur_bus* next;
};

struct _aur_device {
    CHAR name[64];
    UINT32 class_id;     /* Device class */
    UINT32 vendor_id;
    UINT32 device_id;
    UINT32 state;        /* Device state */
    UINT32 flags;
    PVOID  drvdata;      /* Driver private data */
    
    /* Device operations */
    aur_dev_read_t     read;
    aur_dev_write_t    write;
    aur_dev_ioctl_t    ioctl;
    aur_dev_suspend_t  suspend;
    aur_dev_resume_t   resume;
    aur_dev_reset_t    reset;
    aur_dev_shutdown_t shutdown;
    
    /* Device hierarchy */
    aur_driver_t*      driver;
    aur_bus_t*         bus;
    aur_device_node_t* of_node;
    struct _aur_device* parent;
    struct _aur_device* next;
    
    /* Resource management */
    UINT64 base_addr[6];
    UINT32 irq_line;
    UINT32 dma_mask;
    
    /* Power management */
    UINT32 power_state;
    UINT64 power_usage;
};

struct _aur_driver {
    const CHAR* name;
    UINT32 class_id;
    UINT32 vendor_id;
    UINT32 device_id;
    UINT32 flags;
    
    /* Driver operations */
    aur_dev_probe_t   probe;
    aur_dev_remove_t  remove;
    aur_dev_suspend_t suspend;
    aur_dev_resume_t  resume;
    aur_dev_reset_t   reset;
    aur_dev_shutdown_t shutdown;
    
    /* Driver management */
    UINT32 ref_count;
    struct _aur_driver* next;
};

/* Simple IRQ registration placeholder */
typedef void (*aur_irq_handler_t)(UINT32 irq, PVOID ctx);
aur_status_t aur_register_irq(UINT32 irq, aur_irq_handler_t h, PVOID ctx);

/* Enhanced device and driver management */
aur_status_t aur_driver_register(aur_driver_t* drv);
aur_status_t aur_driver_unregister(aur_driver_t* drv);
aur_status_t aur_device_add(aur_device_t* dev);
aur_status_t aur_device_remove(aur_device_t* dev);
aur_device_t* aur_device_find(const CHAR* name);
aur_device_t* aur_device_find_by_class(UINT32 class_id);
aur_status_t aur_device_enumerate(UINT32 class_id, aur_device_t** devices, UINT32* count);

/* Bus management */
aur_status_t aur_bus_register(aur_bus_t* bus);
aur_status_t aur_bus_unregister(aur_bus_t* bus);
aur_bus_t* aur_bus_find(const CHAR* name);

/* Device tree operations */
aur_device_node_t* aur_of_find_node_by_name(const CHAR* name);
aur_device_node_t* aur_of_find_compatible_node(const CHAR* compatible);
aur_status_t aur_of_get_property(aur_device_node_t* node, const CHAR* name, void* value, UINT32* size);

/* Power management */
aur_status_t aur_device_suspend(aur_device_t* dev);
aur_status_t aur_device_resume(aur_device_t* dev);
aur_status_t aur_device_set_power_state(aur_device_t* dev, UINT32 state);

/* Resource management */
aur_status_t aur_device_request_region(aur_device_t* dev, UINT64 start, UINT64 size, const CHAR* name);
aur_status_t aur_device_release_region(aur_device_t* dev, UINT64 start, UINT64 size);
PVOID aur_device_ioremap(aur_device_t* dev, UINT64 phys_addr, UINT64 size);
void aur_device_iounmap(PVOID virt_addr);

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

/* IOCTL codes - organized by device class */
/* Storage (0x1000-0x1FFF) */
#define AUR_IOCTL_GET_BLOCK_SIZE     0x1001
#define AUR_IOCTL_GET_DISK_SIZE      0x1002
#define AUR_IOCTL_GET_DISK_INFO      0x1003
#define AUR_IOCTL_FLUSH_CACHE        0x1004
#define AUR_IOCTL_TRIM               0x1005
#define AUR_IOCTL_GET_SMART_DATA     0x1006

/* Display (0x2000-0x2FFF) */
#define AUR_IOCTL_GET_FB_INFO        0x2001
#define AUR_IOCTL_SET_FB_MODE        0x2002
#define AUR_IOCTL_GET_EDID           0x2003
#define AUR_IOCTL_SET_BRIGHTNESS     0x2004
#define AUR_IOCTL_GET_GPU_INFO       0x2005
#define AUR_IOCTL_ALLOC_GPU_MEM      0x2006
#define AUR_IOCTL_FREE_GPU_MEM       0x2007

/* Audio (0x3000-0x3FFF) */
#define AUR_IOCTL_GET_AUDIO_PARAMS   0x3001
#define AUR_IOCTL_SET_AUDIO_PARAMS   0x3002
#define AUR_IOCTL_GET_MIXER_INFO     0x3003
#define AUR_IOCTL_SET_VOLUME         0x3004
#define AUR_IOCTL_GET_CODEC_INFO     0x3005

/* Input/HID (0x4000-0x4FFF) */
#define AUR_IOCTL_GET_HID_INFO       0x4001
#define AUR_IOCTL_SET_HID_REPORT     0x4002
#define AUR_IOCTL_GET_HID_REPORT     0x4003
#define AUR_IOCTL_SET_LED_STATE      0x4004
#define AUR_IOCTL_GET_KEY_STATE      0x4005

/* Network (0x5000-0x5FFF) */
#define AUR_IOCTL_GET_NET_INFO       0x5001
#define AUR_IOCTL_SET_MAC_ADDR       0x5002
#define AUR_IOCTL_GET_LINK_STATE     0x5003
#define AUR_IOCTL_SET_PROMISCUOUS    0x5004
#define AUR_IOCTL_GET_STATS          0x5005

/* Power (0x6000-0x6FFF) */
#define AUR_IOCTL_GET_POWER_INFO     0x6001
#define AUR_IOCTL_SET_POWER_STATE    0x6002
#define AUR_IOCTL_GET_BATTERY_INFO   0x6003
#define AUR_IOCTL_SET_CPU_FREQ       0x6004
#define AUR_IOCTL_GET_THERMAL_INFO   0x6005

/* PCI (0x7000-0x7FFF) */
#define AUR_IOCTL_GET_PCI_INFO       0x7001
#define AUR_IOCTL_READ_PCI_CONFIG    0x7002
#define AUR_IOCTL_WRITE_PCI_CONFIG   0x7003
#define AUR_IOCTL_ENABLE_MSI         0x7004
#define AUR_IOCTL_DISABLE_MSI        0x7005

/* Enhanced data structures */
typedef struct _aur_disk_info {
    UINT64 total_sectors;
    UINT32 sector_size;
    UINT32 physical_sector_size;
    UINT32 queue_depth;
    UINT32 max_transfer_size;
    CHAR model[64];
    CHAR serial[32];
    CHAR firmware[16];
    UINT32 features;
} aur_disk_info_t;

typedef struct _aur_gpu_info {
    CHAR name[64];
    UINT32 vendor_id;
    UINT32 device_id;
    UINT64 vram_size;
    UINT64 vram_free;
    UINT32 core_clock;
    UINT32 memory_clock;
    UINT32 temperature;
} aur_gpu_info_t;

typedef struct _aur_network_info {
    CHAR name[32];
    UINT8 mac_addr[6];
    UINT32 speed;        /* Mbps */
    UINT32 duplex;       /* 0=half, 1=full */
    UINT32 link_state;   /* 0=down, 1=up */
    UINT64 rx_packets;
    UINT64 tx_packets;
    UINT64 rx_bytes;
    UINT64 tx_bytes;
    UINT64 rx_errors;
    UINT64 tx_errors;
} aur_network_info_t;

typedef struct _aur_hid_info {
    UINT32 vendor_id;
    UINT32 product_id;
    UINT32 version;
    UINT32 usage_page;
    UINT32 usage;
    UINT32 report_size;
    UINT32 feature_count;
    CHAR manufacturer[64];
    CHAR product[64];
} aur_hid_info_t;

typedef struct _aur_power_info {
    UINT32 ac_online;
    UINT32 battery_present;
    UINT32 battery_capacity;
    UINT32 battery_voltage;
    UINT32 battery_current;
    UINT32 battery_temperature;
    UINT32 cpu_frequency;
    UINT32 cpu_usage;
    UINT32 system_temperature;
} aur_power_info_t;

typedef struct _aur_pci_info {
    UINT32 bus;
    UINT32 device;
    UINT32 function;
    UINT32 vendor_id;
    UINT32 device_id;
    UINT32 class_code;
    UINT32 revision;
    UINT64 bar[6];
    UINT32 irq;
    UINT32 msi_enabled;
} aur_pci_info_t;

#endif

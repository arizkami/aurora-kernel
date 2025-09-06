/* Aurora Storage Block Driver Prototype */
#include "../../../aurora.h"
#include "../../../include/kern/driver.h"

typedef struct _stor_req {
    UINT64 lba;
    UINT32 count;
    PVOID  buffer;
    int write;
    volatile int done;
    struct _stor_req* next;
} stor_req_t;

typedef struct _stor_priv {
    AURORA_SPINLOCK lock;
    stor_req_t* head; stor_req_t* tail;
    aur_block_geometry_t geo;
    UINT8* backing; /* simple in-memory backing store */
} stor_priv_t;

extern void aur_mem_fence(void);

static void queue_push(stor_priv_t* p, stor_req_t* r){
    AURORA_IRQL o; AuroraAcquireSpinLock(&p->lock,&o);
    r->next=NULL; if(p->tail) p->tail->next=r; else p->head=r; p->tail=r;
    AuroraReleaseSpinLock(&p->lock,o);
}
static stor_req_t* queue_pop(stor_priv_t* p){
    AURORA_IRQL o; AuroraAcquireSpinLock(&p->lock,&o);
    stor_req_t* r=p->head; if(r){ p->head=r->next; if(!p->head) p->tail=NULL; }
    AuroraReleaseSpinLock(&p->lock,o); return r;
}

static void process_one(stor_priv_t* p, stor_req_t* r){
    UINT64 off = r->lba * p->geo.block_size;
    UINT64 bytes = (UINT64)r->count * p->geo.block_size;
    if(r->write) memcpy(p->backing+off, r->buffer, bytes); else memcpy(r->buffer, p->backing+off, bytes);
    aur_mem_fence(); r->done=1;
}

static void process_all(stor_priv_t* p){ stor_req_t* r; while((r=queue_pop(p))!=NULL) process_one(p,r); }

static INT64 drv_storage_rw(aur_device_t* dev, void* buf, size_t size, UINT64 off, int write){
    stor_priv_t* priv = (stor_priv_t*)dev->drvdata;
    if(size==0) return 0; if(off % priv->geo.block_size) return -1;
    UINT32 blocks = (UINT32)(size / priv->geo.block_size);
    if(blocks==0) return -1;
    stor_req_t req; req.lba = off / priv->geo.block_size; req.count=blocks; req.buffer=buf; req.write=write; req.done=0; req.next=NULL;
    queue_push(priv,&req); process_all(priv); /* synchronous poll */
    while(!req.done) { /* spin */ }
    return (INT64)(blocks * priv->geo.block_size);
}

static INT64 drv_storage_read(aur_device_t* dev, void* buf, size_t size, UINT64 off){
    return drv_storage_rw(dev, buf, size, off, 0);
}
static INT64 drv_storage_write(aur_device_t* dev, const void* buf, size_t size, UINT64 off){
    return drv_storage_rw(dev, (void*)buf, size, off, 1);
}

static aur_status_t drv_storage_ioctl(aur_device_t* dev, UINT32 code, void* inout){
    stor_priv_t* p=(stor_priv_t*)dev->drvdata;
    if(code==AUR_IOCTL_GET_BLOCK_SIZE){ *(UINT32*)inout = p->geo.block_size; return AUR_OK; }
    if(code==AUR_IOCTL_GET_DISK_SIZE){ *(UINT64*)inout = p->geo.total_bytes; return AUR_OK; }
    return AUR_ERR_UNSUPPORTED;
}

static aur_status_t drv_storage_probe(aur_device_t* dev){
    static UINT8 backing_store[4*1024*1024]; /* 4 MiB */
    stor_priv_t* priv = (stor_priv_t*)AuroraAllocateMemory(sizeof(stor_priv_t));
    if(!priv) return AUR_ERR_NOMEM; memset(priv,0,sizeof(*priv));
    AuroraInitializeSpinLock(&priv->lock);
    priv->geo.block_size = 512; priv->geo.total_bytes = sizeof(backing_store);
    priv->backing = backing_store;
    dev->drvdata = priv;
    dev->read  = drv_storage_read;
    dev->write = drv_storage_write;
    dev->ioctl = drv_storage_ioctl;
    return AUR_OK;
}

static aur_driver_t g_storage_driver = { "aurora-storage", drv_storage_probe, NULL };
static aur_device_t g_storage_device;

void StorageDriverInitialize(void){
    aur_driver_register(&g_storage_driver);
    memset(&g_storage_device,0,sizeof(g_storage_device));
    strncpy(g_storage_device.name, "aurblk0", sizeof(g_storage_device.name)-1);
    g_storage_device.driver = &g_storage_driver;
    if(drv_storage_probe(&g_storage_device)==AUR_OK){ aur_device_add(&g_storage_device); }
}

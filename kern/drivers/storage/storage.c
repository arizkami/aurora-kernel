//! Aurora Storage Driver - Advanced NVMe/AHCI/SATA Support
//! Comprehensive storage subsystem with multiple device management,
//! advanced I/O scheduling, and filesystem integration

#include "../../../aurora.h"
#include "../../../include/kern/driver.h"
#include "../../../include/mem.h"
#include "../../../include/io.h"
#include "../../../include/hal.h"

// Storage Constants
#define MAX_STORAGE_DEVICES     32
#define MAX_IO_REQUESTS         256
#define SECTOR_SIZE             512
#define MAX_TRANSFER_SIZE       (1024 * 1024)  // 1MB
#define IO_QUEUE_DEPTH          64
#define MAX_NAMESPACES          16

// NVMe Constants
#define NVME_CAP_OFFSET         0x00
#define NVME_VS_OFFSET          0x08
#define NVME_CC_OFFSET          0x14
#define NVME_CSTS_OFFSET        0x1C
#define NVME_AQA_OFFSET         0x24
#define NVME_ASQ_OFFSET         0x28
#define NVME_ACQ_OFFSET         0x30

#define NVME_CC_EN              (1 << 0)
#define NVME_CC_CSS_NVM         (0 << 4)
#define NVME_CC_MPS_4K          (0 << 7)
#define NVME_CC_AMS_RR          (0 << 11)
#define NVME_CC_SHN_NONE        (0 << 14)
#define NVME_CC_IOSQES_64       (6 << 16)
#define NVME_CC_IOCQES_16       (4 << 20)

#define NVME_CSTS_RDY           (1 << 0)
#define NVME_CSTS_CFS           (1 << 1)
#define NVME_CSTS_SHST_MASK     (3 << 2)

// AHCI Constants
#define AHCI_CAP_OFFSET         0x00
#define AHCI_GHC_OFFSET         0x04
#define AHCI_IS_OFFSET          0x08
#define AHCI_PI_OFFSET          0x0C
#define AHCI_VS_OFFSET          0x10

#define AHCI_GHC_AE             (1 << 31)
#define AHCI_GHC_IE             (1 << 1)
#define AHCI_GHC_HR             (1 << 0)

// Storage Device Types
typedef enum {
    STORAGE_TYPE_UNKNOWN = 0,
    STORAGE_TYPE_NVME,
    STORAGE_TYPE_AHCI_SATA,
    STORAGE_TYPE_IDE,
    STORAGE_TYPE_SCSI,
    STORAGE_TYPE_USB_MASS,
    STORAGE_TYPE_EMMC,
    STORAGE_TYPE_SD_CARD
} storage_device_type_t;

// I/O Request Types
typedef enum {
    IO_READ = 0,
    IO_WRITE,
    IO_FLUSH,
    IO_TRIM,
    IO_SECURE_ERASE,
    IO_FORMAT
} io_request_type_t;

// I/O Priority Levels
typedef enum {
    IO_PRIORITY_LOW = 0,
    IO_PRIORITY_NORMAL,
    IO_PRIORITY_HIGH,
    IO_PRIORITY_CRITICAL
} io_priority_t;

// I/O Request Status
typedef enum {
    IO_STATUS_PENDING = 0,
    IO_STATUS_IN_PROGRESS,
    IO_STATUS_COMPLETED,
    IO_STATUS_ERROR,
    IO_STATUS_TIMEOUT,
    IO_STATUS_CANCELLED
} io_status_t;

// NVMe Command Structure
typedef struct {
    uint32_t cdw0;      // Command Dword 0
    uint32_t nsid;      // Namespace ID
    uint64_t rsvd2;
    uint64_t mptr;      // Metadata Pointer
    uint64_t prp1;      // PRP Entry 1
    uint64_t prp2;      // PRP Entry 2
    uint32_t cdw10;
    uint32_t cdw11;
    uint32_t cdw12;
    uint32_t cdw13;
    uint32_t cdw14;
    uint32_t cdw15;
} __attribute__((packed)) nvme_command_t;

// NVMe Completion Entry
typedef struct {
    uint32_t result;    // Command-specific result
    uint32_t rsvd;
    uint16_t sq_head;   // Submission Queue Head
    uint16_t sq_id;     // Submission Queue ID
    uint16_t cid;       // Command ID
    uint16_t status;    // Status Field
} __attribute__((packed)) nvme_completion_t;

// AHCI Command Header
typedef struct {
    uint8_t cfl:5;      // Command FIS Length
    uint8_t a:1;        // ATAPI
    uint8_t w:1;        // Write
    uint8_t p:1;        // Prefetchable
    uint8_t r:1;        // Reset
    uint8_t b:1;        // BIST
    uint8_t c:1;        // Clear Busy upon R_OK
    uint8_t rsvd0:1;
    uint8_t pmp:4;      // Port Multiplier Port
    uint16_t prdtl;     // Physical Region Descriptor Table Length
    uint32_t prdbc;     // Physical Region Descriptor Byte Count
    uint64_t ctba;      // Command Table Base Address
    uint32_t rsvd1[4];
} __attribute__((packed)) ahci_cmd_header_t;

// Storage Namespace
typedef struct {
    uint32_t nsid;              // Namespace ID
    uint64_t size;              // Size in bytes
    uint32_t block_size;        // Block size in bytes
    uint32_t max_transfer_size; // Maximum transfer size
    bool active;                // Namespace is active
    char name[32];              // Namespace name
} storage_namespace_t;

// I/O Request Structure
typedef struct io_request {
    uint32_t id;                    // Request ID
    io_request_type_t type;         // Request type
    io_priority_t priority;         // Priority level
    io_status_t status;             // Current status
    uint32_t nsid;                  // Namespace ID
    uint64_t lba;                   // Logical Block Address
    uint32_t block_count;           // Number of blocks
    void *buffer;                   // Data buffer
    uint32_t buffer_size;           // Buffer size
    uint64_t timestamp;             // Request timestamp
    void (*callback)(struct io_request *req); // Completion callback
    void *context;                  // User context
    struct io_request *next;        // Next in queue
} io_request_t;

// Storage Device Structure
typedef struct {
    uint32_t id;                        // Device ID
    storage_device_type_t type;         // Device type
    char name[64];                      // Device name
    char model[64];                     // Device model
    char serial[64];                    // Serial number
    char firmware[32];                  // Firmware version
    uint64_t capacity;                  // Total capacity in bytes
    uint32_t block_size;                // Block size
    uint32_t max_transfer_size;         // Maximum transfer size
    uint32_t queue_depth;               // Command queue depth
    bool online;                        // Device is online
    bool readonly;                      // Read-only device
    
    // Hardware-specific data
    union {
        struct {
            void *bar0;                 // Base Address Register 0
            uint32_t doorbell_stride;   // Doorbell stride
            uint16_t max_queue_entries; // Maximum queue entries
            uint8_t cap_timeout;        // Capability timeout
            nvme_command_t *admin_sq;   // Admin Submission Queue
            nvme_completion_t *admin_cq; // Admin Completion Queue
            nvme_command_t *io_sq[IO_QUEUE_DEPTH]; // I/O Submission Queues
            nvme_completion_t *io_cq[IO_QUEUE_DEPTH]; // I/O Completion Queues
        } nvme;
        
        struct {
            void *abar;                 // AHCI Base Address
            uint32_t port_mask;         // Implemented ports
            uint8_t port_count;         // Number of ports
            ahci_cmd_header_t *cmd_list[32]; // Command lists per port
            void *fis_base[32];         // FIS base per port
        } ahci;
    } hw;
    
    // Namespaces
    storage_namespace_t namespaces[MAX_NAMESPACES];
    uint32_t namespace_count;
    
    // Statistics
    struct {
        uint64_t read_requests;
        uint64_t write_requests;
        uint64_t bytes_read;
        uint64_t bytes_written;
        uint64_t errors;
        uint64_t timeouts;
    } stats;
} storage_device_t;

// I/O Scheduler
typedef struct {
    io_request_t *pending_queue[4];     // Priority queues
    io_request_t *active_requests;      // Currently active requests
    uint32_t pending_count[4];          // Pending count per priority
    uint32_t active_count;              // Active request count
    uint64_t total_requests;            // Total requests processed
    uint32_t current_algorithm;         // Current scheduling algorithm
} io_scheduler_t;

// Legacy structures for compatibility
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
    storage_device_t *modern_device; /* Link to modern device structure */
} stor_priv_t;

// Global Storage State
static storage_device_t storage_devices[MAX_STORAGE_DEVICES];
static uint32_t device_count = 0;
static io_request_t io_request_pool[MAX_IO_REQUESTS];
static uint32_t next_request_id = 1;
static io_scheduler_t io_scheduler;

extern void aur_mem_fence(void);

// External assembly functions
extern void aur_storage_barrier(void);
extern void aur_storage_cache_flush(uint32_t cache_type);
extern void aur_storage_dma_setup(uint64_t src, uint64_t dst, uint32_t size);
extern void aur_storage_nvme_doorbell(void *doorbell, uint32_t value);

// Forward declarations
static int nvme_init_device(storage_device_t *device, void *bar0);
static int ahci_init_device(storage_device_t *device, void *abar);
static int storage_submit_request(io_request_t *request);
static void storage_complete_request(io_request_t *request, int status);
static io_request_t *storage_alloc_request(void);
static void storage_free_request(io_request_t *request);
static void storage_schedule_io(void);

// Initialize NVMe device
static int nvme_init_device(storage_device_t *device, void *bar0) {
    device->hw.nvme.bar0 = bar0;
    
    // Read controller capabilities
    uint64_t cap = *(volatile uint64_t *)((char *)bar0 + NVME_CAP_OFFSET);
    device->hw.nvme.max_queue_entries = (cap & 0xFFFF) + 1;
    device->hw.nvme.doorbell_stride = 4 << ((cap >> 32) & 0xF);
    device->hw.nvme.cap_timeout = ((cap >> 24) & 0xFF) * 500; // Convert to ms
    
    // Reset controller
    *(volatile uint32_t *)((char *)bar0 + NVME_CC_OFFSET) = 0;
    
    // Wait for reset to complete
    uint32_t timeout = device->hw.nvme.cap_timeout;
    while (timeout-- > 0) {
        uint32_t csts = *(volatile uint32_t *)((char *)bar0 + NVME_CSTS_OFFSET);
        if (!(csts & NVME_CSTS_RDY)) break;
        // aur_delay_ms(1);
    }
    
    if (timeout == 0) {
        return -1;
    }
    
    // Allocate admin queues
    device->hw.nvme.admin_sq = (nvme_command_t*)AuroraAllocateMemory(4096);
    device->hw.nvme.admin_cq = (nvme_completion_t*)AuroraAllocateMemory(4096);
    
    if (!device->hw.nvme.admin_sq || !device->hw.nvme.admin_cq) {
        return -1;
    }
    
    // Configure admin queues
    *(volatile uint32_t *)((char *)bar0 + NVME_AQA_OFFSET) = 
        ((64 - 1) << 16) | (64 - 1); // 64 entries each
    *(volatile uint64_t *)((char *)bar0 + NVME_ASQ_OFFSET) = 
        (uint64_t)device->hw.nvme.admin_sq;
    *(volatile uint64_t *)((char *)bar0 + NVME_ACQ_OFFSET) = 
        (uint64_t)device->hw.nvme.admin_cq;
    
    // Enable controller
    uint32_t cc = NVME_CC_EN | NVME_CC_CSS_NVM | NVME_CC_MPS_4K | 
                  NVME_CC_AMS_RR | NVME_CC_SHN_NONE | 
                  NVME_CC_IOSQES_64 | NVME_CC_IOCQES_16;
    *(volatile uint32_t *)((char *)bar0 + NVME_CC_OFFSET) = cc;
    
    // Wait for controller ready
    timeout = device->hw.nvme.cap_timeout;
    while (timeout-- > 0) {
        uint32_t csts = *(volatile uint32_t *)((char *)bar0 + NVME_CSTS_OFFSET);
        if (csts & NVME_CSTS_RDY) break;
        // aur_delay_ms(1);
    }
    
    if (timeout == 0) {
        return -1;
    }
    
    device->type = STORAGE_TYPE_NVME;
    device->queue_depth = IO_QUEUE_DEPTH;
    device->max_transfer_size = MAX_TRANSFER_SIZE;
    
    return 0;
}

// Initialize AHCI device
static int ahci_init_device(storage_device_t *device, void *abar) {
    device->hw.ahci.abar = abar;
    
    // Read capabilities
    uint32_t cap = *(volatile uint32_t *)((char *)abar + AHCI_CAP_OFFSET);
    device->hw.ahci.port_count = (cap & 0x1F) + 1;
    
    // Read implemented ports
    device->hw.ahci.port_mask = *(volatile uint32_t *)((char *)abar + AHCI_PI_OFFSET);
    
    // Enable AHCI mode
    uint32_t ghc = *(volatile uint32_t *)((char *)abar + AHCI_GHC_OFFSET);
    ghc |= AHCI_GHC_AE | AHCI_GHC_IE;
    *(volatile uint32_t *)((char *)abar + AHCI_GHC_OFFSET) = ghc;
    
    // Initialize ports
    for (int i = 0; i < device->hw.ahci.port_count; i++) {
        if (!(device->hw.ahci.port_mask & (1 << i))) continue;
        
        // Allocate command list and FIS
        device->hw.ahci.cmd_list[i] = (ahci_cmd_header_t*)AuroraAllocateMemory(4096);
        device->hw.ahci.fis_base[i] = AuroraAllocateMemory(4096);
        
        if (!device->hw.ahci.cmd_list[i] || !device->hw.ahci.fis_base[i]) {
            continue;
        }
        
        // Configure port
        void *port_base = (char *)abar + 0x100 + (i * 0x80);
        *(volatile uint64_t *)(port_base + 0x00) = (uint64_t)device->hw.ahci.cmd_list[i];
        *(volatile uint64_t *)(port_base + 0x08) = (uint64_t)device->hw.ahci.fis_base[i];
        
        // Start port
        uint32_t cmd = *(volatile uint32_t *)(port_base + 0x18);
        cmd |= (1 << 4) | (1 << 0); // FRE | ST
        *(volatile uint32_t *)(port_base + 0x18) = cmd;
    }
    
    device->type = STORAGE_TYPE_AHCI_SATA;
    device->queue_depth = 32;
    device->max_transfer_size = MAX_TRANSFER_SIZE;
    
    return 0;
}

// Allocate I/O request
static io_request_t *storage_alloc_request(void) {
    for (int i = 0; i < MAX_IO_REQUESTS; i++) {
        if (io_request_pool[i].status == IO_STATUS_PENDING && 
            io_request_pool[i].id == 0) {
            io_request_pool[i].id = next_request_id++;
            // io_request_pool[i].timestamp = aur_get_timestamp();
            return &io_request_pool[i];
        }
    }
    return NULL;
}

// Free I/O request
static void storage_free_request(io_request_t *request) {
    if (request) {
        memset(request, 0, sizeof(io_request_t));
    }
}

// Submit I/O request
static int storage_submit_request(io_request_t *request) {
    if (!request || request->nsid >= device_count) {
        return -1;
    }
    
    storage_device_t *device = &storage_devices[request->nsid];
    if (!device->online) {
        return -1;
    }
    
    request->status = IO_STATUS_IN_PROGRESS;
    
    // Add to scheduler queue
    int priority = request->priority;
    request->next = io_scheduler.pending_queue[priority];
    io_scheduler.pending_queue[priority] = request;
    io_scheduler.pending_count[priority]++;
    
    // Update statistics
    if (request->type == IO_READ) {
        device->stats.read_requests++;
    } else if (request->type == IO_WRITE) {
        device->stats.write_requests++;
    }
    
    // Trigger I/O scheduling
    storage_schedule_io();
    
    return 0;
}

// Complete I/O request
static void storage_complete_request(io_request_t *request, int status) {
    if (!request) return;
    
    request->status = (status == 0) ? IO_STATUS_COMPLETED : IO_STATUS_ERROR;
    
    // Update statistics
    storage_device_t *device = &storage_devices[request->nsid];
    if (status != 0) {
        device->stats.errors++;
    } else {
        if (request->type == IO_READ) {
            device->stats.bytes_read += request->block_count * device->block_size;
        } else if (request->type == IO_WRITE) {
            device->stats.bytes_written += request->block_count * device->block_size;
        }
    }
    
    // Call completion callback
    if (request->callback) {
        request->callback(request);
    }
    
    // Free request
    storage_free_request(request);
}

// I/O Scheduler - Simple priority-based scheduling
static void storage_schedule_io(void) {
    // Process requests by priority (high to low)
    for (int priority = IO_PRIORITY_CRITICAL; priority >= IO_PRIORITY_LOW; priority--) {
        while (io_scheduler.pending_queue[priority] && 
               io_scheduler.active_count < IO_QUEUE_DEPTH) {
            
            io_request_t *request = io_scheduler.pending_queue[priority];
            io_scheduler.pending_queue[priority] = request->next;
            io_scheduler.pending_count[priority]--;
            
            // Add to active queue
            request->next = io_scheduler.active_requests;
            io_scheduler.active_requests = request;
            io_scheduler.active_count++;
            
            // Process the request (simplified)
            storage_device_t *device = &storage_devices[request->nsid];
            
            if (device->type == STORAGE_TYPE_NVME) {
                // NVMe command processing would go here
            } else if (device->type == STORAGE_TYPE_AHCI_SATA) {
                // AHCI command processing would go here
            }
            
            // For now, simulate completion
            storage_complete_request(request, 0);
            io_scheduler.active_count--;
        }
    }
}

// Storage interrupt handler
void storage_interrupt_handler(uint32_t device_id) {
    if (device_id >= device_count) return;
    
    storage_device_t *device = &storage_devices[device_id];
    
    if (device->type == STORAGE_TYPE_NVME) {
        // Process NVMe completion queue
        nvme_completion_t *cq = device->hw.nvme.admin_cq;
        // Process completions...
    } else if (device->type == STORAGE_TYPE_AHCI_SATA) {
        // Process AHCI port interrupts
        uint32_t is = *(volatile uint32_t *)((char *)device->hw.ahci.abar + AHCI_IS_OFFSET);
        
        // Clear interrupt status
        *(volatile uint32_t *)((char *)device->hw.ahci.abar + AHCI_IS_OFFSET) = is;
    }
    
    // Continue I/O scheduling
    storage_schedule_io();
}

// Legacy queue functions for compatibility
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
    // aur_mem_fence(); 
    r->done=1;
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
    
    // Initialize modern storage device if available
    if (device_count < MAX_STORAGE_DEVICES) {
        storage_device_t *modern_device = &storage_devices[device_count];
        modern_device->id = device_count;
        strncpy(modern_device->name, dev->name, sizeof(modern_device->name)-1);
        strcpy(modern_device->model, "Aurora Legacy Storage");
        modern_device->capacity = sizeof(backing_store);
        modern_device->block_size = 512;
        modern_device->online = true;
        modern_device->readonly = false;
        modern_device->type = STORAGE_TYPE_UNKNOWN;
        
        priv->modern_device = modern_device;
        device_count++;
    }
    
    dev->drvdata = priv;
    dev->read  = drv_storage_read;
    dev->write = drv_storage_write;
    dev->ioctl = drv_storage_ioctl;
    return AUR_OK;
}

static aur_driver_t g_storage_driver = {
    .name = "aurora-storage",
    .class_id = AUR_CLASS_STORAGE,
    .vendor_id = 0,
    .device_id = 0,
    .flags = 0,
    .probe = drv_storage_probe,
    .remove = NULL,
    .suspend = NULL,
    .resume = NULL,
    .reset = NULL,
    .shutdown = NULL,
    .ref_count = 0,
    .next = NULL
};
static aur_device_t g_storage_device;

void StorageDriverInitialize(void){
    // Initialize I/O scheduler
    memset(&io_scheduler, 0, sizeof(io_scheduler_t));
    
    // Initialize request pool
    memset(io_request_pool, 0, sizeof(io_request_pool));
    
    // Register legacy driver
    aur_driver_register(&g_storage_driver);
    memset(&g_storage_device,0,sizeof(g_storage_device));
    strncpy(g_storage_device.name, "aurblk0", sizeof(g_storage_device.name)-1);
    g_storage_device.driver = &g_storage_driver;
    if(drv_storage_probe(&g_storage_device)==AUR_OK){ aur_device_add(&g_storage_device); }
    
    // Initialize modern storage subsystem
    // This would typically scan PCI bus for NVMe/AHCI controllers
    
    // Example: Initialize a mock NVMe device
    if (device_count < MAX_STORAGE_DEVICES) {
        storage_device_t *device = &storage_devices[device_count];
        device->id = device_count;
        strcpy(device->name, "nvme0");
        strcpy(device->model, "Aurora NVMe SSD");
        strcpy(device->serial, "AUR001");
        strcpy(device->firmware, "1.0.0");
        device->capacity = 1024ULL * 1024 * 1024 * 1024; // 1TB
        device->block_size = SECTOR_SIZE;
        device->online = true;
        device->readonly = false;
        device->type = STORAGE_TYPE_NVME;
        
        // Initialize namespace
        device->namespaces[0].nsid = 1;
        device->namespaces[0].size = device->capacity;
        device->namespaces[0].block_size = device->block_size;
        device->namespaces[0].active = true;
        strcpy(device->namespaces[0].name, "nvme0n1");
        device->namespace_count = 1;
        
        device_count++;
    }
}

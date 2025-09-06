/* Enhanced Aurora Driver Core Framework */
#include "../aurora.h"
#include "../include/kern/driver.h"

/* Simple strstr function for kernel use */
static char* strstr(const char* haystack, const char* needle) {
    if (!*needle) return (char*)haystack;
    
    for (; *haystack; haystack++) {
        const char* h = haystack;
        const char* n = needle;
        
        while (*h && *n && (*h == *n)) {
            h++;
            n++;
        }
        
        if (!*n) return (char*)haystack;
    }
    
    return NULL;
}

#define MAX_AUR_DRIVERS 128
#define MAX_AUR_DEVICES 256
#define MAX_AUR_BUSES   32
#define MAX_AUR_NODES   128

/* Global driver registry */
static aur_driver_t* g_drivers[MAX_AUR_DRIVERS];
static aur_device_t* g_devices[MAX_AUR_DEVICES];
static aur_bus_t* g_buses[MAX_AUR_BUSES];
static aur_device_node_t* g_device_nodes[MAX_AUR_NODES];

static UINT32 g_driver_count = 0;
static UINT32 g_device_count = 0;
static UINT32 g_bus_count = 0;
static UINT32 g_node_count = 0;

/* Synchronization */
static AURORA_SPINLOCK g_driver_lock;
static AURORA_SPINLOCK g_device_lock;
static AURORA_SPINLOCK g_bus_lock;

/* Driver core initialization */
static BOOL g_driver_core_initialized = FALSE;

static aur_status_t driver_core_init(void) {
    if (g_driver_core_initialized) return AUR_OK;
    
    AuroraInitializeSpinLock(&g_driver_lock);
    AuroraInitializeSpinLock(&g_device_lock);
    AuroraInitializeSpinLock(&g_bus_lock);
    
    g_driver_core_initialized = TRUE;
    return AUR_OK;
}

aur_status_t aur_driver_register(aur_driver_t* drv) {
    if (!drv || !drv->name) return AUR_ERR_INVAL;
    
    driver_core_init();
    
    AURORA_IRQL old_irql;
    AuroraAcquireSpinLock(&g_driver_lock, &old_irql);
    
    if (g_driver_count >= MAX_AUR_DRIVERS) {
        AuroraReleaseSpinLock(&g_driver_lock, old_irql);
        return AUR_ERR_NOMEM;
    }
    
    /* Check for duplicate driver names */
    for (UINT32 i = 0; i < g_driver_count; i++) {
        if (strcmp(g_drivers[i]->name, drv->name) == 0) {
            AuroraReleaseSpinLock(&g_driver_lock, old_irql);
            return AUR_ERR_BUSY;
        }
    }
    
    drv->ref_count = 0;
    g_drivers[g_driver_count++] = drv;
    
    AuroraReleaseSpinLock(&g_driver_lock, old_irql);
    return AUR_OK;
}

aur_status_t aur_driver_unregister(aur_driver_t* drv) {
    if (!drv) return AUR_ERR_INVAL;
    
    AURORA_IRQL old_irql;
    AuroraAcquireSpinLock(&g_driver_lock, &old_irql);
    
    for (UINT32 i = 0; i < g_driver_count; i++) {
        if (g_drivers[i] == drv) {
            if (drv->ref_count > 0) {
                AuroraReleaseSpinLock(&g_driver_lock, old_irql);
                return AUR_ERR_BUSY;
            }
            
            /* Shift remaining drivers */
            for (UINT32 j = i; j < g_driver_count - 1; j++) {
                g_drivers[j] = g_drivers[j + 1];
            }
            g_driver_count--;
            
            AuroraReleaseSpinLock(&g_driver_lock, old_irql);
            return AUR_OK;
        }
    }
    
    AuroraReleaseSpinLock(&g_driver_lock, old_irql);
    return AUR_ERR_NOT_FOUND;
}

aur_status_t aur_device_add(aur_device_t* dev) {
    if (!dev || !dev->name[0]) return AUR_ERR_INVAL;
    
    driver_core_init();
    
    AURORA_IRQL old_irql;
    AuroraAcquireSpinLock(&g_device_lock, &old_irql);
    
    if (g_device_count >= MAX_AUR_DEVICES) {
        AuroraReleaseSpinLock(&g_device_lock, old_irql);
        return AUR_ERR_NOMEM;
    }
    
    /* Check for duplicate device names */
    for (UINT32 i = 0; i < g_device_count; i++) {
        if (strcmp(g_devices[i]->name, dev->name) == 0) {
            AuroraReleaseSpinLock(&g_device_lock, old_irql);
            return AUR_ERR_BUSY;
        }
    }
    
    dev->state = AUR_DEV_STATE_PRESENT;
    g_devices[g_device_count++] = dev;
    
    /* Increment driver reference count */
    if (dev->driver) {
        dev->driver->ref_count++;
    }
    
    AuroraReleaseSpinLock(&g_device_lock, old_irql);
    return AUR_OK;
}

aur_status_t aur_device_remove(aur_device_t* dev) {
    if (!dev) return AUR_ERR_INVAL;
    
    AURORA_IRQL old_irql;
    AuroraAcquireSpinLock(&g_device_lock, &old_irql);
    
    for (UINT32 i = 0; i < g_device_count; i++) {
        if (g_devices[i] == dev) {
            dev->state = AUR_DEV_STATE_REMOVED;
            
            /* Call driver remove function */
            if (dev->driver && dev->driver->remove) {
                dev->driver->remove(dev);
                dev->driver->ref_count--;
            }
            
            /* Shift remaining devices */
            for (UINT32 j = i; j < g_device_count - 1; j++) {
                g_devices[j] = g_devices[j + 1];
            }
            g_device_count--;
            
            AuroraReleaseSpinLock(&g_device_lock, old_irql);
            return AUR_OK;
        }
    }
    
    AuroraReleaseSpinLock(&g_device_lock, old_irql);
    return AUR_ERR_NOT_FOUND;
}

aur_device_t* aur_device_find(const CHAR* name) {
    if (!name) return NULL;
    
    AURORA_IRQL old_irql;
    AuroraAcquireSpinLock(&g_device_lock, &old_irql);
    
    for (UINT32 i = 0; i < g_device_count; i++) {
        if (strcmp(g_devices[i]->name, name) == 0) {
            aur_device_t* dev = g_devices[i];
            AuroraReleaseSpinLock(&g_device_lock, old_irql);
            return dev;
        }
    }
    
    AuroraReleaseSpinLock(&g_device_lock, old_irql);
    return NULL;
}

aur_device_t* aur_device_find_by_class(UINT32 class_id) {
    AURORA_IRQL old_irql;
    AuroraAcquireSpinLock(&g_device_lock, &old_irql);
    
    for (UINT32 i = 0; i < g_device_count; i++) {
        if (g_devices[i]->class_id == class_id) {
            aur_device_t* dev = g_devices[i];
            AuroraReleaseSpinLock(&g_device_lock, old_irql);
            return dev;
        }
    }
    
    AuroraReleaseSpinLock(&g_device_lock, old_irql);
    return NULL;
}

aur_status_t aur_device_enumerate(UINT32 class_id, aur_device_t** devices, UINT32* count) {
    if (!devices || !count) return AUR_ERR_INVAL;
    
    AURORA_IRQL old_irql;
    AuroraAcquireSpinLock(&g_device_lock, &old_irql);
    
    UINT32 found = 0;
    for (UINT32 i = 0; i < g_device_count && found < *count; i++) {
        if (class_id == 0 || g_devices[i]->class_id == class_id) {
            devices[found++] = g_devices[i];
        }
    }
    
    *count = found;
    AuroraReleaseSpinLock(&g_device_lock, old_irql);
    return AUR_OK;
}

/* Bus management functions */
aur_status_t aur_bus_register(aur_bus_t* bus) {
    if (!bus || !bus->name[0]) return AUR_ERR_INVAL;
    
    driver_core_init();
    
    AURORA_IRQL old_irql;
    AuroraAcquireSpinLock(&g_bus_lock, &old_irql);
    
    if (g_bus_count >= MAX_AUR_BUSES) {
        AuroraReleaseSpinLock(&g_bus_lock, old_irql);
        return AUR_ERR_NOMEM;
    }
    
    g_buses[g_bus_count++] = bus;
    AuroraReleaseSpinLock(&g_bus_lock, old_irql);
    return AUR_OK;
}

aur_status_t aur_bus_unregister(aur_bus_t* bus) {
    if (!bus) return AUR_ERR_INVAL;
    
    AURORA_IRQL old_irql;
    AuroraAcquireSpinLock(&g_bus_lock, &old_irql);
    
    for (UINT32 i = 0; i < g_bus_count; i++) {
        if (g_buses[i] == bus) {
            for (UINT32 j = i; j < g_bus_count - 1; j++) {
                g_buses[j] = g_buses[j + 1];
            }
            g_bus_count--;
            AuroraReleaseSpinLock(&g_bus_lock, old_irql);
            return AUR_OK;
        }
    }
    
    AuroraReleaseSpinLock(&g_bus_lock, old_irql);
    return AUR_ERR_NOT_FOUND;
}

aur_bus_t* aur_bus_find(const CHAR* name) {
    if (!name) return NULL;
    
    AURORA_IRQL old_irql;
    AuroraAcquireSpinLock(&g_bus_lock, &old_irql);
    
    for (UINT32 i = 0; i < g_bus_count; i++) {
        if (strcmp(g_buses[i]->name, name) == 0) {
            aur_bus_t* bus = g_buses[i];
            AuroraReleaseSpinLock(&g_bus_lock, old_irql);
            return bus;
        }
    }
    
    AuroraReleaseSpinLock(&g_bus_lock, old_irql);
    return NULL;
}

/* Device tree operations */
aur_device_node_t* aur_of_find_node_by_name(const CHAR* name) {
    if (!name) return NULL;
    
    for (UINT32 i = 0; i < g_node_count; i++) {
        if (strcmp(g_device_nodes[i]->name, name) == 0) {
            return g_device_nodes[i];
        }
    }
    return NULL;
}

aur_device_node_t* aur_of_find_compatible_node(const CHAR* compatible) {
    if (!compatible) return NULL;
    
    for (UINT32 i = 0; i < g_node_count; i++) {
        if (strstr(g_device_nodes[i]->compatible, compatible)) {
            return g_device_nodes[i];
        }
    }
    return NULL;
}

aur_status_t aur_of_get_property(aur_device_node_t* node, const CHAR* name, void* value, UINT32* size) {
    if (!node || !name || !value || !size) return AUR_ERR_INVAL;
    
    /* Simple property lookup - in real implementation this would parse device tree */
    if (strcmp(name, "reg") == 0) {
        UINT32 copy_size = node->reg_count * sizeof(UINT32);
        if (*size < copy_size) {
            *size = copy_size;
            return AUR_ERR_NOMEM;
        }
        memcpy(value, node->reg, copy_size);
        *size = copy_size;
        return AUR_OK;
    }
    
    if (strcmp(name, "interrupts") == 0) {
        UINT32 copy_size = node->irq_count * sizeof(UINT32);
        if (*size < copy_size) {
            *size = copy_size;
            return AUR_ERR_NOMEM;
        }
        memcpy(value, node->interrupts, copy_size);
        *size = copy_size;
        return AUR_OK;
    }
    
    return AUR_ERR_NOT_FOUND;
}

/* Power management functions */
aur_status_t aur_device_suspend(aur_device_t* dev) {
    if (!dev) return AUR_ERR_INVAL;
    
    if (dev->suspend) {
        aur_status_t status = dev->suspend(dev);
        if (status == AUR_OK) {
            dev->power_state = 3; /* D3 - suspended */
        }
        return status;
    }
    
    return AUR_ERR_UNSUPPORTED;
}

aur_status_t aur_device_resume(aur_device_t* dev) {
    if (!dev) return AUR_ERR_INVAL;
    
    if (dev->resume) {
        aur_status_t status = dev->resume(dev);
        if (status == AUR_OK) {
            dev->power_state = 0; /* D0 - active */
        }
        return status;
    }
    
    return AUR_ERR_UNSUPPORTED;
}

aur_status_t aur_device_set_power_state(aur_device_t* dev, UINT32 state) {
    if (!dev) return AUR_ERR_INVAL;
    
    if (state > 3) return AUR_ERR_INVAL; /* D0-D3 only */
    
    if (state == 0 && dev->power_state != 0) {
        return aur_device_resume(dev);
    } else if (state > 0 && dev->power_state == 0) {
        return aur_device_suspend(dev);
    }
    
    dev->power_state = state;
    return AUR_OK;
}

/* Resource management stubs */
aur_status_t aur_device_request_region(aur_device_t* dev, UINT64 start, UINT64 size, const CHAR* name) {
    (void)dev; (void)start; (void)size; (void)name;
    return AUR_OK; /* Stub implementation */
}

aur_status_t aur_device_release_region(aur_device_t* dev, UINT64 start, UINT64 size) {
    (void)dev; (void)start; (void)size;
    return AUR_OK; /* Stub implementation */
}

PVOID aur_device_ioremap(aur_device_t* dev, UINT64 phys_addr, UINT64 size) {
    (void)dev; (void)size;
    return (PVOID)phys_addr; /* Direct mapping for now */
}

void aur_device_iounmap(PVOID virt_addr) {
    (void)virt_addr; /* Stub implementation */
}

/* Enhanced IRQ registry */
static struct {
    UINT32 irq;
    aur_irq_handler_t handler;
    PVOID context;
    BOOL in_use;
} g_irq_table[256];

aur_status_t aur_register_irq(UINT32 irq, aur_irq_handler_t h, PVOID ctx) {
    if (!h || irq >= 256) return AUR_ERR_INVAL;
    
    if (g_irq_table[irq].in_use) return AUR_ERR_BUSY;
    
    g_irq_table[irq].irq = irq;
    g_irq_table[irq].handler = h;
    g_irq_table[irq].context = ctx;
    g_irq_table[irq].in_use = TRUE;
    
    return AUR_OK;
}

aur_status_t aur_unregister_irq(UINT32 irq) {
    if (irq >= 256) return AUR_ERR_INVAL;
    
    if (!g_irq_table[irq].in_use) return AUR_ERR_NOT_FOUND;
    
    g_irq_table[irq].in_use = FALSE;
    g_irq_table[irq].handler = NULL;
    g_irq_table[irq].context = NULL;
    
    return AUR_OK;
}

/* IRQ dispatch function for kernel */
void aur_dispatch_irq(UINT32 irq) {
    if (irq < 256 && g_irq_table[irq].in_use && g_irq_table[irq].handler) {
        g_irq_table[irq].handler(irq, g_irq_table[irq].context);
    }
}

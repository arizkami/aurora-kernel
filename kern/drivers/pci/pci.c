/* Aurora PCI/PCIe Bus Driver */
#include "../../../aurora.h"
#include "../../../include/kern/driver.h"

/* PCI Configuration Space Registers */
#define PCI_VENDOR_ID           0x00
#define PCI_DEVICE_ID           0x02
#define PCI_COMMAND             0x04
#define PCI_STATUS              0x06
#define PCI_REVISION_ID         0x08
#define PCI_CLASS_PROG          0x09
#define PCI_CLASS_DEVICE        0x0A
#define PCI_CLASS_CODE          0x0B
#define PCI_CACHE_LINE_SIZE     0x0C
#define PCI_LATENCY_TIMER       0x0D
#define PCI_HEADER_TYPE         0x0E
#define PCI_BIST                0x0F
#define PCI_BAR0                0x10
#define PCI_BAR1                0x14
#define PCI_BAR2                0x18
#define PCI_BAR3                0x1C
#define PCI_BAR4                0x20
#define PCI_BAR5                0x24
#define PCI_CARDBUS_CIS         0x28
#define PCI_SUBSYSTEM_VENDOR_ID 0x2C
#define PCI_SUBSYSTEM_ID        0x2E
#define PCI_ROM_ADDRESS         0x30
#define PCI_CAPABILITIES_PTR    0x34
#define PCI_INTERRUPT_LINE      0x3C
#define PCI_INTERRUPT_PIN       0x3D
#define PCI_MIN_GNT             0x3E
#define PCI_MAX_LAT             0x3F

/* PCI Command Register Bits */
#define PCI_COMMAND_IO          0x0001
#define PCI_COMMAND_MEMORY      0x0002
#define PCI_COMMAND_MASTER      0x0004
#define PCI_COMMAND_SPECIAL     0x0008
#define PCI_COMMAND_INVALIDATE  0x0010
#define PCI_COMMAND_VGA_PALETTE 0x0020
#define PCI_COMMAND_PARITY      0x0040
#define PCI_COMMAND_WAIT        0x0080
#define PCI_COMMAND_SERR        0x0100
#define PCI_COMMAND_FAST_BACK   0x0200
#define PCI_COMMAND_INTX_DISABLE 0x0400

/* PCI Status Register Bits */
#define PCI_STATUS_INTERRUPT    0x0008
#define PCI_STATUS_CAP_LIST     0x0010
#define PCI_STATUS_66MHZ        0x0020
#define PCI_STATUS_UDF          0x0040
#define PCI_STATUS_FAST_BACK    0x0080
#define PCI_STATUS_PARITY       0x0100
#define PCI_STATUS_DEVSEL_MASK  0x0600
#define PCI_STATUS_SIG_TARGET_ABORT 0x0800
#define PCI_STATUS_REC_TARGET_ABORT 0x1000
#define PCI_STATUS_REC_MASTER_ABORT 0x2000
#define PCI_STATUS_SIG_SYSTEM_ERROR 0x4000
#define PCI_STATUS_DETECTED_PARITY  0x8000

/* PCI Capability IDs */
#define PCI_CAP_ID_PM           0x01
#define PCI_CAP_ID_AGP          0x02
#define PCI_CAP_ID_VPD          0x03
#define PCI_CAP_ID_SLOTID       0x04
#define PCI_CAP_ID_MSI          0x05
#define PCI_CAP_ID_CHSWP        0x06
#define PCI_CAP_ID_PCIX         0x07
#define PCI_CAP_ID_HT           0x08
#define PCI_CAP_ID_VNDR         0x09
#define PCI_CAP_ID_DBG          0x0A
#define PCI_CAP_ID_CCRC         0x0B
#define PCI_CAP_ID_SHPC         0x0C
#define PCI_CAP_ID_SSVID        0x0D
#define PCI_CAP_ID_AGP3         0x0E
#define PCI_CAP_ID_SECDEV       0x0F
#define PCI_CAP_ID_EXP          0x10
#define PCI_CAP_ID_MSIX         0x11
#define PCI_CAP_ID_SATA         0x12
#define PCI_CAP_ID_AF           0x13

/* MSI Capability Structure */
#define PCI_MSI_FLAGS           0x02
#define PCI_MSI_ADDRESS_LO      0x04
#define PCI_MSI_ADDRESS_HI      0x08
#define PCI_MSI_DATA_32         0x08
#define PCI_MSI_DATA_64         0x0C
#define PCI_MSI_MASK_64         0x10
#define PCI_MSI_PENDING_64      0x14

#define PCI_MSI_FLAGS_ENABLE    0x0001
#define PCI_MSI_FLAGS_QMASK     0x000E
#define PCI_MSI_FLAGS_QSIZE     0x0070
#define PCI_MSI_FLAGS_64BIT     0x0080
#define PCI_MSI_FLAGS_MASKBIT   0x0100

/* PCI Device Structure */
typedef struct _pci_device {
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
    UINT32 msix_enabled;
    UINT8 capabilities[256];
    struct _pci_device* next;
} pci_device_t;

/* PCI Bus Private Data */
typedef struct _pci_bus_priv {
    AURORA_SPINLOCK lock;
    pci_device_t* devices;
    UINT32 device_count;
    UINT64 config_base;  /* ECAM base address */
    UINT32 bus_start;
    UINT32 bus_end;
} pci_bus_priv_t;

static pci_bus_priv_t g_pci_bus;

/* PCI Configuration Space Access */
static UINT32 pci_config_read32(UINT32 bus, UINT32 device, UINT32 function, UINT32 offset) {
    UINT32 address = 0x80000000 | (bus << 16) | (device << 11) | (function << 8) | (offset & 0xFC);
    
    /* Use I/O ports for legacy PCI access */
    // __asm__ volatile ("outl %0, $0xCF8" : : "a"(address));
    // 
    // UINT32 value;
    // __asm__ volatile ("inl $0xCFC, %0" : "=a"(value));
    // return value;
    return 0; // Placeholder
}

static void pci_config_write32(UINT32 bus, UINT32 device, UINT32 function, UINT32 offset, UINT32 value) {
    UINT32 address = 0x80000000 | (bus << 16) | (device << 11) | (function << 8) | (offset & 0xFC);
    
    // __asm__ volatile ("outl %0, $0xCF8" : : "a"(address));
    // __asm__ volatile ("outl %0, $0xCFC" : : "a"(value));
}

static UINT16 pci_config_read16(UINT32 bus, UINT32 device, UINT32 function, UINT32 offset) {
    UINT32 value = pci_config_read32(bus, device, function, offset & ~3);
    return (UINT16)(value >> ((offset & 3) * 8));
}

static void pci_config_write16(UINT32 bus, UINT32 device, UINT32 function, UINT32 offset, UINT16 value) {
    UINT32 old_value = pci_config_read32(bus, device, function, offset & ~3);
    UINT32 shift = (offset & 3) * 8;
    UINT32 mask = 0xFFFF << shift;
    UINT32 new_value = (old_value & ~mask) | ((UINT32)value << shift);
    pci_config_write32(bus, device, function, offset & ~3, new_value);
}

static UINT8 pci_config_read8(UINT32 bus, UINT32 device, UINT32 function, UINT32 offset) {
    UINT32 value = pci_config_read32(bus, device, function, offset & ~3);
    return (UINT8)(value >> ((offset & 3) * 8));
}

static void pci_config_write8(UINT32 bus, UINT32 device, UINT32 function, UINT32 offset, UINT8 value) {
    UINT32 old_value = pci_config_read32(bus, device, function, offset & ~3);
    UINT32 shift = (offset & 3) * 8;
    UINT32 mask = 0xFF << shift;
    UINT32 new_value = (old_value & ~mask) | ((UINT32)value << shift);
    pci_config_write32(bus, device, function, offset & ~3, new_value);
}

/* PCI Capability Functions */
static UINT8 pci_find_capability(pci_device_t* pci_dev, UINT8 cap_id) {
    UINT16 status = pci_config_read16(pci_dev->bus, pci_dev->device, pci_dev->function, PCI_STATUS);
    if (!(status & PCI_STATUS_CAP_LIST)) {
        return 0;
    }
    
    UINT8 pos = pci_config_read8(pci_dev->bus, pci_dev->device, pci_dev->function, PCI_CAPABILITIES_PTR);
    
    while (pos) {
        UINT8 id = pci_config_read8(pci_dev->bus, pci_dev->device, pci_dev->function, pos);
        if (id == cap_id) {
            return pos;
        }
        pos = pci_config_read8(pci_dev->bus, pci_dev->device, pci_dev->function, pos + 1);
    }
    
    return 0;
}

/* MSI Support */
static aur_status_t pci_enable_msi(pci_device_t* pci_dev) {
    UINT8 msi_pos = pci_find_capability(pci_dev, PCI_CAP_ID_MSI);
    if (!msi_pos) {
        return AUR_ERR_UNSUPPORTED;
    }
    
    UINT16 msi_flags = pci_config_read16(pci_dev->bus, pci_dev->device, pci_dev->function, msi_pos + PCI_MSI_FLAGS);
    
    /* Configure MSI address and data */
    UINT32 msi_addr = 0xFEE00000; /* Local APIC base */
    UINT16 msi_data = 0x4000 | pci_dev->irq; /* Edge triggered, vector */
    
    pci_config_write32(pci_dev->bus, pci_dev->device, pci_dev->function, msi_pos + PCI_MSI_ADDRESS_LO, msi_addr);
    
    if (msi_flags & PCI_MSI_FLAGS_64BIT) {
        pci_config_write32(pci_dev->bus, pci_dev->device, pci_dev->function, msi_pos + PCI_MSI_ADDRESS_HI, 0);
        pci_config_write16(pci_dev->bus, pci_dev->device, pci_dev->function, msi_pos + PCI_MSI_DATA_64, msi_data);
    } else {
        pci_config_write16(pci_dev->bus, pci_dev->device, pci_dev->function, msi_pos + PCI_MSI_DATA_32, msi_data);
    }
    
    /* Enable MSI */
    msi_flags |= PCI_MSI_FLAGS_ENABLE;
    pci_config_write16(pci_dev->bus, pci_dev->device, pci_dev->function, msi_pos + PCI_MSI_FLAGS, msi_flags);
    
    pci_dev->msi_enabled = 1;
    return AUR_OK;
}

static aur_status_t pci_disable_msi(pci_device_t* pci_dev) {
    UINT8 msi_pos = pci_find_capability(pci_dev, PCI_CAP_ID_MSI);
    if (!msi_pos) {
        return AUR_ERR_UNSUPPORTED;
    }
    
    UINT16 msi_flags = pci_config_read16(pci_dev->bus, pci_dev->device, pci_dev->function, msi_pos + PCI_MSI_FLAGS);
    msi_flags &= ~PCI_MSI_FLAGS_ENABLE;
    pci_config_write16(pci_dev->bus, pci_dev->device, pci_dev->function, msi_pos + PCI_MSI_FLAGS, msi_flags);
    
    pci_dev->msi_enabled = 0;
    return AUR_OK;
}

/* PCI Device Enumeration */
static pci_device_t* pci_create_device(UINT32 bus, UINT32 device, UINT32 function) {
    pci_device_t* pci_dev = (pci_device_t*)AuroraAllocateMemory(sizeof(pci_device_t));
    if (!pci_dev) return NULL;
    
    memset(pci_dev, 0, sizeof(pci_device_t));
    
    pci_dev->bus = bus;
    pci_dev->device = device;
    pci_dev->function = function;
    pci_dev->vendor_id = pci_config_read16(bus, device, function, PCI_VENDOR_ID);
    pci_dev->device_id = pci_config_read16(bus, device, function, PCI_DEVICE_ID);
    pci_dev->class_code = pci_config_read32(bus, device, function, PCI_CLASS_CODE) >> 8;
    pci_dev->revision = pci_config_read8(bus, device, function, PCI_REVISION_ID);
    pci_dev->irq = pci_config_read8(bus, device, function, PCI_INTERRUPT_LINE);
    
    /* Read BARs */
    for (int i = 0; i < 6; i++) {
        UINT32 bar_offset = PCI_BAR0 + (i * 4);
        UINT32 bar_value = pci_config_read32(bus, device, function, bar_offset);
        
        if (bar_value) {
            /* Write all 1s to determine size */
            pci_config_write32(bus, device, function, bar_offset, 0xFFFFFFFF);
            UINT32 size_mask = pci_config_read32(bus, device, function, bar_offset);
            pci_config_write32(bus, device, function, bar_offset, bar_value);
            
            if (bar_value & 1) {
                /* I/O BAR */
                pci_dev->bar[i] = bar_value & 0xFFFFFFFC;
            } else {
                /* Memory BAR */
                if ((bar_value & 6) == 4) {
                    /* 64-bit BAR */
                    UINT32 bar_high = pci_config_read32(bus, device, function, bar_offset + 4);
                    pci_dev->bar[i] = ((UINT64)bar_high << 32) | (bar_value & 0xFFFFFFF0);
                    i++; /* Skip next BAR */
                } else {
                    /* 32-bit BAR */
                    pci_dev->bar[i] = bar_value & 0xFFFFFFF0;
                }
            }
        }
    }
    
    return pci_dev;
}

static void pci_scan_bus(UINT32 bus) {
    for (UINT32 device = 0; device < 32; device++) {
        UINT16 vendor_id = pci_config_read16(bus, device, 0, PCI_VENDOR_ID);
        if (vendor_id == 0xFFFF) continue;
        
        pci_device_t* pci_dev = pci_create_device(bus, device, 0);
        if (pci_dev) {
            AURORA_IRQL old_irql;
            AuroraAcquireSpinLock(&g_pci_bus.lock, &old_irql);
            
            pci_dev->next = g_pci_bus.devices;
            g_pci_bus.devices = pci_dev;
            g_pci_bus.device_count++;
            
            AuroraReleaseSpinLock(&g_pci_bus.lock, old_irql);
        }
        
        /* Check for multi-function device */
        UINT8 header_type = pci_config_read8(bus, device, 0, PCI_HEADER_TYPE);
        if (header_type & 0x80) {
            for (UINT32 function = 1; function < 8; function++) {
                vendor_id = pci_config_read16(bus, device, function, PCI_VENDOR_ID);
                if (vendor_id == 0xFFFF) continue;
                
                pci_dev = pci_create_device(bus, device, function);
                if (pci_dev) {
                    AURORA_IRQL old_irql;
                    AuroraAcquireSpinLock(&g_pci_bus.lock, &old_irql);
                    
                    pci_dev->next = g_pci_bus.devices;
                    g_pci_bus.devices = pci_dev;
                    g_pci_bus.device_count++;
                    
                    AuroraReleaseSpinLock(&g_pci_bus.lock, old_irql);
                }
            }
        }
    }
}

/* PCI Bus Driver Operations */
static aur_status_t pci_bus_match(aur_device_t* dev, aur_driver_t* drv) {
    if (!dev || !drv) return AUR_ERR_INVAL;
    
    /* Match by class, vendor, or device ID */
    if (drv->class_id != 0 && dev->class_id == drv->class_id) return AUR_OK;
    if (drv->vendor_id != 0 && dev->vendor_id == drv->vendor_id) {
        if (drv->device_id == 0 || dev->device_id == drv->device_id) return AUR_OK;
    }
    
    return AUR_ERR_UNSUPPORTED;
}

static aur_status_t pci_bus_probe(aur_device_t* dev) {
    if (!dev || !dev->driver || !dev->driver->probe) return AUR_ERR_INVAL;
    
    return dev->driver->probe(dev);
}

static void pci_bus_remove(aur_device_t* dev) {
    if (dev && dev->driver && dev->driver->remove) {
        dev->driver->remove(dev);
    }
}

/* PCI Device IOCTL Handler */
static aur_status_t pci_device_ioctl(aur_device_t* dev, UINT32 code, void* inout) {
    if (!dev || !dev->drvdata) return AUR_ERR_INVAL;
    
    pci_device_t* pci_dev = (pci_device_t*)dev->drvdata;
    
    switch (code) {
        case AUR_IOCTL_GET_PCI_INFO: {
            aur_pci_info_t* info = (aur_pci_info_t*)inout;
            if (!info) return AUR_ERR_INVAL;
            
            info->bus = pci_dev->bus;
            info->device = pci_dev->device;
            info->function = pci_dev->function;
            info->vendor_id = pci_dev->vendor_id;
            info->device_id = pci_dev->device_id;
            info->class_code = pci_dev->class_code;
            info->revision = pci_dev->revision;
            memcpy(info->bar, pci_dev->bar, sizeof(info->bar));
            info->irq = pci_dev->irq;
            info->msi_enabled = pci_dev->msi_enabled;
            return AUR_OK;
        }
        
        case AUR_IOCTL_READ_PCI_CONFIG: {
            UINT32* config_data = (UINT32*)inout;
            if (!config_data) return AUR_ERR_INVAL;
            
            UINT32 offset = config_data[0];
            config_data[1] = pci_config_read32(pci_dev->bus, pci_dev->device, pci_dev->function, offset);
            return AUR_OK;
        }
        
        case AUR_IOCTL_WRITE_PCI_CONFIG: {
            UINT32* config_data = (UINT32*)inout;
            if (!config_data) return AUR_ERR_INVAL;
            
            UINT32 offset = config_data[0];
            UINT32 value = config_data[1];
            pci_config_write32(pci_dev->bus, pci_dev->device, pci_dev->function, offset, value);
            return AUR_OK;
        }
        
        case AUR_IOCTL_ENABLE_MSI:
            return pci_enable_msi(pci_dev);
            
        case AUR_IOCTL_DISABLE_MSI:
            return pci_disable_msi(pci_dev);
            
        default:
            return AUR_ERR_UNSUPPORTED;
    }
}

/* PCI Bus Structure */
static aur_bus_t g_pci_bus_driver = {
    .name = "pci",
    .type = AUR_CLASS_PCI,
    .match = pci_bus_match,
    .probe = pci_bus_probe,
    .remove = pci_bus_remove,
    .next = NULL
};

/* PCI Bus Initialization */
void PCIDriverInitialize(void) {
    /* Initialize PCI bus private data */
    memset(&g_pci_bus, 0, sizeof(g_pci_bus));
    AuroraInitializeSpinLock(&g_pci_bus.lock);
    
    /* Register PCI bus */
    aur_bus_register(&g_pci_bus_driver);
    
    /* Scan all PCI buses */
    for (UINT32 bus = 0; bus < 256; bus++) {
        pci_scan_bus(bus);
    }
    
    /* Create Aurora devices for each PCI device */
    pci_device_t* pci_dev = g_pci_bus.devices;
    UINT32 device_index = 0;
    
    while (pci_dev) {
        aur_device_t* aur_dev = (aur_device_t*)AuroraAllocateMemory(sizeof(aur_device_t));
        if (aur_dev) {
            memset(aur_dev, 0, sizeof(aur_device_t));
            
            strcpy(aur_dev->name, "pci_device");
            aur_dev->class_id = AUR_CLASS_PCI;
            aur_dev->vendor_id = pci_dev->vendor_id;
            aur_dev->device_id = pci_dev->device_id;
            aur_dev->state = AUR_DEV_STATE_PRESENT;
            aur_dev->drvdata = pci_dev;
            aur_dev->ioctl = pci_device_ioctl;
            aur_dev->bus = &g_pci_bus_driver;
            aur_dev->irq_line = pci_dev->irq;
            
            /* Copy BARs to device */
            for (int i = 0; i < 6; i++) {
                aur_dev->base_addr[i] = pci_dev->bar[i];
            }
            
            aur_device_add(aur_dev);
        }
        
        pci_dev = pci_dev->next;
    }
}

/* PCI Utility Functions */
pci_device_t* pci_find_device(UINT16 vendor_id, UINT16 device_id) {
    AURORA_IRQL old_irql;
    AuroraAcquireSpinLock(&g_pci_bus.lock, &old_irql);
    
    pci_device_t* pci_dev = g_pci_bus.devices;
    while (pci_dev) {
        if (pci_dev->vendor_id == vendor_id && pci_dev->device_id == device_id) {
            AuroraReleaseSpinLock(&g_pci_bus.lock, old_irql);
            return pci_dev;
        }
        pci_dev = pci_dev->next;
    }
    
    AuroraReleaseSpinLock(&g_pci_bus.lock, old_irql);
    return NULL;
}

pci_device_t* pci_find_class(UINT32 class_code) {
    AURORA_IRQL old_irql;
    AuroraAcquireSpinLock(&g_pci_bus.lock, &old_irql);
    
    pci_device_t* pci_dev = g_pci_bus.devices;
    while (pci_dev) {
        if ((pci_dev->class_code & 0xFFFF00) == (class_code & 0xFFFF00)) {
            AuroraReleaseSpinLock(&g_pci_bus.lock, old_irql);
            return pci_dev;
        }
        pci_dev = pci_dev->next;
    }
    
    AuroraReleaseSpinLock(&g_pci_bus.lock, old_irql);
    return NULL;
}
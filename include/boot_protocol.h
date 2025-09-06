// Aurora Boot Protocol - Interface between bootloaders and kernel
// Defines structures and constants for passing boot information to kernel

#ifndef _BOOT_PROTOCOL_H_
#define _BOOT_PROTOCOL_H_

#include <stdint.h>

#define AURORA_BOOT_MAGIC 0x41555241  // 'AURA'
#define AURORA_BOOT_VERSION 1

// Memory map entry types
#define AURORA_MEMORY_AVAILABLE     1
#define AURORA_MEMORY_RESERVED      2
#define AURORA_MEMORY_ACPI_RECLAIM  3
#define AURORA_MEMORY_ACPI_NVS      4
#define AURORA_MEMORY_BAD           5
#define AURORA_MEMORY_BOOTLOADER    6
#define AURORA_MEMORY_KERNEL        7

// Boot information flags
#define AURORA_BOOT_FLAG_EFI        0x01
#define AURORA_BOOT_FLAG_LEGACY     0x02
#define AURORA_BOOT_FLAG_ACPI       0x04
#define AURORA_BOOT_FLAG_GRAPHICS   0x08

typedef struct {
    uint64_t base_address;
    uint64_t length;
    uint32_t type;
    uint32_t attributes;
} aurora_memory_descriptor_t;

typedef struct {
    uint32_t horizontal_resolution;
    uint32_t vertical_resolution;
    uint32_t pixels_per_scan_line;
    uint64_t framebuffer_base;
    uint64_t framebuffer_size;
    uint32_t pixel_format;  // 0=RGB, 1=BGR, 2=Bitmask, 3=BltOnly
    uint32_t reserved;
} aurora_graphics_info_t;

typedef struct {
    uint32_t magic;                    // AURORA_BOOT_MAGIC
    uint32_t version;                  // AURORA_BOOT_VERSION
    uint32_t flags;                    // Boot flags
    uint32_t memory_map_size;          // Size of memory map in bytes
    uint64_t memory_map_address;       // Physical address of memory map
    uint32_t memory_descriptor_size;   // Size of each memory descriptor
    uint64_t kernel_physical_base;     // Physical address where kernel is loaded
    uint64_t kernel_virtual_base;      // Virtual address kernel expects to run at
    uint64_t kernel_size;              // Size of kernel image
    uint64_t initrd_physical_base;     // Physical address of initial ramdisk (if any)
    uint64_t initrd_size;              // Size of initial ramdisk
    aurora_graphics_info_t graphics;   // Graphics information
    uint64_t acpi_rsdp_address;        // ACPI RSDP address (if available)
    uint64_t efi_system_table;         // EFI System Table address (EFI boot only)
    char cmdline[256];                 // Kernel command line
} aurora_boot_info_t;

// Function prototypes for bootloader implementations
void aurora_boot_prepare_memory_map(aurora_boot_info_t* boot_info);
void aurora_boot_setup_graphics(aurora_boot_info_t* boot_info);
void aurora_boot_transfer_control(aurora_boot_info_t* boot_info, void* kernel_entry);

#endif // _BOOT_PROTOCOL_H_
// Aurora Legacy Bootloader (x86_64)
// Complete bootloader with real mode to long mode transition
// Loads kernel from disk and transfers control with boot information

#include "../../include/boot_protocol.h"
#include <stdint.h>
#include <stddef.h>

#define KERNEL_LOAD_ADDRESS 0x100000  // 1MB
#define BOOT_INFO_ADDRESS   0x90000   // 576KB
#define STACK_ADDRESS       0x80000   // 512KB
#define GDT_ADDRESS         0x70000   // 448KB
#define PAGE_TABLE_ADDRESS  0x60000   // 384KB
#define STRINGIFY(x) #x

// Memory map entry for BIOS E820
typedef struct {
    uint64_t base;
    uint64_t length;
    uint32_t type;
    uint32_t acpi_attributes;
} __attribute__((packed)) e820_entry_t;

// Global variables
static aurora_boot_info_t* g_boot_info = (aurora_boot_info_t*)BOOT_INFO_ADDRESS;
static e820_entry_t* g_memory_map = (e820_entry_t*)(BOOT_INFO_ADDRESS + sizeof(aurora_boot_info_t));
static uint32_t g_memory_map_entries = 0;

// Assembly functions (implemented below)
extern void setup_gdt(void);
extern void enable_a20(void);
extern void setup_paging(void);
extern void switch_to_long_mode(void);
extern uint32_t detect_memory(e820_entry_t* map, uint32_t max_entries);
extern uint32_t load_kernel_from_disk(void* buffer, uint32_t lba_start, uint32_t sectors);
extern void print_string(const char* str);
extern void print_hex(uint32_t value);

// Convert E820 memory map to Aurora format
static void convert_memory_map(void) {
    aurora_memory_descriptor_t* aurora_map = (aurora_memory_descriptor_t*)g_memory_map;
    
    for (uint32_t i = 0; i < g_memory_map_entries; i++) {
        e820_entry_t* e820 = &g_memory_map[i];
        aurora_memory_descriptor_t* aurora = &aurora_map[i];
        
        aurora->base_address = e820->base;
        aurora->length = e820->length;
        aurora->attributes = e820->acpi_attributes;
        
        // Convert E820 types to Aurora types
        switch (e820->type) {
            case 1: // Available
                aurora->type = AURORA_MEMORY_AVAILABLE;
                break;
            case 2: // Reserved
                aurora->type = AURORA_MEMORY_RESERVED;
                break;
            case 3: // ACPI Reclaimable
                aurora->type = AURORA_MEMORY_ACPI_RECLAIM;
                break;
            case 4: // ACPI NVS
                aurora->type = AURORA_MEMORY_ACPI_NVS;
                break;
            case 5: // Bad memory
                aurora->type = AURORA_MEMORY_BAD;
                break;
            default:
                aurora->type = AURORA_MEMORY_RESERVED;
                break;
        }
    }
    
    g_boot_info->memory_map_address = (uint64_t)(uintptr_t)aurora_map;
    g_boot_info->memory_map_size = g_memory_map_entries * sizeof(aurora_memory_descriptor_t);
    g_boot_info->memory_descriptor_size = sizeof(aurora_memory_descriptor_t);
}

// Main C entry point (called from assembly)
void legacy_main(void) {
    print_string("Aurora Legacy Bootloader starting...\r\n");
    
    // Initialize boot info structure
    g_boot_info->magic = AURORA_BOOT_MAGIC;
    g_boot_info->version = AURORA_BOOT_VERSION;
    g_boot_info->flags = AURORA_BOOT_FLAG_LEGACY;
    g_boot_info->efi_system_table = 0;
    
    // Enable A20 line
    print_string("Enabling A20 line...\r\n");
    enable_a20();
    
    // Detect memory using BIOS E820
    print_string("Detecting memory...\r\n");
    g_memory_map_entries = detect_memory(g_memory_map, 64);
    if (g_memory_map_entries == 0) {
        print_string("ERROR: Memory detection failed\r\n");
        goto halt;
    }
    print_string("Memory entries detected: ");
    print_hex(g_memory_map_entries);
    print_string("\r\n");
    
    // Convert memory map to Aurora format
    convert_memory_map();
    
    // Load kernel from disk (assuming it starts at LBA 1)
    print_string("Loading kernel from disk...\r\n");
    uint32_t kernel_sectors = load_kernel_from_disk((void*)KERNEL_LOAD_ADDRESS, 1, 256);
    if (kernel_sectors == 0) {
        print_string("ERROR: Kernel loading failed\r\n");
        goto halt;
    }
    
    // Validate kernel PE header
    uint8_t* kernel_base = (uint8_t*)KERNEL_LOAD_ADDRESS;
    if (*(uint16_t*)kernel_base != 0x5A4D) { // MZ signature
        print_string("ERROR: Invalid kernel format (no MZ signature)\r\n");
        goto halt;
    }
    
    uint32_t pe_offset = *(uint32_t*)(kernel_base + 0x3C);
    if (*(uint32_t*)(kernel_base + pe_offset) != 0x00004550) { // PE signature
        print_string("ERROR: Invalid kernel format (no PE signature)\r\n");
        goto halt;
    }
    
    // Get kernel information
    uint32_t entry_rva = *(uint32_t*)(kernel_base + pe_offset + 0x28);
    uint64_t image_base = *(uint64_t*)(kernel_base + pe_offset + 0x30);
    uint32_t image_size = *(uint32_t*)(kernel_base + pe_offset + 0x50);
    
    // Update boot info with kernel information
    g_boot_info->kernel_physical_base = KERNEL_LOAD_ADDRESS;
    g_boot_info->kernel_virtual_base = image_base;
    g_boot_info->kernel_size = image_size;
    
    print_string("Kernel loaded at: ");
    print_hex(KERNEL_LOAD_ADDRESS);
    print_string("\r\n");
    print_string("Entry RVA: ");
    print_hex(entry_rva);
    print_string("\r\n");
    
    // Setup GDT for long mode
    print_string("Setting up GDT...\r\n");
    setup_gdt();
    
    // Setup paging for long mode
    print_string("Setting up paging...\r\n");
    setup_paging();
    
    // Switch to long mode and jump to kernel
    print_string("Switching to long mode...\r\n");
    switch_to_long_mode();
    
    // Calculate kernel entry point
    void (*kernel_entry)(aurora_boot_info_t*) = (void(*)(aurora_boot_info_t*))(KERNEL_LOAD_ADDRESS + entry_rva);
    
    // Transfer control to kernel
    kernel_entry(g_boot_info);
    
halt:
    print_string("Boot failed. Halting.\r\n");
    for(;;) {
        __asm__("hlt");
    }
}

// Assembly code section
__asm__(
    ".code16\n"
    ".section .text\n"
    ".global _start\n"
    "_start:\n"
    "    cli\n"                    // Clear interrupts
    "    xor %ax, %ax\n"         // Zero AX
    "    mov %ax, %ds\n"         // Set DS = 0
    "    mov %ax, %es\n"         // Set ES = 0
    "    mov %ax, %ss\n"         // Set SS = 0
    "    mov $0x7C00, %sp\n"     // Set stack pointer
    "    "
    "    // Switch to protected mode temporarily for C code\n"
    "    lgdt gdt_descriptor\n"
    "    mov %cr0, %eax\n"
    "    or $1, %eax\n"
    "    mov %eax, %cr0\n"
    "    ljmp $0x08, $protected_mode\n"
    ""
    ".code32\n"
    "protected_mode:\n"
    "    mov $0x10, %ax\n"       // Data segment selector
    "    mov %ax, %ds\n"
    "    mov %ax, %es\n"
    "    mov %ax, %fs\n"
    "    mov %ax, %gs\n"
    "    mov %ax, %ss\n"
    "    mov $0x80000, %esp\n"  // Set 32-bit stack
    "    "
    "    call legacy_main\n"     // Call C main function
    "    "
    "halt_loop:\n"
    "    hlt\n"
    "    jmp halt_loop\n"
    ""
    "// GDT for initial protected mode\n"
    "gdt_start:\n"
    "    .quad 0x0000000000000000\n"  // Null descriptor
    "    .quad 0x00CF9A000000FFFF\n"  // Code segment (32-bit)
    "    .quad 0x00CF92000000FFFF\n"  // Data segment (32-bit)
    "gdt_end:\n"
    ""
    "gdt_descriptor:\n"
    "    .word gdt_end - gdt_start - 1\n"  // GDT size
    "    .long gdt_start\n"                // GDT address
    ""
    ".code32\n"
);

// Function implementations are in boot_asm.S

# Aurora Legacy Bootloader Assembly Functions
# Low-level functions for real mode to long mode transition

.code16
.section .text

# Enable A20 line using multiple methods
.global enable_a20
enable_a20:
    # Method 1: Try BIOS INT 15h
    mov $0x2401, %ax
    int $0x15
    jnc a20_enabled
    
    # Method 2: Keyboard controller
    call wait_8042
    mov $0xAD, %al      # Disable keyboard
    out %al, $0x64
    
    call wait_8042
    mov $0xD0, %al      # Read output port
    out %al, $0x64
    
    call wait_8042_data
    in $0x60, %al
    push %ax
    
    call wait_8042
    mov $0xD1, %al      # Write output port
    out %al, $0x64
    
    call wait_8042
    pop %ax
    or $0x02, %al       # Enable A20
    out %al, $0x60
    
    call wait_8042
    mov $0xAE, %al      # Enable keyboard
    out %al, $0x64
    
    call wait_8042
    
a20_enabled:
    ret

wait_8042:
    in $0x64, %al
    test $0x02, %al
    jnz wait_8042
    ret

wait_8042_data:
    in $0x64, %al
    test $0x01, %al
    jz wait_8042_data
    ret

# Detect memory using BIOS E820
.global detect_memory
detect_memory:
    push %bp
    mov %sp, %bp
    push %di
    push %si
    push %bx
    
    mov 4(%bp), %di     # Buffer address
    mov 6(%bp), %cx     # Max entries
    xor %bx, %bx        # Continuation value
    xor %si, %si        # Entry counter
    
e820_loop:
    mov $0xE820, %eax
    mov $24, %ecx       # Buffer size
    mov $0x534D4150, %edx  # 'SMAP' signature
    int $0x15
    
    jc e820_done        # Carry flag = error
    cmp $0x534D4150, %eax  # Check signature
    jne e820_done
    
    # Valid entry found
    add $24, %di        # Move to next entry
    inc %si             # Increment counter
    
    test %bx, %bx       # Check if more entries
    jz e820_done
    cmp %cx, %si        # Check max entries
    jae e820_done
    
    jmp e820_loop
    
e820_done:
    mov %si, %ax        # Return entry count
    
    pop %bx
    pop %si
    pop %di
    pop %bp
    ret

# Load kernel from disk using BIOS INT 13h
.global load_kernel_from_disk
load_kernel_from_disk:
    push %bp
    mov %sp, %bp
    push %bx
    push %cx
    push %dx
    push %si
    push %di
    
    mov 4(%bp), %bx     # Buffer address (segment:offset)
    mov 6(%bp), %cx     # LBA start (low)
    mov 8(%bp), %dx     # LBA start (high)
    mov 10(%bp), %si    # Sector count
    
    # Convert buffer address to segment:offset
    mov %bx, %ax
    shr $4, %ax         # Segment = address >> 4
    mov %ax, %es
    and $0x0F, %bx      # Offset = address & 0x0F
    
    # Setup disk address packet
    mov $disk_packet, %si
    mov $0x10, (%si)    # Packet size
    mov %dx, 2(%si)     # Sector count
    mov %bx, 4(%si)     # Buffer offset
    mov %es, 6(%si)     # Buffer segment
    mov %cx, 8(%si)     # LBA low
    mov %dx, 12(%si)    # LBA high
    
    # Extended read
    mov $0x42, %ah
    mov $0x80, %dl      # Drive 0x80 (first hard disk)
    int $0x13
    
    jc disk_error
    mov %dx, %ax        # Return sectors read
    jmp disk_done
    
disk_error:
    xor %ax, %ax        # Return 0 on error
    
disk_done:
    pop %di
    pop %si
    pop %dx
    pop %cx
    pop %bx
    pop %bp
    ret

# Print string using BIOS INT 10h
.global print_string
print_string:
    push %bp
    mov %sp, %bp
    push %si
    push %ax
    push %bx
    
    mov 4(%bp), %si     # String address
    
print_loop:
    lodsb               # Load byte from [SI] to AL
    test %al, %al       # Check for null terminator
    jz print_done
    
    mov $0x0E, %ah      # BIOS teletype output
    mov $0x07, %bx      # Page 0, attribute 7
    int $0x10
    
    jmp print_loop
    
print_done:
    pop %bx
    pop %ax
    pop %si
    pop %bp
    ret

# Print 32-bit hex value
.global print_hex
print_hex:
    push %bp
    mov %sp, %bp
    push %ax
    push %bx
    push %cx
    push %dx
    
    mov 4(%bp), %eax    # Value to print
    mov $8, %cx         # 8 hex digits
    
print_hex_loop:
    rol $4, %eax        # Rotate left 4 bits
    mov %al, %dl
    and $0x0F, %dl      # Get low 4 bits
    
    cmp $10, %dl
    jb hex_digit
    add $('A' - '0' - 10), %dl
    
hex_digit:
    add $'0', %dl
    
    mov $0x0E, %ah      # BIOS teletype output
    mov %dl, %al
    mov $0x07, %bx      # Page 0, attribute 7
    int $0x10
    
    loop print_hex_loop
    
    pop %dx
    pop %cx
    pop %bx
    pop %ax
    pop %bp
    ret

.code32

# Setup GDT for long mode
.global setup_gdt
setup_gdt:
    # Copy long mode GDT to designated address
    mov $long_mode_gdt, %esi
    mov $0x70000, %edi  # GDT_ADDRESS
    mov $gdt_end - long_mode_gdt, %ecx
    rep movsb
    
    # Load new GDT
    lgdt gdt64_descriptor
    ret

# Setup paging for long mode
.global setup_paging
setup_paging:
    # Clear page tables
    mov $0x60000, %edi  # PAGE_TABLE_ADDRESS
    mov $0x6000, %ecx   # Clear 24KB (6 pages)
    xor %eax, %eax
    rep stosl
    
    # Setup PML4 (Page Map Level 4)
    mov $0x60000, %edi  # PML4 address
    mov $0x61003, %eax  # PDPT address + flags (present, writable)
    stosl
    xor %eax, %eax
    stosl
    
    # Setup PDPT (Page Directory Pointer Table)
    mov $0x61000, %edi  # PDPT address
    mov $0x62003, %eax  # PD address + flags
    stosl
    xor %eax, %eax
    stosl
    
    # Setup PD (Page Directory) - identity map first 2MB
    mov $0x62000, %edi  # PD address
    mov $0x83, %eax     # 2MB page + flags (present, writable, large page)
    stosl
    xor %eax, %eax
    stosl
    
    # Enable PAE
    mov %cr4, %eax
    or $0x20, %eax      # Set PAE bit
    mov %eax, %cr4
    
    # Load PML4 address
    mov $0x60000, %eax
    mov %eax, %cr3
    
    ret

# Switch to long mode
.global switch_to_long_mode
switch_to_long_mode:
    # Enable long mode in EFER MSR
    mov $0xC0000080, %ecx  # EFER MSR
    rdmsr
    or $0x100, %eax        # Set LME bit
    wrmsr
    
    # Enable paging
    mov %cr0, %eax
    or $0x80000000, %eax   # Set PG bit
    mov %eax, %cr0
    
    # Jump to 64-bit code segment
    ljmp $0x08, $long_mode_entry
    
.code64
long_mode_entry:
    # Setup 64-bit segments
    mov $0x10, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs
    mov %ax, %ss
    
    # Setup 64-bit stack
    mov $0x80000, %rsp
    
    # Return to C code (now in 64-bit mode)
    ret

# Data section
.section .data

# Disk address packet for INT 13h extended read
disk_packet:
    .byte 0x10          # Packet size
    .byte 0             # Reserved
    .word 0             # Sector count
    .word 0             # Buffer offset
    .word 0             # Buffer segment
    .quad 0             # LBA address

# Long mode GDT
long_mode_gdt:
    .quad 0x0000000000000000    # Null descriptor
    .quad 0x00AF9A000000FFFF    # 64-bit code segment
    .quad 0x00CF92000000FFFF    # 64-bit data segment
gdt_end:

gdt64_descriptor:
    .word gdt_end - long_mode_gdt - 1
    .quad 0x70000               # GDT address
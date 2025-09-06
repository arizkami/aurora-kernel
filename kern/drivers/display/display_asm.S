# Aurora Display Driver Assembly Functions
# Hardware-specific display and GPU operations

.section .text
.global display_hw_init
.global display_set_mode
.global display_vsync_wait
.global display_memory_barrier
.global display_cache_flush
.global display_dma_setup
.global display_gpu_reset
.global display_atomic_swap

# Display hardware initialization
display_hw_init:
    push %rbp
    mov %rsp, %rbp
    
    # Initialize display controller registers
    mov $0x3C0, %dx        # VGA attribute controller
    mov $0x20, %al         # Enable display
    out %al, %dx
    
    # Initialize GPU memory controller
    mov $0x3000, %dx       # GPU base address
    mov $0x01, %eax        # Enable GPU
    out %eax, %dx
    
    # Set up display timing
    mov $0x3004, %dx       # GPU timing register
    mov $0x12345678, %eax  # Default timing values
    out %eax, %dx
    
    # Initialize framebuffer mapping
    mov $0x3008, %dx       # GPU framebuffer base
    mov $0x10000000, %eax  # Framebuffer physical address
    out %eax, %dx
    
    # Memory barrier to ensure all writes complete
    mfence
    
    pop %rbp
    ret

# Set display mode (width, height, refresh rate)
# Parameters: RDI = display_id, RSI = width, RDX = height, RCX = refresh
display_set_mode:
    push %rbp
    mov %rsp, %rbp
    push %rbx
    push %r12
    push %r13
    push %r14
    
    mov %rdi, %r12         # Save display_id
    mov %rsi, %r13         # Save width
    mov %rdx, %r14         # Save height
    mov %rcx, %rbx         # Save refresh rate
    
    # Calculate display mode register address
    mov $0x3010, %dx       # Base mode register
    add %r12w, %dx         # Add display offset
    
    # Set width
    mov %r13d, %eax
    out %eax, %dx
    
    # Set height
    add $4, %dx
    mov %r14d, %eax
    out %eax, %dx
    
    # Set refresh rate
    add $4, %dx
    mov %ebx, %eax
    out %eax, %dx
    
    # Calculate pixel clock
    mov %r13, %rax         # width
    mul %r14               # width * height
    mul %rbx               # * refresh_rate
    
    # Set pixel clock register
    add $4, %dx
    out %eax, %dx
    
    # Enable new mode
    add $4, %dx
    mov $0x01, %eax        # Enable bit
    out %eax, %dx
    
    # Wait for mode change completion
    mov $1000, %ecx        # Timeout counter
mode_wait_loop:
    in %dx, %eax
    test $0x80000000, %eax # Check ready bit
    jnz mode_ready
    dec %ecx
    jnz mode_wait_loop
    
mode_ready:
    # Memory barrier
    mfence
    
    pop %r14
    pop %r13
    pop %r12
    pop %rbx
    pop %rbp
    ret

# Wait for vertical sync
display_vsync_wait:
    push %rbp
    mov %rsp, %rbp
    
    mov $0x3DA, %dx        # VGA status register
    
    # Wait for end of current vsync
vsync_wait1:
    in %dx, %al
    test $0x08, %al        # Check vsync bit
    jnz vsync_wait1
    
    # Wait for start of next vsync
vsync_wait2:
    in %dx, %al
    test $0x08, %al        # Check vsync bit
    jz vsync_wait2
    
    pop %rbp
    ret

# Memory barrier for display operations
display_memory_barrier:
    mfence
    ret

# Flush display cache
# Parameter: RDI = cache_type (0=instruction, 1=data, 2=both)
display_cache_flush:
    push %rbp
    mov %rsp, %rbp
    
    cmp $0, %rdi
    je flush_icache
    cmp $1, %rdi
    je flush_dcache
    cmp $2, %rdi
    je flush_both
    jmp flush_end
    
flush_icache:
    # Flush instruction cache
    mov %cr0, %rax
    or $0x40000000, %rax   # Set CD bit
    mov %rax, %cr0
    wbinvd                 # Write back and invalidate
    and $0xBFFFFFFF, %eax  # Clear CD bit
    mov %rax, %cr0
    jmp flush_end
    
flush_dcache:
    # Flush data cache
    wbinvd
    jmp flush_end
    
flush_both:
    # Flush both caches
    mov %cr0, %rax
    or $0x40000000, %rax
    mov %rax, %cr0
    wbinvd
    and $0xBFFFFFFF, %eax
    mov %rax, %cr0
    
flush_end:
    pop %rbp
    ret

# Setup DMA for display operations
# Parameters: RDI = src_addr, RSI = dst_addr, RDX = size, RCX = flags
display_dma_setup:
    push %rbp
    mov %rsp, %rbp
    push %rbx
    push %r12
    push %r13
    push %r14
    
    mov %rdi, %r12         # Source address
    mov %rsi, %r13         # Destination address
    mov %rdx, %r14         # Size
    mov %rcx, %rbx         # Flags
    
    # DMA controller base address
    mov $0x4000, %dx
    
    # Set source address
    mov %r12d, %eax
    out %eax, %dx
    add $4, %dx
    shr $32, %r12
    mov %r12d, %eax
    out %eax, %dx
    
    # Set destination address
    add $4, %dx
    mov %r13d, %eax
    out %eax, %dx
    add $4, %dx
    shr $32, %r13
    mov %r13d, %eax
    out %eax, %dx
    
    # Set transfer size
    add $4, %dx
    mov %r14d, %eax
    out %eax, %dx
    
    # Set control flags and start
    add $4, %dx
    mov %ebx, %eax
    or $0x80000000, %eax   # Start bit
    out %eax, %dx
    
    pop %r14
    pop %r13
    pop %r12
    pop %rbx
    pop %rbp
    ret

# Reset GPU hardware
display_gpu_reset:
    push %rbp
    mov %rsp, %rbp
    
    # GPU control register
    mov $0x3000, %dx
    
    # Assert reset
    mov $0x80000000, %eax
    out %eax, %dx
    
    # Wait for reset
    mov $10000, %ecx
reset_wait:
    pause
    dec %ecx
    jnz reset_wait
    
    # Deassert reset
    mov $0x00000000, %eax
    out %eax, %dx
    
    # Wait for ready
    mov $10000, %ecx
ready_wait:
    in %dx, %eax
    test $0x40000000, %eax # Ready bit
    jnz gpu_ready
    dec %ecx
    jnz ready_wait
    
gpu_ready:
    pop %rbp
    ret

# Atomic swap operation for framebuffer pointers
# Parameters: RDI = ptr1, RSI = ptr2
# Returns: RAX = old value of ptr1
display_atomic_swap:
    push %rbp
    mov %rsp, %rbp
    
    mov (%rdi), %rax       # Load current value
    mov (%rsi), %rdx       # Load swap value
    
    # Atomic exchange
    lock xchg %rdx, (%rdi)
    mov %rdx, (%rsi)       # Store old value
    
    pop %rbp
    ret

# GPU command submission (low-level)
# Parameter: RDI = command_buffer_addr
display_gpu_submit:
    push %rbp
    mov %rsp, %rbp
    
    # GPU command register
    mov $0x3008, %dx
    
    # Submit command buffer address
    mov %edi, %eax
    out %eax, %dx
    
    # High 32 bits
    add $4, %dx
    shr $32, %rdi
    mov %edi, %eax
    out %eax, %dx
    
    # Trigger execution
    add $4, %dx
    mov $0x01, %eax
    out %eax, %dx
    
    pop %rbp
    ret

# Fast memory copy optimized for framebuffer operations
# Parameters: RDI = dest, RSI = src, RDX = count (in 64-bit words)
display_fast_copy:
    push %rbp
    mov %rsp, %rbp
    
    # Use string operations for fast copy
    mov %rdx, %rcx         # Count
    cld                    # Clear direction flag
    rep movsq              # Copy 64-bit words
    
    # Memory barrier
    sfence
    
    pop %rbp
    ret

# Display interrupt acknowledge
display_irq_ack:
    push %rbp
    mov %rsp, %rbp
    
    # GPU interrupt status register
    mov $0x3018, %dx
    
    # Read current status
    in %dx, %eax
    
    # Write back to acknowledge
    out %eax, %dx
    
    pop %rbp
    ret

.section .data
.align 8

# Display timing tables
display_timing_1920x1080_60:
    .quad 1920, 1080, 60, 148500000
    
display_timing_1680x1050_60:
    .quad 1680, 1050, 60, 146250000
    
display_timing_1280x1024_75:
    .quad 1280, 1024, 75, 135000000
    
display_timing_1024x768_75:
    .quad 1024, 768, 75, 78750000

# GPU register offsets table
gpu_registers:
    .long 0x3000    # Control
    .long 0x3004    # Status
    .long 0x3008    # Command
    .long 0x300C    # Memory base
    .long 0x3010    # Memory size
    .long 0x3014    # IRQ mask
    .long 0x3018    # IRQ status
    .long 0x301C    # Performance

.section .bss
.align 8

# Temporary buffers for display operations
display_temp_buffer:
    .space 4096
    
gpu_command_temp:
    .space 256
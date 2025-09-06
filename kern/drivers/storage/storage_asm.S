# Aurora Storage Driver Assembly Functions
# Hardware-specific assembly functions for NVMe, AHCI, and storage operations

.section .text
.global aur_storage_barrier
.global aur_storage_cache_flush
.global aur_storage_dma_setup
.global aur_storage_nvme_doorbell
.global aur_storage_ahci_fis_setup
.global aur_storage_atomic_read64
.global aur_storage_atomic_write64
.global aur_storage_memory_copy
.global aur_storage_crc32
.global aur_storage_wait_ready
.global aur_storage_reset_controller

# Storage memory barrier - ensures all memory operations complete
aur_storage_barrier:
    mfence          # Full memory fence
    sfence          # Store fence
    lfence          # Load fence
    ret

# Cache flush operations
# Input: RDI = cache type (0=data, 1=instruction, 2=both)
aur_storage_cache_flush:
    push %rax
    push %rcx
    push %rdx
    
    cmp $0, %rdi
    je .flush_data
    cmp $1, %rdi
    je .flush_instruction
    cmp $2, %rdi
    je .flush_both
    jmp .flush_done
    
.flush_data:
    # Flush data cache
    mov $0x0F, %eax    # WBINVD instruction prefix
    mov $0x09, %ecx
    wbinvd             # Write back and invalidate cache
    jmp .flush_done
    
.flush_instruction:
    # Flush instruction cache
    mov $1, %eax
    cpuid              # CPUID flushes instruction cache
    jmp .flush_done
    
.flush_both:
    wbinvd             # Flush data cache
    mov $1, %eax
    cpuid              # Flush instruction cache
    
.flush_done:
    pop %rdx
    pop %rcx
    pop %rax
    ret

# DMA setup for storage transfers
# Input: RDI = source address, RSI = destination address, RDX = size
aur_storage_dma_setup:
    push %rax
    push %rcx
    
    # Ensure addresses are properly aligned
    test $0x7, %rdi    # Check 8-byte alignment
    jnz .dma_unaligned
    test $0x7, %rsi
    jnz .dma_unaligned
    
    # Setup DMA transfer (simplified)
    mov %rdi, %rax     # Source
    mov %rsi, %rcx     # Destination
    # In real implementation, this would program DMA controller
    
    # Memory barrier before DMA
    mfence
    
    jmp .dma_done
    
.dma_unaligned:
    # Handle unaligned transfers
    # This would typically use slower byte-by-byte copy
    
.dma_done:
    pop %rcx
    pop %rax
    ret

# NVMe doorbell ring
# Input: RDI = doorbell address, RSI = value
aur_storage_nvme_doorbell:
    push %rax
    
    # Ensure previous operations complete
    mfence
    
    # Write to doorbell register
    mov %rsi, %rax
    mov %rax, (%rdi)
    
    # Memory barrier after doorbell
    mfence
    
    pop %rax
    ret

# AHCI FIS (Frame Information Structure) setup
# Input: RDI = FIS base address, RSI = command, RDX = LBA, RCX = count
aur_storage_ahci_fis_setup:
    push %rax
    push %rbx
    
    # Setup H2D Register FIS
    movb $0x27, (%rdi)      # FIS Type: Register H2D
    movb $0x80, 1(%rdi)     # Command bit set
    movb %sil, 2(%rdi)      # Command
    movb $0, 3(%rdi)        # Features
    
    # LBA (48-bit)
    mov %rdx, %rax
    movb %al, 4(%rdi)       # LBA[7:0]
    shr $8, %rax
    movb %al, 5(%rdi)       # LBA[15:8]
    shr $8, %rax
    movb %al, 6(%rdi)       # LBA[23:16]
    shr $8, %rax
    movb %al, 12(%rdi)      # LBA[31:24]
    shr $8, %rax
    movb %al, 13(%rdi)      # LBA[39:32]
    shr $8, %rax
    movb %al, 14(%rdi)      # LBA[47:40]
    
    # Device register
    movb $0xE0, 7(%rdi)     # LBA mode, master drive
    
    # Sector count
    mov %rcx, %rax
    movb %al, 12(%rdi)      # Count[7:0]
    shr $8, %rax
    movb %al, 13(%rdi)      # Count[15:8]
    
    # Clear remaining bytes
    xor %rax, %rax
    mov %rax, 8(%rdi)
    mov %rax, 16(%rdi)
    
    # Memory barrier
    mfence
    
    pop %rbx
    pop %rax
    ret

# Atomic 64-bit read
# Input: RDI = address
# Output: RAX = value
aur_storage_atomic_read64:
    mov (%rdi), %rax
    mfence
    ret

# Atomic 64-bit write
# Input: RDI = address, RSI = value
aur_storage_atomic_write64:
    mfence
    mov %rsi, (%rdi)
    mfence
    ret

# Fast memory copy for storage buffers
# Input: RDI = destination, RSI = source, RDX = size
aur_storage_memory_copy:
    push %rcx
    push %rax
    
    # Use REP MOVSQ for 8-byte aligned copies
    mov %rdx, %rcx
    shr $3, %rcx           # Divide by 8
    rep movsq              # Copy 8 bytes at a time
    
    # Handle remaining bytes
    mov %rdx, %rcx
    and $7, %rcx           # Remainder
    rep movsb              # Copy remaining bytes
    
    pop %rax
    pop %rcx
    ret

# CRC32 calculation for data integrity
# Input: RDI = data pointer, RSI = length
# Output: RAX = CRC32 value
aur_storage_crc32:
    push %rcx
    push %rdx
    push %rbx
    
    xor %rax, %rax         # Initialize CRC to 0
    mov %rsi, %rcx         # Length
    test %rcx, %rcx
    jz .crc32_done
    
.crc32_loop:
    movzbl (%rdi), %edx    # Load byte
    crc32b %dl, %eax       # CRC32 instruction (if available)
    inc %rdi
    dec %rcx
    jnz .crc32_loop
    
.crc32_done:
    pop %rbx
    pop %rdx
    pop %rcx
    ret

# Wait for device ready status
# Input: RDI = status register address, RSI = ready bit mask, RDX = timeout
# Output: RAX = 0 on success, -1 on timeout
aur_storage_wait_ready:
    push %rcx
    push %rbx
    
    mov %rdx, %rcx         # Timeout counter
    
.wait_loop:
    test %rcx, %rcx
    jz .wait_timeout
    
    mov (%rdi), %eax       # Read status register
    and %rsi, %rax         # Apply mask
    cmp %rsi, %rax         # Check if ready
    je .wait_success
    
    # Small delay
    pause
    dec %rcx
    jmp .wait_loop
    
.wait_timeout:
    mov $-1, %rax          # Timeout
    jmp .wait_done
    
.wait_success:
    xor %rax, %rax         # Success
    
.wait_done:
    pop %rbx
    pop %rcx
    ret

# Reset storage controller
# Input: RDI = control register address, RSI = reset bit
aur_storage_reset_controller:
    push %rax
    push %rcx
    
    # Set reset bit
    mov (%rdi), %eax
    or %rsi, %rax
    mov %rax, (%rdi)
    
    # Memory barrier
    mfence
    
    # Wait a bit for reset to take effect
    mov $1000, %rcx
.reset_delay:
    pause
    dec %rcx
    jnz .reset_delay
    
    # Clear reset bit
    mov (%rdi), %eax
    and %rsi, %rax
    xor %rsi, %rax         # Clear the bit
    mov %rax, (%rdi)
    
    # Final memory barrier
    mfence
    
    pop %rcx
    pop %rax
    ret

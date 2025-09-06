.intel_syntax noprefix

# Audio DMA Assembly Functions
.global audio_dma_setup
.global audio_dma_start
.global audio_dma_stop
.global audio_dma_status
.global audio_dma_barrier
.global audio_dma_flush_cache
.global audio_dma_invalidate_cache
.global audio_dma_copy_block

# DMA register offsets
.equ DMA_SRC_ADDR, 0x00
.equ DMA_DST_ADDR, 0x08
.equ DMA_SIZE, 0x10
.equ DMA_CONTROL, 0x14
.equ DMA_STATUS, 0x18
.equ DMA_IRQ_MASK, 0x1C

# DMA control bits
.equ DMA_ENABLE, 0x01
.equ DMA_IRQ_ENABLE, 0x02
.equ DMA_BURST_MODE, 0x04
.equ DMA_CONTINUOUS, 0x08

# Setup DMA transfer
# Parameters: rdi = src_addr, rsi = dst_addr, rdx = size
audio_dma_setup:
    push rbp
    mov rbp, rsp
    push rbx
    push r12
    
    # Assume DMA controller base address in r12
    mov r12, 0x2000  # Example DMA controller base
    
    # Set source address
    mov [r12 + DMA_SRC_ADDR], rdi
    
    # Set destination address
    mov [r12 + DMA_DST_ADDR], rsi
    
    # Set transfer size
    mov [r12 + DMA_SIZE], edx
    
    # Configure DMA control (enable interrupts, burst mode)
    mov eax, DMA_IRQ_ENABLE | DMA_BURST_MODE
    mov [r12 + DMA_CONTROL], eax
    
    pop r12
    pop rbx
    pop rbp
    ret

# Start DMA transfer
audio_dma_start:
    push rbp
    mov rbp, rsp
    
    mov rax, 0x2000  # DMA controller base
    
    # Enable DMA transfer
    mov edx, [rax + DMA_CONTROL]
    or edx, DMA_ENABLE
    mov [rax + DMA_CONTROL], edx
    
    # Memory barrier to ensure write completion
    mfence
    
    pop rbp
    ret

# Stop DMA transfer
audio_dma_stop:
    push rbp
    mov rbp, rsp
    
    mov rax, 0x2000  # DMA controller base
    
    # Disable DMA transfer
    mov edx, [rax + DMA_CONTROL]
    and edx, ~DMA_ENABLE
    mov [rax + DMA_CONTROL], edx
    
    # Wait for DMA to stop
.wait_stop:
    mov edx, [rax + DMA_STATUS]
    test edx, 0x01  # Check if DMA is still active
    jnz .wait_stop
    
    pop rbp
    ret

# Get DMA status
# Returns: rax = status register value
audio_dma_status:
    mov rax, 0x2000  # DMA controller base
    mov eax, [rax + DMA_STATUS]
    ret

# DMA memory barrier
audio_dma_barrier:
    lfence
    sfence
    mfence
    ret

# Flush cache for DMA coherency
# Parameters: rdi = address, rsi = size
audio_dma_flush_cache:
    push rbp
    mov rbp, rsp
    push rcx
    push rdx
    
    mov rcx, rdi     # Start address
    mov rdx, rsi     # Size
    add rdx, rcx     # End address
    
    # Align to cache line boundary (64 bytes)
    and rcx, ~63
    
.flush_loop:
    clflush [rcx]
    add rcx, 64
    cmp rcx, rdx
    jl .flush_loop
    
    mfence
    
    pop rdx
    pop rcx
    pop rbp
    ret

# Invalidate cache for DMA coherency
# Parameters: rdi = address, rsi = size
audio_dma_invalidate_cache:
    push rbp
    mov rbp, rsp
    push rcx
    push rdx
    
    mov rcx, rdi     # Start address
    mov rdx, rsi     # Size
    add rdx, rcx     # End address
    
    # Align to cache line boundary (64 bytes)
    and rcx, ~63
    
.invalidate_loop:
    clflush [rcx]
    add rcx, 64
    cmp rcx, rdx
    jl .invalidate_loop
    
    mfence
    
    pop rdx
    pop rcx
    pop rbp
    ret

# High-performance DMA block copy
# Parameters: rdi = dst, rsi = src, rdx = size
audio_dma_copy_block:
    push rbp
    mov rbp, rsp
    push rcx
    push r8
    push r9
    
    mov rcx, rdx     # Size
    mov r8, rdi      # Destination
    mov r9, rsi      # Source
    
    # Check alignment for optimized copy
    test rcx, 31     # Check if size is 32-byte aligned
    jnz .byte_copy
    
    # 32-byte aligned copy using AVX
    shr rcx, 5       # Divide by 32
    
.avx_copy_loop:
    vmovdqa ymm0, [r9]
    vmovdqa [r8], ymm0
    add r8, 32
    add r9, 32
    dec rcx
    jnz .avx_copy_loop
    
    vzeroupper
    jmp .copy_done
    
.byte_copy:
    # Fallback to byte copy
    rep movsb
    
.copy_done:
    pop r9
    pop r8
    pop rcx
    pop rbp
    ret

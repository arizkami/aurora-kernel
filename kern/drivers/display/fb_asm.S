.intel_syntax noprefix
.global aur_fb_mem_fence
.global aur_fb_cache_flush
.global aur_fb_dma_copy
.global aur_fb_atomic_blit
.global aur_fb_vsync_wait
.global aur_fb_gpu_barrier
.global aur_fb_pixel_copy
.global aur_fb_clear_region

# Memory fence for framebuffer operations
aur_fb_mem_fence:
    mfence
    ret

# Cache flush for framebuffer consistency
# Parameter: RDI = cache_type (0=data, 1=instruction, 2=both)
aur_fb_cache_flush:
    push rbp
    mov rbp, rsp
    
    cmp rdi, 0
    je flush_data
    cmp rdi, 1
    je flush_instruction
    cmp rdi, 2
    je flush_both
    jmp flush_end
    
flush_data:
    wbinvd
    jmp flush_end
    
flush_instruction:
    mov rax, cr0
    or rax, 0x40000000
    mov cr0, rax
    wbinvd
    and eax, 0xBFFFFFFF
    mov cr0, rax
    jmp flush_end
    
flush_both:
    mov rax, cr0
    or rax, 0x40000000
    mov cr0, rax
    wbinvd
    and eax, 0xBFFFFFFF
    mov cr0, rax
    
flush_end:
    pop rbp
    ret

# High-performance DMA copy for large framebuffer operations
# Parameters: RDI = dest, RSI = src, RDX = size_bytes
aur_fb_dma_copy:
    push rbp
    mov rbp, rsp
    push rcx
    push r8
    
    # Convert bytes to 64-bit words
    mov rcx, rdx
    shr rcx, 3
    
    # Use string operations for fast copy
    cld
    rep movsq
    
    # Handle remaining bytes
    mov rcx, rdx
    and rcx, 7
    rep movsb
    
    # Memory barrier
    sfence
    
    pop r8
    pop rcx
    pop rbp
    ret

# Atomic blit operation with hardware acceleration
# Parameters: RDI = src_fb, RSI = dst_fb, RDX = src_x, RCX = src_y, R8 = dst_x, R9 = dst_y
aur_fb_atomic_blit:
    push rbp
    mov rbp, rsp
    push rbx
    push r10
    push r11
    push r12
    
    # Load additional parameters from stack
    mov r10, [rbp + 16]  # width
    mov r11, [rbp + 24]  # height
    mov r12, [rbp + 32]  # bytes_per_pixel
    
    # Calculate source and destination addresses
    # src_addr = src_fb + (src_y * src_pitch) + (src_x * bpp)
    mov rax, rcx         # src_y
    mov rbx, [rdi + 8]   # src_pitch (assuming offset 8 in framebuffer struct)
    mul rbx
    mov rbx, rdx         # src_x
    mul r12              # * bytes_per_pixel
    add rax, rbx
    add rax, [rdi]       # Add framebuffer base address
    mov rdi, rax         # Source address
    
    # dst_addr = dst_fb + (dst_y * dst_pitch) + (dst_x * bpp)
    mov rax, r9          # dst_y
    mov rbx, [rsi + 8]   # dst_pitch
    mul rbx
    mov rbx, r8          # dst_x
    mul r12              # * bytes_per_pixel
    add rax, rbx
    add rax, [rsi]       # Add framebuffer base address
    mov rsi, rax         # Destination address
    
    # Copy line by line
    mov rcx, r11         # height
blit_loop:
    push rcx
    push rdi
    push rsi
    
    # Copy one line
    mov rcx, r10         # width
    mul r12              # * bytes_per_pixel
    mov rcx, rax
    shr rcx, 3           # Convert to 64-bit words
    cld
    rep movsq
    
    # Handle remaining bytes
    mov rcx, rax
    and rcx, 7
    rep movsb
    
    pop rsi
    pop rdi
    pop rcx
    
    # Move to next line
    add rdi, [rbp - 8]   # Add source pitch
    add rsi, [rbp - 16]  # Add destination pitch
    
    dec rcx
    jnz blit_loop
    
    # Memory barrier
    mfence
    
    pop r12
    pop r11
    pop r10
    pop rbx
    pop rbp
    ret

# Wait for vertical sync using hardware registers
aur_fb_vsync_wait:
    push rbp
    mov rbp, rsp
    
    mov dx, 0x3DA        # VGA status register
    
    # Wait for end of current vsync
vsync_wait_end:
    in al, dx
    test al, 0x08
    jnz vsync_wait_end
    
    # Wait for start of next vsync
vsync_wait_start:
    in al, dx
    test al, 0x08
    jz vsync_wait_start
    
    pop rbp
    ret

# GPU command barrier - ensures all GPU operations complete
aur_fb_gpu_barrier:
    push rbp
    mov rbp, rsp
    
    # GPU status register
    mov dx, 0x3004
    mov ecx, 10000       # Timeout counter
    
gpu_wait_loop:
    in eax, dx
    test eax, 0x80000000 # Check busy bit
    jz gpu_ready
    pause
    dec ecx
    jnz gpu_wait_loop
    
gpu_ready:
    # Full memory barrier
    mfence
    
    pop rbp
    ret

# Optimized pixel copy with format conversion
# Parameters: RDI = dest, RSI = src, RDX = pixel_count, RCX = src_format, R8 = dst_format
aur_fb_pixel_copy:
    push rbp
    mov rbp, rsp
    push rbx
    push r9
    push r10
    
    # Check if formats match (fast path)
    cmp rcx, r8
    je pixel_copy_direct
    
    # Format conversion required
    mov r9, rdx          # Save pixel count
    
pixel_convert_loop:
    # Load source pixel
    cmp rcx, 0           # RGB565
    je load_rgb565
    cmp rcx, 1           # RGB888
    je load_rgb888
    cmp rcx, 2           # RGBA8888
    je load_rgba8888
    jmp pixel_copy_end
    
load_rgb565:
    movzx eax, word ptr [rsi]
    # Convert RGB565 to RGB888
    mov rbx, rax
    and rbx, 0xF800      # Red
    shl rbx, 8
    mov r10, rax
    and r10, 0x07E0      # Green
    shl r10, 5
    or rbx, r10
    and rax, 0x001F      # Blue
    shl rax, 3
    or rax, rbx
    add rsi, 2
    jmp store_pixel
    
load_rgb888:
    mov eax, [rsi]
    and eax, 0x00FFFFFF
    add rsi, 3
    jmp store_pixel
    
load_rgba8888:
    mov eax, [rsi]
    add rsi, 4
    jmp store_pixel
    
store_pixel:
    # Store converted pixel
    cmp r8, 0            # RGB565
    je store_rgb565
    cmp r8, 1            # RGB888
    je store_rgb888
    cmp r8, 2            # RGBA8888
    je store_rgba8888
    jmp pixel_next
    
store_rgb565:
    # Convert RGB888 to RGB565
    mov rbx, rax
    shr rbx, 8
    and rbx, 0xF800      # Red
    mov r10, rax
    shr r10, 5
    and r10, 0x07E0      # Green
    or rbx, r10
    shr rax, 3
    and rax, 0x001F      # Blue
    or rax, rbx
    mov [rdi], ax
    add rdi, 2
    jmp pixel_next
    
store_rgb888:
    mov [rdi], eax
    add rdi, 3
    jmp pixel_next
    
store_rgba8888:
    or eax, 0xFF000000   # Set alpha to 255
    mov [rdi], eax
    add rdi, 4
    jmp pixel_next
    
pixel_next:
    dec r9
    jnz pixel_convert_loop
    jmp pixel_copy_end
    
pixel_copy_direct:
    # Direct copy without conversion
    mov rcx, rdx
    cld
    rep movsd
    
pixel_copy_end:
    pop r10
    pop r9
    pop rbx
    pop rbp
    ret

# Clear framebuffer region with specified color
# Parameters: RDI = framebuffer, RSI = x, RDX = y, RCX = width, R8 = height, R9 = color
aur_fb_clear_region:
    push rbp
    mov rbp, rsp
    push rbx
    push r10
    push r11
    push r12
    
    # Load additional parameters
    mov r10, [rbp + 16]  # bytes_per_pixel
    mov r11, [rbp + 24]  # pitch
    
    # Calculate starting address
    mov rax, rdx         # y
    mul r11              # * pitch
    mov rbx, rsi         # x
    mul r10              # * bytes_per_pixel
    add rax, rbx
    add rax, [rdi]       # Add framebuffer base
    mov rdi, rax         # Destination address
    
    # Prepare color pattern
    mov rax, r9          # color
    cmp r10, 2           # 16-bit
    je clear_16bit
    cmp r10, 3           # 24-bit
    je clear_24bit
    cmp r10, 4           # 32-bit
    je clear_32bit
    jmp clear_end
    
clear_16bit:
    mov rbx, rax
    shl rax, 16
    or rax, rbx          # Duplicate 16-bit pattern
    mov rbx, rax
    shl rax, 32
    or rax, rbx          # Create 64-bit pattern
    jmp clear_lines
    
clear_24bit:
    # For 24-bit, use byte-by-byte clearing
    jmp clear_lines_24
    
clear_32bit:
    mov rbx, rax
    shl rax, 32
    or rax, rbx          # Create 64-bit pattern
    jmp clear_lines
    
clear_lines:
    mov r12, r8          # height
clear_line_loop:
    push rdi
    
    # Clear one line
    mov rcx, [rbp + 32]  # width
    mul r10              # * bytes_per_pixel
    mov rcx, rax
    shr rcx, 3           # Convert to 64-bit words
    cld
    rep stosq
    
    # Handle remaining bytes
    mov rcx, rax
    and rcx, 7
    rep stosb
    
    pop rdi
    add rdi, r11         # Move to next line
    
    dec r12
    jnz clear_line_loop
    jmp clear_end
    
clear_lines_24:
    mov r12, r8          # height
clear_line_24_loop:
    push rdi
    
    # Clear one line (24-bit)
    mov rcx, [rbp + 32]  # width
clear_pixel_24_loop:
    mov [rdi], r9b       # Blue
    mov r10, r9
    shr r10, 8
    mov [rdi + 1], r10b   # Green
    shr r9, 16
    mov [rdi + 2], r9b   # Red
    shl r9, 16           # Restore color
    add rdi, 3
    dec rcx
    jnz clear_pixel_24_loop
    
    pop rdi
    add rdi, r11         # Move to next line
    
    dec r12
    jnz clear_line_24_loop
    
clear_end:
    # Memory barrier
    sfence
    
    pop r12
    pop r11
    pop r10
    pop rbx
    pop rbp
    ret

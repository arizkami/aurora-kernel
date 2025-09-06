/* Low level MMIO helpers (amd64) */
.intel_syntax noprefix
.global aur_mmio_read32
aur_mmio_read32:
    mov rax, [rdi]
    ret
.global aur_mmio_write32
aur_mmio_write32:
    mov [rdi], esi
    ret
.global aur_mem_fence
aur_mem_fence:
    mfence
    ret

.intel_syntax noprefix
.text

# Flush Translation Lookaside Buffer
.global Amd64FlushTlb
Amd64FlushTlb:
    mov rax, cr3
    mov cr3, rax
    ret

# Get CR3 register value
.global Amd64GetCR3
Amd64GetCR3:
    mov rax, cr3
    ret

# Set CR3 register value
.global Amd64SetCR3
Amd64SetCR3:
    mov cr3, rdi  # First argument in RDI
    ret

# Invalidate specific page in TLB
.global Amd64InvalidatePage
Amd64InvalidatePage:
    invlpg [rdi]  # First argument in RDI
    ret

# Get CR0 register value
.global Amd64GetCR0
Amd64GetCR0:
    mov rax, cr0
    ret

# Set CR0 register value
.global Amd64SetCR0
Amd64SetCR0:
    mov cr0, rdi  # First argument in RDI
    ret

# Get CR4 register value
.global Amd64GetCR4
Amd64GetCR4:
    mov rax, cr4
    ret

# Set CR4 register value
.global Amd64SetCR4
Amd64SetCR4:
    mov cr4, rdi  # First argument in RDI
    ret

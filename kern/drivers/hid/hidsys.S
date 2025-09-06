.intel_syntax noprefix

# HID System Assembly Functions
.global aur_hid_barrier
.global aur_hid_atomic_read
.global aur_hid_atomic_write
.global aur_hid_spinlock_acquire
.global aur_hid_spinlock_release
.global aur_hid_context_switch
.global aur_hid_irq_save
.global aur_hid_irq_restore

# Memory barrier for HID operations
aur_hid_barrier:
    lfence
    sfence
    mfence
    ret

# Atomic read operation
aur_hid_atomic_read:
    mov rax, [rdi]  # Load value atomically
    ret

# Atomic write operation
aur_hid_atomic_write:
    mov [rdi], rsi  # Store value atomically
    mfence
    ret

# Spinlock acquire
aur_hid_spinlock_acquire:
    mov eax, 1
.spin_loop:
    xchg [rdi], eax
    test eax, eax
    jnz .spin_loop
    ret

# Spinlock release
aur_hid_spinlock_release:
    mov dword ptr [rdi], 0
    ret

# Context switch for HID interrupt handling
aur_hid_context_switch:
    pushfq
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15
    
    # Switch stack pointer
    mov [rdi], rsp  # Save current stack
    mov rsp, [rsi]  # Load new stack
    
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rbp
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax
    popfq
    ret

# Save interrupt flags and disable interrupts
aur_hid_irq_save:
    pushfq
    pop rax
    cli
    ret

# Restore interrupt flags
aur_hid_irq_restore:
    push rdi
    popfq
    ret

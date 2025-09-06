/* Aurora HAL - AMD64 low level assembly (GAS) */
    .text
    .intel_syntax noprefix

    .globl HalAsmReadTsc
HalAsmReadTsc:
    rdtsc                /* EDX:EAX = TSC */
    shl rdx, 32          /* shift high into position */
    or  rax, rdx         /* combine */
    ret

    .globl HalAsmPause
HalAsmPause:
    pause
    ret

    .globl HalAsmHalt
HalAsmHalt:
1:  hlt
    jmp 1b
    .att_syntax prefix

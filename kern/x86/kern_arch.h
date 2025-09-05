/*
 * Aurora Kernel - x86 Architecture Specific Definitions
 * Copyright (c) 2024 Aurora Project
 */

#ifndef _KERN_ARCH_X86_H_
#define _KERN_ARCH_X86_H_

#include "../../aurora.h"
#include "../../include/kern.h"

/* x86 Register Context */
typedef struct _X86_CONTEXT {
    /* General purpose registers */
    UINT32 Eax;
    UINT32 Ebx;
    UINT32 Ecx;
    UINT32 Edx;
    UINT32 Esi;
    UINT32 Edi;
    UINT32 Ebp;
    UINT32 Esp;
    
    /* Segment registers */
    UINT16 Cs;
    UINT16 Ds;
    UINT16 Es;
    UINT16 Fs;
    UINT16 Gs;
    UINT16 Ss;
    
    /* Control registers */
    UINT32 Eip;
    UINT32 Eflags;
    
    /* FPU state (simplified) */
    UINT8 FpuState[108];
} X86_CONTEXT, *PX86_CONTEXT;

/* x86 Segment Selectors */
#define X86_KERNEL_CS   0x08
#define X86_KERNEL_DS   0x10
#define X86_USER_CS     0x1B
#define X86_USER_DS     0x23

/* x86 EFLAGS bits */
#define X86_EFLAGS_CF   0x00000001  /* Carry Flag */
#define X86_EFLAGS_PF   0x00000004  /* Parity Flag */
#define X86_EFLAGS_AF   0x00000010  /* Auxiliary Flag */
#define X86_EFLAGS_ZF   0x00000040  /* Zero Flag */
#define X86_EFLAGS_SF   0x00000080  /* Sign Flag */
#define X86_EFLAGS_TF   0x00000100  /* Trap Flag */
#define X86_EFLAGS_IF   0x00000200  /* Interrupt Flag */
#define X86_EFLAGS_DF   0x00000400  /* Direction Flag */
#define X86_EFLAGS_OF   0x00000800  /* Overflow Flag */
#define X86_EFLAGS_IOPL 0x00003000  /* I/O Privilege Level */
#define X86_EFLAGS_NT   0x00004000  /* Nested Task */
#define X86_EFLAGS_RF   0x00010000  /* Resume Flag */
#define X86_EFLAGS_VM   0x00020000  /* Virtual Mode */

/* Default EFLAGS for new threads */
#define X86_DEFAULT_EFLAGS (X86_EFLAGS_IF | 0x02)

/* x86 Architecture Functions */

/* Context management */
VOID X86SaveContext(IN PTHREAD Thread);
VOID X86RestoreContext(IN PTHREAD Thread);
VOID X86InitializeThreadContext(
    IN PTHREAD Thread,
    IN PVOID StartAddress,
    IN PVOID Parameter
);

/* CPU control */
VOID X86HaltProcessor(void);
VOID X86EnableInterrupts(void);
VOID X86DisableInterrupts(void);
BOOL X86AreInterruptsEnabled(void);

/* System calls */
VOID X86InitializeSystemCallInterface(void);
VOID X86SystemCallEntry(void);

/* Memory management */
VOID X86InitializeMemoryManagement(void);
VOID X86SwitchAddressSpace(IN PVOID PageDirectory);

/* Interrupt handling */
VOID X86InitializeInterrupts(void);
VOID X86RegisterInterruptHandler(IN UINT8 Vector, IN PVOID Handler);

/* Timer */
VOID X86InitializeTimer(void);
VOID X86TimerInterruptHandler(void);

/* Architecture-specific initialization */
NTSTATUS X86InitializeArchitecture(void);

/* Inline assembly helpers */
static inline UINT32 X86ReadCr0(void)
{
    /* Placeholder implementation */
    return 0;
}

static inline VOID X86WriteCr0(UINT32 value)
{
    /* Placeholder implementation */
    UNREFERENCED_PARAMETER(value);
}

static inline UINT32 X86ReadCr3(void)
{
    /* Placeholder implementation */
    return 0;
}

static inline VOID X86WriteCr3(UINT32 value)
{
    /* Placeholder implementation */
    UNREFERENCED_PARAMETER(value);
}

static inline UINT32 X86ReadEflags(void)
{
    /* Placeholder implementation - return default EFLAGS */
    return X86_DEFAULT_EFLAGS;
}

static inline VOID X86WriteEflags(UINT32 eflags)
{
    /* Placeholder implementation */
    UNREFERENCED_PARAMETER(eflags);
}

static inline VOID X86Halt(void)
{
    /* Placeholder implementation */
    while (1) { /* Infinite loop */ }
}

static inline VOID X86Cli(void)
{
    /* Placeholder implementation */
}

static inline VOID X86Sti(void)
{
    /* Placeholder implementation */
}

static inline UINT8 X86InByte(UINT16 port)
{
    /* Placeholder implementation */
    UNREFERENCED_PARAMETER(port);
    return 0;
}

static inline VOID X86OutByte(UINT16 port, UINT8 value)
{
    /* Placeholder implementation */
    UNREFERENCED_PARAMETER(port);
    UNREFERENCED_PARAMETER(value);
}

static inline UINT16 X86InWord(UINT16 port)
{
    /* Placeholder implementation */
    UNREFERENCED_PARAMETER(port);
    return 0;
}

static inline VOID X86OutWord(UINT16 port, UINT16 value)
{
    /* Placeholder implementation */
    UNREFERENCED_PARAMETER(port);
    UNREFERENCED_PARAMETER(value);
}

static inline UINT32 X86InDword(UINT16 port)
{
    /* Placeholder implementation */
    UNREFERENCED_PARAMETER(port);
    return 0;
}

static inline VOID X86OutDword(UINT16 port, UINT32 value)
{
    /* Placeholder implementation */
    UNREFERENCED_PARAMETER(port);
    UNREFERENCED_PARAMETER(value);
}

#endif /* _KERN_ARCH_X86_H_ */
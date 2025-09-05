/*
 * Aurora Kernel - AMD64 Architecture Specific Definitions
 * Copyright (c) 2024 Aurora Project
 */

#ifndef _KERN_ARCH_AMD64_H_
#define _KERN_ARCH_AMD64_H_

#include "../../aurora.h"
#include "../../include/kern.h"

/* AMD64 Register Context */
typedef struct _AMD64_CONTEXT {
    /* General purpose registers */
    UINT64 Rax;
    UINT64 Rbx;
    UINT64 Rcx;
    UINT64 Rdx;
    UINT64 Rsi;
    UINT64 Rdi;
    UINT64 Rbp;
    UINT64 Rsp;
    UINT64 R8;
    UINT64 R9;
    UINT64 R10;
    UINT64 R11;
    UINT64 R12;
    UINT64 R13;
    UINT64 R14;
    UINT64 R15;
    
    /* Segment registers */
    UINT16 Cs;
    UINT16 Ds;
    UINT16 Es;
    UINT16 Fs;
    UINT16 Gs;
    UINT16 Ss;
    
    /* Control registers */
    UINT64 Rip;
    UINT64 Rflags;
    
    /* SSE/AVX state (simplified) */
    UINT8 FxsaveArea[512];
} AMD64_CONTEXT, *PAMD64_CONTEXT;

/* AMD64 Segment Selectors */
#define AMD64_KERNEL_CS   0x08
#define AMD64_KERNEL_DS   0x10
#define AMD64_USER_CS     0x1B
#define AMD64_USER_DS     0x23

/* AMD64 RFLAGS bits */
#define AMD64_RFLAGS_CF   0x0000000000000001ULL  /* Carry Flag */
#define AMD64_RFLAGS_PF   0x0000000000000004ULL  /* Parity Flag */
#define AMD64_RFLAGS_AF   0x0000000000000010ULL  /* Auxiliary Flag */
#define AMD64_RFLAGS_ZF   0x0000000000000040ULL  /* Zero Flag */
#define AMD64_RFLAGS_SF   0x0000000000000080ULL  /* Sign Flag */
#define AMD64_RFLAGS_TF   0x0000000000000100ULL  /* Trap Flag */
#define AMD64_RFLAGS_IF   0x0000000000000200ULL  /* Interrupt Flag */
#define AMD64_RFLAGS_DF   0x0000000000000400ULL  /* Direction Flag */
#define AMD64_RFLAGS_OF   0x0000000000000800ULL  /* Overflow Flag */
#define AMD64_RFLAGS_IOPL 0x0000000000003000ULL  /* I/O Privilege Level */
#define AMD64_RFLAGS_NT   0x0000000000004000ULL  /* Nested Task */
#define AMD64_RFLAGS_RF   0x0000000000010000ULL  /* Resume Flag */
#define AMD64_RFLAGS_VM   0x0000000000020000ULL  /* Virtual Mode */

/* Default RFLAGS for new threads */
#define AMD64_DEFAULT_RFLAGS (AMD64_RFLAGS_IF | 0x02)

/* AMD64 Architecture Functions */

/* Context management */
VOID Amd64SaveContext(IN PTHREAD Thread);
VOID Amd64RestoreContext(IN PTHREAD Thread);
VOID Amd64InitializeThreadContext(
    IN PTHREAD Thread,
    IN PVOID StartAddress,
    IN PVOID Parameter
);

/* CPU control */
VOID Amd64HaltProcessor(void);
VOID Amd64EnableInterrupts(void);
VOID Amd64DisableInterrupts(void);
BOOL Amd64AreInterruptsEnabled(void);

/* System calls */
VOID Amd64InitializeSystemCallInterface(void);
VOID Amd64SystemCallEntry(void);

/* Memory management */
VOID Amd64InitializeMemoryManagement(void);
VOID Amd64SwitchAddressSpace(IN PVOID PageDirectory);

/* Interrupt handling */
VOID Amd64InitializeInterrupts(void);
VOID Amd64RegisterInterruptHandler(IN UINT8 Vector, IN PVOID Handler);

/* Timer */
VOID Amd64InitializeTimer(void);
VOID Amd64TimerInterruptHandler(void);

/* Architecture-specific initialization */
NTSTATUS Amd64InitializeArchitecture(void);

/* Inline assembly helpers */
static inline UINT64 Amd64ReadCr0(void)
{
    UINT64 value;
    __asm__ volatile ("movq %%cr0, %0" : "=r" (value));
    return value;
}

static inline VOID Amd64WriteCr0(UINT64 value)
{
    __asm__ volatile ("movq %0, %%cr0" : : "r" (value) : "memory");
}

static inline UINT64 Amd64ReadCr3(void)
{
    UINT64 value;
    __asm__ volatile ("movq %%cr3, %0" : "=r" (value));
    return value;
}

static inline VOID Amd64WriteCr3(UINT64 value)
{
    __asm__ volatile ("movq %0, %%cr3" : : "r" (value) : "memory");
}

static inline UINT64 Amd64ReadRflags(void)
{
    UINT64 rflags;
    __asm__ volatile ("pushfq; popq %0" : "=r" (rflags));
    return rflags;
}

static inline VOID Amd64WriteRflags(UINT64 rflags)
{
    __asm__ volatile ("pushq %0; popfq" : : "r" (rflags) : "memory", "cc");
}

static inline VOID Amd64Halt(void)
{
    __asm__ volatile ("hlt");
}

static inline VOID Amd64Cli(void)
{
    __asm__ volatile ("cli" : : : "memory");
}

static inline VOID Amd64Sti(void)
{
    __asm__ volatile ("sti" : : : "memory");
}

static inline UINT8 Amd64InByte(UINT16 port)
{
    UINT8 value;
    __asm__ volatile ("inb %1, %0" : "=a" (value) : "Nd" (port));
    return value;
}

static inline VOID Amd64OutByte(UINT16 port, UINT8 value)
{
    __asm__ volatile ("outb %0, %1" : : "a" (value), "Nd" (port));
}

static inline UINT16 Amd64InWord(UINT16 port)
{
    UINT16 value;
    __asm__ volatile ("inw %1, %0" : "=a" (value) : "Nd" (port));
    return value;
}

static inline VOID Amd64OutWord(UINT16 port, UINT16 value)
{
    __asm__ volatile ("outw %0, %1" : : "a" (value), "Nd" (port));
}

static inline UINT32 Amd64InDword(UINT16 port)
{
    UINT32 value;
    __asm__ volatile ("inl %1, %0" : "=a" (value) : "Nd" (port));
    return value;
}

static inline VOID Amd64OutDword(UINT16 port, UINT32 value)
{
    __asm__ volatile ("outl %0, %1" : : "a" (value), "Nd" (port));
}

static inline UINT64 Amd64ReadMsr(UINT32 msr)
{
    UINT32 low, high;
    __asm__ volatile ("rdmsr" : "=a" (low), "=d" (high) : "c" (msr));
    return ((UINT64)high << 32) | low;
}

static inline VOID Amd64WriteMsr(UINT32 msr, UINT64 value)
{
    UINT32 low = (UINT32)value;
    UINT32 high = (UINT32)(value >> 32);
    __asm__ volatile ("wrmsr" : : "a" (low), "d" (high), "c" (msr));
}

static inline UINT64 Amd64ReadTsc(void)
{
    UINT32 low, high;
    __asm__ volatile ("rdtsc" : "=a" (low), "=d" (high));
    return ((UINT64)high << 32) | low;
}

#endif /* _KERN_ARCH_AMD64_H_ */
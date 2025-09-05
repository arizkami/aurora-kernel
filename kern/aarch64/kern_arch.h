/*
 * Aurora Kernel - AArch64 Architecture Specific Definitions
 * Copyright (c) 2024 Aurora Project
 */

#ifndef _KERN_ARCH_AARCH64_H_
#define _KERN_ARCH_AARCH64_H_

#include "../../aurora.h"
#include "../../include/kern.h"

/* AArch64 Register Context */
typedef struct _AARCH64_CONTEXT {
    /* General purpose registers */
    UINT64 X0;
    UINT64 X1;
    UINT64 X2;
    UINT64 X3;
    UINT64 X4;
    UINT64 X5;
    UINT64 X6;
    UINT64 X7;
    UINT64 X8;
    UINT64 X9;
    UINT64 X10;
    UINT64 X11;
    UINT64 X12;
    UINT64 X13;
    UINT64 X14;
    UINT64 X15;
    UINT64 X16;
    UINT64 X17;
    UINT64 X18;
    UINT64 X19;
    UINT64 X20;
    UINT64 X21;
    UINT64 X22;
    UINT64 X23;
    UINT64 X24;
    UINT64 X25;
    UINT64 X26;
    UINT64 X27;
    UINT64 X28;
    UINT64 X29; /* Frame Pointer */
    UINT64 X30; /* Link Register */
    
    /* Stack Pointer */
    UINT64 SP;
    
    /* Program Counter */
    UINT64 PC;
    
    /* Processor State */
    UINT64 PSTATE;
    
    /* NEON/FPU state (simplified) */
    UINT8 FpuState[512];
} AARCH64_CONTEXT, *PAARCH64_CONTEXT;

/* AArch64 Exception Levels */
#define AARCH64_EL0   0  /* User mode */
#define AARCH64_EL1   1  /* Kernel mode */
#define AARCH64_EL2   2  /* Hypervisor mode */
#define AARCH64_EL3   3  /* Secure monitor mode */

/* AArch64 PSTATE bits */
#define AARCH64_PSTATE_N    0x80000000ULL  /* Negative condition flag */
#define AARCH64_PSTATE_Z    0x40000000ULL  /* Zero condition flag */
#define AARCH64_PSTATE_C    0x20000000ULL  /* Carry condition flag */
#define AARCH64_PSTATE_V    0x10000000ULL  /* Overflow condition flag */
#define AARCH64_PSTATE_D    0x00000200ULL  /* Debug exception mask */
#define AARCH64_PSTATE_A    0x00000100ULL  /* SError interrupt mask */
#define AARCH64_PSTATE_I    0x00000080ULL  /* IRQ interrupt mask */
#define AARCH64_PSTATE_F    0x00000040ULL  /* FIQ interrupt mask */
#define AARCH64_PSTATE_M    0x0000000FULL  /* Exception level and selected SP */

/* Default PSTATE for new threads */
#define AARCH64_DEFAULT_PSTATE_EL1 (AARCH64_EL1 | AARCH64_PSTATE_D | AARCH64_PSTATE_A)
#define AARCH64_DEFAULT_PSTATE_EL0 (AARCH64_EL0)

/* System register definitions */
#define AARCH64_SCTLR_EL1   "sctlr_el1"
#define AARCH64_TCR_EL1     "tcr_el1"
#define AARCH64_TTBR0_EL1   "ttbr0_el1"
#define AARCH64_TTBR1_EL1   "ttbr1_el1"
#define AARCH64_MAIR_EL1    "mair_el1"
#define AARCH64_VBAR_EL1    "vbar_el1"
#define AARCH64_ESR_EL1     "esr_el1"
#define AARCH64_FAR_EL1     "far_el1"
#define AARCH64_ELR_EL1     "elr_el1"
#define AARCH64_SPSR_EL1    "spsr_el1"
#define AARCH64_SP_EL0      "sp_el0"
#define AARCH64_SP_EL1      "sp_el1"

/* AArch64 Architecture Functions */

/* Context management */
VOID Aarch64SaveContext(IN PTHREAD Thread);
VOID Aarch64RestoreContext(IN PTHREAD Thread);
VOID Aarch64InitializeThreadContext(
    IN PTHREAD Thread,
    IN PVOID StartAddress,
    IN PVOID Parameter
);

/* CPU control */
VOID Aarch64HaltProcessor(void);
/* Interrupt functions are now inline below */
BOOL Aarch64AreInterruptsEnabled(void);

/* System calls */
VOID Aarch64InitializeSystemCallInterface(void);
VOID Aarch64SystemCallEntry(void);

/* Memory management */
VOID Aarch64InitializeMemoryManagement(void);
VOID Aarch64SwitchAddressSpace(IN PVOID PageDirectory);

/* Interrupt handling */
VOID Aarch64InitializeInterrupts(void);
VOID Aarch64RegisterInterruptHandler(IN UINT32 Vector, IN PVOID Handler);

/* Timer */
VOID Aarch64InitializeTimer(void);
VOID Aarch64TimerInterruptHandler(void);

/* Architecture-specific initialization */
NTSTATUS Aarch64InitializeArchitecture(void);

/* Cache operations */
VOID Aarch64InvalidateInstructionCache(void);
VOID Aarch64InvalidateDataCache(void);
VOID Aarch64CleanDataCache(void);
VOID Aarch64CleanInvalidateDataCache(void);

/* Inline assembly helpers */
static inline UINT64 Aarch64ReadSystemRegister(const char* reg)
{
    UINT64 value;
    if (strcmp(reg, AARCH64_SCTLR_EL1) == 0) {
        __asm__ volatile ("mrs %0, sctlr_el1" : "=r" (value));
    } else if (strcmp(reg, AARCH64_TCR_EL1) == 0) {
        __asm__ volatile ("mrs %0, tcr_el1" : "=r" (value));
    } else if (strcmp(reg, AARCH64_TTBR0_EL1) == 0) {
        __asm__ volatile ("mrs %0, ttbr0_el1" : "=r" (value));
    } else if (strcmp(reg, AARCH64_TTBR1_EL1) == 0) {
        __asm__ volatile ("mrs %0, ttbr1_el1" : "=r" (value));
    } else {
        value = 0;
    }
    return value;
}

static inline VOID Aarch64WriteSystemRegister(const char* reg, UINT64 value)
{
    if (strcmp(reg, AARCH64_SCTLR_EL1) == 0) {
        __asm__ volatile ("msr sctlr_el1, %0" : : "r" (value));
    } else if (strcmp(reg, AARCH64_TCR_EL1) == 0) {
        __asm__ volatile ("msr tcr_el1, %0" : : "r" (value));
    } else if (strcmp(reg, AARCH64_TTBR0_EL1) == 0) {
        __asm__ volatile ("msr ttbr0_el1, %0" : : "r" (value));
    } else if (strcmp(reg, AARCH64_TTBR1_EL1) == 0) {
        __asm__ volatile ("msr ttbr1_el1, %0" : : "r" (value));
    }
    __asm__ volatile ("isb" : : : "memory");
}

static inline UINT64 Aarch64ReadSctlrEl1(void) {
    return 0; /* Placeholder implementation */
}

static inline VOID Aarch64WriteSctlrEl1(UINT64 value)
{
    UNREFERENCED_PARAMETER(value); /* Placeholder implementation */
}

static inline UINT64 Aarch64ReadTcrEl1(void) {
    return 0; /* Placeholder implementation */
}

static inline VOID Aarch64WriteTtbr0El1(UINT64 value)
{
    UNREFERENCED_PARAMETER(value); /* Placeholder implementation */
}

static inline UINT64 Aarch64ReadCntpctEl0(void)
{
    return 0; /* Placeholder implementation */
}

static inline UINT64 Aarch64ReadCntfrqEl0(void)
{
    return 0; /* Placeholder implementation */
}

static inline UINT64 Aarch64ReadDaif(void)
{
    return 0; /* Placeholder implementation */
}

static inline VOID Aarch64WriteDaif(UINT64 value)
{
    UNREFERENCED_PARAMETER(value); /* Placeholder implementation */
}

static inline void Aarch64Wfi(void)
{
    /* Wait for interrupt - placeholder implementation */
}

static inline void Aarch64Wfe(void)
{
    /* Wait for event - placeholder implementation */
}

static inline void Aarch64Sev(void)
{
    /* Send event - placeholder implementation */
}

static inline VOID Aarch64Dsb(void)
{
    /* Data synchronization barrier - placeholder implementation */
}

static inline void Aarch64Dmb(void)
{
    /* Data memory barrier - placeholder implementation */
}

static inline VOID Aarch64Isb(void)
{
    /* Instruction synchronization barrier - placeholder implementation */
}

static inline void Aarch64EnableInterrupts(void)
{
    /* Enable interrupts - placeholder implementation */
}

static inline void Aarch64DisableInterrupts(void)
{
    /* Disable interrupts - placeholder implementation */
}

static inline UINT64 Aarch64ReadTtbr0El1(void) {
    return 0; /* Placeholder implementation */
}

static inline UINT64 Aarch64ReadTtbr1El1(void) {
    return 0; /* Placeholder implementation */
}

static inline void Aarch64WriteCntpTvalEl0(UINT32 value)
{
    UNREFERENCED_PARAMETER(value); /* Placeholder implementation */
}

static inline VOID Aarch64WriteCntpCtlEl0(UINT32 value)
{
    UNREFERENCED_PARAMETER(value); /* Placeholder implementation */
}

static inline VOID Aarch64WriteSpEl0(UINT64 value)
{
    UNREFERENCED_PARAMETER(value); /* Placeholder implementation */
}

static inline UINT64 Aarch64ReadElrEl1(void)
{
    return 0; /* Placeholder implementation */
}

static inline VOID Aarch64WriteElrEl1(UINT64 value)
{
    UNREFERENCED_PARAMETER(value); /* Placeholder implementation */
}

static inline UINT64 Aarch64ReadSpsrEl1(void)
{
    return 0; /* Placeholder implementation */
}

#endif /* _KERN_ARCH_AARCH64_H_ */
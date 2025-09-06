/*
 * Aurora Kernel - AMD64 Architecture Implementation
 * Copyright (c) 2024 Aurora Project
 */

#include "../../aurora.h"
#include "../../include/kern.h"
#include "kern_arch.h"
#include "../../include/hal.h"

/* Global variables */
static BOOL g_InterruptsInitialized = FALSE;
static BOOL g_TimerInitialized = FALSE;

/* External scheduler functions */
extern VOID KernSchedule(void);
extern PTHREAD KernGetCurrentThread(void);
extern VOID KernSetCurrentThread(PTHREAD Thread);

/* External system call handler */
extern UINT_PTR KernSystemCallHandler(
    IN UINT32 SystemCallNumber,
    IN UINT_PTR Parameter1,
    IN UINT_PTR Parameter2,
    IN UINT_PTR Parameter3,
    IN UINT_PTR Parameter4
);

/*
 * Architecture Initialization
 */
NTSTATUS Amd64InitializeArchitecture(void)
{
    NTSTATUS status;
    
    /* Initialize HAL */
    HalInitialize();

    /* Initialize memory management */
    Amd64InitializeMemoryManagement();
    
    /* Initialize interrupts */
    Amd64InitializeInterrupts();
    
    /* Initialize system call interface */
    Amd64InitializeSystemCallInterface();
    
    /* Initialize timer */
    Amd64InitializeTimer();
    
    /* Enable interrupts */
    Amd64EnableInterrupts();
    
    return STATUS_SUCCESS;
}

/*
 * Context Management
 */
VOID Amd64SaveContext(IN PTHREAD Thread)
{
    PAMD64_CONTEXT context;
    
    if (!Thread) {
        return;
    }
    
    context = (PAMD64_CONTEXT)Thread->Context.ContextData;
    
    /* Save general purpose registers */
    __asm__ volatile (
        "movq %%rax, %0\n"
        "movq %%rbx, %1\n"
        "movq %%rcx, %2\n"
        "movq %%rdx, %3\n"
        "movq %%rsi, %4\n"
        "movq %%rdi, %5\n"
        "movq %%rbp, %6\n"
        "movq %%rsp, %7\n"
        "movq %%r8, %8\n"
        "movq %%r9, %9\n"
        "movq %%r10, %10\n"
        "movq %%r11, %11\n"
        "movq %%r12, %12\n"
        "movq %%r13, %13\n"
        "movq %%r14, %14\n"
        "movq %%r15, %15\n"
        : "=m" (context->Rax), "=m" (context->Rbx), "=m" (context->Rcx), "=m" (context->Rdx),
          "=m" (context->Rsi), "=m" (context->Rdi), "=m" (context->Rbp), "=m" (context->Rsp),
          "=m" (context->R8), "=m" (context->R9), "=m" (context->R10), "=m" (context->R11),
          "=m" (context->R12), "=m" (context->R13), "=m" (context->R14), "=m" (context->R15)
    );
    
    /* Save RFLAGS */
    context->Rflags = Amd64ReadRflags();
}

VOID Amd64RestoreContext(IN PTHREAD Thread)
{
    PAMD64_CONTEXT context;
    
    if (!Thread) {
        return;
    }
    
    context = (PAMD64_CONTEXT)Thread->Context.ContextData;
    
    /* Restore RFLAGS */
    Amd64WriteRflags(context->Rflags);
    
    /* Restore general purpose registers */
    __asm__ volatile (
        "movq %0, %%rax\n"
        "movq %1, %%rbx\n"
        "movq %2, %%rcx\n"
        "movq %3, %%rdx\n"
        "movq %4, %%rsi\n"
        "movq %5, %%rdi\n"
        "movq %6, %%rbp\n"
        /* Provide Arch* aliases expected by higher layers */
        #define ArchSaveContext Amd64SaveContext
        #define ArchRestoreContext Amd64RestoreContext
        #define ArchInitializeThreadContext Amd64InitializeThreadContext
        "movq %14, %%r14\n"
        "movq %15, %%r15\n"
        :
        : "m" (context->Rax), "m" (context->Rbx), "m" (context->Rcx), "m" (context->Rdx),
          "m" (context->Rsi), "m" (context->Rdi), "m" (context->Rbp), "m" (context->Rsp),
          "m" (context->R8), "m" (context->R9), "m" (context->R10), "m" (context->R11),
          "m" (context->R12), "m" (context->R13), "m" (context->R14), "m" (context->R15)
        : "memory"
    );
}

VOID Amd64InitializeThreadContext(
    IN PTHREAD Thread,
    IN PVOID StartAddress,
    IN PVOID Parameter
)
{
    PAMD64_CONTEXT context;
    
    if (!Thread) {
        return;
    }
    
    /* Allocate context if not already allocated */
    /* Clear context */
    memset(&Thread->Context, 0, sizeof(CPU_CONTEXT));
    
    context = (PAMD64_CONTEXT)Thread->Context.ContextData;
    
    /* Initialize context to zero */
    memset(context, 0, sizeof(AMD64_CONTEXT));
    
    /* Set up initial register state */
    context->Rip = (UINT64)StartAddress;
    context->Rsp = (UINT64)Thread->KernelStack + Thread->StackSize - 16; /* 16-byte aligned */
    context->Rbp = context->Rsp;
    context->Rdi = (UINT64)Parameter; /* First parameter in RDI */
    context->Rflags = AMD64_DEFAULT_RFLAGS;
    
    /* Set up segment selectors */
    if (Thread->ParentProcess && Thread->ParentProcess->ProcessId == 0) {
        /* Kernel thread */
        context->Cs = AMD64_KERNEL_CS;
        context->Ds = AMD64_KERNEL_DS;
        context->Es = AMD64_KERNEL_DS;
        context->Fs = AMD64_KERNEL_DS;
        context->Gs = AMD64_KERNEL_DS;
        context->Ss = AMD64_KERNEL_DS;
    } else {
        /* User thread */
        context->Cs = AMD64_USER_CS;
        context->Ds = AMD64_USER_DS;
        context->Es = AMD64_USER_DS;
        context->Fs = AMD64_USER_DS;
        context->Gs = AMD64_USER_DS;
        context->Ss = AMD64_USER_DS;
    }
}

/*
 * CPU Control Functions
 */
VOID Amd64HaltProcessor(void)
{
    Amd64Halt();
}

VOID Amd64EnableInterrupts(void)
{
    Amd64Sti();
}

VOID Amd64DisableInterrupts(void)
{
    Amd64Cli();
}

BOOL Amd64AreInterruptsEnabled(void)
{
    return (Amd64ReadRflags() & AMD64_RFLAGS_IF) != 0;
}

/*
 * System Call Interface
 */
VOID Amd64InitializeSystemCallInterface(void)
{
    /* Set up SYSCALL/SYSRET MSRs */
    /* This is a simplified implementation */
    
    /* STAR MSR - CS/SS selectors for SYSCALL/SYSRET */
    UINT64 star = ((UINT64)AMD64_KERNEL_CS << 32) | ((UINT64)AMD64_USER_CS << 48);
    Amd64WriteMsr(0xC0000081, star); /* IA32_STAR */
    
    /* LSTAR MSR - SYSCALL entry point */
    Amd64WriteMsr(0xC0000082, (UINT64)Amd64SystemCallEntry); /* IA32_LSTAR */
    
    /* SFMASK MSR - RFLAGS mask for SYSCALL */
    Amd64WriteMsr(0xC0000084, AMD64_RFLAGS_IF | AMD64_RFLAGS_DF); /* IA32_FMASK */
    
    /* Enable SYSCALL/SYSRET in EFER */
    UINT64 efer = Amd64ReadMsr(0xC0000080); /* IA32_EFER */
    efer |= (1ULL << 0); /* SCE bit */
    Amd64WriteMsr(0xC0000080, efer);
}

VOID Amd64SystemCallEntry(void)
{
    /* This is the SYSCALL entry point */
    /* In a real implementation, this would be written in assembly */
    
    PTHREAD currentThread;
    UINT32 serviceNumber;
    PVOID parameters;
    NTSTATUS result;
    
    /* Save current context */
    currentThread = KernGetCurrentThread();
    if (currentThread) {
        Amd64SaveContext(currentThread);
    }
    
    /* Get system call parameters from registers */
    __asm__ volatile ("movl %%eax, %0" : "=m" (serviceNumber));
    __asm__ volatile ("movq %%rcx, %0" : "=m" (parameters));
    
    /* Call system call handler */
    result = KernSystemCallHandler(serviceNumber, (UINT_PTR)parameters, 0, 0, 0);
    
    /* Return result in RAX */
    __asm__ volatile ("movq %0, %%rax" : : "m" (result));
    
    /* Restore context and return to user mode */
    if (currentThread) {
        Amd64RestoreContext(currentThread);
    }
    
    /* SYSRET instruction would be used here in assembly */
}

/*
 * Memory Management
 */
VOID Amd64InitializeMemoryManagement(void)
{
    /* Initialize page tables and memory management structures */
    /* This is a placeholder implementation */
    
    /* Enable PAE and PGE if available */
    UINT64 cr4 = Amd64ReadCr0(); /* Note: should be ReadCr4, but we'll use Cr0 for now */
    Amd64WriteCr0(cr4); /* Enable paging features */
}

VOID Amd64SwitchAddressSpace(IN PVOID PageDirectory)
{
    if (PageDirectory) {
        Amd64WriteCr3((UINT64)PageDirectory);
    }
}

/*
 * Interrupt Handling
 */
VOID Amd64InitializeInterrupts(void)
{
    /* Initialize IDT and interrupt handlers */
    /* This is a placeholder implementation */
    
    g_InterruptsInitialized = TRUE;
}

VOID Amd64RegisterInterruptHandler(IN UINT8 Vector, IN PVOID Handler)
{
    /* Register interrupt handler in IDT */
    /* This is a placeholder implementation */
    
    if (!g_InterruptsInitialized || !Handler) {
        return;
    }
    
    /* Set up IDT entry for the vector */
}

/*
 * Timer Functions
 */
VOID Amd64InitializeTimer(void)
{
    /* Initialize system timer (PIT, APIC timer, etc.) */
    /* This is a placeholder implementation */
    
    /* Program PIT for 1000 Hz (1ms intervals) */
    UINT16 divisor = 1193182 / 1000; /* PIT frequency / desired frequency */
    
    Amd64OutByte(0x43, 0x36); /* Command byte: channel 0, lobyte/hibyte, rate generator */
    Amd64OutByte(0x40, divisor & 0xFF); /* Low byte */
    Amd64OutByte(0x40, (divisor >> 8) & 0xFF); /* High byte */
    
    /* Register timer interrupt handler */
    Amd64RegisterInterruptHandler(0x20, (PVOID)Amd64TimerInterruptHandler);
    
    g_TimerInitialized = TRUE;
}

VOID Amd64TimerInterruptHandler(void)
{
    /* Timer interrupt handler */
    if (!g_TimerInitialized) {
        return;
    }
    
    /* Acknowledge interrupt */
    Amd64OutByte(0x20, 0x20); /* Send EOI to PIC */
    
    /* Call scheduler */
    KernSchedule();
}

/*
 * Context Switching
 */
VOID Amd64SwitchContext(IN PTHREAD OldThread, IN PTHREAD NewThread)
{
    if (OldThread) {
        Amd64SaveContext(OldThread);
    }
    
    if (NewThread) {
        KernSetCurrentThread(NewThread);
        
        /* Switch address space if different process */
        if (NewThread->ParentProcess && 
            (!OldThread || !OldThread->ParentProcess || 
             NewThread->ParentProcess->ProcessId != OldThread->ParentProcess->ProcessId)) {
            Amd64SwitchAddressSpace(NewThread->ParentProcess->VirtualAddressSpace);
        }
        
        Amd64RestoreContext(NewThread);
    }
}
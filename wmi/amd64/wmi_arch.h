/*
 * Aurora Kernel - WMI AMD64 Architecture Specific Header
 * Windows Management Instrumentation - x86_64 Implementation
 * Copyright (c) 2024 NTCore Project
 */

#ifndef _WMI_ARCH_AMD64_H_
#define _WMI_ARCH_AMD64_H_

/* AMD64 specific includes */
#include "../../aurora.h"

/* AMD64 Architecture Constants */
#define WMI_AMD64_PAGE_SIZE             4096
#define WMI_AMD64_CACHE_LINE_SIZE       64
#define WMI_AMD64_MAX_CPUS              256
#define WMI_AMD64_STACK_ALIGNMENT       16

/* AMD64 Performance Counter Definitions */
#define WMI_AMD64_PMC_CYCLES            0x00000001
#define WMI_AMD64_PMC_INSTRUCTIONS      0x00000002
#define WMI_AMD64_PMC_CACHE_MISSES      0x00000004
#define WMI_AMD64_PMC_BRANCH_MISSES     0x00000008
#define WMI_AMD64_PMC_TLB_MISSES        0x00000010

/* AMD64 MSR (Model Specific Register) Definitions */
#define WMI_AMD64_MSR_TSC               0x00000010
#define WMI_AMD64_MSR_APIC_BASE         0x0000001B
#define WMI_AMD64_MSR_EFER              0xC0000080
#define WMI_AMD64_MSR_STAR              0xC0000081
#define WMI_AMD64_MSR_LSTAR             0xC0000082
#define WMI_AMD64_MSR_CSTAR             0xC0000083
#define WMI_AMD64_MSR_SFMASK            0xC0000084
#define WMI_AMD64_MSR_KERNELGSBASE      0xC0000102

/* AMD64 CPU Information Structure */
typedef struct _WMI_AMD64_CPU_INFO {
    UINT32 ProcessorId;
    UINT32 Family;
    UINT32 Model;
    UINT32 Stepping;
    UINT32 Features[4];  /* CPUID feature flags */
    UINT64 Frequency;
    UINT32 CacheL1Size;
    UINT32 CacheL2Size;
    UINT32 CacheL3Size;
    CHAR VendorString[13];
    CHAR BrandString[49];
} WMI_AMD64_CPU_INFO, *PWMI_AMD64_CPU_INFO;

/* AMD64 Memory Information Structure */
typedef struct _WMI_AMD64_MEMORY_INFO {
    UINT64 TotalPhysicalMemory;
    UINT64 AvailablePhysicalMemory;
    UINT64 TotalVirtualMemory;
    UINT64 AvailableVirtualMemory;
    UINT64 PageSize;
    UINT64 AllocationGranularity;
    UINT32 PageFaultCount;
    UINT32 PageFileUsage;
} WMI_AMD64_MEMORY_INFO, *PWMI_AMD64_MEMORY_INFO;

/* AMD64 Performance Counter Structure */
typedef struct _WMI_AMD64_PERF_COUNTER {
    UINT32 CounterId;
    UINT32 CounterType;
    UINT64 CounterValue;
    UINT64 CounterFrequency;
    UINT64 TimeStamp;
} WMI_AMD64_PERF_COUNTER, *PWMI_AMD64_PERF_COUNTER;

/* AMD64 Context Structure */
typedef struct _WMI_AMD64_CONTEXT {
    UINT64 Rax, Rbx, Rcx, Rdx;
    UINT64 Rsi, Rdi, Rbp, Rsp;
    UINT64 R8, R9, R10, R11;
    UINT64 R12, R13, R14, R15;
    UINT64 Rip, Rflags;
    UINT16 Cs, Ds, Es, Fs, Gs, Ss;
    UINT64 Dr0, Dr1, Dr2, Dr3, Dr6, Dr7;
    UINT64 Cr0, Cr2, Cr3, Cr4, Cr8;
} WMI_AMD64_CONTEXT, *PWMI_AMD64_CONTEXT;

/* AMD64 Interrupt Frame Structure */
typedef struct _WMI_AMD64_INTERRUPT_FRAME {
    UINT64 Rip;
    UINT64 Cs;
    UINT64 Rflags;
    UINT64 Rsp;
    UINT64 Ss;
} WMI_AMD64_INTERRUPT_FRAME, *PWMI_AMD64_INTERRUPT_FRAME;

/* AMD64 Function Prototypes */

/* CPU Information Functions */
NTSTATUS WmiAmd64GetCpuInfo(OUT PWMI_AMD64_CPU_INFO CpuInfo);
NTSTATUS WmiAmd64GetCpuCount(OUT PUINT32 CpuCount);
NTSTATUS WmiAmd64GetCpuUsage(IN UINT32 CpuId, OUT PUINT32 Usage);
NTSTATUS WmiAmd64GetCpuFrequency(IN UINT32 CpuId, OUT PUINT64 Frequency);

/* Memory Management Functions */
NTSTATUS WmiAmd64GetMemoryInfo(OUT PWMI_AMD64_MEMORY_INFO MemInfo);
NTSTATUS WmiAmd64GetPageFaultCount(OUT PUINT32 PageFaults);
NTSTATUS WmiAmd64GetMemoryUsage(OUT PUINT64 UsedMemory, OUT PUINT64 TotalMemory);

/* Performance Counter Functions */
NTSTATUS WmiAmd64InitPerformanceCounters(void);
NTSTATUS WmiAmd64ReadPerformanceCounter(IN UINT32 CounterId, OUT PWMI_AMD64_PERF_COUNTER Counter);
NTSTATUS WmiAmd64StartPerformanceCounter(IN UINT32 CounterId, IN UINT32 CounterType);
NTSTATUS WmiAmd64StopPerformanceCounter(IN UINT32 CounterId);

/* MSR (Model Specific Register) Functions */
UINT64 WmiAmd64ReadMsr(IN UINT32 MsrIndex);
void WmiAmd64WriteMsr(IN UINT32 MsrIndex, IN UINT64 Value);
NTSTATUS WmiAmd64GetTimeStampCounter(OUT PUINT64 Tsc);

/* Context and State Functions */
NTSTATUS WmiAmd64SaveContext(OUT PWMI_AMD64_CONTEXT Context);
NTSTATUS WmiAmd64RestoreContext(IN PWMI_AMD64_CONTEXT Context);
NTSTATUS WmiAmd64GetCurrentContext(OUT PWMI_AMD64_CONTEXT Context);

/* Cache Management Functions */
NTSTATUS WmiAmd64FlushCache(void);
NTSTATUS WmiAmd64InvalidateCache(IN PVOID Address, IN UINT64 Size);
NTSTATUS WmiAmd64GetCacheInfo(OUT PUINT32 L1Size, OUT PUINT32 L2Size, OUT PUINT32 L3Size);

/* Interrupt and Exception Handling */
NTSTATUS WmiAmd64RegisterInterruptHandler(IN UINT32 Vector, IN PVOID Handler);
NTSTATUS WmiAmd64UnregisterInterruptHandler(IN UINT32 Vector);
NTSTATUS WmiAmd64GetInterruptCount(IN UINT32 Vector, OUT PUINT32 Count);

/* Debug and Tracing Functions */
NTSTATUS WmiAmd64EnableTracing(IN UINT32 TraceFlags);
NTSTATUS WmiAmd64DisableTracing(void);
NTSTATUS WmiAmd64GetTraceData(OUT PVOID Buffer, IN OUT PUINT32 BufferSize);

/* Low-level Assembly Functions */
void WmiAmd64Cpuid(IN UINT32 Function, OUT PUINT32 Eax, OUT PUINT32 Ebx, OUT PUINT32 Ecx, OUT PUINT32 Edx);
void WmiAmd64Halt(void);
void WmiAmd64Pause(void);
UINT64 WmiAmd64GetRflags(void);
void WmiAmd64SetRflags(IN UINT64 Rflags);
UINT64 WmiAmd64GetCr0(void);
void WmiAmd64SetCr0(IN UINT64 Cr0);
UINT64 WmiAmd64GetCr3(void);
void WmiAmd64SetCr3(IN UINT64 Cr3);
UINT64 WmiAmd64GetCr4(void);
void WmiAmd64SetCr4(IN UINT64 Cr4);

/* Memory Barrier Functions */
void WmiAmd64MemoryBarrier(void);
void WmiAmd64LoadFence(void);
void WmiAmd64StoreFence(void);
void WmiAmd64FullFence(void);

/* Atomic Operations */
UINT32 WmiAmd64AtomicIncrement(IN volatile UINT32* Value);
UINT32 WmiAmd64AtomicDecrement(IN volatile UINT32* Value);
UINT32 WmiAmd64AtomicExchange(IN volatile UINT32* Target, IN UINT32 Value);
UINT32 WmiAmd64AtomicCompareExchange(IN volatile UINT32* Target, IN UINT32 Exchange, IN UINT32 Comparand);

/* Utility Macros */
#define WMI_AMD64_ALIGN_PAGE(addr) AURORA_ALIGN_UP(addr, WMI_AMD64_PAGE_SIZE)
#define WMI_AMD64_ALIGN_CACHE(addr) AURORA_ALIGN_UP(addr, WMI_AMD64_CACHE_LINE_SIZE)
#define WMI_AMD64_ALIGN_STACK(addr) AURORA_ALIGN_DOWN(addr, WMI_AMD64_STACK_ALIGNMENT)

/* Feature Detection Macros */
#define WMI_AMD64_HAS_SSE(features) ((features[3] & (1 << 25)) != 0)
#define WMI_AMD64_HAS_SSE2(features) ((features[3] & (1 << 26)) != 0)
#define WMI_AMD64_HAS_SSE3(features) ((features[2] & (1 << 0)) != 0)
#define WMI_AMD64_HAS_SSSE3(features) ((features[2] & (1 << 9)) != 0)
#define WMI_AMD64_HAS_SSE41(features) ((features[2] & (1 << 19)) != 0)
#define WMI_AMD64_HAS_SSE42(features) ((features[2] & (1 << 20)) != 0)
#define WMI_AMD64_HAS_AVX(features) ((features[2] & (1 << 28)) != 0)
#define WMI_AMD64_HAS_AVX2(features) ((features[1] & (1 << 5)) != 0)

/* Performance Counter Macros */
#define WMI_AMD64_START_PMC(id) WmiAmd64StartPerformanceCounter(id, WMI_AMD64_PMC_CYCLES)
#define WMI_AMD64_STOP_PMC(id) WmiAmd64StopPerformanceCounter(id)
#define WMI_AMD64_READ_TSC() WmiAmd64ReadMsr(WMI_AMD64_MSR_TSC)

/* Debug Macros */
#ifdef DEBUG
    #define WMI_AMD64_DEBUG_PRINT(Format, ...) AuroraDebugPrint("[WMI-AMD64] " Format, ##__VA_ARGS__)
#else
    #define WMI_AMD64_DEBUG_PRINT(Format, ...)
#endif

#endif /* _WMI_ARCH_AMD64_H_ */
/*
 * Aurora Kernel - WMI AArch64 Architecture Specific Header
 * Windows Management Instrumentation - ARM64 Implementation
 * Copyright (c) 2024 NTCore Project
 */

#ifndef _WMI_ARCH_AARCH64_H_
#define _WMI_ARCH_AARCH64_H_

/* AArch64 specific includes */
#include "../../aurora.h"

/* AArch64 Architecture Constants */
#define WMI_AARCH64_PAGE_SIZE           4096
#define WMI_AARCH64_CACHE_LINE_SIZE     64
#define WMI_AARCH64_MAX_CPUS            64
#define WMI_AARCH64_STACK_ALIGNMENT     16

/* AArch64 Performance Counter Definitions */
#define WMI_AARCH64_PMC_CYCLES          0x00000001
#define WMI_AARCH64_PMC_INSTRUCTIONS    0x00000002
#define WMI_AARCH64_PMC_CACHE_MISSES    0x00000004
#define WMI_AARCH64_PMC_BRANCH_MISSES   0x00000008
#define WMI_AARCH64_PMC_TLB_MISSES      0x00000010

/* AArch64 System Register Definitions */
#define WMI_AARCH64_SYSREG_MIDR_EL1     "S3_0_C0_C0_0"
#define WMI_AARCH64_SYSREG_MPIDR_EL1    "S3_0_C0_C0_5"
#define WMI_AARCH64_SYSREG_REVIDR_EL1   "S3_0_C0_C0_6"
#define WMI_AARCH64_SYSREG_CNTFRQ_EL0   "S3_3_C14_C0_0"
#define WMI_AARCH64_SYSREG_CNTVCT_EL0   "S3_3_C14_C0_2"
#define WMI_AARCH64_SYSREG_CNTPCT_EL0   "S3_3_C14_C0_1"

/* AArch64 Exception Level Definitions */
#define WMI_AARCH64_EL0                 0
#define WMI_AARCH64_EL1                 1
#define WMI_AARCH64_EL2                 2
#define WMI_AARCH64_EL3                 3

/* AArch64 CPU Information Structure */
typedef struct _WMI_AARCH64_CPU_INFO {
    UINT32 ProcessorId;
    UINT32 Implementer;
    UINT32 Variant;
    UINT32 Architecture;
    UINT32 PartNumber;
    UINT32 Revision;
    UINT64 Features[4];  /* ID_AA64PFR0_EL1, ID_AA64PFR1_EL1, ID_AA64ISAR0_EL1, ID_AA64ISAR1_EL1 */
    UINT64 Frequency;
    UINT32 CacheL1ISize;
    UINT32 CacheL1DSize;
    UINT32 CacheL2Size;
    UINT32 CacheL3Size;
    CHAR ImplementerName[32];
    CHAR PartName[32];
} WMI_AARCH64_CPU_INFO, *PWMI_AARCH64_CPU_INFO;

/* AArch64 Memory Information Structure */
typedef struct _WMI_AARCH64_MEMORY_INFO {
    UINT64 TotalPhysicalMemory;
    UINT64 AvailablePhysicalMemory;
    UINT64 TotalVirtualMemory;
    UINT64 AvailableVirtualMemory;
    UINT64 PageSize;
    UINT64 AllocationGranularity;
    UINT32 PageFaultCount;
    UINT32 PageFileUsage;
    UINT32 TranslationGranule;
    UINT32 AddressSpaceSize;
} WMI_AARCH64_MEMORY_INFO, *PWMI_AARCH64_MEMORY_INFO;

/* AArch64 Performance Counter Structure */
typedef struct _WMI_AARCH64_PERF_COUNTER {
    UINT32 CounterId;
    UINT32 CounterType;
    UINT64 CounterValue;
    UINT64 CounterFrequency;
    UINT64 TimeStamp;
    UINT32 EventType;
} WMI_AARCH64_PERF_COUNTER, *PWMI_AARCH64_PERF_COUNTER;

/* AArch64 Context Structure */
typedef struct _WMI_AARCH64_CONTEXT {
    UINT64 X[31];       /* General purpose registers X0-X30 */
    UINT64 Sp;          /* Stack pointer */
    UINT64 Pc;          /* Program counter */
    UINT64 Pstate;      /* Processor state */
    UINT64 Tpidr_el0;   /* Thread ID register */
    UINT64 Tpidrro_el0; /* Read-only thread ID register */
    UINT64 Elr_el1;     /* Exception link register */
    UINT64 Spsr_el1;    /* Saved program status register */
    UINT64 Far_el1;     /* Fault address register */
    UINT64 Esr_el1;     /* Exception syndrome register */
} WMI_AARCH64_CONTEXT, *PWMI_AARCH64_CONTEXT;

/* AArch64 Exception Frame Structure */
typedef struct _WMI_AARCH64_EXCEPTION_FRAME {
    UINT64 X[31];
    UINT64 Sp;
    UINT64 Pc;
    UINT64 Pstate;
} WMI_AARCH64_EXCEPTION_FRAME, *PWMI_AARCH64_EXCEPTION_FRAME;

/* AArch64 SIMD/FP Context */
typedef struct _WMI_AARCH64_SIMD_CONTEXT {
    UINT64 V[32][2];    /* SIMD/FP registers V0-V31 (128-bit each) */
    UINT32 Fpcr;        /* Floating-point control register */
    UINT32 Fpsr;        /* Floating-point status register */
} WMI_AARCH64_SIMD_CONTEXT, *PWMI_AARCH64_SIMD_CONTEXT;

/* AArch64 Function Prototypes */

/* CPU Information Functions */
NTSTATUS WmiAarch64GetCpuInfo(OUT PWMI_AARCH64_CPU_INFO CpuInfo);
NTSTATUS WmiAarch64GetCpuCount(OUT PUINT32 CpuCount);
NTSTATUS WmiAarch64GetCpuUsage(IN UINT32 CpuId, OUT PUINT32 Usage);
NTSTATUS WmiAarch64GetCpuFrequency(IN UINT32 CpuId, OUT PUINT64 Frequency);
NTSTATUS WmiAarch64GetCurrentEL(OUT PUINT32 ExceptionLevel);

/* Memory Management Functions */
NTSTATUS WmiAarch64GetMemoryInfo(OUT PWMI_AARCH64_MEMORY_INFO MemInfo);
NTSTATUS WmiAarch64GetPageFaultCount(OUT PUINT32 PageFaults);
NTSTATUS WmiAarch64GetMemoryUsage(OUT PUINT64 UsedMemory, OUT PUINT64 TotalMemory);
NTSTATUS WmiAarch64GetTranslationGranule(OUT PUINT32 Granule);

/* Performance Counter Functions */
NTSTATUS WmiAarch64InitPerformanceCounters(void);
NTSTATUS WmiAarch64ReadPerformanceCounter(IN UINT32 CounterId, OUT PWMI_AARCH64_PERF_COUNTER Counter);
NTSTATUS WmiAarch64StartPerformanceCounter(IN UINT32 CounterId, IN UINT32 CounterType);
NTSTATUS WmiAarch64StopPerformanceCounter(IN UINT32 CounterId);
NTSTATUS WmiAarch64ConfigurePerformanceCounter(IN UINT32 CounterId, IN UINT32 EventType);

/* System Register Functions */
UINT64 WmiAarch64ReadSystemRegister(IN PCHAR RegName);
void WmiAarch64WriteSystemRegister(IN PCHAR RegName, IN UINT64 Value);
NTSTATUS WmiAarch64GetProcessorId(OUT PUINT64 ProcessorId);
NTSTATUS WmiAarch64GetCounterFrequency(OUT PUINT64 Frequency);
UINT64 WmiAarch64GetSystemCounter(void);
UINT64 WmiAarch64GetVirtualCounter(void);

/* Context and State Functions */
NTSTATUS WmiAarch64SaveContext(OUT PWMI_AARCH64_CONTEXT Context);
NTSTATUS WmiAarch64RestoreContext(IN PWMI_AARCH64_CONTEXT Context);
NTSTATUS WmiAarch64GetCurrentContext(OUT PWMI_AARCH64_CONTEXT Context);
NTSTATUS WmiAarch64SaveSimdContext(OUT PWMI_AARCH64_SIMD_CONTEXT SimdContext);
NTSTATUS WmiAarch64RestoreSimdContext(IN PWMI_AARCH64_SIMD_CONTEXT SimdContext);

/* Cache Management Functions */
NTSTATUS WmiAarch64FlushCache(void);
NTSTATUS WmiAarch64InvalidateCache(IN PVOID Address, IN UINT64 Size);
NTSTATUS WmiAarch64GetCacheInfo(OUT PUINT32 L1ISize, OUT PUINT32 L1DSize, OUT PUINT32 L2Size, OUT PUINT32 L3Size);
NTSTATUS WmiAarch64FlushCacheRange(IN PVOID Address, IN UINT64 Size);
NTSTATUS WmiAarch64InvalidateCacheRange(IN PVOID Address, IN UINT64 Size);

/* Interrupt and Exception Handling */
NTSTATUS WmiAarch64RegisterExceptionHandler(IN UINT32 ExceptionType, IN PVOID Handler);
NTSTATUS WmiAarch64UnregisterExceptionHandler(IN UINT32 ExceptionType);
NTSTATUS WmiAarch64GetExceptionCount(IN UINT32 ExceptionType, OUT PUINT32 Count);
NTSTATUS WmiAarch64EnableInterrupts(void);
NTSTATUS WmiAarch64DisableInterrupts(void);

/* Debug and Tracing Functions */
NTSTATUS WmiAarch64EnableTracing(IN UINT32 TraceFlags);
NTSTATUS WmiAarch64DisableTracing(void);
NTSTATUS WmiAarch64GetTraceData(OUT PVOID Buffer, IN OUT PUINT32 BufferSize);
NTSTATUS WmiAarch64SetBreakpoint(IN PVOID Address, IN UINT32 Type);
NTSTATUS WmiAarch64ClearBreakpoint(IN PVOID Address);

/* Low-level Assembly Functions */
void WmiAarch64Wfi(void);           /* Wait for interrupt */
void WmiAarch64Wfe(void);           /* Wait for event */
void WmiAarch64Sev(void);           /* Send event */
void WmiAarch64Sevl(void);          /* Send event local */
void WmiAarch64Yield(void);         /* Yield */
void WmiAarch64Nop(void);           /* No operation */
void WmiAarch64Isb(void);           /* Instruction synchronization barrier */
void WmiAarch64Dsb(void);           /* Data synchronization barrier */
void WmiAarch64Dmb(void);           /* Data memory barrier */

/* Memory Barrier Functions */
void WmiAarch64MemoryBarrier(void);
void WmiAarch64LoadAcquire(void);
void WmiAarch64StoreRelease(void);
void WmiAarch64FullBarrier(void);

/* Atomic Operations */
UINT32 WmiAarch64AtomicIncrement(IN volatile UINT32* Value);
UINT32 WmiAarch64AtomicDecrement(IN volatile UINT32* Value);
UINT32 WmiAarch64AtomicExchange(IN volatile UINT32* Target, IN UINT32 Value);
UINT32 WmiAarch64AtomicCompareExchange(IN volatile UINT32* Target, IN UINT32 Exchange, IN UINT32 Comparand);
UINT64 WmiAarch64AtomicIncrement64(IN volatile UINT64* Value);
UINT64 WmiAarch64AtomicDecrement64(IN volatile UINT64* Value);
UINT64 WmiAarch64AtomicExchange64(IN volatile UINT64* Target, IN UINT64 Value);
UINT64 WmiAarch64AtomicCompareExchange64(IN volatile UINT64* Target, IN UINT64 Exchange, IN UINT64 Comparand);

/* SIMD/NEON Functions */
BOOL WmiAarch64HasNeon(void);
BOOL WmiAarch64HasCrypto(void);
BOOL WmiAarch64HasSve(void);
NTSTATUS WmiAarch64EnableNeon(void);
NTSTATUS WmiAarch64DisableNeon(void);

/* Utility Macros */
#define WMI_AARCH64_ALIGN_PAGE(addr) AURORA_ALIGN_UP(addr, WMI_AARCH64_PAGE_SIZE)
#define WMI_AARCH64_ALIGN_CACHE(addr) AURORA_ALIGN_UP(addr, WMI_AARCH64_CACHE_LINE_SIZE)
#define WMI_AARCH64_ALIGN_STACK(addr) AURORA_ALIGN_DOWN(addr, WMI_AARCH64_STACK_ALIGNMENT)

/* Feature Detection Macros */
#define WMI_AARCH64_HAS_EL0(features) (((features[0] >> 0) & 0xF) != 0)
#define WMI_AARCH64_HAS_EL1(features) (((features[0] >> 4) & 0xF) != 0)
#define WMI_AARCH64_HAS_EL2(features) (((features[0] >> 8) & 0xF) != 0)
#define WMI_AARCH64_HAS_EL3(features) (((features[0] >> 12) & 0xF) != 0)
#define WMI_AARCH64_HAS_FP(features) (((features[0] >> 16) & 0xF) != 0xF)
#define WMI_AARCH64_HAS_ADVSIMD(features) (((features[0] >> 20) & 0xF) != 0xF)
#define WMI_AARCH64_HAS_GIC(features) (((features[0] >> 24) & 0xF) != 0)
#define WMI_AARCH64_HAS_RAS(features) (((features[0] >> 28) & 0xF) != 0)
#define WMI_AARCH64_HAS_SVE(features) (((features[0] >> 32) & 0xF) != 0)
#define WMI_AARCH64_HAS_SEL2(features) (((features[0] >> 36) & 0xF) != 0)
#define WMI_AARCH64_HAS_MPAM(features) (((features[0] >> 40) & 0xF) != 0)
#define WMI_AARCH64_HAS_AMU(features) (((features[0] >> 44) & 0xF) != 0)
#define WMI_AARCH64_HAS_DIT(features) (((features[0] >> 48) & 0xF) != 0)
#define WMI_AARCH64_HAS_CSV2(features) (((features[0] >> 56) & 0xF) != 0)
#define WMI_AARCH64_HAS_CSV3(features) (((features[0] >> 60) & 0xF) != 0)

/* Instruction Set Architecture Features */
#define WMI_AARCH64_HAS_AES(features) (((features[2] >> 4) & 0xF) != 0)
#define WMI_AARCH64_HAS_PMULL(features) (((features[2] >> 4) & 0xF) >= 2)
#define WMI_AARCH64_HAS_SHA1(features) (((features[2] >> 8) & 0xF) != 0)
#define WMI_AARCH64_HAS_SHA256(features) (((features[2] >> 12) & 0xF) != 0)
#define WMI_AARCH64_HAS_SHA512(features) (((features[2] >> 12) & 0xF) >= 2)
#define WMI_AARCH64_HAS_CRC32(features) (((features[2] >> 16) & 0xF) != 0)
#define WMI_AARCH64_HAS_ATOMIC(features) (((features[2] >> 20) & 0xF) != 0)
#define WMI_AARCH64_HAS_RDM(features) (((features[2] >> 28) & 0xF) != 0)
#define WMI_AARCH64_HAS_SHA3(features) (((features[2] >> 32) & 0xF) != 0)
#define WMI_AARCH64_HAS_SM3(features) (((features[2] >> 36) & 0xF) != 0)
#define WMI_AARCH64_HAS_SM4(features) (((features[2] >> 40) & 0xF) != 0)
#define WMI_AARCH64_HAS_DP(features) (((features[2] >> 44) & 0xF) != 0)
#define WMI_AARCH64_HAS_FHM(features) (((features[2] >> 48) & 0xF) != 0)
#define WMI_AARCH64_HAS_TS(features) (((features[2] >> 52) & 0xF) != 0)
#define WMI_AARCH64_HAS_TLB(features) (((features[2] >> 56) & 0xF) != 0)
#define WMI_AARCH64_HAS_RNDR(features) (((features[2] >> 60) & 0xF) != 0)

/* Performance Counter Macros */
#define WMI_AARCH64_START_PMC(id) WmiAarch64StartPerformanceCounter(id, WMI_AARCH64_PMC_CYCLES)
#define WMI_AARCH64_STOP_PMC(id) WmiAarch64StopPerformanceCounter(id)
#define WMI_AARCH64_READ_COUNTER() WmiAarch64GetSystemCounter()

/* Exception Level Macros */
#define WMI_AARCH64_GET_CURRENT_EL() ((WmiAarch64ReadSystemRegister("CurrentEL") >> 2) & 3)
#define WMI_AARCH64_IS_EL0() (WMI_AARCH64_GET_CURRENT_EL() == WMI_AARCH64_EL0)
#define WMI_AARCH64_IS_EL1() (WMI_AARCH64_GET_CURRENT_EL() == WMI_AARCH64_EL1)
#define WMI_AARCH64_IS_EL2() (WMI_AARCH64_GET_CURRENT_EL() == WMI_AARCH64_EL2)
#define WMI_AARCH64_IS_EL3() (WMI_AARCH64_GET_CURRENT_EL() == WMI_AARCH64_EL3)

/* Debug Macros */
#ifdef DEBUG
    #define WMI_AARCH64_DEBUG_PRINT(Format, ...) AuroraDebugPrint("[WMI-AARCH64] " Format, ##__VA_ARGS__)
#else
    #define WMI_AARCH64_DEBUG_PRINT(Format, ...)
#endif

/* ARM Implementer IDs */
#define WMI_AARCH64_IMPLEMENTER_ARM     0x41
#define WMI_AARCH64_IMPLEMENTER_CAVIUM  0x43
#define WMI_AARCH64_IMPLEMENTER_NVIDIA  0x4E
#define WMI_AARCH64_IMPLEMENTER_APM     0x50
#define WMI_AARCH64_IMPLEMENTER_QUALCOMM 0x51
#define WMI_AARCH64_IMPLEMENTER_SAMSUNG 0x53
#define WMI_AARCH64_IMPLEMENTER_APPLE   0x61

/* Common ARM Part Numbers */
#define WMI_AARCH64_PART_CORTEX_A53     0xD03
#define WMI_AARCH64_PART_CORTEX_A57     0xD07
#define WMI_AARCH64_PART_CORTEX_A72     0xD08
#define WMI_AARCH64_PART_CORTEX_A73     0xD09
#define WMI_AARCH64_PART_CORTEX_A75     0xD0A
#define WMI_AARCH64_PART_CORTEX_A76     0xD0B
#define WMI_AARCH64_PART_CORTEX_A77     0xD0D
#define WMI_AARCH64_PART_CORTEX_A78     0xD41
#define WMI_AARCH64_PART_CORTEX_X1      0xD44
#define WMI_AARCH64_PART_NEOVERSE_N1    0xD0C
#define WMI_AARCH64_PART_NEOVERSE_V1    0xD40

#endif /* _WMI_ARCH_AARCH64_H_ */
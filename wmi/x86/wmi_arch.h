/*
 * Aurora Kernel - WMI x86 Architecture Specific Header
 * Windows Management Instrumentation - x86 (32-bit) Implementation
 * Copyright (c) 2024 NTCore Project
 */

#ifndef _WMI_ARCH_X86_H_
#define _WMI_ARCH_X86_H_

/* x86 specific includes */
#include "../../aurora.h"

/* x86 Architecture Constants */
#define WMI_X86_PAGE_SIZE               4096
#define WMI_X86_CACHE_LINE_SIZE         32
#define WMI_X86_MAX_CPUS                32
#define WMI_X86_STACK_ALIGNMENT         4
#define WMI_X86_MAX_VIRTUAL_ADDRESS     0xFFFFFFFF

/* x86 Performance Counter Definitions */
#define WMI_X86_PMC_CYCLES              0x00000001
#define WMI_X86_PMC_INSTRUCTIONS        0x00000002
#define WMI_X86_PMC_CACHE_MISSES        0x00000004
#define WMI_X86_PMC_BRANCH_MISSES       0x00000008
#define WMI_X86_PMC_TLB_MISSES          0x00000010

/* x86 MSR (Model Specific Register) Definitions */
#define WMI_X86_MSR_TSC                 0x00000010
#define WMI_X86_MSR_APIC_BASE           0x0000001B
#define WMI_X86_MSR_MTRR_CAP            0x000000FE
#define WMI_X86_MSR_SYSENTER_CS         0x00000174
#define WMI_X86_MSR_SYSENTER_ESP        0x00000175
#define WMI_X86_MSR_SYSENTER_EIP        0x00000176

/* x86 CPU Information Structure */
typedef struct _WMI_X86_CPU_INFO {
    UINT32 ProcessorId;
    UINT32 Family;
    UINT32 Model;
    UINT32 Stepping;
    UINT32 Features[2];  /* CPUID feature flags (EDX, ECX) */
    UINT32 Frequency;
    UINT32 CacheL1Size;
    UINT32 CacheL2Size;
    CHAR VendorString[13];
    CHAR BrandString[49];
    BOOL HasMMX;
    BOOL HasSSE;
    BOOL HasSSE2;
} WMI_X86_CPU_INFO, *PWMI_X86_CPU_INFO;

/* x86 Memory Information Structure */
typedef struct _WMI_X86_MEMORY_INFO {
    UINT32 TotalPhysicalMemory;
    UINT32 AvailablePhysicalMemory;
    UINT32 TotalVirtualMemory;
    UINT32 AvailableVirtualMemory;
    UINT32 PageSize;
    UINT32 AllocationGranularity;
    UINT32 PageFaultCount;
    UINT32 PageFileUsage;
} WMI_X86_MEMORY_INFO, *PWMI_X86_MEMORY_INFO;

/* x86 Performance Counter Structure */
typedef struct _WMI_X86_PERF_COUNTER {
    UINT32 CounterId;
    UINT32 CounterType;
    UINT32 CounterValueLow;
    UINT32 CounterValueHigh;
    UINT32 CounterFrequency;
    UINT32 TimeStampLow;
    UINT32 TimeStampHigh;
} WMI_X86_PERF_COUNTER, *PWMI_X86_PERF_COUNTER;

/* x86 Context Structure */
typedef struct _WMI_X86_CONTEXT {
    UINT32 Eax, Ebx, Ecx, Edx;
    UINT32 Esi, Edi, Ebp, Esp;
    UINT32 Eip, Eflags;
    UINT16 Cs, Ds, Es, Fs, Gs, Ss;
    UINT32 Dr0, Dr1, Dr2, Dr3, Dr6, Dr7;
    UINT32 Cr0, Cr2, Cr3, Cr4;
} WMI_X86_CONTEXT, *PWMI_X86_CONTEXT;

/* x86 Interrupt Frame Structure */
typedef struct _WMI_X86_INTERRUPT_FRAME {
    UINT32 Eip;
    UINT32 Cs;
    UINT32 Eflags;
    UINT32 Esp;
    UINT32 Ss;
} WMI_X86_INTERRUPT_FRAME, *PWMI_X86_INTERRUPT_FRAME;

/* x86 Segment Descriptor */
typedef struct _WMI_X86_SEGMENT_DESCRIPTOR {
    UINT16 Limit0;
    UINT16 Base0;
    UINT8 Base1;
    UINT8 Access;
    UINT8 Granularity;
    UINT8 Base2;
} WMI_X86_SEGMENT_DESCRIPTOR, *PWMI_X86_SEGMENT_DESCRIPTOR;

/* x86 Function Prototypes */

/* CPU Information Functions */
NTSTATUS WmiX86GetCpuInfo(OUT PWMI_X86_CPU_INFO CpuInfo);
NTSTATUS WmiX86GetCpuCount(OUT PUINT32 CpuCount);
NTSTATUS WmiX86GetCpuUsage(IN UINT32 CpuId, OUT PUINT32 Usage);
NTSTATUS WmiX86GetCpuFrequency(IN UINT32 CpuId, OUT PUINT32 Frequency);

/* Memory Management Functions */
NTSTATUS WmiX86GetMemoryInfo(OUT PWMI_X86_MEMORY_INFO MemInfo);
NTSTATUS WmiX86GetPageFaultCount(OUT PUINT32 PageFaults);
NTSTATUS WmiX86GetMemoryUsage(OUT PUINT32 UsedMemory, OUT PUINT32 TotalMemory);

/* Performance Counter Functions */
NTSTATUS WmiX86InitPerformanceCounters(void);
NTSTATUS WmiX86ReadPerformanceCounter(IN UINT32 CounterId, OUT PWMI_X86_PERF_COUNTER Counter);
NTSTATUS WmiX86StartPerformanceCounter(IN UINT32 CounterId, IN UINT32 CounterType);
NTSTATUS WmiX86StopPerformanceCounter(IN UINT32 CounterId);

/* MSR (Model Specific Register) Functions */
UINT64 WmiX86ReadMsr(IN UINT32 MsrIndex);
void WmiX86WriteMsr(IN UINT32 MsrIndex, IN UINT64 Value);
NTSTATUS WmiX86GetTimeStampCounter(OUT PUINT32 TscLow, OUT PUINT32 TscHigh);

/* Context and State Functions */
NTSTATUS WmiX86SaveContext(OUT PWMI_X86_CONTEXT Context);
NTSTATUS WmiX86RestoreContext(IN PWMI_X86_CONTEXT Context);
NTSTATUS WmiX86GetCurrentContext(OUT PWMI_X86_CONTEXT Context);

/* Cache Management Functions */
NTSTATUS WmiX86FlushCache(void);
NTSTATUS WmiX86InvalidateCache(IN PVOID Address, IN UINT32 Size);
NTSTATUS WmiX86GetCacheInfo(OUT PUINT32 L1Size, OUT PUINT32 L2Size);

/* Interrupt and Exception Handling */
NTSTATUS WmiX86RegisterInterruptHandler(IN UINT32 Vector, IN PVOID Handler);
NTSTATUS WmiX86UnregisterInterruptHandler(IN UINT32 Vector);
NTSTATUS WmiX86GetInterruptCount(IN UINT32 Vector, OUT PUINT32 Count);

/* Debug and Tracing Functions */
NTSTATUS WmiX86EnableTracing(IN UINT32 TraceFlags);
NTSTATUS WmiX86DisableTracing(void);
NTSTATUS WmiX86GetTraceData(OUT PVOID Buffer, IN OUT PUINT32 BufferSize);

/* Low-level Assembly Functions */
void WmiX86Cpuid(IN UINT32 Function, OUT PUINT32 Eax, OUT PUINT32 Ebx, OUT PUINT32 Ecx, OUT PUINT32 Edx);
void WmiX86Halt(void);
UINT32 WmiX86GetEflags(void);
void WmiX86SetEflags(IN UINT32 Eflags);
UINT32 WmiX86GetCr0(void);
void WmiX86SetCr0(IN UINT32 Cr0);
UINT32 WmiX86GetCr3(void);
void WmiX86SetCr3(IN UINT32 Cr3);
UINT32 WmiX86GetCr4(void);
void WmiX86SetCr4(IN UINT32 Cr4);

/* Segment Management Functions */
NTSTATUS WmiX86LoadGdt(IN PWMI_X86_SEGMENT_DESCRIPTOR Gdt, IN UINT16 Size);
NTSTATUS WmiX86LoadIdt(IN PVOID Idt, IN UINT16 Size);
UINT16 WmiX86GetCs(void);
UINT16 WmiX86GetDs(void);
UINT16 WmiX86GetEs(void);
UINT16 WmiX86GetFs(void);
UINT16 WmiX86GetGs(void);
UINT16 WmiX86GetSs(void);

/* Port I/O Functions */
UINT8 WmiX86InByte(IN UINT16 Port);
UINT16 WmiX86InWord(IN UINT16 Port);
UINT32 WmiX86InDword(IN UINT16 Port);
void WmiX86OutByte(IN UINT16 Port, IN UINT8 Value);
void WmiX86OutWord(IN UINT16 Port, IN UINT16 Value);
void WmiX86OutDword(IN UINT16 Port, IN UINT32 Value);

/* Atomic Operations */
UINT32 WmiX86AtomicIncrement(IN volatile UINT32* Value);
UINT32 WmiX86AtomicDecrement(IN volatile UINT32* Value);
UINT32 WmiX86AtomicExchange(IN volatile UINT32* Target, IN UINT32 Value);
UINT32 WmiX86AtomicCompareExchange(IN volatile UINT32* Target, IN UINT32 Exchange, IN UINT32 Comparand);

/* FPU and SIMD Functions */
NTSTATUS WmiX86SaveFpuState(OUT PVOID FpuState);
NTSTATUS WmiX86RestoreFpuState(IN PVOID FpuState);
NTSTATUS WmiX86InitFpu(void);
BOOL WmiX86HasFpu(void);
BOOL WmiX86HasMmx(void);
BOOL WmiX86HasSse(void);
BOOL WmiX86HasSse2(void);

/* Utility Macros */
#define WMI_X86_ALIGN_PAGE(addr) AURORA_ALIGN_UP(addr, WMI_X86_PAGE_SIZE)
#define WMI_X86_ALIGN_CACHE(addr) AURORA_ALIGN_UP(addr, WMI_X86_CACHE_LINE_SIZE)
#define WMI_X86_ALIGN_STACK(addr) AURORA_ALIGN_DOWN(addr, WMI_X86_STACK_ALIGNMENT)

/* Feature Detection Macros */
#define WMI_X86_HAS_FPU(features) ((features[0] & (1 << 0)) != 0)
#define WMI_X86_HAS_VME(features) ((features[0] & (1 << 1)) != 0)
#define WMI_X86_HAS_DE(features) ((features[0] & (1 << 2)) != 0)
#define WMI_X86_HAS_PSE(features) ((features[0] & (1 << 3)) != 0)
#define WMI_X86_HAS_TSC(features) ((features[0] & (1 << 4)) != 0)
#define WMI_X86_HAS_MSR(features) ((features[0] & (1 << 5)) != 0)
#define WMI_X86_HAS_PAE(features) ((features[0] & (1 << 6)) != 0)
#define WMI_X86_HAS_MCE(features) ((features[0] & (1 << 7)) != 0)
#define WMI_X86_HAS_CX8(features) ((features[0] & (1 << 8)) != 0)
#define WMI_X86_HAS_APIC(features) ((features[0] & (1 << 9)) != 0)
#define WMI_X86_HAS_SEP(features) ((features[0] & (1 << 11)) != 0)
#define WMI_X86_HAS_MTRR(features) ((features[0] & (1 << 12)) != 0)
#define WMI_X86_HAS_PGE(features) ((features[0] & (1 << 13)) != 0)
#define WMI_X86_HAS_MCA(features) ((features[0] & (1 << 14)) != 0)
#define WMI_X86_HAS_CMOV(features) ((features[0] & (1 << 15)) != 0)
#define WMI_X86_HAS_PAT(features) ((features[0] & (1 << 16)) != 0)
#define WMI_X86_HAS_PSE36(features) ((features[0] & (1 << 17)) != 0)
#define WMI_X86_HAS_PSN(features) ((features[0] & (1 << 18)) != 0)
#define WMI_X86_HAS_CLFSH(features) ((features[0] & (1 << 19)) != 0)
#define WMI_X86_HAS_DS(features) ((features[0] & (1 << 21)) != 0)
#define WMI_X86_HAS_ACPI(features) ((features[0] & (1 << 22)) != 0)
#define WMI_X86_HAS_MMX(features) ((features[0] & (1 << 23)) != 0)
#define WMI_X86_HAS_FXSR(features) ((features[0] & (1 << 24)) != 0)
#define WMI_X86_HAS_SSE(features) ((features[0] & (1 << 25)) != 0)
#define WMI_X86_HAS_SSE2(features) ((features[0] & (1 << 26)) != 0)
#define WMI_X86_HAS_SS(features) ((features[0] & (1 << 27)) != 0)
#define WMI_X86_HAS_HTT(features) ((features[0] & (1 << 28)) != 0)
#define WMI_X86_HAS_TM(features) ((features[0] & (1 << 29)) != 0)
#define WMI_X86_HAS_PBE(features) ((features[0] & (1 << 31)) != 0)

/* ECX feature flags */
#define WMI_X86_HAS_SSE3(features) ((features[1] & (1 << 0)) != 0)
#define WMI_X86_HAS_PCLMULQDQ(features) ((features[1] & (1 << 1)) != 0)
#define WMI_X86_HAS_DTES64(features) ((features[1] & (1 << 2)) != 0)
#define WMI_X86_HAS_MONITOR(features) ((features[1] & (1 << 3)) != 0)
#define WMI_X86_HAS_DSCPL(features) ((features[1] & (1 << 4)) != 0)
#define WMI_X86_HAS_VMX(features) ((features[1] & (1 << 5)) != 0)
#define WMI_X86_HAS_SMX(features) ((features[1] & (1 << 6)) != 0)
#define WMI_X86_HAS_EST(features) ((features[1] & (1 << 7)) != 0)
#define WMI_X86_HAS_TM2(features) ((features[1] & (1 << 8)) != 0)
#define WMI_X86_HAS_SSSE3(features) ((features[1] & (1 << 9)) != 0)
#define WMI_X86_HAS_CNXTID(features) ((features[1] & (1 << 10)) != 0)
#define WMI_X86_HAS_FMA(features) ((features[1] & (1 << 12)) != 0)
#define WMI_X86_HAS_CX16(features) ((features[1] & (1 << 13)) != 0)
#define WMI_X86_HAS_XTPR(features) ((features[1] & (1 << 14)) != 0)
#define WMI_X86_HAS_PDCM(features) ((features[1] & (1 << 15)) != 0)
#define WMI_X86_HAS_PCID(features) ((features[1] & (1 << 17)) != 0)
#define WMI_X86_HAS_DCA(features) ((features[1] & (1 << 18)) != 0)
#define WMI_X86_HAS_SSE41(features) ((features[1] & (1 << 19)) != 0)
#define WMI_X86_HAS_SSE42(features) ((features[1] & (1 << 20)) != 0)
#define WMI_X86_HAS_X2APIC(features) ((features[1] & (1 << 21)) != 0)
#define WMI_X86_HAS_MOVBE(features) ((features[1] & (1 << 22)) != 0)
#define WMI_X86_HAS_POPCNT(features) ((features[1] & (1 << 23)) != 0)
#define WMI_X86_HAS_TSC_DEADLINE(features) ((features[1] & (1 << 24)) != 0)
#define WMI_X86_HAS_AES(features) ((features[1] & (1 << 25)) != 0)
#define WMI_X86_HAS_XSAVE(features) ((features[1] & (1 << 26)) != 0)
#define WMI_X86_HAS_OSXSAVE(features) ((features[1] & (1 << 27)) != 0)
#define WMI_X86_HAS_AVX(features) ((features[1] & (1 << 28)) != 0)
#define WMI_X86_HAS_F16C(features) ((features[1] & (1 << 29)) != 0)
#define WMI_X86_HAS_RDRAND(features) ((features[1] & (1 << 30)) != 0)

/* Performance Counter Macros */
#define WMI_X86_START_PMC(id) WmiX86StartPerformanceCounter(id, WMI_X86_PMC_CYCLES)
#define WMI_X86_STOP_PMC(id) WmiX86StopPerformanceCounter(id)
#define WMI_X86_READ_TSC(low, high) WmiX86GetTimeStampCounter(low, high)

/* Debug Macros */
#ifdef DEBUG
    #define WMI_X86_DEBUG_PRINT(Format, ...) AuroraDebugPrint("[WMI-X86] " Format, ##__VA_ARGS__)
#else
    #define WMI_X86_DEBUG_PRINT(Format, ...)
#endif

#endif /* _WMI_ARCH_X86_H_ */
/* Generic (architecture-neutral) Aurora runtime support */
#include "../aurora.h"

/* Some toolchains under freestanding may require explicit stdarg include. */
#ifndef va_start
#include <stdarg.h>
#endif

/* ---------------- Memory / Allocation ---------------- */
PVOID AuroraAllocateMemory(IN UINT64 Size){ return AuroraAllocatePool(Size); }
PVOID AuroraAllocatePool(IN UINT64 Size){
    /* Simple bump allocator (shared, NOT thread-safe before spinlocks working) */
    static unsigned char pool[1<<19]; /* 512KB */
    static UINT64 off=0;
    if(Size==0) return NULL;
    UINT64 aligned = (Size + 7) & ~7ULL;
    if(off + aligned > sizeof(pool)) return NULL;
    void* p = &pool[off];
    off += aligned;
    return p;
}
void AuroraFreeMemory(IN PVOID Memory){ (void)Memory; /* no-op for bump allocator */ }
void AuroraFreePool(IN PVOID Memory){ (void)Memory; }
void AuroraMemoryZero(IN PVOID Memory, IN UINT64 Size){ if(Memory && Size) memset(Memory, 0, (size_t)Size); }
void AuroraMemoryCopy(OUT PVOID Destination, IN PVOID Source, IN UINT64 Size){ if(Destination && Source && Size) memcpy(Destination, Source, (size_t)Size); }

/* ---------------- Time / Identification ---------------- */
UINT64 AuroraGetSystemTime(void){
    /* Placeholder monotonically increasing tick; arch layer can override */
    static UINT64 t=0; return ++t;
}
UINT32 AuroraGetCurrentProcessId(void){ return 1; }
UINT32 AuroraGetCurrentThreadId(void){ return 1; }

/* ---------------- GUID / Utility ---------------- */
BOOL AuroraIsEqualGuid(IN PGUID a, IN PGUID b){ if(!a||!b) return FALSE; return memcmp(a,b,sizeof(GUID))==0; }

/* ---------------- Events (very small stub) ---------------- */
NTSTATUS AuroraInitializeEvent(OUT PAURORA_EVENT Event, IN BOOL ManualReset, IN BOOL InitialState){ (void)ManualReset; if(!Event) return STATUS_INVALID_PARAMETER; *Event = InitialState ? 1u : 0u; return STATUS_SUCCESS; }
NTSTATUS AuroraSetEvent(IN PVOID Event){ (void)Event; return STATUS_SUCCESS; }
NTSTATUS AuroraWaitForSingleObject(IN PVOID Object, IN UINT32 TimeoutMs){ (void)Object; (void)TimeoutMs; return STATUS_TIMEOUT; }

/* ---------------- Virtual Memory Mapping (stub) ---------------- */
NTSTATUS AuroraMapMemory(IN PVOID VirtualAddress, IN UINT64 PhysicalAddress, IN UINT64 Size, IN UINT32 Protection){ (void)VirtualAddress; (void)PhysicalAddress; (void)Size; (void)Protection; return STATUS_NOT_IMPLEMENTED; }
NTSTATUS AuroraUnmapMemory(IN PVOID VirtualAddress, IN UINT64 Size){ (void)VirtualAddress; (void)Size; return STATUS_NOT_IMPLEMENTED; }

/* ---------------- Debug Output (ring buffer) ---------------- */
#define AURORA_LOG_BUFFER_SIZE 4096
static CHAR g_LogBuffer[AURORA_LOG_BUFFER_SIZE];
static UINT32 g_LogHead = 0; /* next write position */

static void AuroraLogPutChar(char c){
    g_LogBuffer[g_LogHead++] = c;
    if(g_LogHead >= AURORA_LOG_BUFFER_SIZE) g_LogHead = 0;
}

static void AuroraLogWrite(const char* s){
    while(*s){ AuroraLogPutChar(*s++); }
}

static void AuroraLogWriteHex(UINT64 v, int width){
    static const char* hx = "0123456789ABCDEF";
    char buf[17];
    int i = 0;
    if(width < 1 || width > 16) width = 1;
    for(i = width-1; i>=0; --i){ buf[i] = hx[v & 0xF]; v >>= 4; }
    buf[width] = '\0';
    AuroraLogWrite(buf);
}

void AuroraDebugPrint(IN PCHAR Format, ...){
    const char* p;
    va_list ap;
    char spec;
    if(!Format) return;
    va_start(ap, Format);
    p = Format;
    while(*p){
        char ch = *p++;
        if(ch != '%') { AuroraLogPutChar(ch); continue; }
        spec = *p ? *p++ : '\0';
        if(!spec) break;
        if(spec == '%') { AuroraLogPutChar('%'); continue; }
        if(spec == 's'){
            const char* str = va_arg(ap, const char*);
            if(!str) str = "(null)";
            AuroraLogWrite(str);
        } else if(spec == 'c'){
            int vch = va_arg(ap,int);
            AuroraLogPutChar((char)vch);
        } else if(spec == 'x' || spec == 'X'){
            unsigned int v = va_arg(ap, unsigned int);
            AuroraLogWriteHex((UINT64)v, 8);
        } else if(spec == 'p'){
            void* vptr = va_arg(ap, void*);
            AuroraLogWrite("0x");
            AuroraLogWriteHex((UINT64)vptr, 16);
        } else if(spec == 'u'){
            unsigned int v = va_arg(ap, unsigned int);
            char tmp[32]; int idx=0; if(v==0){ AuroraLogPutChar('0'); continue; }
            while(v && idx < 31){ tmp[idx++] = (char)('0' + (v % 10)); v/=10; }
            while(idx--) AuroraLogPutChar(tmp[idx]);
        } else if(spec == 'd'){
            int v = va_arg(ap,int); unsigned int uv; char tmp[32]; int idx=0; if(v<0){ AuroraLogPutChar('-'); v=-v; }
            uv = (unsigned int)v; if(uv==0){ AuroraLogPutChar('0'); continue; }
            while(uv && idx < 31){ tmp[idx++] = (char)('0' + (uv % 10)); uv/=10; }
            while(idx--) AuroraLogPutChar(tmp[idx]);
        } else {
            AuroraLogPutChar('%'); AuroraLogPutChar(spec);
        }
    }
    va_end(ap);
    AuroraLogPutChar('\n');
}

const char* AuroraDebugGetLogBuffer(void){ return g_LogBuffer; }

/* ---------------- Spinlocks (weak generic fallback) ---------------- */
NTSTATUS AuroraInitializeSpinLock(OUT PAURORA_SPINLOCK SpinLock){ if(!SpinLock) return STATUS_INVALID_PARAMETER; *SpinLock=0; return STATUS_SUCCESS; }
void AuroraAcquireSpinLock(IN PAURORA_SPINLOCK SpinLock, OUT PAURORA_IRQL OldIrql){ (void)SpinLock; if(OldIrql) *OldIrql=0; }
void AuroraReleaseSpinLock(IN PAURORA_SPINLOCK SpinLock, IN AURORA_IRQL OldIrql){ (void)SpinLock; (void)OldIrql; }

/* Architecture helpers (generic fallback) */
void AuroraCpuPause(void){ }
void AuroraCpuHalt(void){ for(;;) { /* halt fallback */ } }
void AuroraMemoryBarrier(void){ __sync_synchronize(); }
UINT64 AuroraReadTimeStamp(void){ return AuroraGetSystemTime(); }


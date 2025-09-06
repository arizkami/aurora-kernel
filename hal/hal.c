/* Aurora HAL generic layer */
#include "../aurora.h"
#include "../include/hal.h"

/* Assembly helpers */
extern UINT64 HalAsmReadTsc(void);
extern void HalAsmPause(void);
extern void HalAsmHalt(void);

static UINT64 g_Freq = 1000000000ULL; /* placeholder 1GHz */

NTSTATUS HalInitialize(void){
    /* Could calibrate TSC vs PIT/APIC here */
    return STATUS_SUCCESS;
}

void HalEnableInterrupts(void){ __asm__ volatile ("sti"); }
void HalDisableInterrupts(void){ __asm__ volatile ("cli"); }
BOOL HalInterruptsEnabled(void){ UINT64 r; __asm__ volatile ("pushfq; pop %0" : "=r"(r)); return (r & (1ULL<<9)) != 0; }

UINT64 HalQueryPerformanceCounter(void){ return HalAsmReadTsc(); }
UINT64 HalQueryPerformanceFrequency(void){ return g_Freq; }

void HalCpuPause(void){ HalAsmPause(); }
void HalCpuHalt(void){ HalAsmHalt(); }

UINT8 HalInByte(UINT16 Port){ UINT8 v; __asm__ volatile ("inb %1,%0" : "=a"(v) : "d"(Port)); return v; }
void HalOutByte(UINT16 Port, UINT8 Value){ __asm__ volatile ("outb %0,%1" : : "a"(Value), "d"(Port)); }

void HalMemoryBarrier(void){ __sync_synchronize(); }

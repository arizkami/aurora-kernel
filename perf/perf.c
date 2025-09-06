#include "../aurora.h"
#include "../include/perf.h"
#include "../include/hal.h"

#define PERF_MAX 16
static PERF_COUNTER g_Counters[PERF_MAX];

NTSTATUS PerfInitialize(void){
    AuroraMemoryZero(g_Counters, sizeof(g_Counters));
    return STATUS_SUCCESS;
}

void PerfTick(void){
    /* simple counter 0 = TSC snapshot */
    g_Counters[0].Last = g_Counters[0].Value;
    g_Counters[0].Value = HalQueryPerformanceCounter();
}

UINT64 PerfGetCounter(IN UINT32 Id){ if(Id>=PERF_MAX) return 0; return g_Counters[Id].Value; }
UINT64 PerfDiff(IN UINT32 Id){ if(Id>=PERF_MAX) return 0; return g_Counters[Id].Value - g_Counters[Id].Last; }

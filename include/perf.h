/* Performance counters interface */
#ifndef _AURORA_PERF_H_
#define _AURORA_PERF_H_
#include "../aurora.h"

typedef struct _PERF_COUNTER { UINT64 Value; UINT64 Last; } PERF_COUNTER, *PPERF_COUNTER;

NTSTATUS PerfInitialize(void);
void PerfTick(void);
UINT64 PerfGetCounter(IN UINT32 Id);
UINT64 PerfDiff(IN UINT32 Id);

#endif

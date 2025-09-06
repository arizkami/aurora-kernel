/* Aurora Hardware Abstraction Layer Interface */
#ifndef _AURORA_HAL_H_
#define _AURORA_HAL_H_
#include "../aurora.h"

/* Initialization */
NTSTATUS HalInitialize(void);

/* Interrupt controller */
void HalEnableInterrupts(void);
void HalDisableInterrupts(void);
BOOL HalInterruptsEnabled(void);

/* Time */
UINT64 HalQueryPerformanceCounter(void);
UINT64 HalQueryPerformanceFrequency(void);

/* CPU */
void HalCpuPause(void);
void HalCpuHalt(void);

/* I/O Ports (legacy x86) */
UINT8 HalInByte(UINT16 Port);
void HalOutByte(UINT16 Port, UINT8 Value);

/* Memory barriers */
void HalMemoryBarrier(void);

#endif /* _AURORA_HAL_H_ */

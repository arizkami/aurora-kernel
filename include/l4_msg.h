/* Minimal L4 message tag / rights shim (C version) inspired by Fiasco.OC */
#ifndef _AURORA_L4_MSG_H_
#define _AURORA_L4_MSG_H_

#include "../aurora.h"

typedef struct _L4_MSG_TAG {
    UINT16 Flags;      /* flags (transfer fpu, schedule, error, etc.) */
    UINT16 Words;      /* number of untyped words (MR count) */
    UINT16 Items;      /* number of typed items (unused now) */
    INT16  Label;      /* protocol label */
} L4_MSG_TAG, *PL4_MSG_TAG;

/* Flag bits (subset) */
#define L4_MSGF_TRANSFER_FPU  0x1000
#define L4_MSGF_SCHEDULE      0x2000
#define L4_MSGF_ERROR         0x8000 /* output flag */

static inline L4_MSG_TAG L4MsgTagBuild(UINT16 words, UINT16 items, INT16 label){
    L4_MSG_TAG t; t.Flags = 0; t.Words = words; t.Items = items; t.Label = label; return t; }

#endif
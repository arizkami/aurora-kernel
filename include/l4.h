/* Aurora L4-style microkernel adaptation layer (inspired by Fiasco.OC)
 * Responsibilities:
 *  - Provide per-thread capability table for secure referencing of kernel objects
 *  - Offer minimal message-based IPC abstraction (register-only for now)
 *  - Serve as substrate for higher-level Fiasco policy fastpaths (see fiasco.c)
 *
 * Roadmap / TODO:
 *  - Add derivation/revocation operations for capabilities
 *  - Add grant/map semantics (flexpage style) once VM mapping interface exposed
 *  - Expand message registers or introduce descriptors for bulk buffers
 *  - Track per-cap object type-specific rights (e.g., thread control vs IPC)
 *  - Global capability audit enumeration for diagnostics
 */
#ifndef _AURORA_L4_H_
#define _AURORA_L4_H_
#include "../aurora.h"
#include "kern.h" /* for PTHREAD */

#define L4_MAX_CAPS          256
#define L4_INVALID_CAP       0xFFFFFFFFu
#define L4_THREAD_CAP_TYPE   1
#define L4_IPC_RIGHT_SEND    0x1
#define L4_IPC_RIGHT_RECV    0x2
#define L4_IPC_RIGHT_MAP     0x4
#define L4_IPC_RIGHT_CTRL    0x8

static inline UINT32 L4IpcRightsMask(void){ return L4_IPC_RIGHT_SEND|L4_IPC_RIGHT_RECV|L4_IPC_RIGHT_MAP|L4_IPC_RIGHT_CTRL; }

typedef UINT32 L4_CAP;

typedef struct _L4_CAP_ENTRY {
    UINT32 Type;      /* object type */
    UINT32 Rights;    /* bitmask */
    PVOID  Object;    /* pointer to backing kernel object */
} L4_CAP_ENTRY, *PL4_CAP_ENTRY;

typedef struct _L4_CAP_TABLE {
    L4_CAP_ENTRY Entries[L4_MAX_CAPS];
} L4_CAP_TABLE, *PL4_CAP_TABLE;

/* Simple L4 style message (register-only). Future: attach descriptors for memory mapping / flexpages */
typedef struct _L4_MSG {
    UINT64 MR[4]; /* message registers */
    UINT32 Length; /* number of valid MR */
} L4_MSG, *PL4_MSG;

/* Thread control block subset for L4 IPC */
typedef struct _L4_TCB_EXTENSION {
    UINT32 ThreadId;
    L4_CAP_TABLE* CapTable;
    L4_MSG Inbox;
    AURORA_SPINLOCK Lock;
} L4_TCB_EXTENSION, *PL4_TCB_EXTENSION;

/* API
 * L4CapInsert: inserts object with rights into a table (returns slot index)
 * L4CapLookup: returns object pointer if rights are sufficient
 * L4IpcSend: non-blocking send to receiver mailbox; returns BUFFER_TOO_SMALL if occupied
 * L4IpcReceive: dequeue current message into output buffer
 * L4GetOrCreateTcbExtension: alloc/init per-thread TCB extension; used from thread lifecycle
 */
NTSTATUS L4Initialize(void);
NTSTATUS L4CapInsert(PL4_CAP_TABLE Table, L4_CAP* OutCap, UINT32 Type, UINT32 Rights, PVOID Object);
PVOID    L4CapLookup(PL4_CAP_TABLE Table, L4_CAP Cap, UINT32 RequiredRights);
NTSTATUS L4CapRevoke(PL4_CAP_TABLE Table, L4_CAP Cap); /* remove single capability slot */
NTSTATUS L4CapDerive(PL4_CAP_TABLE Table, L4_CAP Source, UINT32 NewRights, L4_CAP* Out); /* create reduced-rights copy */
NTSTATUS L4IpcSend(PL4_TCB_EXTENSION Sender, PL4_TCB_EXTENSION Receiver, PL4_MSG Msg);
NTSTATUS L4IpcReceive(PL4_TCB_EXTENSION Receiver, PL4_MSG MsgOut);
PL4_TCB_EXTENSION L4GetOrCreateTcbExtension(PTHREAD Thread);

#endif

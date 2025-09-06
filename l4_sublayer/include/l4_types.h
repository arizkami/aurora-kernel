#ifndef L4_TYPES_H
#define L4_TYPES_H

#include "../../aurora.h"

/* L4 Types Header - Core L4 type definitions for Aurora kernel
 * Adapted from Fiasco L4 implementation
 */

/* L4 Object Reference */
typedef struct {
    UINT64 raw;
} L4_obj_ref;

typedef enum {
    L4_IPC_NONE = 0,
    L4_IPC_SEND = 1,
    L4_IPC_RECV = 2,
    L4_IPC_OPEN_WAIT = 4,
    L4_IPC_REPLY = 8,
    L4_IPC_WAIT = L4_IPC_OPEN_WAIT | L4_IPC_RECV,
    L4_IPC_SEND_AND_WAIT = L4_IPC_OPEN_WAIT | L4_IPC_SEND | L4_IPC_RECV,
    L4_IPC_REPLY_AND_WAIT = L4_IPC_OPEN_WAIT | L4_IPC_SEND | L4_IPC_RECV | L4_IPC_REPLY,
    L4_IPC_CALL = L4_IPC_SEND | L4_IPC_RECV
} L4_obj_ref_operation;

#define L4_CAP_SHIFT        12
#define L4_OP_MASK          0xF
#define L4_SPECIAL_MASK     0x800
#define L4_INVALID          0x800
#define L4_SELF             (~0ULL << 11)

/* L4 Message Tag */
typedef struct {
    UINT64 raw;
} L4_msg_tag;

#define L4_MSG_TAG_ERROR        0x8000
#define L4_MSG_TAG_TRANSFER_FPU 0x1000
#define L4_MSG_TAG_SCHEDULE     0x2000
#define L4_MSG_TAG_PROPAGATE    0x4000

/* L4 Protocol Labels */
typedef enum {
    L4_PROTO_NONE = 0,
    L4_PROTO_ALLOW_SYSCALL = 1,
    L4_PROTO_IRQ = -1,
    L4_PROTO_PAGE_FAULT = -2,
    L4_PROTO_EXCEPTION = -5,
    L4_PROTO_SIGMA0 = -6,
    L4_PROTO_IO_PAGE_FAULT = -8,
    L4_PROTO_KOBJECT = -10,
    L4_PROTO_TASK = -11,
    L4_PROTO_THREAD = -12,
    L4_PROTO_LOG = -13,
    L4_PROTO_SCHEDULER = -14,
    L4_PROTO_FACTORY = -15,
    L4_PROTO_VM = -16,
    L4_PROTO_DMA_SPACE = -17,
    L4_PROTO_IRQ_SENDER = -18,
    L4_PROTO_SEMAPHORE = -20,
    L4_PROTO_IOMMU = -22,
    L4_PROTO_DEBUGGER = -23,
    L4_PROTO_SMC = -24,
    L4_PROTO_VCPU_CONTEXT = -25
} L4_protocol;

/* L4 Flexpage */
typedef struct {
    UINT64 raw;
} L4_fpage;

typedef enum {
    L4_FPAGE_SPECIAL = 0,
    L4_FPAGE_MEMORY = 1,
    L4_FPAGE_IO = 2,
    L4_FPAGE_OBJ = 3
} L4_fpage_type;

typedef enum {
    L4_FPAGE_RO = 0,
    L4_FPAGE_X = 1,
    L4_FPAGE_W = 2,
    L4_FPAGE_WX = 3,
    L4_FPAGE_R = 4,
    L4_FPAGE_RX = 5,
    L4_FPAGE_RW = 6,
    L4_FPAGE_RWX = 7,
    L4_FPAGE_U = 8  /* User accessible */
} L4_fpage_rights;

#define L4_WHOLE_SPACE 63

/* L4 Timeout */
typedef struct {
    UINT16 raw;
} L4_timeout;

#define L4_TIMEOUT_NEVER        0
#define L4_TIMEOUT_ZERO         0x400
#define L4_TIMEOUT_MAN_MASK     0x3FF
#define L4_TIMEOUT_EXP_MASK     0x1F
#define L4_TIMEOUT_EXP_SHIFT    10

/* L4 Error */
typedef struct {
    UINT64 raw;
} L4_error;

typedef enum {
    L4_EOK = 0,
    L4_EPERM = 1,
    L4_ENOENT = 2,
    L4_ENOMEM = 12,
    L4_EFAULT = 14,
    L4_EBUSY = 16,
    L4_EEXIST = 17,
    L4_ENODEV = 19,
    L4_EINVAL = 22,
    L4_ERANGE = 34,
    L4_ENOSYS = 38,
    L4_EBADPROTO = 39,
    L4_EADDRNOTAVAIL = 99,
    L4_EMSGTOOSHORT = 1001,
    L4_EMSGTOOLONG = 1002
} L4_error_code;

/* L4 Buffer Descriptor */
typedef struct {
    UINT64 raw;
} L4_buf_desc;

/* L4 UTCB (User Thread Control Block) */
#define L4_UTCB_MAX_WORDS    63
#define L4_UTCB_MAX_BUFFERS  58
#define L4_UTCB_FREE_MARKER  0

typedef struct {
    UINT64 values[L4_UTCB_MAX_WORDS];
    UINT64 utcb_addr;
    L4_buf_desc buf_desc;
    UINT64 buffers[L4_UTCB_MAX_BUFFERS];
    L4_error error;
    UINT64 free_marker;
    UINT64 user[3];
} L4_utcb;

/* L4 Message Item */
typedef struct {
    UINT64 raw;
} L4_msg_item;

typedef struct {
    UINT64 raw;
} L4_snd_item;

typedef struct {
    UINT64 raw;
} L4_buf_item;

#define L4_MSG_ITEM_MAP 8

/* L4 Object Reference Functions */
L4_obj_ref L4ObjRefCreate(UINT32 cap, L4_obj_ref_operation op);
UINT32 L4ObjRefGetCap(L4_obj_ref ref);
L4_obj_ref_operation L4ObjRefGetOp(L4_obj_ref ref);
BOOL L4ObjRefIsSpecial(L4_obj_ref ref);
BOOL L4ObjRefIsSelf(L4_obj_ref ref);
BOOL L4ObjRefIsInvalid(L4_obj_ref ref);

/* L4 Message Tag Functions */
L4_msg_tag L4MsgTagCreate(UINT32 words, UINT32 items, UINT32 flags, UINT32 proto);
UINT32 L4MsgTagGetWords(L4_msg_tag tag);
UINT32 L4MsgTagGetItems(L4_msg_tag tag);
UINT32 L4MsgTagGetFlags(L4_msg_tag tag);
UINT32 L4MsgTagGetProto(L4_msg_tag tag);
BOOL L4MsgTagHasError(L4_msg_tag tag);
void L4MsgTagSetError(L4_msg_tag* tag);

/* L4 Flexpage Functions */
L4_fpage L4FpageCreate(L4_fpage_type type, UINT64 addr, UINT32 order, L4_fpage_rights rights);
L4_fpage L4FpageMemory(UINT64 addr, UINT32 order, L4_fpage_rights rights);
L4_fpage L4FpageIo(UINT32 port, UINT32 order, L4_fpage_rights rights);
L4_fpage L4FpageObj(UINT32 idx, UINT32 order, L4_fpage_rights rights);
L4_fpage L4FpageNil(void);
L4_fpage L4FpageAllSpaces(L4_fpage_rights rights);
L4_fpage_type L4FpageGetType(L4_fpage fp);
UINT64 L4FpageGetAddr(L4_fpage fp);
UINT32 L4FpageGetOrder(L4_fpage fp);
L4_fpage_rights L4FpageGetRights(L4_fpage fp);
BOOL L4FpageIsNil(L4_fpage fp);
BOOL L4FpageIsAllSpaces(L4_fpage fp);

/* L4 Timeout Functions */
L4_timeout L4TimeoutCreate(UINT32 man, UINT32 exp);
L4_timeout L4TimeoutNever(void);
L4_timeout L4TimeoutZero(void);
UINT32 L4TimeoutGetMan(L4_timeout to);
UINT32 L4TimeoutGetExp(L4_timeout to);
BOOL L4TimeoutIsNever(L4_timeout to);
BOOL L4TimeoutIsZero(L4_timeout to);
BOOL L4TimeoutIsFinite(L4_timeout to);

/* L4 Error Functions */
L4_error L4ErrorCreate(L4_error_code code);
L4_error_code L4ErrorGetCode(L4_error err);
BOOL L4ErrorIsOk(L4_error err);

/* L4 UTCB Functions */
void L4UtcbInit(L4_utcb* utcb);
void L4UtcbSetMR(L4_utcb* utcb, UINT32 index, UINT64 value);
UINT64 L4UtcbGetMR(L4_utcb* utcb, UINT32 index);
void L4UtcbSetBR(L4_utcb* utcb, UINT32 index, UINT64 value);
UINT64 L4UtcbGetBR(L4_utcb* utcb, UINT32 index);
void L4UtcbSetError(L4_utcb* utcb, L4_error error);
L4_error L4UtcbGetError(L4_utcb* utcb);

#endif /* L4_TYPES_H */
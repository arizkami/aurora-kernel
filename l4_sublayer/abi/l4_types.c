#include "../../aurora.h"
#include "../include/l4_types.h"

/* L4 Types Implementation - Adapted from Fiasco L4
 * This provides the core L4 type system for Aurora kernel
 */

/* L4 Object Reference Implementation */
L4_obj_ref L4ObjRefCreate(UINT32 cap, L4_obj_ref_operation op) {
    L4_obj_ref ref;
    ref.raw = (cap << L4_CAP_SHIFT) | (op & L4_OP_MASK);
    return ref;
}

UINT32 L4ObjRefGetCap(L4_obj_ref ref) {
    return (ref.raw >> L4_CAP_SHIFT);
}

L4_obj_ref_operation L4ObjRefGetOp(L4_obj_ref ref) {
    return (L4_obj_ref_operation)(ref.raw & L4_OP_MASK);
}

BOOL L4ObjRefIsSpecial(L4_obj_ref ref) {
    return (ref.raw & L4_SPECIAL_MASK) != 0;
}

BOOL L4ObjRefIsSelf(L4_obj_ref ref) {
    return (ref.raw & L4_SPECIAL_MASK) == L4_SELF;
}

BOOL L4ObjRefIsInvalid(L4_obj_ref ref) {
    return (ref.raw & L4_SPECIAL_MASK) == L4_INVALID;
}

/* L4 Message Tag Implementation */
L4_msg_tag L4MsgTagCreate(UINT32 words, UINT32 items, UINT32 flags, UINT32 proto) {
    L4_msg_tag tag;
    tag.raw = (words & 0x3F) | 
              ((items & 0x3F) << 6) | 
              (flags & 0xF000) | 
              ((proto & 0xFFFF) << 16);
    return tag;
}

UINT32 L4MsgTagGetWords(L4_msg_tag tag) {
    return tag.raw & 0x3F;
}

UINT32 L4MsgTagGetItems(L4_msg_tag tag) {
    return (tag.raw >> 6) & 0x3F;
}

UINT32 L4MsgTagGetFlags(L4_msg_tag tag) {
    return tag.raw & 0xF000;
}

UINT32 L4MsgTagGetProto(L4_msg_tag tag) {
    return (tag.raw >> 16) & 0xFFFF;
}

BOOL L4MsgTagHasError(L4_msg_tag tag) {
    return (tag.raw & L4_MSG_TAG_ERROR) != 0;
}

void L4MsgTagSetError(L4_msg_tag* tag) {
    tag->raw |= L4_MSG_TAG_ERROR;
}

/* L4 Flexpage Implementation */
L4_fpage L4FpageCreate(L4_fpage_type type, UINT64 addr, UINT32 order, L4_fpage_rights rights) {
    L4_fpage fp;
    fp.raw = (addr & ~((1ULL << order) - 1)) | 
             ((UINT64)rights & 0xF) | 
             (((UINT64)order & 0x3F) << 6) | 
             (((UINT64)type & 0x3) << 4);
    return fp;
}

L4_fpage L4FpageMemory(UINT64 addr, UINT32 order, L4_fpage_rights rights) {
    return L4FpageCreate(L4_FPAGE_MEMORY, addr, order, rights);
}

L4_fpage L4FpageIo(UINT32 port, UINT32 order, L4_fpage_rights rights) {
    return L4FpageCreate(L4_FPAGE_IO, port, order, rights);
}

L4_fpage L4FpageObj(UINT32 idx, UINT32 order, L4_fpage_rights rights) {
    return L4FpageCreate(L4_FPAGE_OBJ, idx, order, rights);
}

L4_fpage L4FpageNil(void) {
    L4_fpage fp;
    fp.raw = 0;
    return fp;
}

L4_fpage L4FpageAllSpaces(L4_fpage_rights rights) {
    return L4FpageCreate(L4_FPAGE_SPECIAL, 0, L4_WHOLE_SPACE, rights);
}

L4_fpage_type L4FpageGetType(L4_fpage fp) {
    return (L4_fpage_type)((fp.raw >> 4) & 0x3);
}

UINT64 L4FpageGetAddr(L4_fpage fp) {
    UINT32 order = L4FpageGetOrder(fp);
    return fp.raw & ~((1ULL << order) - 1);
}

UINT32 L4FpageGetOrder(L4_fpage fp) {
    return (fp.raw >> 6) & 0x3F;
}

L4_fpage_rights L4FpageGetRights(L4_fpage fp) {
    return (L4_fpage_rights)(fp.raw & 0xF);
}

BOOL L4FpageIsNil(L4_fpage fp) {
    return fp.raw == 0;
}

BOOL L4FpageIsAllSpaces(L4_fpage fp) {
    return L4FpageGetType(fp) == L4_FPAGE_SPECIAL && L4FpageGetOrder(fp) == L4_WHOLE_SPACE;
}

/* L4 Timeout Implementation */
L4_timeout L4TimeoutCreate(UINT32 man, UINT32 exp) {
    L4_timeout to;
    to.raw = (man & L4_TIMEOUT_MAN_MASK) | 
             ((exp & L4_TIMEOUT_EXP_MASK) << L4_TIMEOUT_EXP_SHIFT);
    return to;
}

L4_timeout L4TimeoutNever(void) {
    L4_timeout to;
    to.raw = L4_TIMEOUT_NEVER;
    return to;
}

L4_timeout L4TimeoutZero(void) {
    L4_timeout to;
    to.raw = L4_TIMEOUT_ZERO;
    return to;
}

UINT32 L4TimeoutGetMan(L4_timeout to) {
    return to.raw & L4_TIMEOUT_MAN_MASK;
}

UINT32 L4TimeoutGetExp(L4_timeout to) {
    return (to.raw >> L4_TIMEOUT_EXP_SHIFT) & L4_TIMEOUT_EXP_MASK;
}

BOOL L4TimeoutIsNever(L4_timeout to) {
    return to.raw == L4_TIMEOUT_NEVER;
}

BOOL L4TimeoutIsZero(L4_timeout to) {
    return to.raw == L4_TIMEOUT_ZERO;
}

BOOL L4TimeoutIsFinite(L4_timeout to) {
    return !L4TimeoutIsNever(to);
}

/* L4 Error Implementation */
L4_error L4ErrorCreate(L4_error_code code) {
    L4_error err;
    err.raw = code;
    return err;
}

L4_error_code L4ErrorGetCode(L4_error err) {
    return (L4_error_code)err.raw;
}

BOOL L4ErrorIsOk(L4_error err) {
    return err.raw == L4_EOK;
}

/* L4 UTCB Implementation */
void L4UtcbInit(L4_utcb* utcb) {
    if (!utcb) return;
    
    memset(utcb, 0, sizeof(L4_utcb));
    utcb->free_marker = L4_UTCB_FREE_MARKER;
}

void L4UtcbSetMR(L4_utcb* utcb, UINT32 index, UINT64 value) {
    if (!utcb || index >= L4_UTCB_MAX_WORDS) return;
    utcb->values[index] = value;
}

UINT64 L4UtcbGetMR(L4_utcb* utcb, UINT32 index) {
    if (!utcb || index >= L4_UTCB_MAX_WORDS) return 0;
    return utcb->values[index];
}

void L4UtcbSetBR(L4_utcb* utcb, UINT32 index, UINT64 value) {
    if (!utcb || index >= L4_UTCB_MAX_BUFFERS) return;
    utcb->buffers[index] = value;
}

UINT64 L4UtcbGetBR(L4_utcb* utcb, UINT32 index) {
    if (!utcb || index >= L4_UTCB_MAX_BUFFERS) return 0;
    return utcb->buffers[index];
}

void L4UtcbSetError(L4_utcb* utcb, L4_error error) {
    if (!utcb) return;
    utcb->error = error;
}

L4_error L4UtcbGetError(L4_utcb* utcb) {
    if (!utcb) return L4ErrorCreate(L4_EINVAL);
    return utcb->error;
}
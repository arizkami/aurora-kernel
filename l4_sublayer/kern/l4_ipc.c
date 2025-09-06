#include "../../aurora.h"
#include "../include/l4_types.h"
#include "../include/l4_ipc.h"

/* L4 IPC Core Implementation - Adapted from Fiasco L4
 * This provides the core IPC functionality for Aurora kernel
 */

/* Global IPC state */
static L4_utcb* current_utcb = NULL;
static UINT32 ipc_timeout_counter = 0;

/* Internal helper functions */
static BOOL validate_obj_ref(L4_obj_ref ref);
static L4_error copy_message_registers(L4_utcb* from, L4_utcb* to, UINT32 words);
static L4_error handle_flexpage_transfer(L4_fpage fp, L4_obj_ref dest);
static L4_error process_message_items(L4_utcb* utcb, UINT32 items);

/* L4 IPC System Call Implementation */
L4_msg_tag L4_Ipc(L4_obj_ref dest, L4_obj_ref from_spec, L4_timeout timeout, L4_msg_tag tag) {
    L4_error error = L4ErrorCreate(L4_EOK);
    L4_msg_tag result_tag = tag;
    
    if (!current_utcb) {
        error = L4ErrorCreate(L4_EFAULT);
        L4MsgTagSetError(&result_tag);
        return result_tag;
    }
    
    /* Validate destination object reference */
    if (!L4ObjRefIsInvalid(dest) && !validate_obj_ref(dest)) {
        error = L4ErrorCreate(L4_EINVAL);
        L4MsgTagSetError(&result_tag);
        L4UtcbSetError(current_utcb, error);
        return result_tag;
    }
    
    /* Handle send phase */
    if (L4ObjRefGetOp(dest) & L4_IPC_SEND) {
        error = L4_IpcSend(dest, timeout, tag);
        if (!L4ErrorIsOk(error)) {
            L4MsgTagSetError(&result_tag);
            L4UtcbSetError(current_utcb, error);
            return result_tag;
        }
    }
    
    /* Handle receive phase */
    if (L4ObjRefGetOp(from_spec) & L4_IPC_RECV) {
        L4_obj_ref actual_sender;
        result_tag = L4_IpcReceive(from_spec, timeout, &actual_sender);
        if (L4MsgTagHasError(result_tag)) {
            return result_tag;
        }
    }
    
    return result_tag;
}

/* L4 IPC Send Implementation */
L4_error L4_IpcSend(L4_obj_ref dest, L4_timeout timeout, L4_msg_tag tag) {
    if (!current_utcb) {
        return L4ErrorCreate(L4_EFAULT);
    }
    
    if (L4ObjRefIsInvalid(dest)) {
        return L4ErrorCreate(L4_EINVAL);
    }
    
    /* Handle special destinations */
    if (L4ObjRefIsSelf(dest)) {
        /* Self-send is a no-op */
        return L4ErrorCreate(L4_EOK);
    }
    
    /* Process message items (flexpages, etc.) */
    UINT32 items = L4MsgTagGetItems(tag);
    if (items > 0) {
        L4_error item_error = process_message_items(current_utcb, items);
        if (!L4ErrorIsOk(item_error)) {
            return item_error;
        }
    }
    
    /* TODO: Implement actual message transfer to destination thread */
    /* This would involve:
     * 1. Finding the destination thread/task
     * 2. Checking permissions
     * 3. Copying message registers
     * 4. Handling timeout
     * 5. Waking up the destination if waiting
     */
    
    return L4ErrorCreate(L4_EOK);
}

/* L4 IPC Receive Implementation */
L4_msg_tag L4_IpcReceive(L4_obj_ref from_spec, L4_timeout timeout, L4_obj_ref* sender) {
    L4_msg_tag tag = L4MsgTagCreate(0, 0, 0, 0);
    
    if (!current_utcb || !sender) {
        L4MsgTagSetError(&tag);
        if (current_utcb) {
            L4UtcbSetError(current_utcb, L4ErrorCreate(L4_EFAULT));
        }
        return tag;
    }
    
    /* Handle open wait */
    if (L4ObjRefGetOp(from_spec) & L4_IPC_OPEN_WAIT) {
        /* Accept messages from any sender */
        *sender = L4ObjRefCreate(0, L4_IPC_NONE); /* Will be filled by actual sender */
    } else {
        /* Closed wait - only accept from specified sender */
        if (!validate_obj_ref(from_spec)) {
            L4MsgTagSetError(&tag);
            L4UtcbSetError(current_utcb, L4ErrorCreate(L4_EINVAL));
            return tag;
        }
        *sender = from_spec;
    }
    
    /* TODO: Implement actual message reception */
    /* This would involve:
     * 1. Blocking the current thread if no message available
     * 2. Handling timeout
     * 3. Copying message from sender's UTCB
     * 4. Processing received flexpages
     * 5. Setting up result tag
     */
    
    return tag;
}

/* L4 IPC Call Implementation (Send + Receive) */
L4_msg_tag L4_IpcCall(L4_obj_ref dest, L4_timeout timeout, L4_msg_tag tag) {
    /* Set up for call operation */
    L4_obj_ref call_dest = L4ObjRefCreate(L4ObjRefGetCap(dest), L4_IPC_CALL);
    L4_obj_ref reply_from = L4ObjRefCreate(L4ObjRefGetCap(dest), L4_IPC_RECV);
    
    return L4_Ipc(call_dest, reply_from, timeout, tag);
}

/* L4 IPC Reply Implementation */
L4_error L4_IpcReply(L4_timeout timeout, L4_msg_tag tag) {
    if (!current_utcb) {
        return L4ErrorCreate(L4_EFAULT);
    }
    
    /* TODO: Implement reply to the thread that called us */
    /* This would involve:
     * 1. Finding the caller thread
     * 2. Copying message registers
     * 3. Waking up the caller
     */
    
    return L4ErrorCreate(L4_EOK);
}

/* L4 IPC Reply and Wait Implementation */
L4_msg_tag L4_IpcReplyAndWait(L4_obj_ref from_spec, L4_timeout timeout, L4_msg_tag tag, L4_obj_ref* sender) {
    /* First reply */
    L4_error reply_error = L4_IpcReply(timeout, tag);
    if (!L4ErrorIsOk(reply_error)) {
        L4_msg_tag error_tag = L4MsgTagCreate(0, 0, 0, 0);
        L4MsgTagSetError(&error_tag);
        if (current_utcb) {
            L4UtcbSetError(current_utcb, reply_error);
        }
        return error_tag;
    }
    
    /* Then wait for next message */
    return L4_IpcReceive(from_spec, timeout, sender);
}

/* UTCB Management */
void L4SetUtcb(L4_utcb* utcb) {
    current_utcb = utcb;
}

L4_utcb* L4GetUtcb(void) {
    return current_utcb;
}

/* Message Register Access */
void L4SetMR(UINT32 index, UINT64 value) {
    if (current_utcb) {
        L4UtcbSetMR(current_utcb, index, value);
    }
}

UINT64 L4GetMR(UINT32 index) {
    if (current_utcb) {
        return L4UtcbGetMR(current_utcb, index);
    }
    return 0;
}

/* Buffer Register Access */
void L4SetBR(UINT32 index, UINT64 value) {
    if (current_utcb) {
        L4UtcbSetBR(current_utcb, index, value);
    }
}

UINT64 L4GetBR(UINT32 index) {
    if (current_utcb) {
        return L4UtcbGetBR(current_utcb, index);
    }
    return 0;
}

/* Internal Helper Functions */
static BOOL validate_obj_ref(L4_obj_ref ref) {
    /* Basic validation - check if it's not invalid */
    if (L4ObjRefIsInvalid(ref)) {
        return FALSE;
    }
    
    /* TODO: Add more validation:
     * - Check if capability exists
     * - Check permissions
     * - Check if object is alive
     */
    
    return TRUE;
}

static L4_error copy_message_registers(L4_utcb* from, L4_utcb* to, UINT32 words) {
    if (!from || !to || words > L4_UTCB_MAX_WORDS) {
        return L4ErrorCreate(L4_EINVAL);
    }
    
    for (UINT32 i = 0; i < words; i++) {
        UINT64 value = L4UtcbGetMR(from, i);
        L4UtcbSetMR(to, i, value);
    }
    
    return L4ErrorCreate(L4_EOK);
}

static L4_error handle_flexpage_transfer(L4_fpage fp, L4_obj_ref dest) {
    if (L4FpageIsNil(fp)) {
        return L4ErrorCreate(L4_EOK);
    }
    
    /* TODO: Implement flexpage transfer */
    /* This would involve:
     * 1. Validating the flexpage
     * 2. Checking sender permissions
     * 3. Mapping/granting to destination address space
     * 4. Updating page tables
     */
    
    return L4ErrorCreate(L4_EOK);
}

static L4_error process_message_items(L4_utcb* utcb, UINT32 items) {
    if (!utcb || items == 0) {
        return L4ErrorCreate(L4_EOK);
    }
    
    /* TODO: Process message items */
    /* This would involve:
     * 1. Parsing message items from UTCB
     * 2. Handling different item types (flexpages, etc.)
     * 3. Performing the requested operations
     */
    
    return L4ErrorCreate(L4_EOK);
}

/* IPC Statistics and Debugging */
UINT32 L4GetIpcTimeoutCounter(void) {
    return ipc_timeout_counter;
}

void L4ResetIpcTimeoutCounter(void) {
    ipc_timeout_counter = 0;
}
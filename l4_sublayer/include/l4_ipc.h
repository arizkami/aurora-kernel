#ifndef L4_IPC_H
#define L4_IPC_H

#include "l4_types.h"

/* L4 IPC Header - Inter-Process Communication interface
 * Adapted from Fiasco L4 implementation
 */

/* L4 IPC System Calls */

/**
 * L4 IPC - Main IPC system call
 * @param dest: Destination object reference for send phase
 * @param from_spec: Source specification for receive phase
 * @param timeout: Timeout for the operation
 * @param tag: Message tag describing the message
 * @return: Result message tag
 */
L4_msg_tag L4_Ipc(L4_obj_ref dest, L4_obj_ref from_spec, L4_timeout timeout, L4_msg_tag tag);

/**
 * L4 IPC Send - Send a message to a destination
 * @param dest: Destination object reference
 * @param timeout: Send timeout
 * @param tag: Message tag
 * @return: Error code
 */
L4_error L4_IpcSend(L4_obj_ref dest, L4_timeout timeout, L4_msg_tag tag);

/**
 * L4 IPC Receive - Receive a message from a sender
 * @param from_spec: Sender specification (or open wait)
 * @param timeout: Receive timeout
 * @param sender: [out] Actual sender object reference
 * @return: Received message tag
 */
L4_msg_tag L4_IpcReceive(L4_obj_ref from_spec, L4_timeout timeout, L4_obj_ref* sender);

/**
 * L4 IPC Call - Send message and wait for reply
 * @param dest: Destination object reference
 * @param timeout: Call timeout
 * @param tag: Message tag
 * @return: Reply message tag
 */
L4_msg_tag L4_IpcCall(L4_obj_ref dest, L4_timeout timeout, L4_msg_tag tag);

/**
 * L4 IPC Reply - Reply to a previous call
 * @param timeout: Reply timeout
 * @param tag: Reply message tag
 * @return: Error code
 */
L4_error L4_IpcReply(L4_timeout timeout, L4_msg_tag tag);

/**
 * L4 IPC Reply and Wait - Reply to caller and wait for next message
 * @param from_spec: Sender specification for next message
 * @param timeout: Operation timeout
 * @param tag: Reply message tag
 * @param sender: [out] Actual sender of next message
 * @return: Next message tag
 */
L4_msg_tag L4_IpcReplyAndWait(L4_obj_ref from_spec, L4_timeout timeout, L4_msg_tag tag, L4_obj_ref* sender);

/* UTCB Management */

/**
 * Set the current thread's UTCB
 * @param utcb: Pointer to UTCB structure
 */
void L4SetUtcb(L4_utcb* utcb);

/**
 * Get the current thread's UTCB
 * @return: Pointer to current UTCB or NULL
 */
L4_utcb* L4GetUtcb(void);

/* Message Register Access */

/**
 * Set a message register
 * @param index: Register index (0-62)
 * @param value: Value to set
 */
void L4SetMR(UINT32 index, UINT64 value);

/**
 * Get a message register
 * @param index: Register index (0-62)
 * @return: Register value
 */
UINT64 L4GetMR(UINT32 index);

/* Buffer Register Access */

/**
 * Set a buffer register
 * @param index: Register index (0-57)
 * @param value: Value to set
 */
void L4SetBR(UINT32 index, UINT64 value);

/**
 * Get a buffer register
 * @param index: Register index (0-57)
 * @return: Register value
 */
UINT64 L4GetBR(UINT32 index);

/* IPC Convenience Macros */

/**
 * Create a send-only IPC operation
 */
#define L4_IPC_SEND_ONLY(dest, timeout, tag) \
    L4Ipc(dest, L4ObjRefCreate(0, L4_IPC_NONE), timeout, tag)

/**
 * Create a receive-only IPC operation (open wait)
 */
#define L4_IPC_WAIT(timeout, sender) \
    L4IpcReceive(L4ObjRefCreate(0, L4_IPC_OPEN_WAIT), timeout, sender)

/**
 * Create a receive-only IPC operation (closed wait)
 */
#define L4_IPC_RECEIVE_FROM(from, timeout, sender) \
    L4IpcReceive(L4ObjRefCreate(L4ObjRefGetCap(from), L4_IPC_RECV), timeout, sender)

/**
 * Create a call operation
 */
#define L4_IPC_CALL_TAG(dest, timeout, tag) \
    L4IpcCall(dest, timeout, tag)

/**
 * Create a reply operation
 */
#define L4_IPC_REPLY_TAG(timeout, tag) \
    L4IpcReply(timeout, tag)

/**
 * Create a reply and wait operation
 */
#define L4_IPC_REPLY_AND_WAIT_TAG(timeout, tag, sender) \
    L4IpcReplyAndWait(L4ObjRefCreate(0, L4_IPC_OPEN_WAIT), timeout, tag, sender)

/* IPC Error Handling */

/**
 * Check if an IPC operation resulted in an error
 * @param tag: Message tag returned from IPC operation
 * @return: TRUE if error occurred
 */
#define L4_IPC_HAS_ERROR(tag) L4MsgTagHasError(tag)

/**
 * Get the error code from current UTCB
 * @return: Error code
 */
#define L4_IPC_GET_ERROR() L4UtcbGetError(L4GetUtcb())

/* IPC Message Construction Helpers */

/**
 * Create a simple message tag
 * @param words: Number of message words
 * @param proto: Protocol number
 */
#define L4_MSG_TAG_SIMPLE(words, proto) \
    L4MsgTagCreate(words, 0, 0, proto)

/**
 * Create a message tag with items
 * @param words: Number of message words
 * @param items: Number of message items
 * @param proto: Protocol number
 */
#define L4_MSG_TAG_WITH_ITEMS(words, items, proto) \
    L4MsgTagCreate(words, items, 0, proto)

/* IPC Statistics and Debugging */

/**
 * Get the number of IPC timeouts that have occurred
 * @return: Timeout counter value
 */
UINT32 L4GetIpcTimeoutCounter(void);

/**
 * Reset the IPC timeout counter
 */
void L4ResetIpcTimeoutCounter(void);

/* IPC Constants */

/* Special object references */
#define L4_INVALID_CAP      L4ObjRefCreate(0, L4_IPC_NONE)
#define L4_SELF_CAP         L4ObjRefCreate(~0U, L4_IPC_NONE)

/* Common timeouts */
#define L4_IPC_NEVER        L4TimeoutNever()
#define L4_IPC_ZERO         L4TimeoutZero()

/* Common message tags */
#define L4_MSG_TAG_EMPTY    L4MsgTagCreate(0, 0, 0, 0)

#endif /* L4_IPC_H */
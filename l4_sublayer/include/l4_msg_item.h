#ifndef L4_MSG_ITEM_H
#define L4_MSG_ITEM_H

#include "l4_types.h"

/* L4 Message Item Header - Message item handling for IPC
 * Adapted from Fiasco L4 implementation
 */

/* Message Item Types */
typedef enum {
    L4_MSG_ITEM_NONE = 0,
    L4_MSG_ITEM_FPAGE = 1,
    L4_MSG_ITEM_STRING = 2,
    L4_MSG_ITEM_CLOCK = 3
} L4_msg_item_type;

/* Send Item Types */
typedef enum {
    L4_SND_ITEM_MAP = 0,
    L4_SND_ITEM_GRANT = 1
} L4_snd_item_type;

/* Message Item Constants */
#define L4_MSG_ITEM_TYPE_MASK       0x7
#define L4_MSG_ITEM_TYPE_SHIFT      61
#define L4_MSG_ITEM_DATA_MASK       0x1FFFFFFFFFFFFFFFULL

/* Send Item Constants */
#define L4_SND_ITEM_FPAGE_MASK      0xFFFFFFFFFFFFF000ULL
#define L4_SND_ITEM_BASE_MASK       0xFFFFFFFFFFFFF000ULL
#define L4_SND_ITEM_BASE_SHIFT      12
#define L4_SND_ITEM_TYPE_MASK       0x1
#define L4_SND_ITEM_TYPE_SHIFT      0

/* Buffer Item Constants */
#define L4_BUF_ITEM_FPAGE_MASK      0xFFFFFFFFFFFFF000ULL
#define L4_BUF_ITEM_WINDOW_MASK     0xFFFFFFFFFFFFF000ULL
#define L4_BUF_ITEM_WINDOW_SHIFT    12

/* Buffer Descriptor Constants */
#define L4_BUF_DESC_COUNT_MASK      0x3F
#define L4_BUF_DESC_FLAGS_MASK      0x3FF
#define L4_BUF_DESC_FLAGS_SHIFT     6

/* Message Item Limits */
#define L4_MAX_MSG_ITEMS            63
#define L4_MAX_BUF_ITEMS            58

/* Message Item Functions */

/**
 * Create a message item
 * @param type: Item type
 * @param data: Item data
 * @return: Message item
 */
L4_msg_item L4MsgItemCreate(L4_msg_item_type type, UINT64 data);

/**
 * Get message item type
 * @param item: Message item
 * @return: Item type
 */
L4_msg_item_type L4MsgItemGetType(L4_msg_item item);

/**
 * Get message item data
 * @param item: Message item
 * @return: Item data
 */
UINT64 L4MsgItemGetData(L4_msg_item item);

/**
 * Check if message item is a map item
 * @param item: Message item
 * @return: TRUE if map item
 */
BOOL L4MsgItemIsMap(L4_msg_item item);

/**
 * Set map flag on message item
 * @param item: Pointer to message item
 */
void L4MsgItemSetMap(L4_msg_item* item);

/* Send Item Functions */

/**
 * Create a send item
 * @param fp: Flexpage to send
 * @param snd_base: Send base address
 * @param type: Send item type (map/grant)
 * @return: Send item
 */
L4_snd_item L4SndItemCreate(L4_fpage fp, UINT64 snd_base, L4_snd_item_type type);

/**
 * Create a map send item
 * @param fp: Flexpage to map
 * @param snd_base: Send base address
 * @return: Map send item
 */
L4_snd_item L4SndItemMap(L4_fpage fp, UINT64 snd_base);

/**
 * Create a grant send item
 * @param fp: Flexpage to grant
 * @param snd_base: Send base address
 * @return: Grant send item
 */
L4_snd_item L4SndItemGrant(L4_fpage fp, UINT64 snd_base);

/**
 * Get flexpage from send item
 * @param item: Send item
 * @return: Flexpage
 */
L4_fpage L4SndItemGetFpage(L4_snd_item item);

/**
 * Get send base from send item
 * @param item: Send item
 * @return: Send base address
 */
UINT64 L4SndItemGetSndBase(L4_snd_item item);

/**
 * Get send item type
 * @param item: Send item
 * @return: Send item type
 */
L4_snd_item_type L4SndItemGetType(L4_snd_item item);

/**
 * Check if send item is a map item
 * @param item: Send item
 * @return: TRUE if map item
 */
BOOL L4SndItemIsMap(L4_snd_item item);

/**
 * Check if send item is a grant item
 * @param item: Send item
 * @return: TRUE if grant item
 */
BOOL L4SndItemIsGrant(L4_snd_item item);

/* Buffer Item Functions */

/**
 * Create a buffer item
 * @param fp: Receive flexpage
 * @param rcv_window: Receive window
 * @return: Buffer item
 */
L4_buf_item L4BufItemCreate(L4_fpage fp, UINT64 rcv_window);

/**
 * Get flexpage from buffer item
 * @param item: Buffer item
 * @return: Flexpage
 */
L4_fpage L4BufItemGetFpage(L4_buf_item item);

/**
 * Get receive window from buffer item
 * @param item: Buffer item
 * @return: Receive window
 */
UINT64 L4BufItemGetRcvWindow(L4_buf_item item);

/* Buffer Descriptor Functions */

/**
 * Create a buffer descriptor
 * @param buf_count: Number of buffer items
 * @param flags: Descriptor flags
 * @return: Buffer descriptor
 */
L4_buf_desc L4BufDescCreate(UINT32 buf_count, UINT32 flags);

/**
 * Get buffer count from descriptor
 * @param desc: Buffer descriptor
 * @return: Buffer count
 */
UINT32 L4BufDescGetCount(L4_buf_desc desc);

/**
 * Get flags from buffer descriptor
 * @param desc: Buffer descriptor
 * @return: Descriptor flags
 */
UINT32 L4BufDescGetFlags(L4_buf_desc desc);

/**
 * Set buffer count in descriptor
 * @param desc: Pointer to buffer descriptor
 * @param count: Buffer count
 */
void L4BufDescSetCount(L4_buf_desc* desc, UINT32 count);

/**
 * Set flags in buffer descriptor
 * @param desc: Pointer to buffer descriptor
 * @param flags: Descriptor flags
 */
void L4BufDescSetFlags(L4_buf_desc* desc, UINT32 flags);

/* Message Item Processing Functions */

/**
 * Process send items during IPC send
 * @param utcb: Current UTCB
 * @param item_count: Number of items to process
 * @param dest: Destination object reference
 * @return: Error code
 */
L4_error L4ProcessSendItems(L4_utcb* utcb, UINT32 item_count, L4_obj_ref dest);

/**
 * Process a map item
 * @param item: Map send item
 * @param dest: Destination object reference
 * @return: Error code
 */
L4_error L4ProcessMapItem(L4_snd_item item, L4_obj_ref dest);

/**
 * Process a grant item
 * @param item: Grant send item
 * @param dest: Destination object reference
 * @return: Error code
 */
L4_error L4ProcessGrantItem(L4_snd_item item, L4_obj_ref dest);

/**
 * Set up receive buffers in UTCB
 * @param utcb: Target UTCB
 * @param buffers: Array of buffer items
 * @param buffer_count: Number of buffer items
 * @return: Error code
 */
L4_error L4SetupReceiveBuffers(L4_utcb* utcb, L4_buf_item* buffers, UINT32 buffer_count);

/**
 * Process received items during IPC receive
 * @param utcb: Current UTCB
 * @param item_count: Number of received items
 * @return: Error code
 */
L4_error L4ProcessReceiveItems(L4_utcb* utcb, UINT32 item_count);

/* Message Item Validation */

/**
 * Validate a send item
 * @param item: Send item to validate
 * @return: TRUE if valid
 */
BOOL L4ValidateSendItem(L4_snd_item item);

/**
 * Validate a buffer item
 * @param item: Buffer item to validate
 * @return: TRUE if valid
 */
BOOL L4ValidateBufItem(L4_buf_item item);

/* Convenience Macros */

/**
 * Create a memory map send item
 */
#define L4_SND_ITEM_MEM_MAP(addr, order, rights, snd_base) \
    L4SndItemMap(L4FpageMemory(addr, order, rights), snd_base)

/**
 * Create a memory grant send item
 */
#define L4_SND_ITEM_MEM_GRANT(addr, order, rights, snd_base) \
    L4SndItemGrant(L4FpageMemory(addr, order, rights), snd_base)

/**
 * Create an I/O map send item
 */
#define L4_SND_ITEM_IO_MAP(port, order, rights, snd_base) \
    L4SndItemMap(L4FpageIo(port, order, rights), snd_base)

/**
 * Create an object map send item
 */
#define L4_SND_ITEM_OBJ_MAP(idx, order, rights, snd_base) \
    L4SndItemMap(L4FpageObj(idx, order, rights), snd_base)

/**
 * Create a memory buffer item
 */
#define L4_BUF_ITEM_MEM(addr, order, rights, rcv_window) \
    L4BufItemCreate(L4FpageMemory(addr, order, rights), rcv_window)

/**
 * Create an I/O buffer item
 */
#define L4_BUF_ITEM_IO(port, order, rights, rcv_window) \
    L4BufItemCreate(L4FpageIo(port, order, rights), rcv_window)

/**
 * Create an object buffer item
 */
#define L4_BUF_ITEM_OBJ(idx, order, rights, rcv_window) \
    L4BufItemCreate(L4FpageObj(idx, order, rights), rcv_window)

#endif /* L4_MSG_ITEM_H */
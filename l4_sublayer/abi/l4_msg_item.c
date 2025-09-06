#include "../../aurora.h"
#include "../include/l4_types.h"
#include "../include/l4_msg_item.h"

/* L4 Message Item Implementation - Adapted from Fiasco L4
 * This provides message item handling for IPC operations
 */

/* L4 Message Item Functions */
L4_msg_item L4MsgItemCreate(L4_msg_item_type type, UINT64 data) {
    L4_msg_item item;
    item.raw = (data & L4_MSG_ITEM_DATA_MASK) | 
               (((UINT64)type & L4_MSG_ITEM_TYPE_MASK) << L4_MSG_ITEM_TYPE_SHIFT);
    return item;
}

L4_msg_item_type L4MsgItemGetType(L4_msg_item item) {
    return (L4_msg_item_type)((item.raw >> L4_MSG_ITEM_TYPE_SHIFT) & L4_MSG_ITEM_TYPE_MASK);
}

UINT64 L4MsgItemGetData(L4_msg_item item) {
    return item.raw & L4_MSG_ITEM_DATA_MASK;
}

BOOL L4MsgItemIsMap(L4_msg_item item) {
    return (item.raw & L4_MSG_ITEM_MAP) != 0;
}

void L4MsgItemSetMap(L4_msg_item* item) {
    if (item) {
        item->raw |= L4_MSG_ITEM_MAP;
    }
}

/* L4 Send Item Functions */
L4_snd_item L4SndItemCreate(L4_fpage fp, UINT64 snd_base, L4_snd_item_type type) {
    L4_snd_item item;
    item.raw = (fp.raw & L4_SND_ITEM_FPAGE_MASK) | 
               ((snd_base & L4_SND_ITEM_BASE_MASK) << L4_SND_ITEM_BASE_SHIFT) |
               (((UINT64)type & L4_SND_ITEM_TYPE_MASK) << L4_SND_ITEM_TYPE_SHIFT);
    return item;
}

L4_snd_item L4SndItemMap(L4_fpage fp, UINT64 snd_base) {
    return L4SndItemCreate(fp, snd_base, L4_SND_ITEM_MAP);
}

L4_snd_item L4SndItemGrant(L4_fpage fp, UINT64 snd_base) {
    return L4SndItemCreate(fp, snd_base, L4_SND_ITEM_GRANT);
}

L4_fpage L4SndItemGetFpage(L4_snd_item item) {
    L4_fpage fp;
    fp.raw = item.raw & L4_SND_ITEM_FPAGE_MASK;
    return fp;
}

UINT64 L4SndItemGetSndBase(L4_snd_item item) {
    return (item.raw >> L4_SND_ITEM_BASE_SHIFT) & L4_SND_ITEM_BASE_MASK;
}

L4_snd_item_type L4SndItemGetType(L4_snd_item item) {
    return (L4_snd_item_type)((item.raw >> L4_SND_ITEM_TYPE_SHIFT) & L4_SND_ITEM_TYPE_MASK);
}

BOOL L4SndItemIsMap(L4_snd_item item) {
    return L4SndItemGetType(item) == L4_SND_ITEM_MAP;
}

BOOL L4SndItemIsGrant(L4_snd_item item) {
    return L4SndItemGetType(item) == L4_SND_ITEM_GRANT;
}

/* L4 Buffer Item Functions */
L4_buf_item L4BufItemCreate(L4_fpage fp, UINT64 rcv_window) {
    L4_buf_item item;
    item.raw = (fp.raw & L4_BUF_ITEM_FPAGE_MASK) | 
               ((rcv_window & L4_BUF_ITEM_WINDOW_MASK) << L4_BUF_ITEM_WINDOW_SHIFT);
    return item;
}

L4_fpage L4BufItemGetFpage(L4_buf_item item) {
    L4_fpage fp;
    fp.raw = item.raw & L4_BUF_ITEM_FPAGE_MASK;
    return fp;
}

UINT64 L4BufItemGetRcvWindow(L4_buf_item item) {
    return (item.raw >> L4_BUF_ITEM_WINDOW_SHIFT) & L4_BUF_ITEM_WINDOW_MASK;
}

/* L4 Buffer Descriptor Functions */
L4_buf_desc L4BufDescCreate(UINT32 buf_count, UINT32 flags) {
    L4_buf_desc desc;
    desc.raw = (buf_count & L4_BUF_DESC_COUNT_MASK) | 
               (((UINT64)flags & L4_BUF_DESC_FLAGS_MASK) << L4_BUF_DESC_FLAGS_SHIFT);
    return desc;
}

UINT32 L4BufDescGetCount(L4_buf_desc desc) {
    return desc.raw & L4_BUF_DESC_COUNT_MASK;
}

UINT32 L4BufDescGetFlags(L4_buf_desc desc) {
    return (desc.raw >> L4_BUF_DESC_FLAGS_SHIFT) & L4_BUF_DESC_FLAGS_MASK;
}

void L4BufDescSetCount(L4_buf_desc* desc, UINT32 count) {
    if (desc) {
        desc->raw = (desc->raw & ~L4_BUF_DESC_COUNT_MASK) | (count & L4_BUF_DESC_COUNT_MASK);
    }
}

void L4BufDescSetFlags(L4_buf_desc* desc, UINT32 flags) {
    if (desc) {
        desc->raw = (desc->raw & ~(L4_BUF_DESC_FLAGS_MASK << L4_BUF_DESC_FLAGS_SHIFT)) | 
                   (((UINT64)flags & L4_BUF_DESC_FLAGS_MASK) << L4_BUF_DESC_FLAGS_SHIFT);
    }
}

/* Message Item Processing Functions */
L4_error L4ProcessSendItems(L4_utcb* utcb, UINT32 item_count, L4_obj_ref dest) {
    if (!utcb || item_count == 0) {
        return L4ErrorCreate(L4_EOK);
    }
    
    if (item_count > L4_MAX_MSG_ITEMS) {
        return L4ErrorCreate(L4_EMSGTOOLONG);
    }
    
    /* Process each send item */
    for (UINT32 i = 0; i < item_count; i++) {
        /* Get the send item from message registers */
        UINT64 item_raw = L4UtcbGetMR(utcb, L4_UTCB_MAX_WORDS - item_count + i);
        L4_snd_item item;
        item.raw = item_raw;
        
        /* Process based on item type */
        if (L4SndItemIsMap(item)) {
            L4_error map_error = L4ProcessMapItem(item, dest);
            if (!L4ErrorIsOk(map_error)) {
                return map_error;
            }
        } else if (L4SndItemIsGrant(item)) {
            L4_error grant_error = L4ProcessGrantItem(item, dest);
            if (!L4ErrorIsOk(grant_error)) {
                return grant_error;
            }
        }
    }
    
    return L4ErrorCreate(L4_EOK);
}

L4_error L4ProcessMapItem(L4_snd_item item, L4_obj_ref dest) {
    L4_fpage fp = L4SndItemGetFpage(item);
    UINT64 snd_base = L4SndItemGetSndBase(item);
    
    if (L4FpageIsNil(fp)) {
        return L4ErrorCreate(L4_EOK);
    }
    
    /* TODO: Implement actual mapping */
    /* This would involve:
     * 1. Validating the flexpage
     * 2. Checking sender permissions
     * 3. Finding destination address space
     * 4. Creating mapping in destination
     * 5. Updating page tables
     */
    
    return L4ErrorCreate(L4_EOK);
}

L4_error L4ProcessGrantItem(L4_snd_item item, L4_obj_ref dest) {
    L4_fpage fp = L4SndItemGetFpage(item);
    UINT64 snd_base = L4SndItemGetSndBase(item);
    
    if (L4FpageIsNil(fp)) {
        return L4ErrorCreate(L4_EOK);
    }
    
    /* TODO: Implement actual granting */
    /* This would involve:
     * 1. Validating the flexpage
     * 2. Checking sender permissions
     * 3. Removing mapping from sender
     * 4. Creating mapping in destination
     * 5. Updating page tables
     */
    
    return L4ErrorCreate(L4_EOK);
}

L4_error L4SetupReceiveBuffers(L4_utcb* utcb, L4_buf_item* buffers, UINT32 buffer_count) {
    if (!utcb || !buffers || buffer_count == 0) {
        return L4ErrorCreate(L4_EINVAL);
    }
    
    if (buffer_count > L4_UTCB_MAX_BUFFERS) {
        return L4ErrorCreate(L4_ERANGE);
    }
    
    /* Set up buffer descriptor */
    L4_buf_desc desc = L4BufDescCreate(buffer_count, 0);
    utcb->buf_desc = desc;
    
    /* Copy buffer items to UTCB */
    for (UINT32 i = 0; i < buffer_count; i++) {
        L4UtcbSetBR(utcb, i, buffers[i].raw);
    }
    
    return L4ErrorCreate(L4_EOK);
}

L4_error L4ProcessReceiveItems(L4_utcb* utcb, UINT32 item_count) {
    if (!utcb || item_count == 0) {
        return L4ErrorCreate(L4_EOK);
    }
    
    UINT32 buffer_count = L4BufDescGetCount(utcb->buf_desc);
    if (buffer_count == 0) {
        /* No receive buffers set up */
        return L4ErrorCreate(L4_EMSGTOOSHORT);
    }
    
    /* Process received items against available buffers */
    UINT32 processed = 0;
    for (UINT32 i = 0; i < item_count && processed < buffer_count; i++) {
        /* Get buffer item */
        UINT64 buf_raw = L4UtcbGetBR(utcb, processed);
        L4_buf_item buf_item;
        buf_item.raw = buf_raw;
        
        /* TODO: Process the received item against this buffer */
        /* This would involve:
         * 1. Matching received flexpage with buffer
         * 2. Creating appropriate mappings
         * 3. Updating receive window
         */
        
        processed++;
    }
    
    return L4ErrorCreate(L4_EOK);
}

/* Message Item Validation */
BOOL L4ValidateSendItem(L4_snd_item item) {
    L4_fpage fp = L4SndItemGetFpage(item);
    
    /* Check if flexpage is valid */
    if (L4FpageIsNil(fp)) {
        return TRUE; /* Nil flexpages are always valid */
    }
    
    /* Check flexpage type */
    L4_fpage_type type = L4FpageGetType(fp);
    if (type > L4_FPAGE_OBJ) {
        return FALSE;
    }
    
    /* Check order is reasonable */
    UINT32 order = L4FpageGetOrder(fp);
    if (order > L4_WHOLE_SPACE) {
        return FALSE;
    }
    
    return TRUE;
}

BOOL L4ValidateBufItem(L4_buf_item item) {
    L4_fpage fp = L4BufItemGetFpage(item);
    
    /* Check if flexpage is valid */
    if (L4FpageIsNil(fp)) {
        return FALSE; /* Buffer items must have valid flexpages */
    }
    
    /* Check flexpage type */
    L4_fpage_type type = L4FpageGetType(fp);
    if (type > L4_FPAGE_OBJ) {
        return FALSE;
    }
    
    /* Check order is reasonable */
    UINT32 order = L4FpageGetOrder(fp);
    if (order > L4_WHOLE_SPACE) {
        return FALSE;
    }
    
    return TRUE;
}
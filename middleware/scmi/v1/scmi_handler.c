/* Copyright Statement:
 *
 * This software/firmware and related documentation ("MediaTek Software") are
 * protected under relevant copyright laws. The information contained herein
 * is confidential and proprietary to MediaTek Inc. and/or its licensors.
 * Without the prior written permission of MediaTek inc. and/or its licensors,
 * any reproduction, modification, use or disclosure of MediaTek Software,
 * and information contained herein, in whole or in part, shall be strictly prohibited.
 */
/* MediaTek Inc. (C) 2021. All rights reserved.
 *
 * BY OPENING THIS FILE, RECEIVER HEREBY UNEQUIVOCALLY ACKNOWLEDGES AND AGREES
 * THAT THE SOFTWARE/FIRMWARE AND ITS DOCUMENTATIONS ("MEDIATEK SOFTWARE")
 * RECEIVED FROM MEDIATEK AND/OR ITS REPRESENTATIVES ARE PROVIDED TO RECEIVER ON
 * AN "AS-IS" BASIS ONLY. MEDIATEK EXPRESSLY DISCLAIMS ANY AND ALL WARRANTIES,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE OR NONINFRINGEMENT.
 * NEITHER DOES MEDIATEK PROVIDE ANY WARRANTY WHATSOEVER WITH RESPECT TO THE
 * SOFTWARE OF ANY THIRD PARTY WHICH MAY BE USED BY, INCORPORATED IN, OR
 * SUPPLIED WITH THE MEDIATEK SOFTWARE, AND RECEIVER AGREES TO LOOK ONLY TO SUCH
 * THIRD PARTY FOR ANY WARRANTY CLAIM RELATING THERETO. RECEIVER EXPRESSLY ACKNOWLEDGES
 * THAT IT IS RECEIVER'S SOLE RESPONSIBILITY TO OBTAIN FROM ANY THIRD PARTY ALL PROPER LICENSES
 * CONTAINED IN MEDIATEK SOFTWARE. MEDIATEK SHALL ALSO NOT BE RESPONSIBLE FOR ANY MEDIATEK
 * SOFTWARE RELEASES MADE TO RECEIVER'S SPECIFICATION OR TO CONFORM TO A PARTICULAR
 * STANDARD OR OPEN FORUM. RECEIVER'S SOLE AND EXCLUSIVE REMEDY AND MEDIATEK'S ENTIRE AND
 * CUMULATIVE LIABILITY WITH RESPECT TO THE MEDIATEK SOFTWARE RELEASED HEREUNDER WILL BE,
 * AT MEDIATEK'S OPTION, TO REVISE OR REPLACE THE MEDIATEK SOFTWARE AT ISSUE,
 * OR REFUND ANY SOFTWARE LICENSE FEES OR SERVICE CHARGE PAID BY RECEIVER TO
 * MEDIATEK FOR SUCH MEDIATEK SOFTWARE AT ISSUE.
 */

#include <stdbool.h>

#include <FreeRTOS.h>
#include <task.h>

#include <driver_api.h>
#include <mbox.h>

#include "internal/scmi.h"
#include "internal/scmi_base.h"
#include "scmi_handler.h"
#include "scmi_tinysys.h"

#define PROTOCOL_TABLE_BASE_PROTOCOL_IDX 1
#define PROTOCOL_TABLE_RESERVED_ENTRIES_COUNT 2

struct scmi_channel_ctx {
    unsigned int id;
    struct scmi_msg_t *shmem, *in, *out;
    volatile bool locked;
    unsigned int max_payload_size;
};

struct scmi_protocol {
    scmi_message_handler_t *message_handler;
    uint8_t id;
};

struct scmi_record_t {
    int seqno;
    struct scmi_rx_record_t rx[SCMI_RECORD_COUNT];
};
struct scmi_record_t scmi_record;
struct scmi_rx_record_t *scmi_rx_record;

/* Default supported protocols
 * table[0] is not used
 * table[1] is reserved for the base protocol implemented
 */
struct scmi_protocol protocol_table[SCMI_PROTOCOL_COUNT \
    + PROTOCOL_TABLE_RESERVED_ENTRIES_COUNT];

unsigned char scmi_rbuf128[MBOX_COUNT_MAX][SCMI_MESSAGE_LENGTH_MAX] = {0};
unsigned char scmi_wbuf128[MBOX_COUNT_MAX][SCMI_MESSAGE_LENGTH_MAX] = {0};
struct scmi_channel_ctx scmi_channel_ctx[MBOX_COUNT_MAX];


MBOX_BASE_4B(mbox0_mem);
MBOX_BASE_4B(mbox1_mem);
MBOX_BASE_4B(mbox2_mem);
MBOX_BASE_4B(mbox3_mem);
MBOX_BASE_4B(mbox4_mem);

extern int scmi_base_message_handler(unsigned int id,
    const uint32_t *payload, size_t payload_size, unsigned int message_id);

static uint32_t pack_message_header(uint8_t message_id,
    uint8_t message_type, uint8_t protocol_id, uint8_t token)
{
    return ((((message_id) << SCMI_MESSAGE_HEADER_MESSAGE_ID_POS) &
        SCMI_MESSAGE_HEADER_MESSAGE_ID_MASK) |
    (((message_type) << SCMI_MESSAGE_HEADER_MESSAGE_TYPE_POS) &
        SCMI_MESSAGE_HEADER_MESSAGE_TYPE_MASK) |
    (((protocol_id) << SCMI_MESSAGE_HEADER_PROTOCOL_ID_POS) &
        SCMI_MESSAGE_HEADER_PROTOCOL_ID_MASK) |
    (((token) << SCMI_MESSAGE_HEADER_TOKEN_POS) &
        SCMI_MESSAGE_HEADER_TOKEN_POS));
}

static uint16_t read_message_id(uint32_t message_header)
{
    return (message_header & SCMI_MESSAGE_HEADER_MESSAGE_ID_MASK) >>
        SCMI_MESSAGE_HEADER_MESSAGE_ID_POS;
}

static uint8_t read_message_type(uint32_t message_header)
{
    return (message_header & SCMI_MESSAGE_HEADER_MESSAGE_TYPE_MASK) >>
        SCMI_MESSAGE_HEADER_MESSAGE_TYPE_POS;
}

static uint8_t read_protocol_id(uint32_t message_header)
{
    return (message_header & SCMI_MESSAGE_HEADER_PROTOCOL_ID_MASK) >>
        SCMI_MESSAGE_HEADER_PROTOCOL_ID_POS;
}

static uint16_t read_token(uint32_t message_header)
{
    return (message_header & SCMI_MESSAGE_HEADER_TOKEN_MASK) >>
        SCMI_MESSAGE_HEADER_TOKEN_POS;
}


static void respond(uint32_t channel_id,
    const void *payload, size_t size, bool isr)
{
    struct scmi_channel_ctx *channel_ctx;
    struct scmi_msg_t *memory;
    int i;

    channel_ctx = &scmi_channel_ctx[channel_id];
    memory = channel_ctx->shmem;

    /* Copy the header from the write buffer */
    *memory = *channel_ctx->out;

    /* Copy the payload from either the write buffer or the payload parameter */
    memcpy(memory->payload,
           (payload == NULL ? channel_ctx->out->payload : payload), size);

#if 0 // TODO: should be in C.S but except base protocol
    if (!isr)
        taskENTER_CRITICAL();
#endif

    channel_ctx->locked = false;

    memory->length = sizeof(memory->message_header) + size;
    memory->status |= SCMI_MEM_STATUS_FREE_MASK;

    for (i = 0; i < size/sizeof(size_t); i++){
        scmi_rx_record->response[i] = memory->payload[i];
    }

#if 0
    if (!isr)
        taskEXIT_CRITICAL();
#endif

    SCMI_TRACE("to response:: sta: %x, flags: %x, len: %d, msg_h: %x\n",
        memory->status, memory->flags, memory->length, memory->message_header);

    SCMI_TRACE("status: %08x para: %08x %08x %08x\n",
        memory->payload[0], memory->payload[1],
        memory->payload[2], memory->payload[3]);

    if (memory->flags & SCMI_MEM_FLAGS_IENABLED_MASK)
        mbox_raise_irq(channel_id, SCMI_CHANNEL_SLOT_IDX);

    scmi_rx_record->ts[4]= get_ts();
    scmi_rx_record->os_timer[4] = get_os_timer();
}

static int transmit(uint32_t channel_id,
    uint32_t message_header, const void *payload, size_t size)
{
    struct scmi_channel_ctx *channel_ctx;
    struct scmi_msg_t *memory;

    if (payload == NULL)
        return SCMI_INVALID_PARAMETERS;

    channel_ctx = &scmi_channel_ctx[channel_id];
    memory = channel_ctx->shmem;

    /*
     * If the agent has not yet read the previous message we
     * abandon this transmission. We don't want to poll on the BUSY/FREE
     * bit, and while it is probably safe to just overwrite the data
     * the agent could be in the process of reading.
     */
    if (!(memory->status & SCMI_MEM_STATUS_FREE_MASK))
        return SCMI_BUSY;

    memory->message_header = message_header;

    /*
     * we do not want the agent to send an interrupt when it receives
     * the message.
     */
    memory->flags = 0;

    /* Copy the payload */
    memcpy(memory->payload, payload, size);

    memory->length = sizeof(memory->message_header) + size;
    memory->status &= ~SCMI_MEM_STATUS_FREE_MASK;

    /* Notify the agent */
    mbox_raise_irq(channel_id, SCMI_CHANNEL_SLOT_IDX);

    return SCMI_SUCCESS;
}


static int protocol_dispatch(uint32_t channel_id, struct scmi_msg_t *msg)
{
    uint8_t protocol_id, message_type;
    uint16_t token, message_id;
    uint32_t payload_size;
    unsigned int scmi_protocol_id, scmi_message_id;
    uint8_t idx;


    protocol_id = read_protocol_id(msg->message_header);
    message_id = read_message_id(msg->message_header);
    message_type = read_message_type(msg->message_header);
    token = read_token(msg->message_header);
    payload_size = msg->length - sizeof(msg->message_header);

    SCMI_TRACE("receive msg:: protocol#: 0x%x, msg#: 0x%x, msg_type: %x, token: %x\n",
        protocol_id, message_id, message_type, token);

    if ((idx = scmi_get_protocol_idx(protocol_id)) >=
        PROTOCOL_TABLE_BASE_PROTOCOL_IDX ) {
        SCMI_TRACE("get protocol 0x%x @table[%d]\n", protocol_id, idx);
        protocol_table[idx].message_handler(channel_id,
            msg->payload, payload_size, message_id);
    } else {
        PRINTF_E("MTK not support protocol 0x%x\n", protocol_id);
        respond(channel_id, &(int32_t) { SCMI_NOT_SUPPORTED }, sizeof(int32_t), true);
    }

    return SCMI_SUCCESS;
}


int scmi_init()
{
    int i;
    unsigned int mbox_mem[MBOX_COUNT_MAX] = {
        (unsigned int)mbox0_mem,
        (unsigned int)mbox1_mem,
        (unsigned int)mbox2_mem,
        (unsigned int)mbox3_mem,
        (unsigned int)mbox4_mem};

    for (i= 0 ; i < MBOX_COUNT_MAX; i++){
        mbox_init(i, false, mbox_mem[i]);

        scmi_channel_ctx[i].id = i;
        scmi_channel_ctx[i].shmem = (struct scmi_msg_t *)mbox_mem[i];
        scmi_channel_ctx[i].shmem->status |= SCMI_MEM_STATUS_FREE_MASK;
        scmi_channel_ctx[i].in = (struct scmi_msg_t *)scmi_rbuf128[i];
        scmi_channel_ctx[i].out = (struct scmi_msg_t *)scmi_wbuf128[i];
        scmi_channel_ctx[i].max_payload_size = 128 - sizeof(struct scmi_msg_t);
        scmi_channel_ctx[i].locked = false;

        SCMI_TRACE("scmi_channel[%d] *shmem=0x%x, *in=0x%x, *out=0x%x, max_payload_size: %d\n",
            i, scmi_channel_ctx[i].shmem,
            scmi_channel_ctx[i].in,
            scmi_channel_ctx[i].out,
            scmi_channel_ctx[i].max_payload_size);
    }

    protocol_table[PROTOCOL_TABLE_BASE_PROTOCOL_IDX].id =
        SCMI_PROTOCOL_ID_BASE;
    protocol_table[PROTOCOL_TABLE_BASE_PROTOCOL_IDX].message_handler =
        scmi_base_message_handler;

    return SCMI_SUCCESS;
}

int scmi_protocol_register(uint8_t protocol_id, scmi_message_handler_t *api)
{
    unsigned int index;

    if ( api == NULL)
        return SCMI_Err;

    taskENTER_CRITICAL();
    for (index = PROTOCOL_TABLE_RESERVED_ENTRIES_COUNT;
        index < SCMI_ARRAY_SIZE(protocol_table);
        index ++) {
        if (protocol_table[index].id ==0) {
            protocol_table[index].id = protocol_id;
            protocol_table[index].message_handler= api;
            SCMI_TRACE("hook protocol 0x%x to table[%d]: 0x%x\n", protocol_id, index, *api);
            taskEXIT_CRITICAL();
            goto exit;
        }
    }
    taskENTER_CRITICAL();

    PRINTF_E("protocol 0x%x register fail\n", protocol_id);
    return SCMI_Err;

exit:
    SCMI_TRACE("register protocol 0x%x to protocol_table[%d]\n", protocol_id, index);
    return SCMI_SUCCESS;
}

uint8_t scmi_get_protocol_idx(uint8_t protocol_id)
{
    unsigned int index;

    for (index = PROTOCOL_TABLE_BASE_PROTOCOL_IDX;
        index < SCMI_ARRAY_SIZE(protocol_table);
        index ++) {
        if (protocol_id == protocol_table[index].id)
            return index;
    }

    return 0;
}

/* Skip the first 2 entries which is for
 * [0] not used & [1] reserved for the base protocol */
uint8_t scmi_get_protocol_id(uint8_t idx)
{
    idx += PROTOCOL_TABLE_RESERVED_ENTRIES_COUNT;

    if (idx >= SCMI_ARRAY_SIZE(protocol_table))
        return 0;
    else
        return protocol_table[idx].id;
}

int scmi_get_max_payload_size(uint32_t channel_id, size_t *size)
{
    struct scmi_channel_ctx *channel_ctx;

    if (size == NULL)
        return SCMI_Err;

    channel_ctx = &scmi_channel_ctx[channel_id];
    size = channel_ctx->max_payload_size;

    return SCMI_SUCCESS;
}

int scmi_write_payload(uint32_t channel_id, size_t offset,
                         const void *payload, size_t size)
{
    struct scmi_channel_ctx *channel_ctx;

    channel_ctx = &scmi_channel_ctx[channel_id];

    if ((payload == NULL)   ||
        (offset  > channel_ctx->max_payload_size)   ||
        (size > channel_ctx->max_payload_size)  ||
        ((offset + size) > channel_ctx->max_payload_size)) {
        PRINTF_E("SCMI Err: can't write %d bytes payload in ch%d ofs 0x%x\n", size, offset);
        return SCMI_Err;
    }

    if (!channel_ctx->locked)
        return SCMI_Err;

    memcpy(((uint8_t*)channel_ctx->out->payload) + offset, payload, size);

    return SCMI_SUCCESS;
}

int scmi_message_handler(uint32_t channel_id, unsigned int addr)
{
    struct scmi_channel_ctx *channel_ctx;
    struct scmi_msg_t *memory, *in, *out;
    struct scmi_rx_record_t *record;
    size_t payload_size;
    int status;

    channel_ctx = &scmi_channel_ctx[channel_id];

    if (channel_ctx->locked) {
        PRINTF_E("SCMI Err: channel %u is already processing\n", channel_ctx->id);
        return SCMI_Err;
    }

    scmi_record.seqno++;
    record = &scmi_record.rx[(scmi_record.seqno % SCMI_RECORD_COUNT)];
    memset(record, 0, sizeof(struct scmi_rx_record_t));

    scmi_rx_record = record;
    scmi_rx_record->ts[0]= get_ts();
    scmi_rx_record->os_timer[0] = get_os_timer();

    memory = ((struct scmi_msg_t*)addr);
    in = channel_ctx->in;
    out = channel_ctx->out;

    SCMI_TRACE("*shmem:: sta: %x, flags: %x, len: %d, msg_h: %x\n",
        memory->status, memory->flags, memory->length, memory->message_header);

    if (memory->status & SCMI_MEM_STATUS_FREE_MASK) {
        PRINTF_E("Err: not ownership of channel %u\n", channel_ctx->id);
        return SCMI_Err;
    }

    channel_ctx->locked = true;

    /* Mirror contents in read and write buffers (without payload) */
    *in  = *memory;
    *out = *memory;

    scmi_rx_record->sta = in->status;
    scmi_rx_record->flag = in->flags;
    scmi_rx_record->len = in->length;
    scmi_rx_record->msg_h = in->message_header;

    out->status &= ~SCMI_MEM_STATUS_ERROR_MASK;

    if ((in->length < sizeof(in->message_header)) ||
        ((in->length - sizeof(in->message_header)) > channel_ctx->max_payload_size)) {

        out->status |= SCMI_MEM_STATUS_ERROR_MASK;

        SCMI_TRACE("verify fail:: len is %d\n", in->length);
        SCMI_TRACE("*out:: sta: %x, flags: %x, len: %d, msg_h: %x\n",
                out->status, out->flags, out->length, out->message_header);

        respond(channel_id, &(int32_t) { SCMI_PROTOCOL_ERROR }, sizeof(int32_t), true);

        return SCMI_SUCCESS;
    }

    /* Copy payload from shared memory to read buffer */
    payload_size = in->length - sizeof(in->message_header);
    memcpy(in->payload, memory->payload, payload_size);

    SCMI_TRACE("*in:: sta: %x, flags: %x, len: %d, msg_h: %x\n",
        in->status, in->flags, in->length, in->message_header);

    return protocol_dispatch(channel_id, in);
}

void scmi_respond(uint32_t channel_id, const void *payload, size_t size)
{
    respond(channel_id, payload, size, false);
}

int scmi_notify(uint32_t channel_id, uint8_t protocol_id, int message_id,
        const void *payload, size_t size)
{
    uint32_t message_header;

    message_header = pack_message_header(message_id,
        SCMI_MESSAGE_TYPE_NOTIFICATION, protocol_id, 0);

    return transmit(channel_id, message_header, payload, size);
}

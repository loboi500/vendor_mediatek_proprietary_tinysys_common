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

#include "FreeRTOS.h"
#include "semphr.h"

#include "scmi_handler.h"
#include "scmi_tinysys.h"


#define TINYSYS_COMMAND_CMD_COUNT \
    (SCMI_TINYSYS_COMMAND_COUNT - (SCMI_PROTOCOL_MESSAGE_ATTRIBUTES+1))

#define tinysys_cmd_to_idx(cmd_id)  \
    (cmd_id - (SCMI_PROTOCOL_MESSAGE_ATTRIBUTES + 1))

struct tinysys_feature_cb {
    tinysys_message_cb_t *cb;
    void *para;
};

/* For slim, use 32bit to record Channel#/Msg#/Notfy_Status */
struct tinysys_feature {
    uint32_t channel_id:4,
        recv_cmd_id:8,
        notify_sta:1,
        resv:19;
    const uint32_t *payload;
    struct tinysys_feature_cb cb_table[TINYSYS_COMMAND_CMD_COUNT];
    SemaphoreHandle_t *recv_lock;
    SemaphoreHandle_t *send_lock;
};

struct tinysys_feature feature_table[SCMI_FEATURE_COUNT - 1];

static int scmi_tinysys_protocol_version_handler(
    unsigned int id, const uint32_t *payload);
static int scmi_tinysy_protocol_attributes_handler(
    unsigned int id, const uint32_t *payload);
static int scmi_tinysys_protocol_message_attributes_handler(
    unsigned int id, const uint32_t *payload);
static int scmi_tinysys_protocol_common_set_handler(
    unsigned int id, const uint32_t *payload);
static int scmi_tinysys_protocol_common_get_handler(
    unsigned int id, const uint32_t *payload);
static int scmi_tinysys_protocol_event_notify_handler(
    unsigned int id, const uint32_t *payload);

static int (*const tinysys_handler_table[])(unsigned int, const uint32_t *) = {
    [SCMI_PROTOCOL_VERSION] = scmi_tinysys_protocol_version_handler,
    [SCMI_PROTOCOL_ATTRIBUTES] = scmi_tinysy_protocol_attributes_handler,
    [SCMI_PROTOCOL_MESSAGE_ATTRIBUTES] =
        scmi_tinysys_protocol_message_attributes_handler,
    [SCMI_TINYSYS_COMMON_SET] = scmi_tinysys_protocol_common_set_handler,
    [SCMI_TINYSYS_COMMON_GET] = scmi_tinysys_protocol_common_get_handler,
    [SCMI_TINYSYS_EVENT_NOTIFY] = scmi_tinysys_protocol_event_notify_handler,
};

static const unsigned int tinysys_payload_size_table[] = {
    [SCMI_PROTOCOL_VERSION] = 0,
    [SCMI_PROTOCOL_ATTRIBUTES] = 0,
    [SCMI_PROTOCOL_MESSAGE_ATTRIBUTES] =
        sizeof(struct scmi_protocol_message_attributes_a2p),
    [SCMI_TINYSYS_COMMON_SET] = sizeof(struct scmi_tinysys_common_set_a2p),
    [SCMI_TINYSYS_COMMON_GET] = sizeof(struct scmi_tinysys_common_get_a2p),
    [SCMI_TINYSYS_EVENT_NOTIFY] = sizeof(struct scmi_tinysys_event_notify_a2p),
};

static const unsigned int tinysys_reponse_size_table[] = {
    [SCMI_TINYSYS_COMMON_SET] = sizeof(struct scmi_tinysys_common_set_p2a),
    [SCMI_TINYSYS_COMMON_GET] = sizeof(struct scmi_tinysys_common_get_p2a),
    [SCMI_TINYSYS_EVENT_NOTIFY] = sizeof(struct scmi_tinysys_event_notify_p2a),
};

static int scmi_tinysys_protocol_version_handler(unsigned int id,
                                              const uint32_t *payload)
{
    struct scmi_protocol_version_p2a return_values = {
        .status = SCMI_SUCCESS,
        .version = SCMI_PROTOCOL_VERSION_TINYSYS,
    };

    scmi_respond(id, &return_values, sizeof(return_values));

    return SCMI_SUCCESS;
}

static int scmi_tinysy_protocol_attributes_handler(unsigned int id,
                                                 const uint32_t *payload)
{
    struct scmi_protocol_attributes_p2a return_values = {
        .status = SCMI_SUCCESS,
        .attributes = SCMI_TINYSYS_NUM_SOURCES,
    };

    scmi_respond(id, &return_values, sizeof(return_values));

    return SCMI_SUCCESS;
}

static int scmi_tinysys_protocol_message_attributes_handler(unsigned int id,
                                                       const uint32_t *payload)
{
    struct scmi_protocol_message_attributes_a2p *parameters;
    struct scmi_protocol_message_attributes_p2a return_values = {
        .status = SCMI_SUCCESS,
    };

    parameters = (struct scmi_protocol_message_attributes_a2p *)payload;

    scmi_respond(id, &return_values, sizeof(return_values));

    return SCMI_SUCCESS;
}

static void handler_to_callback(
    uint32_t ch_id, uint32_t f_id, uint32_t msg_id, const uint32_t *payload)
{
    struct tinysys_feature *f_handler = NULL;
    static BaseType_t xTaskWoken = 0;
    int ret = SCMI_NOT_FOUND;

    f_handler = &feature_table[f_id];

    if ( f_handler->cb_table[tinysys_cmd_to_idx(msg_id)].cb){
        f_handler->channel_id = ch_id;
        f_handler->payload = payload;
        f_handler->recv_cmd_id = msg_id;
        xSemaphoreGiveFromISR(f_handler->recv_lock, &xTaskWoken);

        scmi_rx_record->ts[1] = get_ts();
        scmi_rx_record->os_timer[1] = get_os_timer();
        scmi_rx_record->cmd = msg_id;
        scmi_rx_record->fid = f_id;

        scmi_rx_record->payload[0] = payload[0];
        scmi_rx_record->payload[1] = payload[1];
        scmi_rx_record->payload[2] = payload[2];
        scmi_rx_record->payload[3] = payload[3];
        scmi_rx_record->payload[4] = payload[4];

        SCMI_TRACE("ISR release feature_%d lock with msg#%d *cb @0x%x\n",
            f_id, msg_id, f_handler->cb_table[tinysys_cmd_to_idx(msg_id)].cb);
    } else {
        PRINTF_W("SCMI feature_%d has not bind to msg#%d\n", f_id, msg_id);
        scmi_respond(ch_id, &ret, sizeof(ret));
    }

    portYIELD_FROM_ISR(xTaskWoken);
}

static int scmi_tinysys_protocol_common_set_handler(unsigned int id,
                                              const uint32_t *payload)
{
    struct scmi_tinysys_common_set_a2p *parameters;

    parameters = (struct scmi_tinysys_common_set_a2p *)payload;

    SCMI_TRACE("COMMON_SET for feature_%d with [1] 0x%x [2] 0x%x [3] 0x%x [4] 0x%x [5] 0x%x\n",
         parameters->feature_id, parameters->para1, parameters->para2,
         parameters->para3,  parameters->para4,  parameters->para5);

    handler_to_callback(id, parameters->feature_id,
        SCMI_TINYSYS_COMMON_SET, &parameters->para1);

    return SCMI_SUCCESS;
}

static int scmi_tinysys_protocol_common_get_handler(unsigned int id,
                                              const uint32_t *payload)
{
    struct scmi_tinysys_common_get_a2p *parameters;

    parameters = (struct scmi_tinysys_common_get_a2p *)payload;

    SCMI_TRACE("COMMON_GET for feature_%d with para 0x%x\n",
         parameters->feature_id, parameters->para);

    handler_to_callback(id, parameters->feature_id,
        SCMI_TINYSYS_COMMON_GET, &parameters->para);

    return SCMI_SUCCESS;
}

static int scmi_tinysys_protocol_event_notify_handler(unsigned int id,
                                              const uint32_t *payload)
{
    struct tinysys_feature *f_handler = NULL;
    struct scmi_tinysys_event_notify_a2p *parameters;
    struct scmi_tinysys_event_notify_p2a return_values = {
        .status = SCMI_SUCCESS,
    };

    parameters = (struct scmi_tinysys_event_notify_a2p *)payload;
    f_handler = &feature_table[parameters->feature_id];

    SCMI_TRACE("COMMON_EVENT_NOTIFY for feature_%d: %s notification\n",
         parameters->feature_id, parameters->notify_enable ? "enable" : "disable");

    f_handler->notify_sta = parameters->notify_enable;

    scmi_respond(id, &return_values, sizeof(return_values));

    return SCMI_SUCCESS;
}

static int scmi_tinysys_message_handler(unsigned int id,
    const uint32_t *payload, size_t payload_size, unsigned int message_id)
{
    int32_t return_value;

    SCMI_TRACE("Get msg_%d with %d bytes payload\n", message_id, payload_size);

    if (message_id >= SCMI_ARRAY_SIZE(tinysys_handler_table)) {
        return_value = SCMI_NOT_FOUND;
        goto error;
    }

    if (payload_size != tinysys_payload_size_table[message_id]) {
        return_value = SCMI_PROTOCOL_ERROR;
        goto error;
    }

    return tinysys_handler_table[message_id](id, payload);

error:
    SCMI_TRACE("error response: %d\n", return_value);
    scmi_respond(id, &return_value, sizeof(return_value));

    return SCMI_SUCCESS;
}

void scmi_tinysys_protocol_init()
{
    scmi_protocol_register(SCMI_PROTOCOL_ID_TINYSYS,
        &scmi_tinysys_message_handler);
}

int scmi_tinysys_protocol_bind(uint32_t feature_id,
    unsigned int cmd_id, tinysys_message_cb_t *cb, void *cb_para)
{
    struct tinysys_feature_cb *callback;

    if (feature_id >= SCMI_PROTOCOL_ID_TINYSYS ||
        (cmd_id != SCMI_TINYSYS_COMMON_SET &&
        cmd_id != SCMI_TINYSYS_COMMON_GET)) {
        PRINTF_E("SCMI feature_%d bind to tinysys cmd #0x%x fail.\n");
        return SCMI_NOT_FOUND;
    }

    callback = &feature_table[feature_id].cb_table[tinysys_cmd_to_idx(cmd_id)];

    callback->cb = cb;
    callback->para = cb_para;

    if ( feature_table[feature_id].recv_lock == NULL) {
        feature_table[feature_id].recv_lock =
            (SemaphoreHandle_t *) xSemaphoreCreateBinary();
        SCMI_TRACE("feature %d new binding: create recv_lock \n", feature_id);
    }

    SCMI_TRACE("feature %d bind to tinysys cmd 0x%x with *cb @0x%x\n",
        feature_id, cmd_id, *cb);

    return SCMI_SUCCESS;
}

int scmi_tinysys_message_receive(uint32_t feature_id, void **reply_data)
{
    int ret;
    struct tinysys_feature_cb *callback;
    struct tinysys_feature *f_handler = NULL;

    f_handler = &feature_table[feature_id];

    SCMI_TRACE("before feature %d receive msg, reply data @0x%x\n",
        feature_id, *reply_data);

    /* recvice the message from ISR */
    ret = xSemaphoreTake(*f_handler->recv_lock, portMAX_DELAY);
    scmi_rx_record->ts[2]= get_ts();
    scmi_rx_record->os_timer[2] = get_os_timer();

    if (ret == pdFALSE)
        return SCMI_BUSY;
    else {
        /* do user callback */
        callback = &f_handler->cb_table[tinysys_cmd_to_idx(f_handler->recv_cmd_id)];
        if (callback->cb) {
            callback->cb(callback->para, f_handler->payload);
            scmi_rx_record->ts[3]= get_ts();
            scmi_rx_record->os_timer[3] = get_os_timer();
        }
    }

    SCMI_TRACE("after feature %d handle msg, reply data @0x%x\n",
        feature_id, *reply_data);

    /* send the response */
    scmi_respond(f_handler->channel_id, *reply_data,
        tinysys_reponse_size_table[f_handler->recv_cmd_id]);

    return SCMI_SUCCESS;
}

int scmi_tinysys_event_notifier(uint32_t feature_id,
    uint32_t para1, uint32_t para2, uint32_t para3, uint32_t para4, int retry)
{
    struct tinysys_feature *f_handler = NULL;
    struct scmi_tinysys_event_notifier_p2a return_values;
    int ret = 0;
    bool in_isr = is_in_isr();

    f_handler = &feature_table[feature_id];

    if (f_handler->notify_sta) {
        return_values.feature_id = feature_id;
        return_values.para1 = para1;
        return_values.para2 = para2;
        return_values.para3 = para3;
        return_values.para4 = para4;

        SCMI_TRACE("feature %d notify: data 0x%x 0x%x 0x%x 0x%x to ch%d\n",
            feature_id, para1, para2, para3, para4, f_handler->channel_id);

        /* force retry in tinysys protocol to save the code size */
        while(1) {
            if (!in_isr)
                taskENTER_CRITICAL();
            ret = scmi_notify(SCMI_NOTIFICATION_CHANNEL_TINYSYS,
                SCMI_PROTOCOL_ID_TINYSYS, SCMI_TINYSYS_EVENT_NOTIFIER,
                &return_values, sizeof(return_values));
            if (!in_isr)
                taskEXIT_CRITICAL();
            if (ret != SCMI_BUSY)
                break;
            if (retry == 0)
                break;
            retry--;
            SCMI_TINYSYS_NOTIFIER_DELAY_HOOK();
        }
        return ret;
    } else {
        PRINTF_W("SCMI feature_%d don't subscriber notification.\n", feature_id);
        return SCMI_NOT_FOUND;
    }
}

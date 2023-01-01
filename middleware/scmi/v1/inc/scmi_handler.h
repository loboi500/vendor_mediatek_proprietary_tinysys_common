/* Copyright Statement:
 *
 * This software/firmware and related documentation ("MediaTek Software") are
 * protected under relevant copyright laws. The information contained herein
 * is confidential and proprietary to MediaTek Inc. and/or its licensors.
 * Without the prior written permission of MediaTek inc. and/or its licensors,
 * any reproduction, modification, use or disclosure of MediaTek Software,
 * and information contained herein, in whole or in part, shall be strictly
 * prohibited.
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
 * THIRD PARTY FOR ANY WARRANTY CLAIM RELATING THERETO. RECEIVER EXPRESSLY
 * ACKNOWLEDGES THAT IT IS RECEIVER\'S SOLE RESPONSIBILITY TO OBTAIN FROM ANY
 * THIRD PARTY ALL PROPER LICENSES CONTAINED IN MEDIATEK SOFTWARE. MEDIATEK
 * SHALL ALSO NOT BE RESPONSIBLE FOR ANY MEDIATEK SOFTWARE RELEASES MADE TO
 * RECEIVER\'S SPECIFICATION OR TO CONFORM TO A PARTICULAR STANDARD OR OPEN
 * FORUM. RECEIVER\'S SOLE AND EXCLUSIVE REMEDY AND MEDIATEK\'S ENTIRE AND
 * CUMULATIVE LIABILITY WITH RESPECT TO THE MEDIATEK SOFTWARE RELEASED HEREUNDER
 * WILL BE, AT MEDIATEK\'S OPTION, TO REVISE OR REPLACE THE MEDIATEK SOFTWARE
 * AT ISSUE, OR REFUND ANY SOFTWARE LICENSE FEES OR SERVICE CHARGE PAID BY
 * RECEIVER TO MEDIATEK FOR SUCH MEDIATEK SOFTWARE AT ISSUE.
 *
 * The following software/firmware and/or related documentation
 * ("MediaTek Software") have been modified by MediaTek Inc. All revisions are
 * subject to any receiver\'s applicable license agreements with MediaTek Inc.
 */

#ifndef _SCMI_HANDLER_H_
#define _SCMI_HANDLER_H_

#include <string.h>
#include <stdio.h>
#include <mt_printf.h>
#include <ispeed.h>
#ifdef CFG_TIMER_SUPPORT
#include "ostimer.h"
#endif

#ifdef SCMI_DBG
#define SCMI_TRACE(f, ...)    PRINTF_E("%s(#%d): " f, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#else
#define SCMI_TRACE(f, ...)
#endif

#ifndef SCMI_RECORD_COUNT
#define SCMI_RECORD_COUNT 3
#endif

#define SCMI_ARRAY_SIZE(A) (sizeof(A) / sizeof((A)[0]))
#define SCMI_ALIGN_NEXT(VALUE, INTERVAL) \
    ((((VALUE) + (INTERVAL) - 1) / (INTERVAL)) * (INTERVAL))

typedef int scmi_message_handler_t(unsigned int channel_id,
    const uint32_t *payload, unsigned int payload_size, unsigned int message_id);


// TODO: should be defined by each uP platform
#define get_ts() ispeed_get_ts();
#define get_os_timer() ispeed_get_os_ts();

extern struct scmi_rx_record_t *scmi_rx_record;

enum scmi_message_type {
    SCMI_MESSAGE_TYPE_COMMAND = 0,
    SCMI_MESSAGE_TYPE_DELAYED_RESPONSE = 2,
    SCMI_MESSAGE_TYPE_NOTIFICATION = 3,
};

enum scmi_error {
    SCMI_SUCCESS = 0,
    SCMI_NOT_SUPPORTED = -1,
    SCMI_INVALID_PARAMETERS = -2,
    SCMI_DENIED = -3,
    SCMI_NOT_FOUND = -4,
    SCMI_OUT_OF_RANGE = -5,
    SCMI_BUSY = -6,
    SCMI_COMMS_ERROR = -7,
    SCMI_GENERIC_ERROR = -8,
    SCMI_HARDWARE_ERROR = -9,
    SCMI_PROTOCOL_ERROR = -10,
    SCMI_Err = -99,
};

enum scmi_agent_type {
    SCMI_AGENT_TYPE_PSCI,
    SCMI_AGENT_TYPE_MANAGEMENT,
    SCMI_AGENT_TYPE_OSPM,
    SCMI_AGENT_TYPE_OTHER,
    SCMI_AGENT_TYPE_COUNT,
};

enum scmi_transport_type {
    SCMI_TRANSPORT_MBOX,
    SCMI_TRANSPORT_IPIC,
    SCMI_TRANSPORT_TYPE_COUNT,
};

enum scmi_channel_type {
    SCMI_CHANNEL_TYPE_MASTER,
    SCMI_CHANNEL_TYPE_SLAVE,
    SCMI_CHANNEL_TYPE_COUNT,
};


/* Common command identifiers. */
enum scmi_command_id {
    SCMI_PROTOCOL_VERSION = 0x000,
    SCMI_PROTOCOL_ATTRIBUTES = 0x001,
    SCMI_PROTOCOL_MESSAGE_ATTRIBUTES = 0x002
};

/*Definitions of the SCMI Protocol Identifiers and the command
 * identifiers for each protocol.
 */
#define SCMI_PROTOCOL_ID_BASE UINT8_C(0x10)
enum scmi_base_command_id {
    SCMI_BASE_DISCOVER_VENDOR = 0x003,
    SCMI_BASE_DISCOVER_SUB_VENDOR = 0x004,
    SCMI_BASE_DISCOVER_IMPLEMENTATION_VERSION = 0x005,
    SCMI_BASE_DISCOVER_LIST_PROTOCOLS = 0x006,
    SCMI_BASE_DISCOVER_AGENT = 0x007,
    SCMI_BASE_NOTIFY_ERRORS = 0x008,
    SCMI_BASE_SET_DEVICE_PERMISSIONS = 0x009,
    SCMI_BASE_SET_PROTOCOL_PERMISSIONS = 0x00A,
    SCMI_BASE_RESET_AGENT_CONFIG = 0x00B,
    SCMI_BASE_COMMAND_COUNT,
};

#define SCMI_PROTOCOL_ID_TINYSYS UINT8_C(0x80)
enum scmi_tinysys_command_id {
    SCMI_TINYSYS_COMMON_SET = 0x003,
    SCMI_TINYSYS_COMMON_GET = 0x004,
    SCMI_TINYSYS_EVENT_NOTIFY = 0x005,
    SCMI_TINYSYS_COMMAND_COUNT,
};


/* Generic platform-to-agent message structure. */
struct scmi_protocol_version_p2a {
    int32_t status;
    uint32_t version;
};

struct scmi_protocol_attributes_p2a {
    int32_t status;
    uint32_t attributes;
};

struct scmi_protocol_message_attributes_p2a {
    int32_t status;
    uint32_t attributes;
};

/* Generic agent-to-platform message structure.*/
struct scmi_protocol_message_attributes_a2p {
    uint32_t message_id;
};

/* Record for debug */
struct scmi_rx_record_t {
    unsigned long long ts[5];
    unsigned long long os_timer[5];
    unsigned int sta:4,
        flag:4,
        len:24;
    unsigned int msg_h;
    unsigned int cmd:4,
        fid:4,
        resv:24;
    uint32_t payload[5];
    uint32_t response[4];
};

int scmi_init();

/* Driver API which for protocol layer */
int scmi_protocol_register(uint8_t protocol_id, scmi_message_handler_t *api);
#if 0 // Not implement yet. It's Base & Tinysys protocol un-used API
int scmi_get_agent_count(int *agent_count);
int scmi_get_agent_type(uint32_t agent_id, enum scmi_agent_type *agent_type);
#endif
uint8_t scmi_get_protocol_id(uint8_t idx);
uint8_t scmi_get_protocol_idx(uint8_t protocol_id);
int scmi_get_max_payload_size(uint32_t channel_id, size_t *size);
int scmi_write_payload(uint32_t channel_id, size_t offset,
    const void *payload, size_t size);
void scmi_respond(uint32_t channel_id, const void *payload, size_t size);
int scmi_notify(uint32_t channel_id, uint8_t protocol_id, int message_id,
    const void *payload, size_t size);

/* Driver API which for transport layer */
int scmi_message_handler(uint32_t channel_id, unsigned int addr);

#endif /* _SCMI_HANDLER_H_ */

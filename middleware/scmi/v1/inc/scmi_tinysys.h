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

#ifndef _SCMI_TINYSYS_H
#define _SCMI_TINYSYS_H

#include <stdint.h>
#include <string.h>
#include <stdio.h>

#include <scmi_feature_id.h>


typedef void tinysys_message_cb_t(void *para, const uint32_t *payload);

#define SCMI_PROTOCOL_VERSION_TINYSYS UINT32_C(0x10000)
#define SCMI_NOTIFICATION_CHANNEL_TINYSYS   1
#define SCMI_TINYSYS_NUM_SOURCES    20


enum scmi_tinysys_notification_id {
    SCMI_TINYSYS_EVENT_NOTIFIER = 0x000,
};

/*
 * TINYSYS_COMMON_SET
 */
struct scmi_tinysys_common_set_a2p {
    uint32_t resv;
    uint32_t feature_id;
    uint32_t para1;
    uint32_t para2;
    uint32_t para3;
    uint32_t para4;
    uint32_t para5;
};

struct scmi_tinysys_common_set_p2a {
    int32_t status;
};

/*
 * TINYSYS_COMMON_GET
 */
struct scmi_tinysys_common_get_a2p {
    uint32_t resv;
    uint32_t feature_id;
    uint32_t para;
};

struct scmi_tinysys_common_get_p2a {
    int32_t status;
    int32_t ret1;
    int32_t ret2;
    int32_t ret3;
};

/*
 * TINYSYS_EVENT_NOTIFY
 */
struct scmi_tinysys_event_notify_a2p {
    uint32_t resv;
    uint32_t feature_id;
    uint32_t notify_enable;
};

struct scmi_tinysys_event_notify_p2a {
    int32_t status;
};

/*
 * TINYSYS_EVENT_NOTIFIER
 */
struct scmi_tinysys_event_notifier_p2a {
    uint32_t feature_id;
    uint32_t para1;
    uint32_t para2;
    uint32_t para3;
    uint32_t para4;
};

void scmi_tinysys_protocol_init();

int scmi_tinysys_protocol_bind(uint32_t feature_id,
    unsigned int cmd_id, tinysys_message_cb_t *cb, void *cb_para);

int scmi_tinysys_message_receive(uint32_t feature_id, void **reply_data);

int scmi_tinysys_event_notifier(uint32_t feature_id, uint32_t para1,
    uint32_t para2, uint32_t para3, uint32_t para4, int retry);

#ifndef SCMI_TINYSYS_NOTIFIER_DELAY_HOOK
    #define SCMI_TINYSYS_NOTIFIER_DELAY_HOOK()
#endif

#endif /* _SCMI_TINYSYS_H */


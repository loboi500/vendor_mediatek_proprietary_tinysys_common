/*
 * MTK Tinysys Software
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Description:
 *      SCMI base protocol definitions.
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


#ifndef _SCMI_BASE_H
#define _SCMI_BASE_H

#include <stdint.h>

#define SCMI_IDENTIFIER "mtk"
#define SCMI_SUB_IDENTIFIER "mtk"

// TODO: it should be declared in each tinysys
#define SCMI_BUILD_VERSION  0x5353504D  /* S S P M */
#define SCMI_PROTOCOL_COUNT 1 /* Supported protocol exclude the 'Base' */
#define SCMI_AGENT_COUNT    1

/*
 * PROTOCOL_ATTRIBUTES
 */
#define SCMI_PROTOCOL_VERSION_BASE UINT32_C(0x10000)

#define SCMI_BASE_PROTOCOL_ATTRIBUTES_NUM_PROTOCOLS_POS  0
#define SCMI_BASE_PROTOCOL_ATTRIBUTES_NUM_AGENTS_POS     8

#define SCMI_BASE_PROTOCOL_ATTRIBUTES_NUM_PROTOCOLS_MASK 0xFF
#define SCMI_BASE_PROTOCOL_ATTRIBUTES_NUM_AGENTS_MASK    0xFF00

#define SCMI_BASE_PROTOCOL_ATTRIBUTES(NUM_PROTOCOLS, NUM_AGENTS) \
    ((((NUM_PROTOCOLS) << SCMI_BASE_PROTOCOL_ATTRIBUTES_NUM_PROTOCOLS_POS) \
      & SCMI_BASE_PROTOCOL_ATTRIBUTES_NUM_PROTOCOLS_MASK) | \
     (((NUM_AGENTS) << SCMI_BASE_PROTOCOL_ATTRIBUTES_NUM_AGENTS_POS) \
       & SCMI_BASE_PROTOCOL_ATTRIBUTES_NUM_AGENTS_MASK))

/*
 * BASE_DISCOVER_VENDOR
 */
struct scmi_base_discover_vendor_p2a {
    int32_t status;
    char vendor_identifier[16];
};

/*
 * BASE_DISCOVER_SUB_VENDOR
 */
struct scmi_base_discover_sub_vendor_p2a {
    int32_t status;
    char sub_vendor_identifier[16];
};

/*
 * BASE_DISCOVER_IMPLEMENTATION_VERSION
 * No special structure right now, see protocol_version.
 */

/*
 * BASE_DISCOVER_LIST_PROTOCOLS
 */
struct scmi_base_discover_list_protocols_a2p {
    uint32_t skip;
};

struct scmi_base_discover_list_protocols_p2a {
    int32_t status;
    uint32_t num_protocols;
    uint32_t protocols[];
};

/*
 * BASE_DISCOVER_AGENT
 */
struct scmi_base_discover_agent_a2p {
    uint32_t agent_id;
};

struct scmi_base_discover_agent_p2a {
    int32_t status;
    char name[16];
};

/*
 * BASE_SET_DEVICE_PERMISSIONS
 */
struct __attribute((packed)) scmi_base_set_device_permissions_a2p {
    uint32_t agent_id;
    uint32_t device_id;
    uint32_t flags;
};

struct __attribute((packed)) scmi_base_set_device_permissions_p2a {
    int32_t status;
};

/*
 * BASE_SET_PROTOCOL_PERMISSIONS
 */
struct __attribute((packed)) scmi_base_set_protocol_permissions_a2p {
    uint32_t agent_id;
    uint32_t device_id;
    uint32_t command_id;
    uint32_t flags;
};

struct __attribute((packed)) scmi_base_set_protocol_permissions_p2a {
    int32_t status;
};

/*
 * BASE_RESET_AGENT_CONFIG
 */
struct __attribute((packed)) scmi_base_reset_agent_config_a2p {
    uint32_t agent_id;
    uint32_t flags;
};

struct __attribute((packed)) scmi_base_reset_agent_config_p2a {
    int32_t status;
};

#endif /* _SCMI_BASE_H */

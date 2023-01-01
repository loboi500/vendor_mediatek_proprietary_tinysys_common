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

#include <stdbool.h>

#include "internal/scmi.h"
#include "internal/scmi_base.h"
#include "scmi_handler.h"

static int scmi_base_protocol_version_handler(
    unsigned int id, const uint32_t *payload);
static int scmi_base_protocol_attributes_handler(
    unsigned int id, const uint32_t *payload);
static int scmi_base_protocol_message_attributes_handler(
    unsigned int id, const uint32_t *payload);
static int scmi_base_discover_vendor_handler(
    unsigned int id, const uint32_t *payload);
static int scmi_base_discover_sub_vendor_handler(
    unsigned int id, const uint32_t *payload);
static int scmi_base_discover_implementation_version_handler(
    unsigned int id, const uint32_t *payload);
static int scmi_base_discover_list_protocols_handler(
    unsigned int id, const uint32_t *payload);
static int scmi_base_discover_agent_handler(
    unsigned int id, const uint32_t *payload);

static int (*const base_handler_table[])(unsigned int, const uint32_t *) = {
    [SCMI_PROTOCOL_VERSION] = scmi_base_protocol_version_handler,
    [SCMI_PROTOCOL_ATTRIBUTES] = scmi_base_protocol_attributes_handler,
    [SCMI_PROTOCOL_MESSAGE_ATTRIBUTES] =
        scmi_base_protocol_message_attributes_handler,
    [SCMI_BASE_DISCOVER_VENDOR] = scmi_base_discover_vendor_handler,
    [SCMI_BASE_DISCOVER_SUB_VENDOR] = scmi_base_discover_sub_vendor_handler,
    [SCMI_BASE_DISCOVER_IMPLEMENTATION_VERSION] =
        scmi_base_discover_implementation_version_handler,
    [SCMI_BASE_DISCOVER_LIST_PROTOCOLS] =
        scmi_base_discover_list_protocols_handler,
    [SCMI_BASE_DISCOVER_AGENT] = scmi_base_discover_agent_handler,
};

static const unsigned int base_payload_size_table[] = {
    [SCMI_PROTOCOL_VERSION] = 0,
    [SCMI_PROTOCOL_ATTRIBUTES] = 0,
    [SCMI_PROTOCOL_MESSAGE_ATTRIBUTES] =
        sizeof(struct scmi_protocol_message_attributes_a2p),
    [SCMI_BASE_DISCOVER_VENDOR] = 0,
    [SCMI_BASE_DISCOVER_SUB_VENDOR] = 0,
    [SCMI_BASE_DISCOVER_IMPLEMENTATION_VERSION] = 0,
    [SCMI_BASE_DISCOVER_LIST_PROTOCOLS] =
        sizeof(struct scmi_base_discover_list_protocols_a2p),
    [SCMI_BASE_DISCOVER_AGENT] =
        sizeof(struct scmi_base_discover_agent_a2p),
};

static const char * const default_agent_names[] = {
    [SCMI_AGENT_TYPE_PSCI] = "PSCI",
    [SCMI_AGENT_TYPE_MANAGEMENT] = "MANAGEMENT",
    [SCMI_AGENT_TYPE_OSPM] = "OSPM",
    [SCMI_AGENT_TYPE_OTHER] = "OTHER",
};

// Msg#0x0
static int scmi_base_protocol_version_handler(unsigned int id,
                                              const uint32_t *payload)
{
    struct scmi_protocol_version_p2a return_values = {
        .status = SCMI_SUCCESS,
        .version = SCMI_PROTOCOL_VERSION_BASE,
    };

    scmi_respond(id, &return_values, sizeof(return_values));

    return SCMI_SUCCESS;
}

// Msg#0x1
static int scmi_base_protocol_attributes_handler(unsigned int id,
                                                 const uint32_t *payload)
{
    struct scmi_protocol_attributes_p2a return_values = {
        .status = SCMI_SUCCESS,
    };

    return_values.attributes = SCMI_BASE_PROTOCOL_ATTRIBUTES(
        SCMI_PROTOCOL_COUNT, SCMI_AGENT_COUNT);

    scmi_respond(id, &return_values, sizeof(return_values));

    return SCMI_SUCCESS;
}

// Msg#0x2
static int scmi_base_protocol_message_attributes_handler(unsigned int id,
                                                       const uint32_t *payload)
{
    struct scmi_protocol_message_attributes_a2p *parameters;
    unsigned int message_id;
    struct scmi_protocol_message_attributes_p2a return_values = {
        .status = SCMI_NOT_FOUND,
    };

    parameters = (struct scmi_protocol_message_attributes_a2p *)payload;
    message_id = parameters->message_id;

    if ((message_id < SCMI_ARRAY_SIZE(base_handler_table)) &&
        (base_handler_table[message_id] != NULL))
        return_values.status = SCMI_SUCCESS;

    /* For this protocol, all commands have an attributes value of 0, which
     * has already been set by the initialization of "return_values".
     */
    scmi_respond(id, &return_values,
            (return_values.status == SCMI_SUCCESS) ?
            sizeof(return_values) : sizeof(return_values.status));

    return SCMI_SUCCESS;
}


// Msg#0x3
static int scmi_base_discover_vendor_handler(unsigned int id,
                                             const uint32_t *payload)
{
    struct scmi_base_discover_vendor_p2a return_values = {
        .status = SCMI_SUCCESS,
    };

    strncpy(return_values.vendor_identifier, SCMI_IDENTIFIER,
            sizeof(return_values.vendor_identifier) - 1);

    scmi_respond(id, &return_values, sizeof(return_values));

    return SCMI_SUCCESS;
}

// Msg#0x4
static int scmi_base_discover_sub_vendor_handler(unsigned int id,
                                                 const uint32_t *payload)
{
    struct scmi_base_discover_sub_vendor_p2a return_values = {
        .status = SCMI_SUCCESS,
    };

    strncpy(return_values.sub_vendor_identifier, SCMI_SUB_IDENTIFIER,
            sizeof(return_values.sub_vendor_identifier) - 1);

    scmi_respond(id, &return_values, sizeof(return_values));

    return SCMI_SUCCESS;
}

// Msg#0x5
static int scmi_base_discover_implementation_version_handler(
    unsigned int id, const uint32_t *payload)
{
    struct scmi_protocol_version_p2a return_values = {
        .status = SCMI_SUCCESS,
        .version = SCMI_BUILD_VERSION
    };

    SCMI_TRACE("respond '0x%x'\n", return_values.version);
    scmi_respond(id, &return_values, sizeof(return_values));

    return SCMI_SUCCESS;
}

// Msg#0x6
#ifdef SCMI_BYTE_WRITE_DEFECT
bool _skip_discover=0;
#endif
static int scmi_base_discover_list_protocols_handler(unsigned int id,
                                                     const uint32_t *payload)
{
    int status;
    const struct scmi_base_discover_list_protocols_a2p *parameters;
    struct scmi_base_discover_list_protocols_p2a return_values = {
        .status = SCMI_GENERIC_ERROR,
        .num_protocols = 0,
    };
    size_t payload_size;
    size_t avail_protocol_count = 0;
    unsigned int skip, index;
    uint8_t protocol_id;

    payload_size = sizeof(struct scmi_base_discover_list_protocols_p2a);
    parameters = (const struct scmi_base_discover_list_protocols_a2p *)payload;
    skip = parameters->skip;

#ifdef SCMI_BYTE_WRITE_DEFECT
    if (_skip_discover)
        goto bypass;
    else
        _skip_discover = 1;
#endif

    for (index = skip;; index++) {
        protocol_id = scmi_get_protocol_id(index);
        SCMI_TRACE("[%d] get id: 0x%x \n", index, protocol_id);

        if (protocol_id ==0)
            break;

        status = scmi_write_payload(id, payload_size, &protocol_id,
                               sizeof(protocol_id));
        SCMI_TRACE("write_payload: id = 0x%x (%d) \n", protocol_id, status);

        if (status != SCMI_SUCCESS)
            goto error;
        payload_size += sizeof(protocol_id);
        avail_protocol_count++;
    }

    SCMI_TRACE("skip: %d, avail_protocol_count: %d, payload_size: 0x%x\n",
        skip, avail_protocol_count, payload_size);

    if (skip > SCMI_PROTOCOL_COUNT) {
        return_values.status = SCMI_INVALID_PARAMETERS;
        goto error;
    }

bypass:
    return_values.status = SCMI_SUCCESS;
    return_values.num_protocols = avail_protocol_count;

    status = scmi_write_payload(id, 0, &return_values, sizeof(return_values));
    if (status != SCMI_SUCCESS)
        goto error;

    payload_size = SCMI_ALIGN_NEXT(payload_size, sizeof(uint32_t));

    scmi_respond(id, NULL, payload_size);

    return status;

error:
    SCMI_TRACE("error response: %d\n", return_values.status);
    scmi_respond(id, &return_values,
            (return_values.status == SCMI_SUCCESS) ?
            sizeof(return_values) : sizeof(return_values.status));

    return status;
}

// Msg#0x7
static int scmi_base_discover_agent_handler(unsigned int id,
                                            const uint32_t *payload)
{
    const struct scmi_base_discover_agent_a2p *parameters;
    struct scmi_base_discover_agent_p2a return_values = {
        .status = SCMI_NOT_FOUND,
    };

    parameters = (const struct scmi_base_discover_agent_a2p *)payload;

    if (parameters->agent_id > SCMI_AGENT_COUNT)
        goto exit;

    return_values.status = SCMI_SUCCESS;

    if (parameters->agent_id == SCMI_PLATFORM_ID) {
        static const char name[] = "platform";

        memcpy(return_values.name, name, sizeof(name));
        goto exit;
    }

    // TODO: support more than 'other' agnet
    strncpy(return_values.name,
            default_agent_names[SCMI_AGENT_TYPE_OTHER],
            sizeof(return_values.name) - 1);

exit:
    scmi_respond(id, &return_values,
            (return_values.status == SCMI_SUCCESS) ?
            sizeof(return_values) : sizeof(return_values.status));

    return SCMI_SUCCESS;
}

int scmi_base_message_handler(unsigned int id,
    const uint32_t *payload, size_t payload_size, unsigned int message_id)
{
    int32_t return_value;

    SCMI_TRACE("Get msg_%d with %d bytes payload\n", message_id, payload_size);

    if (message_id >= SCMI_ARRAY_SIZE(base_handler_table)) {
        return_value = SCMI_NOT_FOUND;
        goto error;
    }

    if (payload_size != base_payload_size_table[message_id]) {
        return_value = SCMI_PROTOCOL_ERROR;
        goto error;
    }

    return base_handler_table[message_id](id, payload);

error:
    SCMI_TRACE("error response: %d\n", return_value);
    scmi_respond(id, &return_value, sizeof(return_value));

    return SCMI_SUCCESS;
}

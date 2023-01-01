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

#include <stdio.h>
#include <string.h>

#include "FreeRTOS.h"

#include "driver_api.h"
#include "mt_printf.h"
#include "mbox.h"

#ifdef MBOX_DBG
#define MBOX_TRACE(f, ...)    PRINTF_E("%s(#%d): " f, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#else
#define MBOX_TRACE(f, ...)
#endif

/* The mbox_api should be defined in each tinysys portable layer */
extern struct mbox_driver_api mbox_api;

static int mbox_api_bind(struct mbox_driver_api **api)
{
    if (mbox_api.data_handler != NULL &&
        mbox_api.register_irq_handler != NULL) {
        *api = &mbox_api;
        return MBOX_DONE;
    } else {
        PRINTF_E("mbox_api_bind fail\n");
        return MBOX_CONFIG_ERR;
    }
}

int mbox_init(unsigned int id, bool is64d, unsigned int addr)
{
    int ret;
    unsigned int base_addr;
    struct mbox_info *mbox;

    if (id >= MBOX_COUNT_MAX)
        return MBOX_PARA_ERR;

    mbox = &(mbox_ctx[id]);

    mbox->id = id;
    mbox->is64d = is64d;
    mbox->addr = addr;

    if (mbox_api_bind(&mbox->api)) {
        return MBOX_CONFIG_ERR;
    }

    /* setup mbox memory size: 0 for 32dwords, 1 for 64dwords */
    if (is64d) {
        DRV_SetReg32(MBOX8_7B_BASE, 1 << id);
        base_addr = ((addr >> 8) & 0xFFF) << 1;
    } else {
        DRV_ClrReg32(MBOX8_7B_BASE, 1 << id);
        base_addr = ((addr >> 7) & 0x1FFF) << 0;
    }

    /*setup mbox memory address */
    DRV_WriteReg32(MBOX_BASE(id), base_addr);

    /*setup mbox isr */
    ret = mbox->api->register_irq_handler(mbox_isr, mbox);

    MBOX_TRACE("mbox_%d init %s with %ddword @0x%x (BASE: 0x%x, 8_7B: 0x%x)\n",
        mbox->id, ret ? "fail" : "done", mbox->is64d ? 64 : 32, mbox->addr,
        DRV_Reg32(MBOX_BASE(id)), DRV_Reg32(MBOX8_7B_BASE));

    return ret;
}

int mbox_write(unsigned int id, unsigned int slot, void *data, unsigned int len)
{
    unsigned int slot_ofs;
    struct mbox_info *mbox;

    if (id >= MBOX_COUNT_MAX || slot >= MBOX_SLOT_COUNT_MAX)
        return MBOX_PARA_ERR;

    mbox = &(mbox_ctx[id]);

    /* mbox is64dwords/is32dwords: 8/4 bytes per slot */
    if (MBOX_IS64D(id, DRV_Reg32(MBOX8_7B_BASE)))
        slot_ofs = slot * 8;
    else
        slot_ofs = slot * 4;

    if (data != NULL && len > 0)
        memcpy((void *) (mbox->addr + slot_ofs), data, len);

    MBOX_TRACE("write mbox_%d.slot_%d %d bytes data to 0x%x\n",
        id, slot, len, mbox->addr + slot_ofs);

    return MBOX_DONE;
}

int mbox_read(unsigned int id, unsigned int slot, void *data, unsigned int len)
{
    unsigned int slot_ofs;
    struct mbox_info *mbox;

    if (id >= MBOX_COUNT_MAX || slot >= MBOX_SLOT_COUNT_MAX)
        return MBOX_PARA_ERR;

    mbox = &(mbox_ctx[id]);

    /* mbox is64dwords/is32dwords: 8/4 bytes per slot */
    if (MBOX_IS64D(id, DRV_Reg32(MBOX8_7B_BASE)))
        slot_ofs = slot * 8;
    else
        slot_ofs = slot * 4;

    if (data != NULL && len > 0)
        memcpy(data, (void *) (mbox->addr + slot_ofs), len);

    MBOX_TRACE("read mbox_%d.slot_%d %d bytes data to 0x%x\n",
        id, slot, len, mbox->addr + slot_ofs);

    return MBOX_DONE;
}

int mbox_raise_irq(unsigned int id, unsigned int slot)
{
    if (id >= MBOX_COUNT_MAX || slot >= MBOX_SLOT_COUNT_MAX)
        return MBOX_PARA_ERR;

    DRV_WriteReg32(MBOX_OUT_IRQ(id), 1 << slot);
    MBOX_TRACE("raise mbox%d slot%d (result: 0x%x)\n",
        id, slot, DRV_Reg32(MBOX_OUT_IRQ(id)));

    return MBOX_DONE;
}

int mbox_clr_irq(unsigned int id, unsigned int slot)
{
    struct mbox_info *mbox;

    if (id >= MBOX_COUNT_MAX || slot >= MBOX_SLOT_COUNT_MAX)
        return MBOX_PARA_ERR;

    mbox = &(mbox_ctx[id]);

    DRV_WriteReg32(MBOX_IN_IRQ(id), 1 << slot);

    MBOX_TRACE("clr mbox%d slot%d (result: 0x%x)\n",
        id, slot, DRV_Reg32(MBOX_IN_IRQ(id)));

    return MBOX_DONE;
}

int mbox_polling_done(unsigned int id, unsigned int slot, int retries)
{
    volatile unsigned int reg;

    do {
        reg = DRV_Reg32(MBOX_OUT_IRQ(id));
    } while ( (reg & (0x1 << slot)) && (--retries > 0));

    return (retries > 0) ? MBOX_DONE : MBOX_BUSY;
}

unsigned int mbox_isr(void *data)
{
    unsigned int in, slot;
    struct mbox_info *mbox = (struct mbox_info *) data;

    in = DRV_Reg32(MBOX_IN_IRQ(mbox->id));

    MBOX_TRACE("mbox_%d with slot 0x%x triggered. (data @0x%x)\n",
        mbox->id, in, mbox->addr);

    if (!mbox->api->data_handler) {
        PRINTF_E("mbox_%d have no data_handler\n", mbox->id);
        DRV_WriteReg32(MBOX_IN_IRQ(mbox->id), in);
        return 0;
    }

#define CHK_BIT(x, n) ( ((x) & (1<< (n)))!=0 )

#ifdef CFG_SCMI_SUPPORT
    /* Service all slot events oneshot */
    DRV_WriteReg32(MBOX_IN_IRQ(mbox->id), in);
    mbox->api->data_handler(mbox->id, mbox->addr);
#else
    /* Loop over all the slots */
    for (slot = 0; slot <= MBOX_SLOT_COUNT_MAX; slot++) {
        if (CHK_BIT(in, slot)) {
            MBOX_TRACE("to signal the message of mbox_%d[%x] \n", mbox->id, slot);

            /* signal the message */
            mbox->api->data_handler(mbox->id, mbox->addr);

            /* Acknowledge the interrupt */
            DRV_WriteReg32(MBOX_IN_IRQ(mbox->id), 1 << slot);
        }
    }
#endif

    return 0;
}

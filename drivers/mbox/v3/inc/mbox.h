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
/* MediaTek Inc. (C) 2019. All rights reserved.
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

#ifndef _MBOX_H_
#define _MBOX_H_

#define MBOX_SEC_4B         ".mbox4"
#define MBOX_SEC_8B         ".mbox8"
#define MBOX_4BYTE          (0x80)
#define MBOX_8BYTE          (0x100)
#define MBOX_BASE_4B(x)     unsigned char x[MBOX_4BYTE] __attribute__ ((section (MBOX_SEC_4B))) = {0}
#define MBOX_BASE_8B(x)     unsigned char x[MBOX_8BYTE] __attribute__ ((section (MBOX_SEC_8B))) = {0}

#define MBOX_OFS(x)         ((unsigned int) (x) << 2)
#define MBOX_IN_IRQ(x)      (MBOX0_IN_IRQ + MBOX_OFS(x))
#define MBOX_OUT_IRQ(x)     (MBOX0_OUT_IRQ + MBOX_OFS(x))
#define MBOX_BASE(x)        (MBOX0_BASE + MBOX_OFS(x))
#define MBOX_IS64D(x, r)    (((r) >> (x)) & 0x1)


#define MBOX_COUNT_MAX 5
#define MBOX_SLOT_COUNT_MAX 31
#ifndef MBOX_COUNT
#define MBOX_COUNT MBOX_COUNT_MAX
#endif

 /* Driver API */
struct mbox_driver_api {
    int (*register_irq_handler)(unsigned int (*mbox_isr) (void *data), void *prdata);
    int (*data_handler)(unsigned int id, unsigned int addr);
};

struct mbox_info {
    unsigned int id:15,
        is64d:1,
        resv:16;
    unsigned int addr;
    struct mbox_driver_api *api;
};

enum MBOX_RET {
    MBOX_PARA_ERR    = -2,
    MBOX_CONFIG_ERR = -1,
    MBOX_DONE       = 0,
    MBOX_BUSY    = 1,
};

struct mbox_info mbox_ctx[MBOX_COUNT];

int mbox_init(unsigned int id, bool is64d, unsigned int addr);
int mbox_write(unsigned int id, unsigned int slot, void *data, unsigned int len);
int mbox_read(unsigned int id, unsigned int slot, void *data, unsigned int len);
int mbox_raise_irq(unsigned int id, unsigned int slot);
int mbox_clr_irq(unsigned int id, unsigned int slot);
int mbox_polling_done(unsigned int id, unsigned int slot, int retries);

unsigned int mbox_isr(void *data);

#endif

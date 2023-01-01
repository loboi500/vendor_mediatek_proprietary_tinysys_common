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

#include "FreeRTOS.h"
#include "task.h"
#include <string.h>
#include <driver_api.h>
#include <interrupt.h>
#include "irq.h"
#include "bus_tracker.h"

void dump_bus_tracker_status(void)
{
	uint32_t offset;
	uint64_t *bus_dbg_read_l ,*bus_dbg_write_l;
	int i;
	TRACKER_LOG("dbg_con %08x\n", drv_reg32(BUS_DBG_CON));
	for (i = 3; i >= 0; --i) {
		offset = i << 3;
		bus_dbg_read_l = ((uint64_t*)BUS_DBG_AR_TRACK0_L) + offset;
		bus_dbg_write_l = ((uint64_t*)BUS_DBG_AW_TRACK0_L) + offset;
		if (!drv_reg32(bus_dbg_read_l + 7) && !drv_reg32(bus_dbg_write_l + 7))
			continue;

		TRACKER_LOG("R[%u-%u] %08x %08x %08x %08x %08x %08x %08x %08x\n",
			offset, offset + 7,
			drv_reg32(bus_dbg_read_l),
			drv_reg32(bus_dbg_read_l + 1),
			drv_reg32(bus_dbg_read_l + 2),
			drv_reg32(bus_dbg_read_l + 3),
			drv_reg32(bus_dbg_read_l + 4),
			drv_reg32(bus_dbg_read_l + 5),
			drv_reg32(bus_dbg_read_l + 6),
			drv_reg32(bus_dbg_read_l + 7)
		);
		TRACKER_LOG("W[%u-%u] %08x %08x %08x %08x %08x %08x %08x %08x\n",
			offset, offset + 7,
			drv_reg32(bus_dbg_write_l),
			drv_reg32(bus_dbg_write_l + 1),
			drv_reg32(bus_dbg_write_l + 2),
			drv_reg32(bus_dbg_write_l + 3),
			drv_reg32(bus_dbg_write_l + 4),
			drv_reg32(bus_dbg_write_l + 5),
			drv_reg32(bus_dbg_write_l + 6),
			drv_reg32(bus_dbg_write_l + 7)
		);
	}
}

unsigned int bus_tracker_handler(void *ptr)
{
	//uint32_t ctrl = drv_reg32(BUS_DBG_CON);
	bus_tracker_handler_plat_start();
	dump_bus_tracker_status();
#if 0 //keep IRQ status
	if ((ctrl & BUS_DBG_CON_IRQ_STA) != 0) {
		ctrl |= BUS_DBG_CON_IRQ_CLR;
	}
	if ((ctrl & BUS_DBG_CON_TIMEOUT_STA) != 0) {
		ctrl |= BUS_DBG_CON_TIMEOUT_CLR;
	}
	drv_write_reg32(BUS_DBG_CON, ctrl);
#endif
	bus_tracker_handler_plat_end();
	return 0;
}

/* bus tracker should be initialed before bus transaction start */
void bus_tracker_init(void)
{
	drv_write_reg32(BUS_DBG_TIMER_CON0, BUS_TRACKER_STAGE1_TIMEOUT);
	drv_write_reg32(BUS_DBG_TIMER_CON1, BUS_TRACKER_STAGE2_TIMEOUT);
	drv_write_reg32(BUS_DBG_WP, BUS_TRACKER_WATCHPOINT);
	drv_write_reg32(BUS_DBG_WP_MASK, BUS_TRACKER_WATCHPOINT_MASK);
	drv_write_reg32(BUS_DBG_CON, BUS_DBG_CON_SW_RST | BUS_DBG_CON_IRQ_CLR);
	/* irq or control setting misc */
	bus_tracker_plat_init();
}

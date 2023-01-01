/* Copyright Statement:
 *
 * This software/firmware and related documentation ("MediaTek Software") are
 * protected under relevant copyright laws. The information contained herein
 * is confidential and proprietary to MediaTek Inc. and/or its licensors.
 * Without the prior written permission of MediaTek inc. and/or its licensors,
 * any reproduction, modification, use or disclosure of MediaTek Software,
 * and information contained herein, in whole or in part, shall be strictly prohibited.
 */
/* MediaTek Inc. (C) 2020. All rights reserved.
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
 * THAT IT IS RECEIVER\'S SOLE RESPONSIBILITY TO OBTAIN FROM ANY THIRD PARTY ALL PROPER LICENSES
 * CONTAINED IN MEDIATEK SOFTWARE. MEDIATEK SHALL ALSO NOT BE RESPONSIBLE FOR ANY MEDIATEK
 * SOFTWARE RELEASES MADE TO RECEIVER\'S SPECIFICATION OR TO CONFORM TO A PARTICULAR
 * STANDARD OR OPEN FORUM. RECEIVER\'S SOLE AND EXCLUSIVE REMEDY AND MEDIATEK\'S ENTIRE AND
 * CUMULATIVE LIABILITY WITH RESPECT TO THE MEDIATEK SOFTWARE RELEASED HEREUNDER WILL BE,
 * AT MEDIATEK\'S OPTION, TO REVISE OR REPLACE THE MEDIATEK SOFTWARE AT ISSUE,
 * OR REFUND ANY SOFTWARE LICENSE FEES OR SERVICE CHARGE PAID BY RECEIVER TO
 * MEDIATEK FOR SUCH MEDIATEK SOFTWARE AT ISSUE.
 *
 * The following software/firmware and/or related documentation ("MediaTek Software")
 * have been modified by MediaTek Inc. All revisions are subject to any receiver\'s
 * applicable license agreements with MediaTek Inc.
 */


#include <FreeRTOS.h>
#include "task.h"
#include <stdio.h>
#include <stdlib.h>
#include "semphr.h"
#include <mt_printf.h>

#if defined(CFG_TIMER_SUPPORT)
#include <ostimer.h>
#elif defined(CFG_XGPT_SUPPORT)
#include <xgpt.h>
#elif defined(ISP_CCU)
#include <ccu_utility.h>
#else
#error "no timer defined"
#endif

#include "ccf_common.h"
#include "hwccf_provider.h"

#define HWCCF_DEBUG  0

#if HWCCF_DEBUG
#define HWCCF_LOG(fmt, arg...) PRINTF_E(fmt, ##arg)
#else
#define HWCCF_LOG(...)
#endif

#define PER_DONE_BIT_CHK_T_US 10
#define MAX_DONE_BIT_CHK_PD_US 10000
#define MAX_DONE_BIT_CHK_PLL_US 5000
#define MAX_DONE_BIT_CHK_CG_US 3000

/*
 * function for platform implementation
 */
uint32_t hwccf_remap(uint32_t addr);
void hwccf_write(uint32_t addr, uint32_t value);
uint32_t hwccf_read(uint32_t addr);

/* hwccf base */
uint32_t hwccf_base;

/* clock voter support */

static void hwccf_debug_dump(void)
{
	int i;
	uint32_t addr;

	for (i=0; i<8; i++) {
		addr = hwccf_base + 0x1400 + 4*i;
		PRINTF_E("[0x%x]=0x%x\n", addr, hwccf_read(addr));
	}

	addr = hwccf_base + 0x1500;
	PRINTF_E("[0x%x]=0x%x\n", addr, hwccf_read(addr));
}

static int hwccf_voter_set(struct mtk_clk *mck)
{
	struct hwccf_clk_voter *voter = voter_from_hwccf_clk(mck);
	uint32_t addr;
	int is_pll = 0;
	int i = 0;
	int max_count;

	HWCCF_LOG("hwccf_voter_set\n");

	if (voter == NULL || voter->set_ofs == INV_OFS)
		return -EINVAL;

	addr = hwccf_base + voter->set_ofs;
	hwccf_write(addr, 1 << voter->shift);

	if (voter->sta_ofs == 0x1404) {
		is_pll = 1;
		max_count = (MAX_DONE_BIT_CHK_PLL_US/PER_DONE_BIT_CHK_T_US);

		/* HW_CCF_PLL_DONE = HW_CCF_PLL_STATUS + 0x8 */
		addr = hwccf_base + voter->sta_ofs + 0x8;
	} else {
		is_pll = 0;
		max_count = (MAX_DONE_BIT_CHK_CG_US/PER_DONE_BIT_CHK_T_US);

		/* HW_CCF_CG_DONE = HW_CCF_CG_STATUS + 0x400 */
		addr = hwccf_base + voter->sta_ofs + 0x400;
	}

	/* waiting for done bit == 1 */
	while (1) {
		udelay(PER_DONE_BIT_CHK_T_US);

		if ((hwccf_read(addr) & BIT(voter->shift)) != 0)
			break;

		i++;
		if (i > max_count) {
			PRINTF_E("ERR: %s ON done bit == 0, addr=0x%x, bit=%d\n",
				is_pll?"PLL":"CG", hwccf_base + voter->set_ofs, voter->shift);
			hwccf_debug_dump();
			configASSERT(0);
		}
	}

	return 0;
}

static int hwccf_voter_clr(struct mtk_clk *mck)
{
	struct hwccf_clk_voter *voter = voter_from_hwccf_clk(mck);
	uint32_t addr;
	int is_pll = 0;
	int i = 0;
	int max_count;

	HWCCF_LOG("hwccf_voter_clr\n");

	if (voter == NULL || voter->clr_ofs == INV_OFS)
		return -EINVAL;

	addr = hwccf_base + voter->clr_ofs;
	hwccf_write(addr, 1 << voter->shift);

	if (voter->sta_ofs == 0x1404) {
		is_pll = 1;
		max_count = (MAX_DONE_BIT_CHK_PLL_US/PER_DONE_BIT_CHK_T_US);

		/* HW_CCF_PLL_DONE = HW_CCF_PLL_STATUS + 0x8 */
		addr = hwccf_base + voter->sta_ofs + 0x8;
	} else {
		is_pll = 0;
		max_count = (MAX_DONE_BIT_CHK_CG_US/PER_DONE_BIT_CHK_T_US);

		/* HW_CCF_CG_DONE = HW_CCF_CG_STATUS + 0x400 */
		addr = hwccf_base + voter->sta_ofs + 0x400;
	}

	/* waiting for done bit == 1 */
	while (1) {
		udelay(PER_DONE_BIT_CHK_T_US);

		if ((hwccf_read(addr) & BIT(voter->shift)) != 0)
			break;

		i++;
		if (i > max_count) {
			PRINTF_E("ERR: %s OFF done bit == 0, addr=0x%x, bit=%d\n",
				is_pll?"PLL":"CG", hwccf_base + voter->clr_ofs, voter->shift);
			hwccf_debug_dump();
			configASSERT(0);
		}
	}

	return 0;
}

static int hwccf_voter_is_set(struct mtk_clk *mck)
{
	struct hwccf_clk_voter *voter = voter_from_hwccf_clk(mck);
	uint32_t addr;
	uint32_t r;

	HWCCF_LOG("hwccf_voter_is_set\n");

	if (voter == NULL || voter->set_ofs == INV_OFS)
		return false;

	addr = hwccf_base + voter->set_ofs;
	r = hwccf_read(addr);

	return !!(r & BIT(voter->shift));
}

static int hwccf_pd_voter_set(struct mtk_pd *pd)
{
	struct hwccf_pd_voter *voter = voter_from_hwccf_pd(pd);
	uint32_t addr;
	int i = 0;
	int max_count = (MAX_DONE_BIT_CHK_PD_US/PER_DONE_BIT_CHK_T_US);

	HWCCF_LOG("hwccf_pd_voter_set\n");

	if (voter == NULL || voter->set_ofs == INV_OFS)
		return -EINVAL;

	addr = hwccf_base + voter->set_ofs;
	hwccf_write(addr, 1 << voter->shift);

	/* HW_CCF_MTCMOS_DONE = HW_CCF_MTCMOS_STATUS + 0x8 */
	addr = hwccf_base + voter->sta_ofs + 0x8;

	/* waiting for done bit == 1 */
	while (1) {
		udelay(PER_DONE_BIT_CHK_T_US);

		if ((hwccf_read(addr) & BIT(voter->shift)) != 0)
			break;

		i++;
		if (i > max_count) {
			PRINTF_E("ERR: MTCMOS ON done bit == 0, addr=0x%x, bit=%d\n",
				hwccf_base + voter->set_ofs, voter->shift);
			hwccf_debug_dump();
			configASSERT(0);
		}
	}

	return 0;
}

static int hwccf_pd_voter_clr(struct mtk_pd *pd)
{
	struct hwccf_pd_voter *voter = voter_from_hwccf_pd(pd);
	uint32_t addr;
	int i = 0;
	int max_count = (MAX_DONE_BIT_CHK_PD_US/PER_DONE_BIT_CHK_T_US);

	HWCCF_LOG("hwccf_pd_voter_clr\n");

	if (voter == NULL || voter->clr_ofs == INV_OFS)
		return -EINVAL;

	addr = hwccf_base + voter->clr_ofs;
	hwccf_write(addr, 1 << voter->shift);

	/* HW_CCF_MTCMOS_DONE = HW_CCF_MTCMOS_STATUS + 0x8 */
	addr = hwccf_base + voter->sta_ofs + 0x8;

	/* waiting for done bit == 1 */
	while (1) {
		udelay(PER_DONE_BIT_CHK_T_US);

		if ((hwccf_read(addr) & BIT(voter->shift)) != 0)
			break;

		i++;
		if (i > max_count) {
			PRINTF_E("ERR: MTCMOS OFF done bit == 0, , addr=0x%x, bit=%d\n",
				hwccf_base + voter->clr_ofs, voter->shift);
			hwccf_debug_dump();
			configASSERT(0);
		}
	}

	return 0;
}

static int hwccf_pd_voter_is_set(struct mtk_pd *pd)
{
	struct hwccf_pd_voter *voter = voter_from_hwccf_pd(pd);
	uint32_t addr;
	uint32_t r;

	HWCCF_LOG("hwccf_pd_voter_is_set\n");

	if (voter == NULL || voter->sta_ofs == INV_OFS)
		return false;

	addr = hwccf_base + voter->sta_ofs;
	r = hwccf_read(addr);

	return !!(r & BIT(voter->shift));
}

const struct mtk_clk_ops hwccf_voter_clk_ops = {
	.enable		= hwccf_voter_set,
	.disable	= hwccf_voter_clr,
	.is_enabled	= hwccf_voter_is_set,
};

const struct mtk_clk_ops hwccf_voter_pll_ops = {
	.prepare	= hwccf_voter_set,
	.unprepare	= hwccf_voter_clr,
	.is_prepared	= hwccf_voter_is_set,
};

struct mtk_pd_ops hwccf_voter_pd_ops = {
	.power_on	= hwccf_pd_voter_set,
	.power_off	= hwccf_pd_voter_clr,
	.is_on		= hwccf_pd_voter_is_set,
};

void hwccf_init(uint32_t base)
{
	hwccf_base = hwccf_remap(base);
}

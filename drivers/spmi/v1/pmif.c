/* Copyright Statement:
 *
 * This software/firmware and related documentation ("MediaTek Software") are
 * protected under relevant copyright laws. The information contained herein is
 * confidential and proprietary to MediaTek Inc. and/or its licensors. Without
 * the prior written permission of MediaTek inc. and/or its licensors, any
 * reproduction, modification, use or disclosure of MediaTek Software, and
 * information contained herein, in whole or in part, shall be strictly
 * prohibited.
 *
 * MediaTek Inc. (C) 2021. All rights reserved.
 *
 * BY OPENING THIS FILE, RECEIVER HEREBY UNEQUIVOCALLY ACKNOWLEDGES AND AGREES
 * THAT THE SOFTWARE/FIRMWARE AND ITS DOCUMENTATIONS ("MEDIATEK SOFTWARE")
 * RECEIVED FROM MEDIATEK AND/OR ITS REPRESENTATIVES ARE PROVIDED TO RECEIVER
 * ON AN "AS-IS" BASIS ONLY. MEDIATEK EXPRESSLY DISCLAIMS ANY AND ALL
 * WARRANTIES, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE OR
 * NONINFRINGEMENT. NEITHER DOES MEDIATEK PROVIDE ANY WARRANTY WHATSOEVER WITH
 * RESPECT TO THE SOFTWARE OF ANY THIRD PARTY WHICH MAY BE USED BY,
 * INCORPORATED IN, OR SUPPLIED WITH THE MEDIATEK SOFTWARE, AND RECEIVER AGREES
 * TO LOOK ONLY TO SUCH THIRD PARTY FOR ANY WARRANTY CLAIM RELATING THERETO.
 * RECEIVER EXPRESSLY ACKNOWLEDGES THAT IT IS RECEIVER'S SOLE RESPONSIBILITY TO
 * OBTAIN FROM ANY THIRD PARTY ALL PROPER LICENSES CONTAINED IN MEDIATEK
 * SOFTWARE. MEDIATEK SHALL ALSO NOT BE RESPONSIBLE FOR ANY MEDIATEK SOFTWARE
 * RELEASES MADE TO RECEIVER'S SPECIFICATION OR TO CONFORM TO A PARTICULAR
 * STANDARD OR OPEN FORUM. RECEIVER'S SOLE AND EXCLUSIVE REMEDY AND MEDIATEK'S
 * ENTIRE AND CUMULATIVE LIABILITY WITH RESPECT TO THE MEDIATEK SOFTWARE
 * RELEASED HEREUNDER WILL BE, AT MEDIATEK'S OPTION, TO REVISE OR REPLACE THE
 * MEDIATEK SOFTWARE AT ISSUE, OR REFUND ANY SOFTWARE LICENSE FEES OR SERVICE
 * CHARGE PAID BY RECEIVER TO MEDIATEK FOR SUCH MEDIATEK SOFTWARE AT ISSUE.
 *
 * The following software/firmware and/or related documentation ("MediaTek
 * Software") have been modified by MediaTek Inc. All revisions are subject to
 * any receiver's applicable license agreements with MediaTek Inc.
 */
/*
 * MTK PMIF Driver
 *
 * Copyright 2021 MediaTek Co.,Ltd.
 *
 * DESCRIPTION:
 *     This file provides API for other drivers to access PMIC registers
 *
 */

#include <stdio.h>
#include <FreeRTOS.h>
#include <task.h>
#include "driver_api.h"
#include "tinysys_reg.h"
#include "mt_printf.h"
#include <spmi.h>
#include <spmi_sw.h>
#include <pmif.h>
#include <pmif_sw.h>
#if PMIF_CS_DEBUG
#include <xgpt.h>
unsigned long long RdTime[7] = {0,0,0,0,0,0,0};
unsigned long long WrTime[6] = {0,0,0,0,0,0};
#endif

static int pmif_spmi_read_cmd(struct pmif *arb, unsigned char opc,
		unsigned char sid, unsigned short addr, unsigned char *buf,
		unsigned short len);
static int pmif_spmi_write_cmd(struct pmif *arb, unsigned char opc,
	unsigned char sid, unsigned short addr, const unsigned char *buf,
	unsigned short len);
#if PMIF_NO_PMIC
static int pmif_spmi_read_cmd(struct pmif *arb, unsigned char opc,
		unsigned char sid, unsigned short addr, unsigned char *buf,
		unsigned short len)
{
	PRINTF_I("[PMIF] do Nothing.\n");
	return 0;
}

static int pmif_spmi_write_cmd(struct pmif *arb, unsigned char opc,
	unsigned char sid, unsigned short addr, const unsigned char *buf,
	unsigned short len)
{
	PRINTF_I("[PMIF] do Nothing.\n");
	return 0;
}

/* init interface */
int pmif_spmi_init(int mstid)
{
	PRINTF_I("[PMIF] do Nothing.\n");
	return 0;
}
#else /* #ifdef PMIF_NO_PMIC */
static struct pmif pmif_spmi_arb[] = {
	{
		.mstid = SPMI_MASTER_0,
		.read_cmd = pmif_spmi_read_cmd,
		.write_cmd = pmif_spmi_write_cmd,
		.is_pmif_init_done = is_pmif_spmi_init_done,
	},	{
		.mstid = SPMI_MASTER_1,
		.read_cmd = pmif_spmi_read_cmd,
		.write_cmd = pmif_spmi_write_cmd,
		.is_pmif_init_done = is_pmif_spmi_init_done,
	},
};

/* pmif internal API declaration */
static inline int pmif_check_idle(int mstid) {
	unsigned int reg_rdata;

	do {
		reg_rdata = DRV_Reg32(PMIF_STA);
	} while(GET_SWINF_0_FSM(reg_rdata) != SWINF_FSM_IDLE);

    return 0;
}

static inline int pmif_check_vldclr(int mstid) {
	unsigned int reg_rdata;

	do {
		reg_rdata = DRV_Reg32(PMIF_STA);
	} while(GET_SWINF_0_FSM(reg_rdata) != SWINF_FSM_WFVLDCLR);

    return 0;
}
static int pmif_spmi_read_cmd(struct pmif *arb, unsigned char opc,
		unsigned char sid, unsigned short addr, unsigned char *buf,
		unsigned short len)
{
	int write = 0x0;
	unsigned int in_isr_and_cs = 0, ret = 0, data = 0;
	unsigned char bc = len - 1;

	if ((sid & ~(0xf)) != 0x0)
		return -EINVAL;

	if ((bc & ~(0x1)) != 0x0)
		return -EINVAL;

	/* Check the opcode */
	if (opc >= 0x60 && opc <= 0x7F)
		opc = PMIF_CMD_REG;
	else if (opc >= 0x20 && opc <= 0x2F)
		opc = PMIF_CMD_EXT_REG;
	else if (opc >= 0x38 && opc <= 0x3F)
		opc = PMIF_CMD_EXT_REG_LONG;
	else
		return -EINVAL;

	/* check C.S. */
	in_isr_and_cs = is_in_isr();
	if(!in_isr_and_cs) {
		taskENTER_CRITICAL();
	}
#if PMIF_CS_DEBUG
	RdTime[0] = read_xgpt_stamp_ns();
#endif
	/* Wait for Software Interface FSM state to be IDLE. */
	ret = pmif_check_idle(arb->mstid);
#if PMIF_CS_DEBUG
	RdTime[1] = read_xgpt_stamp_ns();
#endif
	if(ret)
		return ret;

	/* Send the command. */
	DRV_WriteReg32(PMIF_ACC,
		((unsigned)opc << 30) | (write << 29) | (sid << 24) | (bc << 16) | addr);
#if PMIF_CS_DEBUG
	RdTime[2] = read_xgpt_stamp_ns();
#endif

	/* Wait for Software Interface FSM state to be WFVLDCLR,
	 * read the data and clear the valid flag.
	 */
	if(write == 0)
	{
		ret = pmif_check_vldclr(arb->mstid);
		if(ret)
			return ret;
#if PMIF_CS_DEBUG
		RdTime[3] = read_xgpt_stamp_ns();
#endif
		data = DRV_Reg32(PMIF_RDATA_31_0);
		memcpy(buf, &data, (bc & 3) + 1);
#if PMIF_CS_DEBUG
		RdTime[4] = read_xgpt_stamp_ns();
#endif
		DRV_WriteReg32(PMIF_VLD_CLR, 0x1);
#if PMIF_CS_DEBUG
		RdTime[5] = read_xgpt_stamp_ns();
#endif
	}
#if PMIF_CS_DEBUG
	RdTime[6] = RdTime[5]-RdTime[0];
	if (RdTime[6] > 5000000) {
		PRINTF_E("[PMIF_READ]chkidle:%llu ACC:%llu chkvld:%llu memcpy:%llu vldclr:%llu\n",
			(RdTime[1]-RdTime[0]), (RdTime[2]-RdTime[1]), (RdTime[3]-RdTime[2]), (RdTime[4]-RdTime[3]), (RdTime[5]-RdTime[4]));
		PRINTF_E("[PMIF_READ]total_t:%llu PMIF_STA:0x%x Addr:0x%x \n", RdTime[6], DRV_Reg32(PMIF_STA), addr);
	}
#endif
	/* check C.S. */
	if(!in_isr_and_cs) {
		taskEXIT_CRITICAL();
	}

	return 0x0;
}

static int pmif_spmi_write_cmd(struct pmif *arb, unsigned char opc,
	unsigned char sid, unsigned short addr, const unsigned char *buf,
	unsigned short len)
{
	int write = 0x1;
	unsigned int in_isr_and_cs = 0, ret = 0, data = 0;
	unsigned char bc = len - 1;

	if ((sid & ~(0xf)) != 0x0)
		return -EINVAL;

	if ((bc & ~(0x1)) != 0x0)
		return -EINVAL;

	/* Check the opcode */
	if (opc >= 0x40 && opc <= 0x5F)
		opc = PMIF_CMD_REG;
	else if (opc <= 0x0F)
		opc = PMIF_CMD_EXT_REG;
	else if (opc >= 0x30 && opc <= 0x37)
		opc = PMIF_CMD_EXT_REG_LONG;
	else if (opc >= 0x80)
		opc = PMIF_CMD_REG_0;
	else
		return -EINVAL;

	/* check C.S. */
	in_isr_and_cs = is_in_isr();
	if(!in_isr_and_cs) {
		taskENTER_CRITICAL();
	}
#if PMIF_CS_DEBUG
	WrTime[0] = read_xgpt_stamp_ns();
#endif
	/* Wait for Software Interface FSM state to be IDLE. */
	ret = pmif_check_idle(arb->mstid);
#if PMIF_CS_DEBUG
	WrTime[1] = read_xgpt_stamp_ns();
#endif
	if(ret)
		return ret;

	/* Set the write data. */
	if (write == 1)
	{
		memcpy(&data, buf, (bc & 3) + 1);
#if PMIF_CS_DEBUG
		WrTime[2] = read_xgpt_stamp_ns();
#endif
		DRV_WriteReg32(PMIF_WDATA_31_0, data);
#if PMIF_CS_DEBUG
		WrTime[3] = read_xgpt_stamp_ns();
#endif
	}

	/* Send the command. */
	DRV_WriteReg32(PMIF_ACC,
		((unsigned)opc << 30) | (write << 29) | (sid << 24) | (bc << 16) | addr);
#if PMIF_CS_DEBUG
	WrTime[4] = read_xgpt_stamp_ns();
	WrTime[5] = WrTime[4]-WrTime[0];
	if (WrTime[5] > 5000000) {
		PRINTF_E("[PMIF_WRITE]chkidle:%llu memcpy:%llu WDATA:%llu ACC:%llu\n",
			(WrTime[1]-WrTime[0]),(WrTime[2]-WrTime[1]),(WrTime[3]-WrTime[2]),(WrTime[4]-WrTime[3]));
		PRINTF_E("[PMIF_WRITE]total_t:%llu PMIF_STA:0x%x addr: 0x%x data:0x%x\n",
			WrTime[5], DRV_Reg32(PMIF_STA), addr, *buf);
	}
#endif

	/* check C.S. */
	if(!in_isr_and_cs) {
		taskEXIT_CRITICAL();
	}

	return 0x0;
}

struct pmif *get_pmif_controller(int inf, int mstid)
{
	if (inf == PMIF_SPMI)
		return &pmif_spmi_arb[mstid];

	return 0;
}

int is_pmif_spmi_init_done(int mstid)
{
	int ret = 0, reg_data = 0;

#ifdef PMIF_CG
	pmif_cg_enable();
#endif

	reg_data = DRV_Reg32(PMIF_STA);
	ret = GET_PMIF_INIT_DONE(reg_data);
	if ((ret & 0x1) == 1)
		return 0;

	return -ENODEV;
}

int pmif_spmi_init(int mstid)
{
	struct pmif *arb = get_pmif_controller(PMIF_SPMI, mstid);
	int ret = 0;

	ret = spmi_init(arb);
	if(ret) {
		PRINTF_E("[PMIF] init fail\r\n");
		return -ENODEV;
	}

	return 0;
}

#endif /* endif PMIF_NO_PMIC */

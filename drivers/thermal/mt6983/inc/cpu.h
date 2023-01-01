/* Copyright Statement:
 *
 * This software/firmware and related documentation ("MediaTek Software") are
 * protected under relevant copyright laws. The information contained herein
 * is confidential and proprietary to MediaTek Inc. and/or its licensors.
 * Without the prior written permission of MediaTek inc. and/or its licensors,
 * any reproduction, modification, use or disclosure of MediaTek Software,
 * and information contained herein, in whole or in part, shall be strictly prohibited.
 */
/* MediaTek Inc. (C) 2017. All rights reserved.
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

#ifndef _CPU_H
#define _CPU_H

#include "thermal_utils.h"

/*=============================================================
 * Thermal Configuration
 *=============================================================*/

/*
 * module            LVTS Plan
 *=====================================================
 * MCU_BIG         LVTS1-0, LVTS1-1, LVTS1-2, LVTS1-3
 * MCU_BIG         LVTS2-0, LVTS2-1, LVTS2-2, LVTS2-3
 * MCU_LITTLE    LVTS3-0, LVTS3-1, LVTS3-2, LVTS3-3
 * ADCT         LVTS4-0, LVTS4-1, LVTS4-2, LVTS4-3
 * GPU            LVTS5-0, LVTS5-1
 * VPU            LVTS6-0, LVTS6-1, LVTS6-2, LVTS6-3
 * DRAM CH0     LVTS7-0,
 * DRAM CH1     LVTS7-1,
 * SOC TOP      LVTS7-2,
 * DRAM CH3     LVTS8-0,
 * DRAM CH4     LVTS8-1,
 * SOC MM       LVTS8-2,
 * MD-4G        LVTS9-0
 * MD-5G        LVTS9-1
 * MD-3G        LVTS9-2
 * ptp_therm_ctrl_AP  Base address: (+0x1031_5000), 0x1031_5100,
 *                                 0x1031_5200, 0x1031_5300, 0x1031_5400
 * ptp_therm_ctrl_MCU Base address: (+0x1031_6000), 0x1031_6100,
 *                                 0x1031_6200, 0x1031_6300
 */


/* private thermal sensor enum */
enum cpu_lvts_sensor_enum {
	CPU_TS_LVTS1_0 = 0,	/* LVTS1-0 BIG4*/
	CPU_TS_LVTS1_1,		/* LVTS1-1 BIG5*/
	CPU_TS_LVTS1_2,		/* LVTS1-2 BIG4*/
	CPU_TS_LVTS1_3,		/* LVTS1-3 BIG5*/
	CPU_TS_LVTS2_0,		/* LVTS2-0 BIG6*/
	CPU_TS_LVTS2_1,		/* LVTS2-1 BIG7*/
	CPU_TS_LVTS2_2,		/* LVTS2-0 BIG6*/
	CPU_TS_LVTS2_3,		/* LVTS2-1 BIG7*/
	CPU_TS_LVTS3_0,		/* LVTS3-0 LCPU0*/
	CPU_TS_LVTS3_1,		/* LVTS3-1 LCPU2*/
	CPU_TS_LVTS3_2,		/* LVTS3-2 LCPU1*/
	CPU_TS_LVTS3_3,		/* LVTS3-3 LCPU3*/
	CPU_TS_LVTS_NUM
};

enum cpu_id_enum {
	CPU0 = 0,
	CPU1,
	CPU2,
	CPU3,
	CPU4,
	CPU5,
	CPU6,
	CPU7,
	CPU_NUM
};

enum cluster_id_enum {
	L_CLUSTER = 0,
	M_CLUSTER,
	B_CLUSTER,
	CLUSTER_NUM
};


extern struct lvts_config cpu_lvts_cfg;
extern int max_cpu_temp[CPU_NUM];

#endif /*_CPU_H */

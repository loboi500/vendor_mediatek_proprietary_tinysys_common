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
#include "thermal_utils.h"
#include "thermal_hw.h"
#include "cpu.h"


//	L_TS_LVTS1_0 = 0,	/* LITTLE */
//	L_TS_LVTS1_1,		/* LITTLE */
//	L_TS_LVTS1_2,		/* LITTLE */
//	L_TS_LVTS1_3,		/* LITTLE */
//	L_TS_LVTS2_0,		/* BIG */
//	L_TS_LVTS2_1,		/* BIG */
//	L_TS_LVTS2_2,		/* BIG */
//	L_TS_LVTS2_3,		/* BIG */

/*
 * module  			        LVTS Plan
 *=====================================================
 * AP_LITTLE(TS1,TS2,TS3,TS4)		LVTS1-0, LVTS1-1, LVTS1-2, LVTS1-3
 * AP_BIG(TS5,TS6,TS7,TS8			LVTS2-0, LVTS2-1, LVTS2-2, LVTS2-3
 * GPU(TS9,TS10)					LVTS3-0, LVTS3-1
 * SOC(TS11,TS12)					LVTS3-2, LVTS3-3
 * MD(TS13)							LVTS4-0
 */

struct lvts_ts_info cpu_ts[CPU_TS_LVTS_NUM] = {
	[CPU_TS_LVTS1_0] = {
		.tc_id = LVTS_AP_CONTROLLER0,
		.ts_order = 0,
		.cpumask = _BIT_(CPU1),
		.cluster_id = L_CLUSTER,
	},
	[CPU_TS_LVTS1_1] = {
		.tc_id = LVTS_AP_CONTROLLER0,
		.ts_order = 1,
		.cpumask = _BIT_(CPU2),
		.cluster_id = L_CLUSTER,
	},
	[CPU_TS_LVTS1_2] = {
		.tc_id = LVTS_AP_CONTROLLER0,
		.ts_order = 2,
		.cpumask = _BIT_(CPU4),
		.cluster_id = L_CLUSTER,
	},
	[CPU_TS_LVTS1_3] = {
		.tc_id = LVTS_AP_CONTROLLER0,
		.ts_order = 3,
		.cpumask = _BIT_(CPU5),
		.cluster_id = L_CLUSTER,
	},
	[CPU_TS_LVTS2_0] = {
		.tc_id = LVTS_AP_CONTROLLER1,
		.ts_order = 0,
		.cpumask = _BIT_(CPU6),
		.cluster_id = B_CLUSTER,
	},
	[CPU_TS_LVTS2_1] = {
		.tc_id = LVTS_AP_CONTROLLER1,
		.ts_order = 1,
		.cpumask = _BIT_(CPU7) ,
		.cluster_id = B_CLUSTER,
	},
	[CPU_TS_LVTS2_2] = {
		.tc_id = LVTS_AP_CONTROLLER1,
		.ts_order = 2,
		.cpumask = _BIT_(CPU6) ,
		.cluster_id = B_CLUSTER,
	},
	[CPU_TS_LVTS2_3] = {
		.tc_id = LVTS_AP_CONTROLLER1,
		.ts_order = 3,
		.cpumask = _BIT_(CPU7),
		.cluster_id = B_CLUSTER,
	}

};

int cpu_golden_temp[LVTS_AP_CONTROLLER_NUM] = {0};
int cpu_coff_a[LVTS_AP_CONTROLLER_NUM] = {LVTS_COEFF_A_X_1000, LVTS_COEFF_A_X_1000,
	LVTS_COEFF_A_X_1000, LVTS_COEFF_A_X_1000};

struct lvts_config cpu_lvts_cfg = {
    .base_addr = LVTS_CTRL_BASE,
    .ts_list = cpu_ts,
    .coff_a = cpu_coff_a,
    .golden_temp = cpu_golden_temp,
    .ops = {
        .lvts_raw_to_temperature = lvts_raw_to_temperature,
    },
};

int max_cpu_temp[CPU_NUM] = {THER_INVALID_TEMP, THER_INVALID_TEMP, THER_INVALID_TEMP,
                             THER_INVALID_TEMP, THER_INVALID_TEMP, THER_INVALID_TEMP,
                             THER_INVALID_TEMP, THER_INVALID_TEMP};


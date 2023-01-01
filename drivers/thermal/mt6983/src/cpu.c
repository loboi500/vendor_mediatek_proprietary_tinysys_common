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

void lvts_get_golden_temp_ht(struct lvts_config *config, struct lvts_ts_info *ts_info);
void lvts_update_cpu_ts_coef_data(struct lvts_config *config, struct lvts_ts_info *ts_info);
int cali_mode = -1;
int golden_temp_ht = 0;


struct lvts_ts_info cpu_ts[CPU_TS_LVTS_NUM] = {
	[CPU_TS_LVTS1_0] = {
		.tc_id = LVTS_MCU_CONTROLLER0,
		.ts_order = 0,
		.cpumask = _BIT_(CPU4),
		.cluster_id = M_CLUSTER,
	},
	[CPU_TS_LVTS1_1] = {
		.tc_id = LVTS_MCU_CONTROLLER0,
		.ts_order = 1,
		.cpumask = _BIT_(CPU5),
		.cluster_id = M_CLUSTER,
	},
	[CPU_TS_LVTS1_2] = {
		.tc_id = LVTS_MCU_CONTROLLER0,
		.ts_order = 2,
		.cpumask = _BIT_(CPU4),
		.cluster_id = M_CLUSTER,
	},
	[CPU_TS_LVTS1_3] = {
		.tc_id = LVTS_MCU_CONTROLLER0,
		.ts_order = 3,
		.cpumask = _BIT_(CPU5),
		.cluster_id = M_CLUSTER,
	},
	[CPU_TS_LVTS2_0] = {
		.tc_id = LVTS_MCU_CONTROLLER1,
		.ts_order = 0,
		.cpumask = _BIT_(CPU6),
		.cluster_id = M_CLUSTER,
	},
	[CPU_TS_LVTS2_1] = {
		.tc_id = LVTS_MCU_CONTROLLER1,
		.ts_order = 1,
		.cpumask = _BIT_(CPU7),
		.cluster_id = B_CLUSTER,
	},
	[CPU_TS_LVTS2_2] = {
		.tc_id = LVTS_MCU_CONTROLLER1,
		.ts_order = 2,
		.cpumask = _BIT_(CPU6),
		.cluster_id = M_CLUSTER,
	},
	[CPU_TS_LVTS2_3] = {
		.tc_id = LVTS_MCU_CONTROLLER1,
		.ts_order = 3,
		.cpumask = _BIT_(CPU7),
		.cluster_id = B_CLUSTER,
	},
	[CPU_TS_LVTS3_0] = {
		.tc_id = LVTS_MCU_CONTROLLER2,
		.ts_order = 0,
		.cpumask = _BIT_(CPU0),
		.cluster_id = L_CLUSTER,
	},
	[CPU_TS_LVTS3_1] = {
		.tc_id = LVTS_MCU_CONTROLLER2,
		.ts_order = 1,
		.cpumask = _BIT_(CPU2),
		.cluster_id = L_CLUSTER,
	},
	[CPU_TS_LVTS3_2] = {
		.tc_id = LVTS_MCU_CONTROLLER2,
		.ts_order = 2,
		.cpumask = _BIT_(CPU1),
		.cluster_id = L_CLUSTER,
	},
	[CPU_TS_LVTS3_3] = {
		.tc_id = LVTS_MCU_CONTROLLER2,
		.ts_order = 3,
		.cpumask = _BIT_(CPU3),
		.cluster_id = L_CLUSTER,
	}
};
int cpu_golden_temp[LVTS_MCU_CONTROLLER_NUM] = {0, 0, 0, 0};
int cpu_coff_a[LVTS_MCU_CONTROLLER_NUM] = {LVTS_COEFF_A_X_1000, LVTS_COEFF_A_X_1000,
	LVTS_COEFF_A_X_1000, LVTS_COEFF_A_X_1000};

struct lvts_config cpu_lvts_cfg = {
    .base_addr = LVTS_MCU_CTRL_BASE,
    .ts_list = cpu_ts,
    .coff_a = cpu_coff_a,
    .golden_temp = cpu_golden_temp,
    .ops = {
        .lvts_raw_to_temperature = lvts_raw_to_temperature_v3,
        .lvts_get_golden_temp_ht = lvts_get_golden_temp_ht,
        .lvts_update_coef_data = lvts_update_cpu_ts_coef_data,
    },
};

int max_cpu_temp[CPU_NUM] = {THER_INVALID_TEMP, THER_INVALID_TEMP, THER_INVALID_TEMP,
    THER_INVALID_TEMP, THER_INVALID_TEMP, THER_INVALID_TEMP, THER_INVALID_TEMP, THER_INVALID_TEMP};

void lvts_get_golden_temp_ht(struct lvts_config *config, struct lvts_ts_info *ts_info)
{
    int golden_temp;

    if (cali_mode == -1)
        cali_mode = read_cali_mode(GOLDEN_TEMP_EFUSE_ADDR);

    if (golden_temp_ht == 0)
        golden_temp_ht = read_golden_temp_ht(GOLDEN_TEMP_EFUSE_ADDR);

    if (cali_mode == 1) {
        golden_temp = golden_temp_ht;
        config->coff_a[ts_info->tc_id] = LVTS_COEFF_A_X_1000_HT;
        config->golden_temp[ts_info->tc_id] = (!golden_temp) ? DEFAULT_EFUSE_GOLDEN_TEMP_HT : golden_temp;
    }
}

void lvts_update_cpu_ts_coef_data(struct lvts_config *config, struct lvts_ts_info *ts_info)
{
    unsigned int s_index, count_r;

    s_index = ((int)ts_info - (int)config->ts_list) / sizeof(struct lvts_ts_info);
    s_index = (s_index <= 0) ? 0 : ((s_index < CPU_TS_LVTS_NUM) ? s_index : 0);

    count_r = LVTS_COUNT_R_EFUSE(LVTS1_0_COUNT_R_EFUSE_ADDR + 4 * (s_index / 2), 0xFFFF,
        (s_index % 2) * 16);
    count_r = (count_r) ? count_r : DEFAULT_EFUSE_COUNT;

    if (cali_mode == 1) {
        ts_info->ts_coef_a = LVTS_COF_T_SLP_GLD_HT + (count_r * LVTS_COF_T_CONST_OFS_HT /
            LVTS_COF_COUNT_R_GLD_HT - LVTS_COF_T_CONST_OFS_HT);
    } else {
        ts_info->ts_coef_a = LVTS_COF_T_SLP_GLD + (count_r * LVTS_COF_T_CONST_OFS /
            LVTS_COF_COUNT_R_GLD - LVTS_COF_T_CONST_OFS);
    }
}


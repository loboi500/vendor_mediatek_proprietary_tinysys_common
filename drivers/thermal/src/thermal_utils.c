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
#include <intc.h>
#ifdef CFG_TIMER_SUPPORT
#include <ostimer.h>
#endif
#ifdef CFG_REMAP_SUPPORT
#include <remap.h>
#endif
#include "thermal_utils.h"
#include "thermal_hw.h"

int g_golden_temp = 0;

/*=============================================================
 * Local function
 *=============================================================*/
unsigned int thermal_read(unsigned int addr)
{
#ifdef CFG_REMAP_SUPPORT
    return DRV_Reg32(sysreg_remap(addr));
#else
    return DRV_Reg32(addr);
#endif
}

int lvts_raw_to_temperature(struct lvts_config *config, struct lvts_ts_info *ts_info, unsigned int msr_raw)
{
    /* This function returns degree mC
     * temp[i] = a * MSR_RAW/16384 + GOLDEN_TEMP/2 + b
     * a = -252.5
     * b =  252.5
     */
    int temp_mC = 0;
    int temp1 = 0;

    temp1 = (LVTS_COEFF_A_X_1000 * ((long long int)msr_raw)) >> 14;
    temp_mC = temp1 + config->golden_temp[ts_info->tc_id] * 500 - LVTS_COEFF_A_X_1000;

    return temp_mC;
}
int lvts_raw_to_temperature_v2(struct lvts_config *config, struct lvts_ts_info *ts_info, unsigned int msr_raw)
{
    int temp_mC = 0;
    int temp1 = 0;

    temp1 = (((long long)config->coff_a[ts_info->tc_id] << 14)) / msr_raw;
    temp_mC = temp1 + config->golden_temp[ts_info->tc_id] * 500 - config->coff_a[ts_info->tc_id];

    return temp_mC;
}

int lvts_raw_to_temperature_v3(struct lvts_config *config, struct lvts_ts_info *ts_info, unsigned int msr_raw)
{
    int temp_mC = 0;
    int temp1 = 0;

    temp1 = (((long long)ts_info->ts_coef_a << 14)) / msr_raw;
    temp_mC = temp1 + config->golden_temp[ts_info->tc_id] * 500 - ts_info->ts_coef_a;

    return temp_mC;
}

static void lvts_get_golden_temp(struct lvts_config *config, struct lvts_ts_info *ts_info)
{
    if (g_golden_temp == 0)
        g_golden_temp = read_golden_temp(GOLDEN_TEMP_EFUSE_ADDR);

    g_golden_temp = (!g_golden_temp) ? DEFAULT_EFUSE_GOLDEN_TEMP : g_golden_temp;

    config->golden_temp[ts_info->tc_id] = g_golden_temp;
}

/*=============================================================
 * Common APIs
 *=============================================================*/
void lvts_update_sensor_temp(struct lvts_config *config, struct lvts_ts_info *ts_info)
{
    unsigned int raw;
    struct platform_ops *ops = &config->ops;

    if (config->golden_temp[ts_info->tc_id] == 0) {
        lvts_get_golden_temp(config, ts_info);

        if (ops->lvts_get_golden_temp_ht)
            ops->lvts_get_golden_temp_ht(config, ts_info);
    }

    if (ts_info->ts_coef_a == 0) {
        if (ops->lvts_update_coef_data)
            ops->lvts_update_coef_data(config, ts_info);
    }

    raw = thermal_read(LVTS_MSR_REG(config->base_addr, ts_info->tc_id, ts_info->ts_order)) & 0xFFFF;
    ts_info->msr_raw = raw;
    ts_info->lvts_ts_temp = (!raw) ? THRM_DEFUALT_TEMP : ops->lvts_raw_to_temperature(config, ts_info, raw);
}

unsigned int get_ap_timestamp(void)
{
#ifdef CFG_TIMER_SUPPORT
    int in_suspend = 0;
    unsigned long long ts;
    struct sys_time_t ap_timer;

    if (is_in_isr())
        in_suspend = ts_apmcu_time_isr(&ap_timer);
    else /* CTX_TASK */
        in_suspend = ts_apmcu_time(&ap_timer);

    ts = (in_suspend) ? (ap_timer.ts + ap_timer.off_ns) : (ap_timer.ts);

    return (unsigned int)((ts / 1000000000) & 0x7FFFFFFF); //ns to s
#else
    return 0;
#endif
}

enum thermal_sram_status thermal_sram_read(unsigned int addr, int *val)
{
    *val = (int)thermal_read(addr);

    return (IS_SRAM_DATA_VALID(*val)) ? SRAM_DATA_READY : SRAM_DATA_INVALID;
}

void thermal_sram_write(unsigned int addr, int val)
{
#ifdef CFG_REMAP_SUPPORT
    DRV_WriteReg32(sysreg_remap(addr), val);
#else
    DRV_WriteReg32(addr, val);
#endif
}

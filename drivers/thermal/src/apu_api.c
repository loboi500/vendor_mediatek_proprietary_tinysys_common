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
#ifdef CFG_XGPT_SUPPORT
#include "xgpt.h"
#endif
#include "thermal_utils.h"
#include "thermal_hw.h"
#include "apu_api.h"

static int max_apu_temp = THER_INVALID_TEMP;

/*=============================================================
 * Local function
 *=============================================================*/
/* apusys is not support ostimer related APIs and only can get its local timestamp */
#if defined(CFG_XGPT_SUPPORT) && !defined(USE_APU_MBOX)
static unsigned int get_apusys_timestamp(void)
{
    unsigned long long ts;

    ts = read_sys_timestamp_ns();

    return (unsigned int)((ts / 1000000000) & 0x7FFFFFFF); //ns to s
}
#endif

static void update_apu_temp_record(int temp)
{
    max_apu_temp = temp;
#ifdef USE_APU_MBOX
    thermal_sram_write(APU_MBOX_MAX_TEMP_ADDR, max_apu_temp);
#else
    thermal_sram_write(APU_MAX_TEMP_BASE_ADDR, max_apu_temp);
#ifdef CFG_XGPT_SUPPORT
    thermal_sram_write(APU_TIMESTAMP_BASE_ADDR, get_apusys_timestamp());
#endif
#endif
}

static bool get_apu_emul_temp(int *temp)
{
    enum thermal_sram_status ret;
    int emul_temp;

#ifdef USE_APU_MBOX
    ret = thermal_sram_read(APU_MBOX_EMUL_TEMP_ADDR, &emul_temp);
#else
    ret = thermal_sram_read(APU_EMUL_TEMP_BASE_ADDR, &emul_temp);
#endif
    if (ret == SRAM_DATA_INVALID || emul_temp == THER_INVALID_TEMP)
        return false;

    *temp = emul_temp;

    return true;
}

/*=============================================================
 * Thermal Sensor Access APIs
 *=============================================================*/

int get_apu_temp(enum apu_lvts_sensor_enum sensor_id, enum lvts_data_source source)
{
    int emul_temp;

    if (sensor_id >= APU_TS_LVTS_NUM) {
        thermal_error("Error: Array index: %d out of bounds\n", sensor_id);
        return THER_INVALID_TEMP;
    }

    if (get_apu_emul_temp(&emul_temp))
        return emul_temp;

    /* update all sensors and SRAM record */
    if (source == HW_REG)
        get_apu_max_temp(HW_REG);

    return apu_lvts_cfg.ts_list[sensor_id].lvts_ts_temp;
}

int get_apu_max_temp(enum lvts_data_source source)
{
    int i, max = THER_INVALID_TEMP, emul_temp;

    if (get_apu_emul_temp(&emul_temp))
        return emul_temp;

    if (source == SW_RECORD)
        return max_apu_temp;

    for (i = 0; i < APU_TS_LVTS_NUM; i++) {
        lvts_update_sensor_temp(&apu_lvts_cfg, &apu_lvts_cfg.ts_list[i]);
        if (max < apu_lvts_cfg.ts_list[i].lvts_ts_temp)
            max = apu_lvts_cfg.ts_list[i].lvts_ts_temp;
    }
    update_apu_temp_record(max);

    return max;
}

/*=============================================================
 * Thermal SRAM Access APIs
 *=============================================================*/
int get_apu_ttj(void)
{
    int ttj;
    enum thermal_sram_status ret;

#ifdef USE_APU_MBOX
    ret = thermal_sram_read(APU_MBOX_TTJ_ADDR, &ttj);

    /* apu mbox will be reset when Vcore off */
    return (ret == SRAM_DATA_READY && ttj != 0) ? (int)ttj : DEFAULT_TTJ;
#else
    ret = thermal_sram_read(APU_TTJ_BASE_ADDR, &ttj);

    return (ret == SRAM_DATA_READY) ? (int)ttj : DEFAULT_TTJ;
#endif
}

void record_apu_limit_opp(int opp)
{
#ifdef USE_APU_MBOX
    thermal_sram_write(APU_MBOX_LIMIT_OPP_ADDR, opp);
#else
    thermal_sram_write(APU_LIMIT_OPP_BASE_ADDR, opp);
#endif
}

void record_apu_cur_opp(int opp)
{
#ifdef USE_APU_MBOX
    thermal_sram_write(APU_MBOX_CUR_OPP_ADDR, opp);
#else
    thermal_sram_write(APU_CURR_OPP_BASE_ADDR, opp);
#endif
}


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
#include "gpu_api.h"

#define MIN_THROTTLE_FREQ_NO_SPECIFY      (-1)

static int max_gpu_temp = THER_INVALID_TEMP;
static int gpu_ttj = DEFAULT_TTJ;

/*=============================================================
 * Local function
 *=============================================================*/
static void update_gpu_temp_record(int temp)
{
    max_gpu_temp = temp;
    thermal_sram_write(GPU_MAX_TEMP_BASE_ADDR, max_gpu_temp);
    thermal_sram_write(GPU_TIMESTAMP_BASE_ADDR, get_ap_timestamp());
}

static bool get_gpu_emul_temp(int *temp)
{
    enum thermal_sram_status ret;
    int emul_temp;

    ret = thermal_sram_read(GPU_EMUL_TEMP_BASE_ADDR, &emul_temp);
    if (ret == SRAM_DATA_INVALID || emul_temp == THER_INVALID_TEMP)
        return false;

    *temp = emul_temp;

    return true;
}

/*=============================================================
 * Thermal Sensor Access APIs
 *=============================================================*/
int get_gpu_temp(enum gpu_lvts_sensor_enum sensor_id, enum lvts_data_source source)
{
    int emul_temp;

    if (sensor_id >= GPU_TS_LVTS_NUM) {
        thermal_error("Error: Array index: %d out of bounds\n", sensor_id);
        return THER_INVALID_TEMP;
    }

    if (get_gpu_emul_temp(&emul_temp))
        return emul_temp;

    /* update all sensors and SRAM record */
    if (source == HW_REG)
        get_gpu_max_temp(HW_REG);

    return gpu_lvts_cfg.ts_list[sensor_id].lvts_ts_temp;
}

int get_gpu_max_temp(enum lvts_data_source source)
{
    int i, max = THER_INVALID_TEMP, emul_temp;

    if (get_gpu_emul_temp(&emul_temp))
        return emul_temp;

    if (source == SW_RECORD)
        return max_gpu_temp;

    for (i = 0; i < GPU_TS_LVTS_NUM; i++) {
        lvts_update_sensor_temp(&gpu_lvts_cfg, &gpu_lvts_cfg.ts_list[i]);
        if (max < gpu_lvts_cfg.ts_list[i].lvts_ts_temp)
            max = gpu_lvts_cfg.ts_list[i].lvts_ts_temp;
    }
    update_gpu_temp_record(max);

    if (max < gpu_ttj)
        thermal_sram_write(GPU_JATM_SUSPEND_ADDR, 0);

    return max;
}

/*=============================================================
 * Thermal SRAM Access APIs
 *=============================================================*/
int get_gpu_ttj(void)
{
    enum thermal_sram_status ret;

    ret = thermal_sram_read(GPU_TTJ_BASE_ADDR, &gpu_ttj);

	if (ret != SRAM_DATA_READY)
		gpu_ttj = DEFAULT_TTJ;

    return gpu_ttj;
}

void record_gpu_limit_freq(unsigned int freq)
{
    thermal_sram_write(GPU_LIMIT_FREQ_BASE_ADDR, freq);
}

void record_gpu_cur_freq(unsigned int freq)
{
    thermal_sram_write(GPU_CURR_FREQ_BASE_ADDR, freq);
}

int get_gpu_min_throttle_freq(void)
{
    int val;
    enum thermal_sram_status ret;

    ret = thermal_sram_read(MIN_THROTTLE_FREQ_BASE_ADDR + 12, &val);

    return (ret == SRAM_DATA_READY) ? val : MIN_THROTTLE_FREQ_NO_SPECIFY;
}

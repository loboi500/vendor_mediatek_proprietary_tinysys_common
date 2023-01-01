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
#ifdef CFG_THERMAL_HEADROOM
#include "thermal_headroom.h"
#endif
#include "cpu_api.h"

#define MIN_OPP_NO_SPECIFY      (-1)
#define MIN_THROTTLE_FREQ_NO_SPECIFY      (-1)
#define ACTIVE_BITMASK_ALL_ON   ((1 << CPU_NUM) - 1)

static int cpu_ttj = DEFAULT_TTJ;
#ifdef CFG_THERMAL_HEADROOM
static struct headroom_info cpu_hr_info[CPU_NUM];
#endif

/*=============================================================
 * Local function
 *=============================================================*/
static int update_core_max_temp(int cpu_id, enum lvts_data_source source)
{
    int i, max = THER_INVALID_TEMP;

    for (i = 0; i < CPU_TS_LVTS_NUM; i++) {
        if ((cpu_lvts_cfg.ts_list[i].cpumask & _BIT_(cpu_id)) == 0)
            continue;
        if (source == HW_REG)
            lvts_update_sensor_temp(&cpu_lvts_cfg, &cpu_lvts_cfg.ts_list[i]);
        if (max < cpu_lvts_cfg.ts_list[i].lvts_ts_temp)
            max = cpu_lvts_cfg.ts_list[i].lvts_ts_temp;
    }

    max_cpu_temp[cpu_id] = max;
    thermal_sram_write(CPU_TEMP_BASE_ADDR + (cpu_id * 4), max);
    thermal_sram_write(CPU_TIMESTAMP_BASE_ADDR + (cpu_id * 4), get_ap_timestamp());

    return max;
}

static bool get_cpu_emul_temp(int *temp)
{
    enum thermal_sram_status ret;
    int emul_temp;

    ret = thermal_sram_read(CPU_EMUL_TEMP_BASE_ADDR, &emul_temp);
    if (ret == SRAM_DATA_INVALID || emul_temp == THER_INVALID_TEMP)
        return false;

    *temp = emul_temp;

    return true;
}

/*=============================================================
 * Thermal Sensor Access APIs
 *=============================================================*/
int get_cpu_temp(enum cpu_lvts_sensor_enum sensor_id, enum lvts_data_source source)
{
    int emul_temp, i;

    if (sensor_id >= CPU_TS_LVTS_NUM) {
        thermal_error("Error: Array index: %d out of bounds\n", sensor_id);
        return THER_INVALID_TEMP;
    }

    if (get_cpu_emul_temp(&emul_temp))
        return emul_temp;

    /* update all sensors in the same CPU core and SRAM record */
    if (source == HW_REG)
        for (i = 0; i < CPU_NUM; i++) {
                if ((cpu_lvts_cfg.ts_list[sensor_id].cpumask & _BIT_(i)) != 0)
                    update_core_max_temp(i, HW_REG);
        }

    return cpu_lvts_cfg.ts_list[sensor_id].lvts_ts_temp;
}

int get_cpu_max_temp(enum lvts_data_source source)
{
    int i, max = THER_INVALID_TEMP, emul_temp;

    if (get_cpu_emul_temp(&emul_temp))
        return emul_temp;

    for (i = 0; i < CPU_TS_LVTS_NUM; i++) {
        if (source == HW_REG)
            lvts_update_sensor_temp(&cpu_lvts_cfg, &cpu_lvts_cfg.ts_list[i]);
        if (max < cpu_lvts_cfg.ts_list[i].lvts_ts_temp)
            max = cpu_lvts_cfg.ts_list[i].lvts_ts_temp;
    }

    if (source == HW_REG) {
        for (i = 0; i < CPU_NUM; i++)
            update_core_max_temp(i, SW_RECORD);
    }

    if (max < cpu_ttj)
        thermal_sram_write(CPU_JATM_SUSPEND_ADDR, 0);

    return max;
}

int get_core_max_temp(enum cpu_id_enum cpu_id, enum lvts_data_source source)
{
    int max = THER_INVALID_TEMP, emul_temp;

    if (cpu_id >= CPU_NUM)
        return THER_INVALID_TEMP;

    if (get_cpu_emul_temp(&emul_temp))
        return emul_temp;

    if (source == HW_REG)
        max = update_core_max_temp(cpu_id, HW_REG);
    else
        max = max_cpu_temp[cpu_id];

    return max;
}

int get_cluster_max_temp(enum cluster_id_enum cluster_id, enum lvts_data_source source)
{
    int i, max = THER_INVALID_TEMP, emul_temp;
    int update_cpumask = 0;

    if (get_cpu_emul_temp(&emul_temp))
        return emul_temp;

    for (i = 0; i < CPU_TS_LVTS_NUM; i++) {
        if (cpu_lvts_cfg.ts_list[i].cluster_id != (int)cluster_id)
            continue;
        if (source == HW_REG) {
            lvts_update_sensor_temp(&cpu_lvts_cfg, &cpu_lvts_cfg.ts_list[i]);
            update_cpumask |= cpu_lvts_cfg.ts_list[i].cpumask;
        }
        if (max < cpu_lvts_cfg.ts_list[i].lvts_ts_temp)
            max = cpu_lvts_cfg.ts_list[i].lvts_ts_temp;
    }

    if (source == HW_REG) {
        for (i = 0; i < CPU_NUM; i++) {
            if (update_cpumask & _BIT_(i))
                update_core_max_temp(i, SW_RECORD);
        }
    }

    return max;
}

int get_cluster_min_temp(enum cluster_id_enum cluster_id, enum lvts_data_source source)
{
    int i, min = THER_MAX_TEMP, emul_temp;
    int update_cpumask = 0;

    if (get_cpu_emul_temp(&emul_temp))
        return emul_temp;

    for (i = 0; i < CPU_TS_LVTS_NUM; i++) {
        if (cpu_lvts_cfg.ts_list[i].cluster_id != (int)cluster_id)
            continue;
        if (source == HW_REG) {
            lvts_update_sensor_temp(&cpu_lvts_cfg, &cpu_lvts_cfg.ts_list[i]);
            update_cpumask |= cpu_lvts_cfg.ts_list[i].cpumask;
        }
        if (min > cpu_lvts_cfg.ts_list[i].lvts_ts_temp)
            min = cpu_lvts_cfg.ts_list[i].lvts_ts_temp;
    }

    if (source == HW_REG) {
        for (i = 0; i < CPU_NUM; i++) {
            if (update_cpumask & _BIT_(i))
                update_core_max_temp(i, SW_RECORD);
        }
    }

    return min;
}

/*=============================================================
 * Thermal SRAM Access APIs
 *=============================================================*/
int get_cpu_ttj(void)
{
    enum thermal_sram_status ret = thermal_sram_read(CPU_TTJ_BASE_ADDR, &cpu_ttj);

	if (ret != SRAM_DATA_READY)
		cpu_ttj = DEFAULT_TTJ;

    return cpu_ttj;
}

int get_cpu_min_opp_hint(enum cluster_id_enum cluster_id)
{
    int val;
    enum thermal_sram_status ret;

    if (cluster_id >= CLUSTER_NUM)
        return MIN_OPP_NO_SPECIFY;

    ret = thermal_sram_read(CPU_MIN_OPP_BASE_ADDR + (cluster_id * 4), &val);

    return (ret == SRAM_DATA_READY) ? val : MIN_OPP_NO_SPECIFY;
}

void record_cpu_limit_opp(enum cluster_id_enum cluster_id, int opp)
{
    if (cluster_id >= CLUSTER_NUM)
        return;

    thermal_sram_write(CPU_LIMIT_OPP_BASE_ADDR + (cluster_id * 4), opp);
}

void record_cpu_limit_freq(enum cluster_id_enum cluster_id, unsigned int freq)
{
    if (cluster_id >= CLUSTER_NUM)
        return;

    thermal_sram_write(CPU_LIMIT_FREQ_BASE_ADDR + (cluster_id * 4), freq);
}

void record_cpu_cur_freq(enum cluster_id_enum cluster_id, unsigned int freq)
{
    if (cluster_id >= CLUSTER_NUM)
        return;

    thermal_sram_write(CPU_CURR_FREQ_BASE_ADDR + (cluster_id * 4), freq);
}

void record_cpu_max_log_temp(enum cluster_id_enum cluster_id, int temp)
{
    if (cluster_id >= CLUSTER_NUM)
        return;

    thermal_sram_write(CPU_MAX_LOG_TEMP_BASE_ADDR + (cluster_id * 4), temp);
}

void record_utc_cnt(int cnt)
{
    thermal_sram_write(UTC_COUNT_ADDR, cnt);
}

unsigned int get_cpu_active_bitmask(void)
{
    int val;
    enum thermal_sram_status ret = thermal_sram_read(CPU_ACTIVE_BASE_ADDR, &val);

    return (ret == SRAM_DATA_READY) ? val : ACTIVE_BITMASK_ALL_ON;
}

int get_cpu_min_throttle_freq(enum cluster_id_enum cluster_id)
{
    int val;
    enum thermal_sram_status ret;

    if (cluster_id >= CLUSTER_NUM)
        return MIN_THROTTLE_FREQ_NO_SPECIFY;

    ret = thermal_sram_read(MIN_THROTTLE_FREQ_BASE_ADDR + (cluster_id * 4), &val);

    return (ret == SRAM_DATA_READY) ? val : MIN_THROTTLE_FREQ_NO_SPECIFY;
}

/*=============================================================
 * Thermal Headroom APIs
 *=============================================================*/
#ifdef CFG_THERMAL_HEADROOM
void update_cpu_thermal_headroom(void)
{
    int i, hr;

    for (i = 0; i < CPU_NUM; i++) {
#ifdef HEADROOM_ADD_PREDICTION
        if (!cpu_hr_info[i].last_update_time)
            cpu_hr_info[i].prev_temp = max_cpu_temp[i];
        else
            cpu_hr_info[i].prev_temp = cpu_hr_info[i].cur_temp;
#endif
        cpu_hr_info[i].cur_temp = max_cpu_temp[i];
        hr = get_thermal_headroom(&cpu_hr_info[i], cpu_ttj, 1000);
        thermal_sram_write(CPU_HEADROOM_BASE_ADDR + (i * 4), hr);
#ifdef HEADROOM_ADD_PREDICTION
        thermal_sram_write(CPU_HEADROOM_RATIO_BASE_ADDR + (i * 4), cpu_hr_info[i].ratio);
        thermal_sram_write(CPU_PREDICT_TEMP_BASE_ADDR + (i * 4), cpu_hr_info[i].predict_temp);
#endif
    }
}
#endif


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
#include "vcore_api.h"

static int min_vcore_temp = THER_MAX_TEMP;

/*=============================================================
 * Local function
 *=============================================================*/
static void update_vcore_temp_record(int temp)
{
    min_vcore_temp = temp;
    thermal_sram_write(VCORE_MIN_TEMP_BASE_ADDR, min_vcore_temp);
    thermal_sram_write(VCORE_TIMESTAMP_BASE_ADDR, get_ap_timestamp());
}

static bool get_vcore_emul_temp(int *temp)
{
    enum thermal_sram_status ret;
    int emul_temp;

    ret = thermal_sram_read(VCORE_EMUL_TEMP_BASE_ADDR, &emul_temp);
    if (ret == SRAM_DATA_INVALID || emul_temp == THER_INVALID_TEMP)
        return false;

    *temp = emul_temp;

    return true;
}

/*=============================================================
 * Thermal Sensor Access APIs
 *=============================================================*/
int get_vcore_temp(enum vcore_lvts_sensor_enum sensor_id, enum lvts_data_source source)
{
    int emul_temp;

    if (sensor_id >= VCORE_TS_LVTS_NUM) {
        thermal_error("Error: Array index: %d out of bounds\n", sensor_id);
        return THER_INVALID_TEMP;
    }

    if (get_vcore_emul_temp(&emul_temp))
        return emul_temp;

    /* update all sensors and SRAM record */
    if (source == HW_REG)
        get_vcore_min_temp(HW_REG);

    return vcore_lvts_cfg.ts_list[sensor_id].lvts_ts_temp;
}

int get_vcore_min_temp(enum lvts_data_source source)
{
    int i, min = THER_MAX_TEMP, emul_temp;

    if (get_vcore_emul_temp(&emul_temp))
        return emul_temp;

    if (source == SW_RECORD)
        return min_vcore_temp;

    for (i = 0; i < VCORE_TS_LVTS_NUM; i++) {
        lvts_update_sensor_temp(&vcore_lvts_cfg, &vcore_lvts_cfg.ts_list[i]);
        if (min > vcore_lvts_cfg.ts_list[i].lvts_ts_temp)
            min = vcore_lvts_cfg.ts_list[i].lvts_ts_temp;
    }
    update_vcore_temp_record(min);

    return min;
}


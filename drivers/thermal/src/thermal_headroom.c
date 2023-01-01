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
#include <ostimer.h>

#include "thermal_utils.h"
#include "thermal_hw.h"
#include "thermal_headroom.h"
#include "cpu.h"

#define FULL_HEADROOM_TEMP      (25000)
#define DEFAULT_SLOPE_RATIO     (20)
#define MAX_SLOPE_RATIO         (100)
#define MAX_HEADROOM            (100)


int get_thermal_headroom(struct headroom_info *hr, int ttj, int forecast_time_us)
{
    int coef, headroom;
#ifdef HEADROOM_ADD_PREDICTION
    unsigned long long cur_time = ostimer_get_ns();
    unsigned long long delta_time_us;
    int same_trend = 0, delta, delta_predict, predict_error;

    /* use cur_temp as predict_temp in first prediction */
    if (!hr->last_update_time) {
        hr->last_update_time = cur_time;
        hr->ratio = DEFAULT_SLOPE_RATIO;
        hr->predict_temp = hr->cur_temp;
        hr->prev_predict_temp = hr->cur_temp;
        goto headroom_calc;
    }

    /* calculate delta t */
    if (cur_time > hr->last_update_time)
        delta_time_us = (cur_time - hr->last_update_time) / 1000;
    else /* timestamp overflow */
        delta_time_us = ((~0ULL) - hr->last_update_time + cur_time) / 1000;
    hr->last_update_time = cur_time;

    delta = hr->cur_temp - hr->prev_temp;
    delta_predict = hr->predict_temp - hr->prev_predict_temp;
    if ((delta > 0 && delta_predict > 0) || (delta < 0 && delta_predict < 0))
        same_trend = 1;

    predict_error = hr->predict_temp - hr->cur_temp;
    hr->prev_predict_temp = hr->predict_temp;

    if (same_trend) {
        hr->ratio -= predict_error * 100 / abs(hr->cur_temp);
        hr->ratio = MAX(0, hr->ratio);
        hr->ratio = MIN(MAX_SLOPE_RATIO, hr->ratio);
    }

    if (delta_time_us)
        hr->predict_temp = hr->cur_temp +
            ((delta * forecast_time_us / delta_time_us) * hr->ratio / 100);
    else
        hr->predict_temp = hr->cur_temp;

headroom_calc:
#else
    /* use current temp as predict temp */
    hr->predict_temp = hr->cur_temp;
#endif

    coef = ttj - FULL_HEADROOM_TEMP;
    headroom = ((ttj - hr->predict_temp) * 100) / coef;
    headroom = MIN(MAX_HEADROOM, headroom);

    return headroom;
}


/* Copyright Statement:
*
* This software/firmware and related documentation ("MediaTek Software") are
* protected under relevant copyright laws. The information contained herein
* is confidential and proprietary to MediaTek Inc. and/or its licensors.
* Without the prior written permission of MediaTek inc. and/or its licensors,
* any reproduction, modification, use or disclosure of MediaTek Software,
* and information contained herein, in whole or in part, shall be strictly prohibited.
*/
/* MediaTek Inc. (C) 2015. All rights reserved.
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
* THAT IT IS RECEIVER'S SOLE RESPONSIBILITY TO OBTAIN FROM ANY THIRD PARTY ALL PROPER LICENSES
* CONTAINED IN MEDIATEK SOFTWARE. MEDIATEK SHALL ALSO NOT BE RESPONSIBLE FOR ANY MEDIATEK
* SOFTWARE RELEASES MADE TO RECEIVER'S SPECIFICATION OR TO CONFORM TO A PARTICULAR
* STANDARD OR OPEN FORUM. RECEIVER'S SOLE AND EXCLUSIVE REMEDY AND MEDIATEK'S ENTIRE AND
* CUMULATIVE LIABILITY WITH RESPECT TO THE MEDIATEK SOFTWARE RELEASED HEREUNDER WILL BE,
* AT MEDIATEK'S OPTION, TO REVISE OR REPLACE THE MEDIATEK SOFTWARE AT ISSUE,
* OR REFUND ANY SOFTWARE LICENSE FEES OR SERVICE CHARGE PAID BY RECEIVER TO
* MEDIATEK FOR SUCH MEDIATEK SOFTWARE AT ISSUE.
*/
#ifndef AUDIO_TASK_TIME_H
#define AUDIO_TASK_TIME_H

#include <stdio.h>
#include <audio_type.h>
#include <time.h>
#include <stdint.h>

/* time define */
#define TIME_NS_TO_US(time_ns) (((uint64_t)(time_ns)) / 1000)
#define TIME_NS_TO_MS(time_ns) (((uint64_t)(time_ns)) / 1000000)
#define TIME_NS_TO_SEC(time_ns) ((uint32_t)(time_ns / 1000000000))
#define TIME_US_TO_MS(time_ms) (((uint64_t)(time_ms)) / 1000)
#define TIME_MS_TO_US(time_ms) (((uint64_t)(time_ms)) * 1000)
#define TIME_MS_TO_US(time_ms) (((uint64_t)(time_ms)) * 1000)


enum {
	TASK_PRIMARY_TIME_ID1 = 0,
	TASK_PRIMARY_TIME_RWBUF,
	TASK_DEEPBUFFER_TIME_ID1,
	TASK_DEEPBUFFER_TIME_RWBUF,
	TASK_VOIP_TIME_ID1,
	TASK_VOIP_TIME_RWBUF,
	TASK_FAST_TIME_ID1,
	TASK_FAST_TIME_RWBUF,
	TASK_CAPTURE_UL1_TIME_ID1,
	TASK_CAPTURE_UL1_TIME_RWBUF,
	TASK_A2DP_TIME_ID1,
	TASK_A2DP_TIME_RWBUF,
	TASK_BLEENC_TIME_ID1,
	TASK_BLEENC_TIME_RWBUF,
	TASK_BLEDEC_TIME_ID1,
	TASK_BLEDEC_TIME_RWBUF,
	TASK_DATAPROVIDER_TIME_ID1,
	TASK_DATAPROVIDER_TIME_RWBUF,
	TASK_CALL_FINAL_TIME_ID1,
	TASK_CALL_FINAL_TIME_RWBUF,
	TASK_AUDPLAYUBACK_TIME_ID1,
	TASK_AUDPLAYUBACK_TIME_RWBUF,
	TASK_MUSIC_TIME_MIXER,
	TASK_MUSIC_TIME_RWBUF,
	TASK_KTV_TIME_ID1,
	TASK_KTV_TIME_ID2,
	TASK_CAPTURE_RAW_TIME_ID1,
	TASK_CAPTURE_RAW_TIME_RWBUF,
	TASK_FM_ADSP_TIME_ID1,
	TASK_FM_ADSP_TIME_RWBUF,
	TASK_BLEUMS_TIME_ID1,
	TASK_BLEUMS_TIME_RWBUF,
	TASK_BLE_UL_TIME_ID1,
	TASK_BLE_UL_TIME_RWBUF,
	TASK_RV_SPK_PROCESS_TIME_ID1,
	TASK_RV_SPK_PROCESS_TIME_RWBUF,
	NUM_OF_TASK_TIME_ID,
};

struct throttle_event{
	char *name;
	bool init;
	unsigned long long pre_time;
	unsigned long long cur_time;
	unsigned int sleep_time;
	int throttle_bound;
	int throttle_count;
	bool first_tag;
	bool first_time_throttle;
};

struct aud_time_profile {
	unsigned long long time_interval_prev;
	unsigned long long time_interval_last;
	unsigned long long time_diff;
	unsigned long long threshold;
};

/*
 * throttle, using when last playback timing and buffer szaie is not blocking or not align
 * with task , need to do throttle to avoid buffer get too fast.
 */
unsigned int get_sleep_time(unsigned int size, unsigned int channel, unsigned int fmt, unsigned int rate);
int set_throttle_event(struct throttle_event *throttle);
int set_throttle_time(struct throttle_event *throttle, unsigned int sleep_time, int throttle_bound, bool first_throttle, char* name);
int trigger_throttle(struct throttle_event *throttle);
int reset_throttle_time(struct throttle_event *throttle);

/* delay whit ms  */
void aud_task_delay(int delayms);
/* busy waiting with unit ns*/
void aud_task_delayus(unsigned int delayns);

/* profile time relate */
int init_time_interval(unsigned int id, unsigned long long thresholdns);
int update_time_threshold(unsigned int id, unsigned long long thresholdns);
int record_time_interval(unsigned int id);
int stop_time_interval(unsigned int id);
unsigned long long get_time_interval_diff(unsigned int id);
unsigned long long get_time_stamp(void);
unsigned long long get_time_diff(unsigned long long t1, unsigned long long t2);

unsigned int get_peroid_mstime(unsigned int rate, unsigned int period);
unsigned int get_peroid_nstime(unsigned int rate, unsigned int period);
unsigned int get_peroid_ustime(unsigned int rate, unsigned int period);
int get_bufsize_to_ms(struct buf_attr attr, int size);
int audio_size_to_ms(unsigned int size, unsigned int rate,
		     unsigned int channel, unsigned int format);

#endif // end of AUDIO_TASK_TIME_H

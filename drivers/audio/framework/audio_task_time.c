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

#ifdef CFG_XGPT_SUPPORT
#include <xgpt.h>
#endif

#include "audio_task_attr.h"
#include "audio_task_common.h"
#include "audio_task_time.h"
#include <limits.h>

#define NOP_CYCLE (15)

static struct aud_time_profile aud_time_profiler[NUM_OF_TASK_TIME_ID];

unsigned int get_sleep_time(unsigned int size,unsigned int channel, unsigned int fmt, unsigned int rate)
{
	if ( (!size) || (!channel) || (!rate)){
		AUD_LOG_W("%s() error size[%u] channel[%u] fmt[%u] rate[%u]\n",
			  __func__, size, channel, fmt, rate);
		return 0;
	}

	AUD_LOG_D("%s() size[%u] channel[%u] fmt[%u] rate[%u]\n",
		  __func__, size, channel, fmt, rate);
	if (rate < 8000)
		rate = 48000;

	unsigned int sleep_time;
	sleep_time = (size * 1000) / channel;
	sleep_time =  sleep_time / rate;
	switch (fmt) {
	case AUDIO_FORMAT_PCM_16_BIT:
		sleep_time = sleep_time / 2;
		break;
	case AUDIO_FORMAT_PCM_8_24_BIT:
	case AUDIO_FORMAT_PCM_32_BIT:
		sleep_time = sleep_time / 4;
		break;
	default:
		sleep_time = sleep_time / 2;
	}
	AUD_LOG_D("%s() sleep_time=%u\n", __func__, sleep_time);
	return sleep_time;
}

int set_throttle_event(struct throttle_event *throttle)
{
	if (!throttle)
		return -1;

	if (throttle->first_tag == false) {
		throttle->pre_time= get_time_stamp();
	}
	else {
		throttle->cur_time = get_time_stamp();
	}
	return 0;
}

int trigger_throttle(struct throttle_event *throttle)
{
	if (throttle->init == false)
		return -1;

	unsigned long long diff_time;
	unsigned int real_sleep_time = 0;
	real_sleep_time = (throttle->sleep_time) / 2;
	diff_time = get_time_diff(throttle->pre_time, throttle->cur_time) / 1000000;

#ifdef THROTTLE_DEBUG
	AUD_LOG_D("%s() name[%s] real_sleep_time[%u] throttle_count[%d] diff_time[%llu] pre_time[%llu] cur_time[%llu]\n",
		  __func__, throttle->name, real_sleep_time, throttle->throttle_count, diff_time, throttle->pre_time/1000000, throttle->cur_time/1000000);
#endif

	/* first time trigger */
	if (throttle->first_tag == false) {
		throttle->first_tag = true;
		/* if need firs time throttle*/
		if (throttle->first_time_throttle == true)
			aud_task_delay(real_sleep_time);
		throttle->pre_time= get_time_stamp();
	} else if (real_sleep_time > diff_time) {
		real_sleep_time = (real_sleep_time - diff_time);
		if (throttle->throttle_count == throttle->throttle_bound){
			AUD_LOG_D("+sleep %s() name[%s] real_sleep_time[%u] throttle_count[%d] diff_time[%llu]\n",
				  __func__, throttle->name, real_sleep_time, throttle->throttle_count, diff_time);
			aud_task_delay(real_sleep_time);
			throttle->throttle_count = 0;
		}
		/* only throttle bound > 0 increase throttle_count */
		if (throttle->throttle_bound > 0) {
			throttle->throttle_count++;
#ifdef THROTTLE_DEBUG
			AUD_LOG_D("+sleep %s() name[%s] throttle_count[%d] throttle_bound[%d]\n",
				  __func__, throttle->name, throttle->throttle_count, throttle->throttle_bound);
#endif
		}
		throttle->pre_time = get_time_stamp();
	} else {
		throttle->throttle_count = 0;
		throttle->pre_time = get_time_stamp();
		return 0;
	}
	return real_sleep_time;
}

int set_throttle_time(struct throttle_event *throttle, unsigned int sleep_time, int throttle_bound, bool first_throttle, char* name)
{
	if (!throttle)
		return -1;

	throttle->name = name;
	throttle->init = true;
	throttle->pre_time = 0;
	throttle->cur_time = 0;;
	throttle->sleep_time = sleep_time;
	if (throttle_bound >= 0)
		throttle->throttle_bound = throttle_bound;
	else
		throttle->throttle_bound = 1;
	throttle->first_time_throttle = first_throttle;
	throttle->first_tag = false;
	AUD_LOG_D("%s() sleep_time[%u] throttle_bound[%d] taskname[%s]\n",
		  __func__,sleep_time, throttle_bound, name);
	return 0;
}

int reset_throttle_time(struct throttle_event *throttle)
{
	if (!throttle)
		return -1;

	memset (throttle, 0 , sizeof(struct throttle_event));
	return 0;
}

void aud_task_delay(int delayms)
{
	if (delayms > 200) {
		AUD_LOG_W("%s delay long %d\n", __func__, delayms);
		return;
	}
	vTaskDelay((portTickType) delayms / portTICK_RATE_MS);
}

void aud_task_delayus(unsigned int delayus)
{
	unsigned long long loopcount;

	// 1000000: us
	loopcount = ((unsigned long long)delayus * (configCPU_CLOCK_HZ / 1000000)) / NOP_CYCLE;
	while(loopcount){
		asm("nop");
		loopcount--;
	}
}

unsigned int get_peroid_ustime(unsigned int rate, unsigned int period)
{
	return get_peroid_nstime(rate, period) * 1000;
}

unsigned int get_peroid_nstime(unsigned int rate, unsigned int period)
{
	return get_peroid_mstime(rate, period) * 1000;
}


unsigned int get_peroid_mstime(unsigned int rate, unsigned int period)
{
	unsigned int peroid_ms = 0;

	if (rate > 0)
		peroid_ms = ((period * 1000) / rate);

	return peroid_ms;
}

int audio_size_to_ms(unsigned int size, unsigned int rate,
		     unsigned int channel, unsigned int format)
{
	int ret = 0;

	if (!size || !rate || !channel) {
		AUD_ASSERT((size != 0) && (rate != 0) && (channel != 0));
		return -1;
	}


	unsigned int framesize = getframesize(channel, format);
	unsigned int framenum = size/framesize;

	ret = (framenum * 1000) / rate;
	return ret;
}

unsigned long long get_time_stamp()
{
#if defined(CFG_MTK_AUDIODSP_SUPPORT) || defined(CFG_SCP_AUDIO_FW_SUPPORT)
	return read_xgpt_stamp_ns();
	//return timer_get_global_timer_tick();
#endif
	return 0;
}

int update_time_threshold(unsigned int id, unsigned long long thresholdns)
{
	if (id >= NUM_OF_TASK_TIME_ID) {
		AUD_LOG_E("%s(), id = %d \n", __func__, id);
		return -1;
	}
	AUD_LOG_V("%s(), id = %d thresholdns[%llu]\n", __func__, id, thresholdns);
	aud_time_profiler[id].threshold = thresholdns;
	return 0;
}


int init_time_interval(unsigned int id, unsigned long long thresholdns)
{
	if (id >= NUM_OF_TASK_TIME_ID) {
		AUD_LOG_E("%s(), id = %d \n", __func__, id);
		return -1;
	}
	memset((void *)&aud_time_profiler[id], 0, sizeof(struct aud_time_profile));
	aud_time_profiler[id].threshold = thresholdns;
	aud_time_profiler[id].time_interval_prev = 0;
	aud_time_profiler[id].time_interval_last = 0;
	return 0;
}

/* here t1 < t2 */
unsigned long long get_time_diff(unsigned long long t1, unsigned long long t2)
{
	if (t2 > t1)
		return t2 - t1;
	else {
		AUD_LOG_W("%s()time overflow ,t1 = %llu t2 = %llu\n", __func__, t1, t2);
		return (ULLONG_MAX - t1 + t2);
	}
}

int record_time_interval(unsigned int id)
{
	int ret = 0;
	if (id >= NUM_OF_TASK_TIME_ID) {
		AUD_LOG_E("%s(), id = %d \n", __func__, id);
		return -1;
	}

	if (aud_time_profiler[id].time_interval_prev == 0) {
		aud_time_profiler[id].time_interval_prev = get_time_stamp();
		return 0;
	} else {
		aud_time_profiler[id].time_interval_last = get_time_stamp();
		aud_time_profiler[id].time_diff = get_time_diff(
							  aud_time_profiler[id].time_interval_prev,
							  aud_time_profiler[id].time_interval_last) ;
		if (aud_time_profiler[id].time_diff > aud_time_profiler[id].threshold
		    &&  aud_time_profiler[id].threshold != 0) {
			AUD_LOG_D("%s id %d threshold %llu prev = %llu last =%llu diff_us = %llu ms = %llu\n",
				  __func__, id,
				  aud_time_profiler[id].threshold,
				  aud_time_profiler[id].time_interval_prev,
				  aud_time_profiler[id].time_interval_last,
				  aud_time_profiler[id].time_diff,
				  aud_time_profiler[id].time_diff / 1000000
				 );
			ret = 1;
		}
		/* record for next */
		aud_time_profiler[id].time_interval_prev =
			aud_time_profiler[id].time_interval_last;
	}
	return ret;
}

int stop_time_interval(unsigned int id)
{
	if (id >= NUM_OF_TASK_TIME_ID) {
		AUD_LOG_E("%s(), id = %d \n", __func__, id);
		return -1;
	}
	aud_time_profiler[id].time_interval_prev = 0;
	aud_time_profiler[id].time_interval_last = 0;
	return 0;
}

unsigned long long get_time_interval_diff(unsigned int id)
{
	if (id >= NUM_OF_TASK_TIME_ID) {
		AUD_LOG_E("%s(), id = %d \n", __func__, id);
		return -1;
	}
	return  aud_time_profiler[id].time_diff;
}

int get_bufsize_to_ms(struct buf_attr attr, int size)
{
	int size_in_ms = size * 1000;
	switch (attr.format) {
	case SNDRV_PCM_FORMAT_S8:
	case SNDRV_PCM_FORMAT_U8:
		break;
	case SNDRV_PCM_FORMAT_S16_LE:
	case SNDRV_PCM_FORMAT_S16_BE:
	case SNDRV_PCM_FORMAT_U16_LE:
	case SNDRV_PCM_FORMAT_U16_BE:
		size_in_ms = size_in_ms / 2;
		break;
	case SNDRV_PCM_FORMAT_S24_LE:
	case SNDRV_PCM_FORMAT_S24_BE:
	case SNDRV_PCM_FORMAT_U24_LE:
	case SNDRV_PCM_FORMAT_U24_BE:
	case SNDRV_PCM_FORMAT_S32_LE:
	case SNDRV_PCM_FORMAT_S32_BE:
	case SNDRV_PCM_FORMAT_U32_LE:
	case SNDRV_PCM_FORMAT_U32_BE:
		size_in_ms = size_in_ms / 4;
		break;
	default:
		break;
	}

	switch (attr.channel) {
	case 2:
		size_in_ms = size_in_ms / 2;
		break;
	default:
		break;
	}
	size_in_ms = size_in_ms / attr.rate;
	return size_in_ms;
}



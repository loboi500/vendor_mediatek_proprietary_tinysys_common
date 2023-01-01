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

#include <audio_ringbuf.h>
#include "audio_messenger_ipi.h"
#include "audio_task_attr.h"
#include "audio_task_common.h"
#include "audio_task_state.h"

static dl_peroid_cbk_t g_cb_func;
static struct aud_state aud_task_state[TASK_SCENE_SIZE];

/* dynamic adjust irq with playback task*/
int set_audio_state(AudioTask *task, unsigned int scene, unsigned int state)
{
	if (scene >= TASK_SCENE_SIZE || state > AUDIO_TASK_STATE_MAX) {
		AUD_LOG_W("%s() scene[%d] state[%d]\n", __func__, scene, state);
		return -1;
	}
	// AUD_LOG_D("%s() scene[%d] state[%d]\n", __func__, scene, state);
	task->state = state;
	aud_task_state[scene].state = state;
	return 0;
}

int get_audio_state(unsigned int scene)
{
	if (scene >= TASK_SCENE_SIZE) {
		AUD_LOG_W("%s() scene[%d]\n", __func__, scene);
		return -1;
	} else
		return  aud_task_state[scene].state;
}

bool music_task_need_stop(unsigned int scene)
{
	if (get_swmixer_source_id(scene) != AUD_SW_MIXER_TYPE_MUSIC)
		return false;
	if ((get_audio_state(TASK_SCENE_PRIMARY) != AUDIO_TASK_WORKING) &&
	    (get_audio_state(TASK_SCENE_DEEPBUFFER) != AUDIO_TASK_WORKING) &&
	    (get_audio_state(TASK_SCENE_MUSIC) == AUDIO_TASK_WORKING))
		return true;
	else
		return false;
}

bool music_task_need_start(unsigned int scene)
{
	if (get_swmixer_source_id(scene) != AUD_SW_MIXER_TYPE_MUSIC)
		return false;
	if (((get_audio_state(TASK_SCENE_PRIMARY) == AUDIO_TASK_WORKING) ||
	     (get_audio_state(TASK_SCENE_DEEPBUFFER) == AUDIO_TASK_WORKING)) &&
	    (get_audio_state(TASK_SCENE_MUSIC) != AUDIO_TASK_WORKING))
		return true;
	else
		return false;
}

bool deep_buffer_state_only()
{
	if (get_audio_state(TASK_SCENE_PRIMARY) == AUDIO_TASK_WORKING ||
	    (get_audio_state(TASK_SCENE_VOIP) == AUDIO_TASK_WORKING) ||
	    (get_audio_state(TASK_SCENE_FAST) == AUDIO_TASK_WORKING) ||
	    (get_audio_state(TASK_SCENE_FM_ADSP) == AUDIO_TASK_WORKING))
		return false;
	if (get_audio_state(TASK_SCENE_DEEPBUFFER) == AUDIO_TASK_WORKING ||
	    (get_audio_state(TASK_SCENE_PLAYBACK_MP3) == AUDIO_TASK_WORKING))
		return true;
	return false;
}

bool check_dl_state(void)
{
	if (get_audio_state(TASK_SCENE_PRIMARY) == AUDIO_TASK_WORKING ||
	    (get_audio_state(TASK_SCENE_DEEPBUFFER) == AUDIO_TASK_WORKING) ||
	    (get_audio_state(TASK_SCENE_FAST) == AUDIO_TASK_WORKING) ||
	    (get_audio_state(TASK_SCENE_VOIP) == AUDIO_TASK_WORKING))
		return true;
	return false;
}

/*
 * when dsp reset ,aud task have to wait other dl task stop done.
 * this situlation usually happen when audioserver die.
 * release cpu let low priority task execute.
 */
void wait_dl_task_stop()
{
	int loop_count = 50;
	while ((check_dl_state() == true) && (loop_count > 0)) {
		AUD_LOG_D("check_dl_state true\n");
		aud_task_delay(2);
		loop_count --;
	}
}

/* trace all active task , and get smallest interval */
static unsigned int cal_task_dl_interval()
{
	int i;
	unsigned int rate_base = 48000, ret = 0;
	unsigned int mul = 0, temp = 0;
	for (i = 0 ; i < TASK_SCENE_SIZE; i++) {
		/* ignore non-open task */
		if (aud_task_state[i].state < AUDIO_TASK_OPEN)
			continue;
		/* aud_playback task no need*/
		if (i == TASK_SCENE_AUDPLAYBACK)
			continue;
		mul = aud_task_state[i].audio_task_buf.aud_buffer.buffer_attr.rate / rate_base;
		temp = aud_task_state[i].audio_task_buf.aud_buffer.period_size * mul;
		if (temp == 0 || mul == 0)
			continue;
		/* using smallest peroid */
		if (temp < ret || ret == 0)
			ret = temp;
	}
	return ret;
}

void reg_dl_cb_period(dl_peroid_cbk_t cb_func)
{
	if (cb_func != NULL)
		g_cb_func = cb_func;
}

int update_audio_playabck_irq(void)
{
	unsigned int dl_interval = 0, refine_interval = 0;

	if (!g_cb_func)
		return 0;

	dl_interval = cal_task_dl_interval();
	refine_interval = hwirq_refine(dl_interval,
				       &aud_task_state[TASK_SCENE_AUDPLAYBACK].audio_task_buf);

	/* callback to aud_playback to modify */
	g_cb_func(refine_interval);
	AUD_LOG_D("%s() refine_interval[%u] dl_interval[%u]\n",
		  __func__, refine_interval, dl_interval);

	return 0;
}
/*
 * when task audio_hw_buffer is ready,
 * update period count and peroid size
 */
int set_audio_runtime_period(unsigned int scene,
			     struct audio_hw_buffer *audio_task_buf)
{
	if (scene >= TASK_SCENE_SIZE) {
		AUD_LOG_W("%s() scene[%d] \n", __func__, scene);
		return -1;
	}
	if (!audio_task_buf) {
		AUD_LOG_W("%s() audio_task_buf NULL \n", __func__);
		return -1;
	}

	/* copy audio_hw_buffer information */
	memcpy(&aud_task_state[scene].audio_task_buf, audio_task_buf,
	       sizeof(struct audio_hw_buffer));
	update_audio_playabck_irq();
	return 0;
}

/*
 * when task audio_hw_buffer is free,
 * release task information and update audio irq
 */
int clear_audio_runtime_period(unsigned int scene)
{
	if (scene >= TASK_SCENE_SIZE) {
		AUD_LOG_W("%s() scene[%d] \n", __func__, scene);
		return -1;
	}

	/* copy audio_hw_buffer information */
	memset(&aud_task_state[scene].audio_task_buf, 0,
	       sizeof(struct audio_hw_buffer));
	update_audio_playabck_irq();
	return 0;
}

/*
 *  when task buffer size is larger than allocate,
 *  adjust buffer size
 */
int hwparam_refine(struct AudioTask *task, struct audio_hw_buffer *audio_hwbuf)
{
	struct audio_task_common *task_common;
	uint32_t expect_out_size = 0;

	if (!task || !audio_hwbuf) {
		AUD_LOG_E("%s task[%p] audio_hwbuf[%p]\n", __func__, task, audio_hwbuf);
		return -1;
	}

	/* only refine with DL task */
	if (!task_is_dl(task->scene))
		return -1;

	task_common = task->task_priv;

	cal_output_buffer_size(
		&task_common->task_aurisys->dsp_config->attribute[DATA_BUF_DOWNLINK_IN],
		&task_common->task_aurisys->dsp_config->attribute[DATA_BUF_DOWNLINK_OUT],
		task_common->mtask_default_buffer_size, &expect_out_size);

	AUD_LOG_D("%s() expect_out_size[%u] mtask_default_buffer_size[%u]\n",
		  __func__, expect_out_size, task_common->mtask_default_buffer_size);

	/* buffer size is large than hw_param's ,set output buffer size as write size */
	if (expect_out_size <= task_common->mtask_default_buffer_size) {
		task_common->mtask_output_buf_size = expect_out_size;
		task_common->mtask_processing_buf_out_size = expect_out_size;
		return 0;
	}

	/* re-init buffer */
	if (task_common->mtask_processing_buf) {
		AUDIO_FREE(task_common->mtask_processing_buf);
		task_common->mtask_processing_buf = (char *)AUDIO_MALLOC(expect_out_size);
		task_common->mtask_processing_buf_out_size = expect_out_size;
	}

	return 0;
}

unsigned int hwirq_refine(unsigned int interval,
			  struct audio_hw_buffer *audio_hwbuf)
{
	if (!interval || !audio_hwbuf) {
		AUD_LOG_W("%s interval[%d] audio_hwbuf[%p] error, return\n",
			  __func__, interval, audio_hwbuf);
		return 0;
	}

	unsigned int period_count, period_size, max_size;
	unsigned int ret = 0;
	period_count = audio_hwbuf->aud_buffer.period_count;
	period_size = audio_hwbuf->aud_buffer.period_size;
	max_size = period_count * period_size;

	/* only max for ping pong buffer*/
	if (interval >= (max_size / 2))
		ret = (max_size / 2);
	else if (interval <= period_size)
		ret = period_size;
	else
		ret = (max_size / 4);

	AUD_LOG_D("%s interval[%d] max_size[%d] ret[%u]\n", __func__, interval,
		  max_size, ret);
	return ret;
}


bool task_is_dl(int task_scene)
{
	if (task_scene == TASK_SCENE_PRIMARY ||
	    task_scene == TASK_SCENE_DEEPBUFFER ||
	    task_scene == TASK_SCENE_PLAYBACK_MP3 ||
	    task_scene == TASK_SCENE_VOIP ||
	    task_scene == TASK_SCENE_FAST)
		return true;
	return false;
}

/* return true if can read iv data */
bool task_iv_read_cond(struct audio_task_common *task_common)
{
	if (!task_common) {
		AUD_LOG_E("%s() task_common == NULL", __func__);
		return false;
	}

	if (ref_handle_valid(task_common) &&
	    (task_common->mtask_iv_buf != NULL) &&
	    (task_common->task_aurisys->dsp_config != NULL) &&
	    task_common->task_aurisys->dsp_config->iv_on) {
		return true;
	}

	return false;
}

/* todo : add with ul path*/
bool task_is_ul(int task_scene)
{
	if (task_scene == TASK_SCENE_CAPTURE_UL1 ||
	    task_scene == TASK_SCENE_CAPTURE_RAW)
		return true;

	return false;
}

bool task_is_audplayback(int task_scene)
{
	if (task_scene == TASK_SCENE_AUDPLAYBACK)
		return true;
	return false;
}

bool task_is_enable(unsigned int task_scene)
{
	if (task_scene >= TASK_SCENE_SIZE) {
		ALOGE("%s task_scene[%d]\n ", __func__, task_scene);
		return false;
	}
	if (aud_task_state[task_scene].state ==  AUDIO_TASK_DEINIT ||
	    aud_task_state[task_scene].state ==  AUDIO_TASK_IDLE )
		return false;
	else
		return true;
}

int playback_handle_valid(struct audio_task_common *task_common)
{
	if (task_common && (task_common->playback_hwbuf_handle >= 0))
		return true;
	else
		return false;
}

int capture_handle_valid(struct audio_task_common *task_common)
{
	if (task_common && (task_common->capture_hwbuf_handle >= 0))
		return true;
	else
		return false;
}

int ref_handle_valid(struct audio_task_common *task_common)
{
	if (task_common && (task_common->ref_hwbuf_handle >= 0))
		return true;
	else
		return false;
}


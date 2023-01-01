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
#ifndef AUDIO_TASK_STATE_H
#define AUDIO_TASK_STATE_H

#include <stdio.h>
#include <audio_type.h>
#include <time.h>
#include <stdint.h>

struct aud_state {
	unsigned int state;
	struct audio_hw_buffer audio_task_buf;
};

typedef int (*dl_peroid_cbk_t)(unsigned int irq_cnt);

struct AudioTask;
struct audio_hw_buffer;

int set_audio_state(struct AudioTask *task, unsigned int scene, unsigned int state);
int get_audio_state(unsigned int scene);
bool check_dl_state(void);
void wait_dl_task_stop(void);
bool music_task_need_stop(unsigned int scene);
bool music_task_need_start(unsigned int scene);
bool deep_buffer_state_only(void);

/* audio irq realted*/
int set_audio_runtime_period(unsigned int scene,
			     struct audio_hw_buffer *audio_task_buf);
int clear_audio_runtime_period(unsigned int scene);
int hwparam_refine(struct AudioTask *task, struct audio_hw_buffer *audio_hwbuf);
unsigned int hwirq_refine(unsigned int interval,
			  struct audio_hw_buffer *audio_hwbuf);
void reg_dl_cb_period(dl_peroid_cbk_t cb_func);
int update_audio_playabck_irq(void);

bool task_is_dl(int task_scene);
bool task_is_ul(int task_scene);
bool task_is_audplayback(int task_scene);
bool task_iv_read_cond(struct audio_task_common *task_common);
bool task_is_enable(unsigned int task_scene);

/* check for task handle is open , handle is invalid with -1.*/
int playback_handle_valid(struct audio_task_common *task_common);
int capture_handle_valid(struct audio_task_common *task_common);
int ref_handle_valid(struct audio_task_common *task_common);

#endif // end of AUDIO_TASK_STATE_H

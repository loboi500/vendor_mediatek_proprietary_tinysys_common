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
#ifndef AUDIO_DUMMY_MIXER_H
#define AUDIO_DUMMY_MIXER_H

#include <stdio.h>
#include <audio_type.h>
#include <time.h>
#include <stdint.h>
#include <audio_ringbuf.h>

#ifdef AUDIO_DUMMY_SHARE_BUFFER
/* ---- audio task share buffer implementation --------------*/

typedef int (*sw_mixer_target_cbk_dummy_t)(
	void *buffer,
	uint32_t bytes,
	void *arg);

struct share_buffer_t {
	TaskHandle_t task;
	struct buf_attr attr;
	struct buf_attr attr_out;
	struct RingBuf buf;
	char *outputbuffer;
	int outputbuffersize;
	struct alock_t *share_buffer_lock;
	int source;
	int target;
	int target_out_size;
	/* swmixer callback function */
	sw_mixer_target_cbk_dummy_t sw_mixer_cb;
	void *cbk_arg;
};

struct sw_mixer_attr_dummy_t {
	struct buf_attr attr;
	unsigned int bufsize;
};

bool share_buffer_construct(void);
/* allocate share buffer with attr and size */
struct share_buffer_t *Init_share_buffer(struct AudioTask *task, int source,
					 int target, unsigned int buffer_size);
/* set share_buffer attr */
int Set_share_buffer_attr(struct share_buffer_t *buffer, struct buf_attr attr);
/* deinit share buffer with */
int Deinit_share_buffer(struct AudioTask *task, struct share_buffer_t *buffer);
/* write share buffer, if empty will be blocked */
int Write_share_buffer(struct AudioTask *task, struct share_buffer_t *buffer,
		       char *linear_buf, int size);
/* Read share buffer, return read size, non-blocking */
unsigned int Read_share_buffer(struct share_buffer_t *buffer, char *linear_buf,
			       unsigned int size);
/* return NULL if no source and target match*/
struct share_buffer_t *get_share_buffer(int source, int target);
/* ---- end of audio task share buffer implementation ----------*/

/* dummy ataach share buffer function */
void *sw_mixer_source_attach_dummy(const uint8_t id,
				   struct sw_mixer_attr_dummy_t *buffer_attr);
void sw_mixer_source_detach_dummy(void *param);
void sw_mixer_source_write_dummy(void *hdl, void *buffer, uint32_t bytes);

/* write this task for swxmier attach */
void *sw_mixer_target_attach_dummy(const uint8_t id,
				   struct sw_mixer_attr_dummy_t *buffer_attr,
				   sw_mixer_target_cbk_dummy_t cbk,
				   void *arg);

void sw_mixer_target_detach_dummy(void *hdl);
#endif


#endif // end of AUDIO_DUMMY_MIXER_H

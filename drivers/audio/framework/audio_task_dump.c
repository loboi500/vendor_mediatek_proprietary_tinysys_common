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
#include "audio_task_dump.h"


static bool send_task_dump_msg(uint8_t scene, char *buf, unsigned int size, unsigned int dump_flag)
{
	struct ipi_msg_t ipi_msg;

	if (!buf || !size)
		return false;

	audio_send_ipi_msg(&ipi_msg, scene,
			   AUDIO_IPI_LAYER_TO_HAL, AUDIO_IPI_DMA,
			   AUDIO_IPI_MSG_BYPASS_ACK,
			   AUDIO_DSP_TASK_PCMDUMP_DATA,
			   size,
			   dump_flag, //dump point serial number
			   buf);
	return true;
}

int task_dump_pcm(struct audio_task_common *task_common,
		  uint8_t scene, char *buf, unsigned int size, unsigned int dump_flag)
{
	if (!task_common) {
		return 0;
	}
	if (dump_flag >= DEBUG_PCMDUMP_NUM) {
		AUD_LOG_E("%s dump_flag[%d]\n ", __func__, dump_flag);
		return 0;
	}

	// pcm dump for debug
	if (task_common->pcmdump_enable == 1) {
		if (task_common->dump_inst) {
			struct RingBuf *dump_buf = task_common->dump_inst->audio_dump_buf[dump_flag];
			unsigned int dump_bound = task_common->dump_inst->dump_bound[dump_flag];
			if (dump_buf == NULL) {
				AUD_LOG_E("%s dump_buf NULL\n ", __func__);
				return 0;
			}
			// copy data to dump buffer
			if (RingBuf_getFreeSpace(dump_buf) >= size)
				RingBuf_copyFromLinear(dump_buf, buf, size);
			else {
				// clear and send task data buffer.
				send_task_dump_msg(scene, dump_buf->pBufBase, RingBuf_getDataCount(dump_buf), dump_flag);
				RingBuf_Reset(dump_buf);
				if (RingBuf_getFreeSpace(dump_buf) >= size)
					RingBuf_copyFromLinear(dump_buf, buf, size);
				else
					send_task_dump_msg(scene, buf, size, dump_flag); // direct send buf to dump
			}
			//  clear and send task data buffer.
			if (RingBuf_getDataCount (dump_buf) >= dump_bound) {
				send_task_dump_msg(scene, dump_buf->pBufBase, RingBuf_getDataCount(dump_buf), dump_flag);
				RingBuf_Reset(dump_buf);
			}
		} else
			send_task_dump_msg(scene, buf, size, dump_flag);
	}
	return 0;
}

bool init_audio_dump_inst(struct task_dump_inst *dump_inst, unsigned int size)
{
	unsigned int i = 0;

	if (dump_inst == NULL)
		return false;

	for (i = 0; i < DEBUG_PCMDUMP_NUM; i++)
		set_audio_dump_size(dump_inst, i, size);

	return true;
}

bool set_audio_dump_size(struct task_dump_inst *dump_inst, unsigned int dump_point, unsigned int size)
{
	if (dump_inst == NULL)
		return false;
	if (dump_point >= DEBUG_PCMDUMP_NUM)
		return false;

	dump_inst->dump_bound[dump_point] = size;
	return true;
}

bool init_audio_dump_buf(struct task_dump_inst *dump_inst)
{
	if (dump_inst == NULL)
		return false;

	unsigned int i = 0;
	struct RingBuf *temp_dump_buf;
	for (i = 0; i < DEBUG_PCMDUMP_NUM; i++) {
		if (dump_inst->audio_dump_buf[i] != NULL) {
			AUD_LOG_W("%s [%u] allocated", __func__, i);
			continue;
		}
		if (dump_inst->dump_bound[i] == 0 ){
			AUD_LOG_W("%s[%u]  size[%u] ", __func__, i, dump_inst->dump_bound[i]);
			continue;
		}
		dump_inst->audio_dump_buf[i] = AUDIO_MALLOC(sizeof(struct RingBuf));
		temp_dump_buf = dump_inst->audio_dump_buf[i];
		memset(temp_dump_buf, 0, sizeof(struct RingBuf));
		// twice buffer size
		temp_dump_buf->pBufBase = AUDIO_MALLOC(dump_inst->dump_bound[i] * 2);
		init_ring_buf(temp_dump_buf, temp_dump_buf->pBufBase, dump_inst->dump_bound[i]);
		AUD_LOG_D("%s [%u] allocated size[%u] ", __func__, i,  dump_inst->dump_bound[i]);
	}
	return true;
}

bool deinit_audio_dump_buf(struct task_dump_inst *dump_inst)
{
	if (dump_inst == NULL)
		return false;

	unsigned int i = 0;
	struct RingBuf *temp_dump_buf;
	for (i = 0; i < DEBUG_PCMDUMP_NUM; i++) {
		if (dump_inst->audio_dump_buf[i] == NULL) {
			AUD_LOG_W("%s [%u] not allocated", __func__, i);
			continue;
		}
		temp_dump_buf = dump_inst->audio_dump_buf[i];
		if (temp_dump_buf->pBufBase) {
			AUDIO_FREE(temp_dump_buf->pBufBase);
			temp_dump_buf->pBufBase = NULL;
		}
		if (dump_inst->audio_dump_buf[i] != NULL) {
			AUDIO_FREE(dump_inst->audio_dump_buf[i]);
			dump_inst->audio_dump_buf[i] = NULL;
		}
		AUD_LOG_D("%s free i = [%u]", __func__, i);
	}
	return true;
}

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
#if defined(CFG_MTK_AUDIODSP_SUPPORT)
#include <xgpt.h>
#endif
#include <audio_type.h>
#include <audio_ringbuf.h>
#include "audio_task_factory.h"
#include "audio_log.h"
#include "audio_hw.h"
#include "audio_task_utility.h"
#include "audio_task_interface.h"
#include "audio_task_common.h"
#include "audio_dsp_hw_hal.h"
#include "audio_messenger_ipi.h"
#include "audio_task_attr.h"
#include <wrapped_audio.h>
#include <limits.h>
#include <audio_sw_mixer.h>
#include <audio_dsp_hw_hal.h>
#include <audio_pipe.h>
#include <sem.h>
#include <time.h>
#include <aurisys_config.h>

#ifdef CFG_VCORE_DVFS_SUPPORT
#include <dvfs.h>
#endif

#ifdef CFG_AUDIO_SUPPORT
#include <dma_api.h>
#endif

#ifdef CFG_AUDIO_SUPPORT
#if defined(CFG_MTK_AUDIODSP_SUPPORT)
#include <adsp_ipi.h>

#if defined(C2C_IPC_SUPPORT) && !defined(VOIP_SEPARATE_CORE)
#include "audio_voip_c2c_client.h"
#endif
#endif
#include <audio_ipi_dma.h>
#else
#include <scp_ipi.h>
#endif

#ifndef true
#define true 1
#endif

#ifndef false
#define false 0
#endif

#define PULSE_SHIFT_24BITS (5)
#define PULSE_SHIFT_32BITS (13)

#ifdef CFG_SEMAPHORE_3WAY_SUPPORT
#define adsp_semaphore_get(flags) semaphore_get_3way(flags)
#define adsp_semaphore_release(flags) semaphore_release_3way(flags)
#else
#define adsp_semaphore_get(flags) semaphore_get(flags)
#define adsp_semaphore_release(flags) semaphore_release(flags)
#endif

uint32_t doDownMix(void *linear_buf,
		   uint32_t data_size,
		   uint32_t audio_format,
		   uint32_t source_ch,
		   uint32_t target_ch)
{
	if (audio_format == AUDIO_FORMAT_PCM_16_BIT)
		return LINEAR_MCH_TO_NCH(linear_buf, data_size, int16_t, source_ch, target_ch);

	else if (audio_format == AUDIO_FORMAT_PCM_32_BIT ||
		 audio_format == AUDIO_FORMAT_PCM_8_24_BIT)
		return LINEAR_MCH_TO_NCH(linear_buf, data_size, int32_t, source_ch, target_ch);
	else
		return 0;
}

#ifdef CFG_MTK_AUDIODSP_SUPPORT
/* semaphore with critical section */
int scp_semaphore_get_3way(unsigned int flags)
{
#ifdef CFG_USE_SCP_SEMAPHORE
	unsigned int val, cnt;

	/* slot 0: dsp core 0 (for scp)
	 * slot 1: dsp core 1 (for adsp)
	 * slot 2: AP
	 */
	flags *= SCP_SEM_3WAY_BITS;
	flags++;

	val = (DRV_Reg32(SCP_SEMAPHORE_3WAY_REG) >> flags) & 0x1;
	if (val == 0) {
		cnt = SEMA_TIMEOUT;

		while (cnt-- > 0) {
			DRV_WriteReg32(SCP_SEMAPHORE_3WAY_REG, (1 << flags));

			val = (DRV_Reg32(SCP_SEMAPHORE_3WAY_REG) >> flags) & 0x1;

			if (val == 1)
				return 1;
		}
	}
	return 0;
#else
	return 1;
#endif
}

int scp_semaphore_release_3way(int flags)
{
#ifdef CFG_USE_SCP_SEMAPHORE
	unsigned int val;

	/* slot 0: dsp core 0 (for scp)
	 * slot 1: dsp core 1 (for adsp)
	 * slot 2: AP
	 */
	flags *= SCP_SEM_3WAY_BITS;
	flags++;

	val = (DRV_Reg32(SCP_SEMAPHORE_3WAY_REG) >> flags) & 0x1;

	if (val == 1) {
		DRV_WriteReg32(SCP_SEMAPHORE_3WAY_REG, (1 << flags));
		return 1;
	}
	return 0;
#else
	return 1;
#endif
}

int audio_semaphore_get_3way(int flag, unsigned int retry_count, unsigned int waitms)
{
	int ret = 0;
	int scp_sem_ret = 0;
	UBaseType_t uxSavedInterruptStatus;

	do {
		uxSavedInterruptStatus = portSET_INTERRUPT_MASK_FROM_ISR();
		if (!ret)
			ret = adsp_semaphore_get(flag);
		if (ret)
			scp_sem_ret = (flag == SEMA_3WAY_AUDIOREG) ?
				      scp_semaphore_get_3way(flag) : 1;

		if (ret && scp_sem_ret) {
			portCLEAR_INTERRUPT_MASK_FROM_ISR(uxSavedInterruptStatus);
			break;
		} else {
			if (ret) {
				adsp_semaphore_release(flag);
				ret = 0;
			}
			portCLEAR_INTERRUPT_MASK_FROM_ISR(uxSavedInterruptStatus);

			vTaskDelay((waitms) / portTICK_RATE_MS);
		}
	} while(retry_count--);
	return ret;
}
int audio_semaphore_release_3way(int flag)
{
	int ret = 0;
	int scp_sem_ret = 0;
	UBaseType_t uxSavedInterruptStatus;
	uxSavedInterruptStatus = portSET_INTERRUPT_MASK_FROM_ISR();

	scp_sem_ret = (flag == SEMA_3WAY_AUDIOREG) ?
		      scp_semaphore_release_3way(flag) : 1;

	ret = adsp_semaphore_release(flag);

	portCLEAR_INTERRUPT_MASK_FROM_ISR(uxSavedInterruptStatus);
	if (!ret || !scp_sem_ret)
		AUD_LOG_E("%s() err flag[%d] ret[%d] scp_sem_ret[%d]\n",
			  __func__, flag, ret, scp_sem_ret);
	return ret;
}
#endif

uint32_t trans_sndrv_fmt_to_audio_fmt(const unsigned int sndrv_pcm_format)
{
	uint32_t audio_format = AUDIO_FORMAT_DEFAULT;

	switch (sndrv_pcm_format) {
	case SNDRV_PCM_FORMAT_S8:
	case SNDRV_PCM_FORMAT_U8:
		audio_format = AUDIO_FORMAT_PCM_8_BIT;
		break;
	case SNDRV_PCM_FORMAT_S16_LE:
	case SNDRV_PCM_FORMAT_S16_BE:
	case SNDRV_PCM_FORMAT_U16_LE:
	case SNDRV_PCM_FORMAT_U16_BE:
		audio_format = AUDIO_FORMAT_PCM_16_BIT;
		break;
	case SNDRV_PCM_FORMAT_S24_LE:
	case SNDRV_PCM_FORMAT_S24_BE:
	case SNDRV_PCM_FORMAT_U24_LE:
	case SNDRV_PCM_FORMAT_U24_BE:
		audio_format = AUDIO_FORMAT_PCM_8_24_BIT;
		break;
	case SNDRV_PCM_FORMAT_S32_LE:
	case SNDRV_PCM_FORMAT_S32_BE:
	case SNDRV_PCM_FORMAT_U32_LE:
	case SNDRV_PCM_FORMAT_U32_BE:
		audio_format = AUDIO_FORMAT_PCM_32_BIT;
		break;
	default:
		AUD_LOG_E("%s(), sndrv_pcm_format %u error\n", __func__,
			  sndrv_pcm_format);
		AUD_ASSERT(0);
		audio_format = AUDIO_FORMAT_PCM_8_24_BIT;
	}

	return audio_format;
}

uint32_t trans_audio_fmt_to_sndrv_fmt(const unsigned int audio_fmt)
{
	uint32_t sndrv_fmt;

	switch (audio_fmt) {
	case AUDIO_FORMAT_PCM_8_BIT:
		return SNDRV_PCM_FORMAT_S8;
	case AUDIO_FORMAT_PCM_16_BIT:
		return SNDRV_PCM_FORMAT_S16_LE;
	case AUDIO_FORMAT_PCM_8_24_BIT:
		return SNDRV_PCM_FORMAT_S24_LE;
	case AUDIO_FORMAT_PCM_32_BIT:
		return SNDRV_PCM_FORMAT_S32_LE;
	default:
		AUD_LOG_E("%s(), audio_fmt %u error\n", __func__,
			  audio_fmt);
		AUD_ASSERT(0);
		return AUDIO_FORMAT_PCM_8_24_BIT;
	}
	return sndrv_fmt;
}

int aud_task_check_state(struct AudioTask *task, unsigned int state)
{
	if (task->state != state) {
		AUD_LOG_E("scene = %d Unexpected start with status=%d\n", task->scene,
			  task->state);
		return -1;
	}
	return 0;
}

unsigned int frame_to_bytes(unsigned int framecount, unsigned int channels,
			    unsigned int format)
{
	framecount = framecount * channels;

	switch (format) {
	case SNDRV_PCM_FORMAT_S8:
	case SNDRV_PCM_FORMAT_U8:
		return framecount;
	case SNDRV_PCM_FORMAT_S16_LE:
	case SNDRV_PCM_FORMAT_S16_BE:
	case SNDRV_PCM_FORMAT_U16_LE:
	case SNDRV_PCM_FORMAT_U16_BE:
		return framecount * 2;
	case SNDRV_PCM_FORMAT_S24_LE:
	case SNDRV_PCM_FORMAT_S24_BE:
	case SNDRV_PCM_FORMAT_U24_LE:
	case SNDRV_PCM_FORMAT_U24_BE:
	case SNDRV_PCM_FORMAT_S32_LE:
	case SNDRV_PCM_FORMAT_S32_BE:
	case SNDRV_PCM_FORMAT_U32_LE:
	case SNDRV_PCM_FORMAT_U32_BE:
		return framecount * 4;
	default:
		return framecount;
	}
	return framecount;
}

unsigned int get_audio_write_size(struct audio_hw_buffer *audio_hwbuf)
{
	/* period size is write size */
	int framesize = frame_to_bytes(
				audio_hwbuf->aud_buffer.period_size,
				audio_hwbuf->aud_buffer.buffer_attr.channel,
				audio_hwbuf->aud_buffer.buffer_attr.format
			);
	return framesize;
}

unsigned int get_audio_memif_size(struct audio_hw_buffer *audio_hwbuf)
{
	int framesize = frame_to_bytes(
				audio_hwbuf->aud_buffer.period_size,
				audio_hwbuf->aud_buffer.buffer_attr.channel,
				audio_hwbuf->aud_buffer.buffer_attr.format
			) * audio_hwbuf->aud_buffer.period_count;
	return framesize;
}

/**
 * Clamp (aka hard limit or clip) a signed 32-bit sample to 16-bit range.
 */
static inline short clamp16(int sample)
{
    if ((sample>>15) ^ (sample>>31))
        sample = 0x7FFF ^ (sample>>31);
    return sample;
}

/**
 * Clamp (aka hard limit or clip) a signed 64-bit sample to 32-bit range.
 */
static inline int clamp32(long long sample)
{
    if ((sample>>31) ^ (sample>>63))
        sample = 0x7fffffff ^ (sample>>63);
    return sample;
}


int volume_process(void *pInBuffer, unsigned int size, unsigned int channel, unsigned int format,
		   unsigned short volume_src, unsigned short volume_tar)
{
	unsigned int framenum, samplesize, framesize;
	int *pbuffer32;
	short *pbuffer16;
	unsigned short volumecur;
	short volumestep;
	long long mulvalue64;
	int mulvalue32;

	if(!pInBuffer)
		AUD_ASSERT(0);
	if(!size)
		AUD_ASSERT(0);
	if(!channel)
		AUD_ASSERT(0);

	samplesize = getsamplesize(format);
	framesize = getframesize(channel, format);
	framenum = size / framesize;
	volumecur = volume_src;

	/* one frame volume step */
	volumestep = (volume_tar - volume_src)/framenum;
	if (volume_tar < volume_src)
		volumestep = volumestep * -1;
	AUD_LOG_D("%s volume_src[%d] volume_tar[%d] volumestep[%d] samplesize[%d] framesize[%d] framenum[%d] size[%d] format[%d]",
		  __func__, volume_src, volume_tar, volumestep, samplesize, framesize, framenum, size, format);

	if (framenum == 0) {
		AUD_LOG_D("%s framenum[%d]", __func__, framenum);
		AUD_ASSERT(0);
	}

	if (samplesize == 2) {
		pbuffer16 = (short*)pInBuffer;
	} else if (samplesize == 4) {
		pbuffer32 = (int*)pInBuffer;
	} else {
		AUD_ASSERT(0);
	}

	// process frame.
	while(framenum) {
		unsigned int channel_num = channel;
		while(channel_num) {
			if (samplesize == 4) {
				mulvalue64 = ((long long)*pbuffer32 * volumecur);
				mulvalue64 = mulvalue64 / AUDIO_VOLUME_MAX;
				*pbuffer32 = (int)clamp32(mulvalue64);
				pbuffer32++;
			} else if (samplesize == 2) {
				mulvalue32 = ((int)*pbuffer16 * volumecur);
				mulvalue32 = mulvalue32 / AUDIO_VOLUME_MAX;
				*pbuffer16 = (short)clamp16(mulvalue32);
				pbuffer16++;
			}
			channel_num--;
		}
		framenum--;
		volumecur+= volumestep;
	}

	return 0;
}

/* dma buffer is a pointer to audiohwbuffer*/
int get_audiobuf_from_msg(struct ipi_msg_t *ipi_msg,
			  struct audio_hw_buffer *task_audio_buf)
{
	if (ipi_msg == NULL)
		return -1;
	if (task_audio_buf == NULL)
		return -1;

	/* get buffer */
#if defined(CFG_MTK_AUDIODSP_SUPPORT)
	memcpy(task_audio_buf, (char *)ap_to_scp(*(uint64_t *)ipi_msg->payload),
	       sizeof(struct audio_hw_buffer));
#elif defined(CFG_SCP_AUDIO_FW_SUPPORT)
	memcpy(task_audio_buf, (char *)(uint32_t)ap_to_scp(*(uint32_t *)ipi_msg->payload),
	       sizeof(struct audio_hw_buffer));
#endif
	return 0;
}

unsigned int getsamplesize(unsigned int format)
{
	int size = 0;
	switch (format) {
	case SNDRV_PCM_FORMAT_S8:
	case SNDRV_PCM_FORMAT_U8:
		size = 1;
		break;
	case SNDRV_PCM_FORMAT_S16_LE:
	case SNDRV_PCM_FORMAT_S16_BE:
	case SNDRV_PCM_FORMAT_U16_LE:
	case SNDRV_PCM_FORMAT_U16_BE:
		size = 2;
		break;
	case SNDRV_PCM_FORMAT_S24_LE:
	case SNDRV_PCM_FORMAT_S24_BE:
	case SNDRV_PCM_FORMAT_U24_LE:
	case SNDRV_PCM_FORMAT_U24_BE:
		size = 4;
		break;
	case SNDRV_PCM_FORMAT_S32_LE:
	case SNDRV_PCM_FORMAT_S32_BE:
	case SNDRV_PCM_FORMAT_U32_LE:
	case SNDRV_PCM_FORMAT_U32_BE:
		size = 4;
		break;
	default:
		AUD_LOG_W("%s unsupport format(%d)\n", __func__, format);
	}
	return size;
}


unsigned int getframesize(unsigned int channel, unsigned int format)
{
	return channel * getsamplesize(format);
}

void DumpTaskMsg(char *appendstring, struct ipi_msg_t *ipi_msg)
{
	AUD_LOG_D("%s(), ts = %llu Msg =0x%x, 0x%x, 0x%x dma = 0x%x\n",
		  appendstring, get_time_stamp(), ipi_msg->msg_id, ipi_msg->param1,
		  ipi_msg->param2,
		  (unsigned int)ipi_msg->dma_addr);
	return ;
}

int enter_write_cond(struct AudioTask *task)
{
	struct audio_task_common *task_common = task->task_priv;
	struct RingBuf *pRingBuf = &task_common->mtask_audio_ring_buf;
	unsigned int default_write_size = task_common->mtask_default_buffer_size;
	unsigned int write_size = get_audio_write_size(&task_common->maudio_dlin_buf);

	/* no data to write */
	if (RingBuf_getDataCount(pRingBuf) < default_write_size) {
		AUD_LOG_D("%s, underflow, written_size[%d] datacount[%d] task_name[%s]\n",
			  __func__,
			  default_write_size,
			  RingBuf_getDataCount(pRingBuf), task_common->task_name);
		return BUF_STATE_UNDERFLOW;
	}

	/* return false if buffer is not filled up */
	if ((RingBuf_getFreeSpace(pRingBuf) >= write_size) && task_common->loop_count) {
		if (RingBuf_getFreeSpace(pRingBuf) > write_size)
			AUD_LOG_D("%s(), %s buffer is decreasing, RingBuf_FreeSpace[%u] write_size[%u] bufLen[%d]\n",
				  __func__, task_common->task_name,
				  RingBuf_getFreeSpace(pRingBuf),
				  write_size, pRingBuf->bufLen);

		return BUF_STATE_NOT_ENOUGH;
	}

	return BUF_STATE_ENOUGH;
}

int enter_read_cond(int memif,
		    struct AudioTask *task,
		    struct audio_hw_buffer *audio_hwbuf,
		    struct audio_hw_buffer *audio_afebuf,
		    struct RingBuf *pRingBuf,
		    unsigned int processing_in_size,
		    unsigned int processing_out_size)
{
	if (memif < 0) {
		AUD_LOG_D("%s hw_buf_handle[%d]\n", __func__,
			  memif);
		return false;
	}

	if (task->state != AUDIO_TASK_WORKING) {
		AUD_LOG_D("%s handle state[%d]\n", __func__, task->state);
		return false;
	}


	/* check ring buffer space */
	if (RingBuf_getFreeSpace(pRingBuf) < processing_out_size) {
		AUD_LOG_D("%s processing_out_size[%d] datacount[%d]\n", __func__,
			  processing_out_size,
			  RingBuf_getDataCount(pRingBuf));
		return false;
	}

	/* check audio_pipe buffer data count*/
	if (audio_pipe_get_avail(task, memif) < (int)processing_in_size) {
		AUD_LOG_D("%s memif[%d] avail[%u] processing_in_size[%u] task->state =%u\n",
			  __func__, memif,
			  audio_pipe_get_avail(task, memif),
			  processing_in_size,
			  task->state);
		return false;
	}

	return true;
}

int first_write_cond(int handle,
		     struct AudioTask *task,
		     struct audio_hw_buffer *audio_hwbuf,
		     struct audio_hw_buffer *audio_afebuf,
		     struct RingBuf *pRingBuf,
		     unsigned int written_size)
{
	bool ret = false;

	/* no data to write */
	if (RingBuf_getDataCount(pRingBuf) < written_size) {
		AUD_LOG_V("%s written_size[%d] datacount[%d]\n", __func__,
			  written_size,
			  RingBuf_getDataCount(pRingBuf));
		return false;
	}

	if (audio_dsp_hw_getavail(handle) <= (int)get_audio_write_size(audio_afebuf)) {
		AUD_LOG_V("%s avail[%u] get_audio_write_size[%u]\n",
			  __func__,
			  audio_dsp_hw_getavail(handle),
			  get_audio_write_size(audio_hwbuf));
		ret = false;
	}

	/* when start condition*/
	if (task->state == AUDIO_TASK_WORKING &&
	    (audio_dsp_hw_status(task, audio_afebuf) & HW_STATE_RUN))
		return true;

	return ret;
}


/*
 * return scene number base on audio irq num
 */
int adsp_audio_irq_entry()
{
	int irqnum = adsp_audio_irq_handler();
	int sceneid = get_audio_scene_by_irqline(irqnum);
	return sceneid;
}

unsigned int get_data_queue_idx(struct AudioTask *this,  unsigned int max_queue_size)
{
	unsigned int queue_idx = this->data_queue_idx;

	this->data_queue_idx++;
	if (this->data_queue_idx == max_queue_size)
		this->data_queue_idx = 0;

	return queue_idx;
}

unsigned int get_queue_idx(struct AudioTask *this,  unsigned int max_queue_size)
{
	unsigned int queue_idx = this->queue_idx;

	this->queue_idx++;
	if (this->queue_idx == max_queue_size)
		this->queue_idx = 0;

	return queue_idx;
}

int send_task_message(struct AudioTask *this, struct ipi_msg_t *ipi_msg,
		      unsigned int max_queue_size)
{
	unsigned int queue_idx = 0, i = 0;

	taskENTER_CRITICAL();
	queue_idx = get_queue_idx(this, max_queue_size);
	this->num_queue_element++;
	taskEXIT_CRITICAL();

	if (this->msg_array[queue_idx].magic != 0) {
		AUD_LOG_E("%s(), task:%s queue_idx:%d, this:%p\n",
			  __func__, pcTaskGetName(this->freertos_task), queue_idx, this);
		for (i = 0; i < max_queue_size; i++)
			AUD_LOG_E("msg_array[%d] = 0x%x\n", i, this->msg_array[i].msg_id);
		AUD_ASSERT(this->msg_array[queue_idx].magic == 0); /* item is clean */
	}

	if (ipi_msg)
		memcpy(&this->msg_array[queue_idx], ipi_msg, sizeof(ipi_msg_t));

	if (xQueueSendToBack(this->msg_idx_queue, &queue_idx, 20) != pdPASS) {
		AUD_LOG_E("xQueueSendToBack msg_idx_queue[%p] fail\n", this->msg_idx_queue);
		return UNKNOWN_ERROR;
	}

	return 0;
}

int send_task_message_fromisr(struct AudioTask *this, struct ipi_msg_t *ipi_msg,
			      unsigned int max_queue_size)
{
	unsigned char queue_idx = 0;
	unsigned int i = 0;
	BaseType_t xHigherPriorityTaskWoken = pdFALSE;

	queue_idx = get_queue_idx(this, max_queue_size);
	this->num_queue_element++;

	if (this->msg_array[queue_idx].magic != 0) {
		AUD_LOG_E("%s(), task:%s queue_idx:%d, this:%p\n",
			  __func__, pcTaskGetName(this->freertos_task), queue_idx, this);
		for (i = 0; i < max_queue_size; i++)
			AUD_LOG_E("msg_array[%d] = 0x%x\n", i, this->msg_array[i].msg_id);
		AUD_ASSERT(this->msg_array[queue_idx].magic == 0); /* item is clean */
	}

	if (ipi_msg)
		memcpy(&this->msg_array[queue_idx], ipi_msg, sizeof(ipi_msg_t));

	if (xQueueSendToBackFromISR(this->msg_idx_queue, &queue_idx,
				    &xHigherPriorityTaskWoken) != pdPASS) {
		AUD_LOG_E("xQueueSendToBackFromISR msg_idx_queue[%p] fail\n", this->msg_idx_queue);
		return UNKNOWN_ERROR;
	}

	portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
	return 0;
}

bool is_no_restart(struct audio_buffer *aud_buffer)
{
	if (aud_buffer == NULL)
		return false;

	if (aud_buffer->stop_threshold > aud_buffer->start_threshold) {
		return true;
	}
	return false;
}


/* example only: place porcessing */
int task_do_processing(char *inputbuffer, int inputbuffersize,
		       char *outputbuffer, int *outputbuffersize,
		       char *referencebuffer, int referencebuffersize)
{
	*outputbuffersize =  inputbuffersize;
	memcpy(outputbuffer, inputbuffer, inputbuffersize);
	ALOGD("%s referencebuffer = %p referencebuffersize = %d\n", __func__,
	      referencebuffer, referencebuffersize);
	return 0;
}

#ifdef MTK_AURISYS_FRAMEWORK_SUPPORT
/* data alignment */
unsigned int aurisys_align(audio_ringbuf_t *ringbuf,
			   uint32_t expect_out_size)
{
	uint32_t data_size_align = audio_ringbuf_count(ringbuf);
	if (data_size_align != expect_out_size) {
		AUD_LOG_W("data_size_align = %d, expect_out_size = %d",
			  data_size_align,
			  expect_out_size);
		if (data_size_align > expect_out_size)
			data_size_align = expect_out_size;
	}
	data_size_align &= (~0x7F);

	return data_size_align;
}

void audio_dsp_aurisys_dl_process(char *data_buf, uint32_t processing_size, void *param)
{
	AudioTask *task = (AudioTask *)param;
	struct audio_task_common *task_common = task->task_priv;
	uint32_t expect_out_size = 0, data_size_align;

	/* pcm dump */
	task_dump_pcm(task_common, task->scene, data_buf, processing_size, DEBUG_PCMDUMP_IN);

	if (bypass_aurisys_process(task_common->task_aurisys)) {
		task_common->mtask_output_buf = data_buf;
		task_common->mtask_output_buf_size = processing_size;

		/* pcm dump */
		task_dump_pcm(task_common, task->scene, task_common->mtask_output_buf,
			      task_common->mtask_output_buf_size, DEBUG_PCMDUMP_OUT);
		return;
	}

	audio_check_buffer(task_common,
			   AURI_OUTPUTBUF | AURI_LINEAROUT | AURI_POOLBUF_DLOUT);

	cal_output_buffer_size(
		&task_common->task_aurisys->dsp_config->attribute[DATA_BUF_DOWNLINK_IN],
		&task_common->task_aurisys->dsp_config->attribute[DATA_BUF_DOWNLINK_OUT],
		processing_size, &expect_out_size);

	AUD_ASSERT(expect_out_size != 0);

	audio_pool_buf_copy_from_linear(
		task_common->task_aurisys->mAudioPoolBufDlIn, data_buf, processing_size);

#ifdef MCPS_PROF_SUPPORT
	mcps_prof_start(&task_common->mcps_profile);
#endif
	/* post processing + SRC + Bit conversion */
	aurisys_process_dl_only(task_common->task_aurisys->manager,
				task_common->task_aurisys->mAudioPoolBufDlIn,
				task_common->task_aurisys->mAudioPoolBufDlOut);

#ifdef MCPS_PROF_SUPPORT
	mcps_prof_stop(&task_common->mcps_profile, SCENE_DL);
#endif
	data_size_align = aurisys_align(&task_common->task_aurisys->mAudioPoolBufDlOut->ringbuf,
					expect_out_size);

	audio_pool_buf_copy_to_linear(
		&task_common->task_aurisys->mLinearOut->p_buffer,
		&task_common->task_aurisys->mLinearOut->memory_size,
		task_common->task_aurisys->mAudioPoolBufDlOut,
		data_size_align);
	task_common->mtask_output_buf =	task_common->task_aurisys->mLinearOut->p_buffer;
	task_common->mtask_output_buf_size = data_size_align;

	audio_check_buffer(task_common,
			   AURI_OUTPUTBUF | AURI_LINEAROUT | AURI_POOLBUF_DLOUT);

	/* pcm dump */
	task_dump_pcm(task_common, task->scene, task_common->mtask_output_buf,
		      task_common->mtask_output_buf_size, DEBUG_PCMDUMP_OUT);
}

int audio_dsp_aurisys_process_dl_to_sink(void *buf, uint32_t size, void *param)
{
	int written = 0, ret = 0, loopcount = 0;
	uint32_t processing_size;
	AudioTask *task = (AudioTask *)param;
	struct audio_task_common *task_common = task->task_priv;
	unsigned int consume_time;
	char *data_buf = (char *)buf;

	if (!buf || size == 0) {
		AUD_LOG_E("%s() buf[%p] size[%u]\n",  __FUNCTION__, buf, size);
		return 0;
	}

	// do aurisys processing and writing to sink by default buffer size per time
	// for better DL latency.
	processing_size = task_common->mtask_default_buffer_size;
	loopcount = (size / processing_size) + 1;

	LOCK_ALOCK_MS(task_common->wb_lock, 1000);
	do {
		/* left data to be process */
		if (processing_size > size)
			processing_size = size;
		/* read reference data */
		if (task_iv_read_cond(task_common)) {
			ret = audio_pipe_read(task,
					      task_common->mtask_audio_ref_buf.audio_memiftype,
					      task_common->mtask_iv_buf,
					      task_common->mtask_iv_buf_size);
			if (ret != task_common->mtask_iv_buf_size) {
				AUD_LOG_E("%s() audio_pipe_read iv fail ret [%d]\n", __FUNCTION__,
					  ret);
			}
			task_dump_pcm(task_common, task->scene, task_common->mtask_iv_buf,
				      task_common->mtask_iv_buf_size, DEBUG_PCMDUMP_IV);

			if (NULL != task_common->task_aurisys->mAudioPoolBufDlIV)
				audio_pool_buf_copy_from_linear(
					task_common->task_aurisys->mAudioPoolBufDlIV,
					task_common->mtask_iv_buf,
					task_common->mtask_iv_buf_size);
		} else {
			/* if iv_on, but can't read iv from pipe (unopened),
			   copy zero data to mAudioPoolBufDlIV to avoid IV underflow */
			if (NULL != task_common->mtask_iv_buf &&
			    NULL != task_common->task_aurisys->mAudioPoolBufDlIV) {
				AUD_LOG_W("%s() audio_pipe unopened, copy zero to mAudioPoolBufDlIV\n", __FUNCTION__);
				memset(task_common->mtask_iv_buf, 0, task_common->mtask_iv_buf_size);
				audio_pool_buf_copy_from_linear(
					task_common->task_aurisys->mAudioPoolBufDlIV,
					task_common->mtask_iv_buf,
					task_common->mtask_iv_buf_size);
			}
		}

		audio_dsp_aurisys_dl_process(data_buf, processing_size, task);

		ret = audio_dsp_write_to_sink(task_common->mtask_output_buf,
						  task_common->mtask_output_buf_size, param, &consume_time);
		if (ret != task_common->mtask_output_buf_size) {
			AUD_LOG_E("%s() fail ret[%d] outputsize[%d], written[%d]\n",
				  __FUNCTION__,
				  ret, task_common->mtask_output_buf_size, written);
			break;
		}
		written += ret;
		size -= processing_size;
		data_buf += processing_size;
		loopcount--;
	} while (size && (loopcount >= 0));
	UNLOCK_ALOCK(task_common->wb_lock);

	return written;
}
#endif

void cal_output_buffer_size(struct stream_attribute_dsp *attr_in,
			    struct stream_attribute_dsp *attr_out,
			    uint32_t size_in,
			    uint32_t *size_out)
{
	double times;

	if (attr_in == NULL || attr_out == NULL || size_out == NULL || size_in == 0)
		return;

	uint32_t size_per_frame_target = attr_out->num_channels *
					 AUDIO_BYTES_PER_SAMPLE(attr_out->audio_format);
	uint32_t unit_bytes_source = attr_in->num_channels * AUDIO_BYTES_PER_SAMPLE(
					     attr_in->audio_format) * attr_in->sample_rate;
	uint32_t unit_bytes_target = size_per_frame_target * attr_out->sample_rate;

	if (unit_bytes_source == 0 || unit_bytes_target == 0)
		return;

	times = (double)unit_bytes_target / (double)unit_bytes_source;
	*size_out = (uint32_t)(times * size_in);

	/* align */
	if (*size_out % size_per_frame_target != 0)
		*size_out = ((*size_out / size_per_frame_target) + 1) * size_per_frame_target;
}

void cal_buffer_size(struct stream_attribute_dsp *attr, int *size)
{
	if (attr == NULL || size == NULL) {
		AUD_LOG_E("%s(), attr or size should not be NULL", __func__);
		return;
	}

	uint32_t size_per_frame = attr->num_channels * AUDIO_BYTES_PER_SAMPLE(
					  attr->audio_format);
	*size = (size_per_frame * attr->sample_rate * attr->latency_ms) / 1000;

	if (size_per_frame == 0) {
		AUD_LOG_E("%s(), size_per_frame == 0", __func__);
		return;
	}

	/* align */
	if (*size % size_per_frame != 0)
		*size = ((*size / size_per_frame) + 1) * size_per_frame;
}

void cal_hw_buffer_size(struct stream_attribute_dsp *attr,
			struct audio_hw_buffer *buf_attr, int *size)
{
	if (attr == NULL || size == NULL || buf_attr == NULL) {
		AUD_LOG_E("%s(), attr or size or buf_attr should not be NULL", __func__);
		return;
	}

	uint32_t size_per_frame = buf_attr->aud_buffer.buffer_attr.channel *
					AUDIO_BYTES_PER_SAMPLE(attr->audio_format);

	*size = (size_per_frame * attr->sample_rate * attr->latency_ms) / 1000;

	if (size_per_frame == 0) {
		AUD_LOG_E("%s(), size_per_frame == 0", __func__);
		return;
	}

	/* align */
	if (*size % size_per_frame != 0)
		*size = ((*size / size_per_frame) + 1) * size_per_frame;
}

bool is_4channel_use(struct stream_attribute_dsp *attr)
{
	return (attr->num_channels > 2) ? true : false;
}

int clean_task_attr(struct audio_hw_buffer *buf_attr)
{
	if (!buf_attr)
		return -1;
	memset(buf_attr, 0, sizeof(struct audio_hw_buffer));
	return 0;
}

uint32_t get_swmixer_prior(int scene)
{
	switch (scene) {
	case TASK_SCENE_PRIMARY:
		return AUD_SW_MIXER_PRIOR_PRIMARY;
	case TASK_SCENE_DEEPBUFFER:
		return AUD_SW_MIXER_PRIOR_DEEP;
	case TASK_SCENE_VOIP:
		return AUD_SW_MIXER_PRIOR_VOIP;
	case TASK_SCENE_AUDPLAYBACK:
		return AUD_SW_MIXER_PRIOR_PLAYBACK;
	case TASK_SCENE_PLAYBACK_MP3:
		return AUD_SW_MIXER_PRIOR_OFFLOAD;
	case TASK_SCENE_MUSIC:
		return AUD_SW_MIXER_PRIOR_MIXER_MUSIC;
	case TASK_SCENE_FAST:
		return AUD_SW_MIXER_PRIOR_FAST;
	case TASK_SCENE_A2DP:
	case TASK_SCENE_BLEENC:
	case TASK_SCENE_BLEDEC:
		return AUD_SW_MIXER_PRIOR_BT;
	case TASK_SCENE_FM_ADSP:
		return AUD_SW_MIXER_PRIOR_FM_ADSP;
	default: {
		AUD_LOG_W("+%s() return max scene[%d] \n", __func__, NUM_AUD_SW_MIXER_TYPE);
		return NUM_AUD_SW_MIXER_TYPE;
	}
	}
	return NUM_AUD_SW_MIXER_TYPE;
}

uint32_t get_swmixer_source_id(int scene)
{
	switch (scene) {
	case TASK_SCENE_PRIMARY:
		return AUD_SW_MIXER_TYPE_MUSIC;
	case TASK_SCENE_DEEPBUFFER:
		return AUD_SW_MIXER_TYPE_MUSIC;
	case TASK_SCENE_PLAYBACK_MP3:
		return AUD_SW_MIXER_TYPE_PLAYBACK;
	case TASK_SCENE_VOIP:
		return AUD_SW_MIXER_TYPE_PLAYBACK;
	case TASK_SCENE_MUSIC:
		return AUD_SW_MIXER_TYPE_PLAYBACK;
	case TASK_SCENE_FAST:
		return AUD_SW_MIXER_TYPE_PLAYBACK;
	case TASK_SCENE_FM_ADSP:
		return AUD_SW_MIXER_TYPE_PLAYBACK;
	default: {
		AUD_LOG_W("+%s() return max scene[%d] \n", __func__, NUM_AUD_SW_MIXER_TYPE);
		return NUM_AUD_SW_MIXER_TYPE;
	}
	}
	return NUM_AUD_SW_MIXER_TYPE;
}

uint32_t get_swmixer_target_id(int scene)
{
	switch (scene) {
	case TASK_SCENE_A2DP:
	case TASK_SCENE_BLEENC:
	case TASK_SCENE_BLEDEC:
		return AUD_SW_MIXER_TYPE_PLAYBACK;
	case TASK_SCENE_AUDPLAYBACK:
		return AUD_SW_MIXER_TYPE_PLAYBACK;
	case TASK_SCENE_BLEDL:
		return AUD_SW_MIXER_TYPE_PLAYBACK;
	case TASK_SCENE_MUSIC:
		return AUD_SW_MIXER_TYPE_MUSIC;
	default: {
		AUD_LOG_W("+%s() return max scene[%d]\n", __func__, NUM_AUD_SW_MIXER_TYPE);
		return NUM_AUD_SW_MIXER_TYPE;
	}
	}
	return NUM_AUD_SW_MIXER_TYPE;
}

void user_attach_sw_mixer_source(struct audio_task_common *task_common,
				 struct sw_mixer_attr_t *attr)
{
#if defined(C2C_IPC_SUPPORT) && !defined(VOIP_SEPARATE_CORE)
	if (task_common->task_scene == TASK_SCENE_VOIP)
		audio_voip_c2c_client_attach_sw_mixer(get_swmixer_source_id(
							      task_common->task_scene), attr);

	else
#endif
	{
		task_common->mixer_source =
			sw_mixer_source_attach(get_swmixer_source_id(task_common->task_scene), attr);
		if (task_common->mixer_source == NULL)
			AUD_LOG_W("sw_mixer_source_attach fail\n");
	}
}

int user_write_sw_mixer_source(struct audio_task_common *task_common,
				void *buf, uint32_t size)
{
	int ret = 0;
#if defined(C2C_IPC_SUPPORT) && !defined(VOIP_SEPARATE_CORE)
	if (task_common->task_scene == TASK_SCENE_VOIP)
		ret = audio_voip_c2c_client_write(buf, size);

	else
#endif
		if (task_common->mixer_source)
			ret = sw_mixer_source_write(task_common->mixer_source, buf, size);
	return ret;
}

void user_detach_sw_mixer_source(struct audio_task_common *task_common)
{
#if defined(C2C_IPC_SUPPORT) && !defined(VOIP_SEPARATE_CORE)
	if (task_common->task_scene == TASK_SCENE_VOIP)
		audio_voip_c2c_client_detach_sw_mixer();

	else
#endif
		if (task_common->mixer_source) {
			sw_mixer_source_detach(task_common->mixer_source);
			task_common->mixer_source = NULL;
		}
}

bool sw_mixer_write_cb(int scene)
{
	if (scene == TASK_SCENE_AUDPLAYBACK || scene == TASK_SCENE_MUSIC)
		return true;
	return false;
}

bool sw_mixer_need_attach_source(int scene)
{
	switch (scene) {
	case TASK_SCENE_PRIMARY:
	case TASK_SCENE_DEEPBUFFER:
	case TASK_SCENE_PLAYBACK_MP3:
	case TASK_SCENE_VOIP:
	case TASK_SCENE_MUSIC:
	case TASK_SCENE_FAST:
	case TASK_SCENE_FM_ADSP:
		return true;
	default:
		return false;
	}
}

bool sw_mixer_need_attach_target(int scene)
{
	switch (scene) {
	case TASK_SCENE_MUSIC:
	case TASK_SCENE_AUDPLAYBACK:
		return true;
	default:
		return false;
	}
}

bool is_task_attach_sw_mixer(struct audio_task_common *task_common, int scene)
{
	if (!capture_handle_valid(task_common) || scene == TASK_SCENE_FM_ADSP)
		return true;
	return false;
}

static struct buf_attr swmixer_out_attr;
void init_swmixer_out_attr(struct buf_attr buffer_attr, int size)
{
	memcpy(&swmixer_out_attr, &buffer_attr, sizeof(struct buf_attr));
	return;
}

void get_swmixer_out_attr(struct buf_attr *attr)
{
	if (!attr)
		return;

	memcpy(attr, &swmixer_out_attr, sizeof(struct buf_attr));
}

static struct audio_hw_buffer capture_hw_buffer;
void init_capture_pipe_attr(struct audio_hw_buffer *hw_buffer)
{
	if (!hw_buffer)
		return;
	memcpy(&capture_hw_buffer, hw_buffer, sizeof(struct audio_hw_buffer));
}

void get_capture_pipe_attr(struct audio_hw_buffer *hw_buffer)
{
	if (!hw_buffer)
		return;
	memcpy(hw_buffer, &capture_hw_buffer, sizeof(struct audio_hw_buffer));
}

/*
 * task write data to sink
 */
int audio_dsp_write_to_sink(void *buf, uint32_t size, void *param, unsigned int *consume_time)
{

	int written = 0, ret = 0;
	unsigned long long start_time, leave_time, time_diffms;
	unsigned int wait_time;


	AudioTask *task = (AudioTask *)param;
	struct audio_task_common *task_common = task->task_priv;

	if (buf == NULL) {
		AUD_LOG_W("%s buf = NULL\n", __func__);
		return 0;
	}
	if (size == 0) {
		AUD_LOG_W("%s size = 0\n", __func__);
		return 0;
	}
	if (consume_time == NULL) {
		AUD_ASSERT(consume_time != NULL);
		return 0;
	}

	/* detect pulse */
	if (task_is_dl(task_common->task_scene)) {
		struct buf_attr *buffer_attr = &task_common->maudio_dlin_buf.aud_buffer.buffer_attr;
		detect_pulse(task_common->pulse_detect_flag, task_common->task_name, 1200, buf, size,
			    buffer_attr->channel, buffer_attr->format, buffer_attr->rate, PULSE_SHIFT_32BITS);
        } else if(task_is_audplayback(task_common->task_scene)){
		struct buf_attr *buffer_attr = &task_common->maudio_dlin_buf.aud_buffer.buffer_attr;
		detect_pulse(task_common->pulse_detect_flag, task_common->task_name, 800, buf, size,
			    buffer_attr->channel, buffer_attr->format, buffer_attr->rate, PULSE_SHIFT_32BITS);
        }

	record_time_interval(task_common->profile_rwbuf_id);

	/* write to afe hardware */
	if (playback_handle_valid(task_common)) {
		if (task_is_audplayback(task->scene)) {
			int hw_status = audio_dsp_hw_status(task, &task_common->maudio_dlout_buf);
			if (hw_status == HW_STATE_IDLE) {
				unsigned int FULL_size = get_audio_memif_size(&task_common->maudio_dlout_buf);
				char *temp_buffer = AUDIO_MALLOC(FULL_size);
				memset(temp_buffer, 0, (FULL_size));
				audio_dsp_hw_write(task_common->playback_hwbuf_handle, temp_buffer, FULL_size);
				AUDIO_FREE(temp_buffer);
			}
		}
		task_print_debug(task_common->debug_flag, "+%s task_name[%s] audio_dsp_hw_write buf [%p] size[%d] datacount[%d] \n",
				__func__,
				task_common->task_name,
				buf, size,
				audio_dsp_hw_getavail(task_common->playback_hwbuf_handle));

		start_time = get_time_stamp();
		written = audio_dsp_hw_write(task_common->playback_hwbuf_handle,
					     buf,
					     size);
		leave_time = get_time_stamp();
		*consume_time = get_time_diff(start_time, leave_time) / 1000000;
		/* print task event */
		if (!written) {
			AUD_LOG_W("%s task_name[%s] written[%d]\n", __func__, task_common->task_name, written);
			task_print_eventall(&task_common->event_tag);
		}

		task_print_info(task_common->debug_flag,"-%s task_name[%s] audio_dsp_hw_write datacount[%d] ms = [%u]\n",
				__func__,
				task_common->task_name,
				audio_dsp_hw_getavail(task_common->playback_hwbuf_handle),
				*consume_time );
	}

	/* temp use for task share buffer, remove later*/
	else if (task_common->mixer_source ||
		 task_common->task_scene == TASK_SCENE_VOIP) {

		start_time = get_time_stamp();
		ret = user_write_sw_mixer_source(task_common, buf, size);
		leave_time = get_time_stamp();
		time_diffms = get_time_diff(start_time, leave_time) / 1000000;
		*consume_time = time_diffms;
		wait_time = get_peroid_mstime(task_common->maudio_dlin_buf.aud_buffer.buffer_attr.rate,
					      task_common->maudio_dlin_buf.aud_buffer.period_size);
		if (ret < 0)
			written = ret;
		else
			written = size;

		task_print_info(task_common->debug_flag, "-%s task_name[%s] user_write_sw_mixer_source buf [%p ]size[%d] ms = [%.3f]\n",
				__func__,
				task_common->task_name, buf, size,
				((float)(unsigned long long)get_time_diff(start_time, leave_time)) / 1000000);

		if ((time_diffms >= wait_time * 2) && task_is_dl(task->scene)) {
			task_print_event(&task_common->event_tag, AUDIO_DSP_TASK_DLWRITE);
			AUD_LOG_W("%s task_name[%s] time_diffms[%llu] wait_time[%u]\n",
				  __func__,
				  task_common->task_name, time_diffms, wait_time);
		}

	} else {
		AUD_LOG_D("%s sleep for a while\n", __func__);
		aud_task_delay(5);
		written = size;
	}
	/* please add with other case,ex:swmixer*/

	record_time_interval(task_common->profile_rwbuf_id);
	stop_time_interval(task_common->profile_rwbuf_id);

	return written;
}

/*
 * task get data from sink
 */
int get_audio_capture_data(char *buf, int size, AudioTask *task)
{
	struct audio_task_common *task_common = task->task_priv;

	if (!buf) {
		AUD_LOG_E("%s() buf is NULL\n", __func__);
		return -1;
	}

	if (capture_handle_valid(task_common))
		return  audio_dsp_hw_read(task_common->capture_hwbuf_handle, buf, size);

#ifdef AUDIO_DUMMY_SHARE_BUFFER
	else if (task_common->task_share_buffer)
		return Read_share_buffer(task_common->task_share_buffer, buf, size);
#endif
	return -1;
}

#ifdef CFG_MTK_AUDIODSP_SUPPORT
static struct audio_core_flag *mshare_mem_core[HIFI3_CORE_TOTAL];
static struct audio_dsp_dram madsp_core_sharemem[HIFI3_CORE_TOTAL];

bool get_share_mem_core(unsigned int core_id)
{
	if (core_id >= HIFI3_CORE_TOTAL) {
		AUD_LOG_W("%s core_id[%d]\n", __func__, core_id);
		return false;
	}

	if (madsp_core_sharemem[core_id].phy_addr)
		return true;
	return false;
}

int init_share_mem_core(char *memory, unsigned int size, unsigned int core_id)
{
	AUD_LOG_D("%s core_id[%d] memory = %p size[%u]\n", __func__, core_id, memory,
		  size);

	if (core_id >= HIFI3_CORE_TOTAL) {
		AUD_LOG_W("%s core_id[%d]\n", __func__, core_id);
		return -1;
	}
	if (size > sizeof(madsp_core_sharemem[core_id])) {
		AUD_LOG_W("%s size[%u]\n", __func__, size);
		return -1;
	}
	memcpy(&madsp_core_sharemem[core_id], memory, size);

	/* remap atod share memory due to memory address of
	 * adsp view might different from ap view.
	 */
	madsp_core_sharemem[core_id].phy_addr =
		ap_to_adsp(madsp_core_sharemem[core_id].phy_addr);

	dump_audio_dsp_dram(&madsp_core_sharemem[core_id]);

	/* copy dram information */
	mshare_mem_core[core_id] = (struct audio_core_flag *)(unsigned int)
				   madsp_core_sharemem[core_id].phy_addr;
	return 0;
}

int set_core_irq(unsigned int core_id, unsigned char task_scene)
{
	int ret = 0;

	if (core_id >= HIFI3_CORE_TOTAL){
		AUD_LOG_W("%s core_id[%d]\n", __func__, core_id);
		return -1;
	}
	/* set flag and invalid cache */
	mshare_mem_core[core_id]->dtoa_flag |= (1 << (task_scene));
	hal_cache_region_operation(HAL_CACHE_TYPE_DATA,
				   HAL_CACHE_OPERATION_FLUSH_INVALIDATE,
				   (void *)(unsigned int)madsp_core_sharemem[core_id].phy_addr,
				   madsp_core_sharemem[core_id].size);
	return ret;
}

int clear_core_irq(unsigned int core_id, unsigned char task_scene)
{
	int ret = 0;

	if (core_id >= HIFI3_CORE_TOTAL){
		AUD_LOG_W("%s core_id[%d]\n", __func__, core_id);
		return -1;
	}
	/* set flag and invalid cache */
	mshare_mem_core[core_id]->dtoa_flag &= (~(1 << task_scene));
	/* AUD_LOG_D("%s core_id[%d] task_scene[%u] dtoa_flag[%llx]\n",
	          __func__, core_id, task_scene, mshare_mem_core[core_id]->dtoa_flag); */
	hal_cache_region_operation(HAL_CACHE_TYPE_DATA,
				   HAL_CACHE_OPERATION_FLUSH_INVALIDATE,
				   (void *)(unsigned int)madsp_core_sharemem[core_id].phy_addr,
				   madsp_core_sharemem[core_id].size);
	return ret;
}
#endif

void detect_pulse(bool enable, const char* taskname, const int pulse_level, const void *ptr, const unsigned int bytes,
		 const unsigned int channels,const unsigned int format, const unsigned int rate,
		 const unsigned int shift_bits) {

	long long sum = 0;
	unsigned int buf_val = 0;
	unsigned int sample_size = getframesize(channels, format);
	unsigned int sample_count = bytes / sample_size * channels;
	unsigned int sample_count_ms = rate / 1000 * channels; // using 1 ms detect
	unsigned int block_count  = sample_count / sample_count_ms;
	int first_detect_block = -1;
	unsigned int i, j;

	/* no detect pulse*/
	if (enable == false) {
		return;
	}

	short *pdata_16 = (short *)ptr;
	int *pdata_32 = (int *)ptr;

	AUD_LOG_D("%s taskname:%s bytes:%u sample_count:%u sample_size:%u channels:%u foramt:%u rate:%u pulse_level:%d block_count:%u sample_count_ms:%u\n",
		  __func__, taskname, bytes, sample_count, sample_size, channels,
		  format, rate, pulse_level, block_count, sample_count_ms);

	if (!ptr || !taskname) {
		AUD_LOG_E("%s ptr %p taskname %p\n", __func__, ptr, taskname);
		return;
	}
	if (!bytes || !channels || !rate) {
		AUD_LOG_E("channels:%u format:%u rate:%u\n", channels, format, rate);
		return;
	}

	for (i = 0; i < block_count; i++) {
		for (j = 0; j < sample_count_ms; j++) {
			// 16bits
			if (format == SNDRV_PCM_FORMAT_S16_LE ||
			    format == SNDRV_PCM_FORMAT_S16_BE ||
			    format == SNDRV_PCM_FORMAT_U16_LE ||
			    format == SNDRV_PCM_FORMAT_U16_BE){
				buf_val = (unsigned short)abs(*pdata_16);
				pdata_16++;
			} else if (format == SNDRV_PCM_FORMAT_S24_LE ||
				   format == SNDRV_PCM_FORMAT_S24_BE ||
				   format == SNDRV_PCM_FORMAT_U24_LE ||
				   format == SNDRV_PCM_FORMAT_U24_BE){ // 24bits
				buf_val = (unsigned int)abs(*pdata_32);
				pdata_32++;
			} else {
				// 32bits
				buf_val = (unsigned int)abs(*pdata_32);
				pdata_32++;
			}
			buf_val >>= shift_bits;
			sum += buf_val * buf_val;
		}
		// record first block detect
		if(sum > pulse_level && (first_detect_block < 0))
			first_detect_block = i;
	}

	if (sum >= pulse_level && (first_detect_block >= 0)) {
		AUD_LOG_D("%s detect pulse taskname:%s sum:%lld first_detect_block:%d\n",
			  __func__, taskname, sum, first_detect_block);
	}
}


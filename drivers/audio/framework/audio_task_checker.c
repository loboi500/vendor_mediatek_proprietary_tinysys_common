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
#include "audio_task_checker.h"
#include <audio_task_offload_mp3_enum.h>

/* =========msg check related function start ============   */

/*
  * below msg only  come from kernel
  */
static bool msg_layer_from_kernel(uint16_t msg_id)
{
	switch(msg_id) {
	case AUDIO_DSP_TASK_OPEN:
	case AUDIO_DSP_TASK_CLOSE:
	case AUDIO_DSP_TASK_PREPARE:
	case AUDIO_DSP_TASK_HWPARAM:
	case AUDIO_DSP_TASK_MSGA2DSHAREMEM:
	case AUDIO_DSP_TASK_MSGD2ASHAREMEM:
	case AUDIO_DSP_TASK_COREMEM_SET:
	case AUDIO_DSP_TASK_EINGBUFASHAREMEM:
	case AUDIO_DSP_TASK_HWFREE:
	case AUDIO_DSP_TASK_INIT:
	case AUDIO_DSP_TASK_RESET:
	case AUDIO_DSP_TASK_SHAREMEMORY_SET:
	case AUDIO_DSP_TASK_PCM_HWPARAM:
	case AUDIO_DSP_TASK_PCM_HWFREE:
	case AUDIO_DSP_TASK_PCM_PREPARE:
		return true;
	default:
		return false;
	}
}

/*
  * below msg should only come from hal
  */
static bool msg_layer_from_hal(uint16_t msg_id)
{
	switch(msg_id) {
	case AUDIO_DSP_TASK_SETPRAM:
		return true;
	default:
		return false;
	}
}

bool check_audio_hw_addr(uint64_t addr)
{
	if (is_ap_addr_valid(addr, MEM_ID_HIFI3_AUDIO) == false &&
	    is_ap_addr_valid(addr, MEM_ID_HIFI3_SHARED) == false) {
	    	AUD_LOG_W("%s addr[0x%llx]\n", __func__, addr);
		return false;
	}
	return true;
}

bool check_audio_hw_buflen(struct ringbuf_bridge buf_bridge)
{
	if (buf_bridge.pBufBase + buf_bridge.bufLen > (buf_bridge.pBufEnd + 1))
		return false;
	else if (buf_bridge.datacount > buf_bridge.bufLen)
		return false;

	return true;
}

bool check_audio_ringbuf_bridge_valid(struct ringbuf_bridge buf_bridge)
{
	if (check_audio_hw_addr(buf_bridge.pBufBase) == false)
		return false;
	 else if (check_audio_hw_addr(buf_bridge.pBufEnd) == false)
		return false;
	else if (check_audio_hw_buflen(buf_bridge) == false)
		return false;

	return true;
}

bool check_audio_buf_attr_valid(struct buf_attr buffer_attr)
{
	if (buffer_attr.format > SNDRV_PCM_FORMAT_FLOAT64_BE)
		return false;
	else if (buffer_attr.direction > AUDIO_DSP_TASK_PCM_HWPARAM_REF)
		return false;

	return true;
}

bool check_audio_buf_valid(struct audio_buffer aud_buffer)
{
	bool ret = true;

	if (check_audio_ringbuf_bridge_valid(aud_buffer.buf_bridge) == false)
		ret = false;
	else if (check_audio_buf_attr_valid(aud_buffer.buffer_attr) == false)
		ret =  false;
	if (!ret) {
		dump_buf_attr(aud_buffer.buffer_attr);
		dump_rbuf_bridge(&aud_buffer.buf_bridge);
	}

	return ret;
}

bool check_audio_hw_buf_valid(struct audio_hw_buffer task_audio_buf)
{
	bool ret = true;

	if (check_audio_buf_valid(task_audio_buf.aud_buffer) == false)
		ret = false;
	if (task_audio_buf.hw_buffer > BUFFER_TYPE_HW_MEM)
		ret = false;
	if (task_audio_buf.memory_type > MEMORY_SYSTEM_TCM)
		ret = false;
	if (!ret)
		dump_audio_hwbuffer(&task_audio_buf);
	return ret;
}

bool check_audio_dram_valid(struct audio_dsp_dram sharemem)
{
	if (is_ap_addr_valid(sharemem.phy_addr, MEM_ID_HIFI3_SHARED) == false){
		dump_audio_dsp_dram(&sharemem);
		AUD_LOG_W("%s phy_addr= %llx\n", __func__, sharemem.phy_addr);
		return false;
	}
	return true;
}


bool audio_check_common_msgid_valid(struct ipi_msg_t *ipi_msg, struct AudioTask *task)
{
	if ((!ipi_msg) || (!task)) {
		AUD_LOG_W("%s ipi_msg[%p] task[%p]\n", __func__, ipi_msg, task);
		return false;
	}

	struct audio_task_common *task_common = task->task_priv;

	switch(ipi_msg->msg_id){
	case AUDIO_DSP_TASK_PREPARE:
	case AUDIO_DSP_TASK_HWPARAM: {
		struct audio_hw_buffer hw_buf;
		unsigned int addr = (unsigned int)
			task_common->mtask_dsp_atod_sharemm.phy_addr;
		if (is_adsp_addr_valid(addr, MEM_ID_HIFI3_SHARED) == false) {
			AUD_LOG_W("%s addr=0x%x msgid = %u\n", __func__, addr, ipi_msg->msg_id);
			return false;
		}
		hal_cache_region_operation(HAL_CACHE_TYPE_DATA,
					   HAL_CACHE_OPERATION_INVALIDATE,
					   (void *)addr,
					   task_common->mtask_dsp_atod_sharemm.size);

		get_audiobuf_from_msg(ipi_msg, &hw_buf);
		if (check_audio_hw_buf_valid(hw_buf) == false){
			AUD_LOG_W("%s msg_id[%u] task[%p]\n", __func__, ipi_msg->msg_id, task);
			return false;
		}
		break;
	}
	case AUDIO_DSP_TASK_COREMEM_SET:
	case AUDIO_DSP_TASK_MSGA2DSHAREMEM:
	case AUDIO_DSP_TASK_MSGD2ASHAREMEM: {
		struct audio_dsp_dram sharemem;
		// check size
		if (ipi_msg->param1 != sizeof (struct audio_dsp_dram)) {
			AUD_LOG_W("%s param1 0x%x\n", __func__, ipi_msg->param1);
			return false;
		}
		// check addr
		memcpy((void *)&sharemem, ipi_msg->payload, ipi_msg->param1);
		if (check_audio_dram_valid(sharemem) == false) {
			return false;
		}
		break;
	}
	case AUDIO_DSP_TASK_PCM_HWPARAM:
	case AUDIO_DSP_TASK_PCM_PREPARE: {
		struct audio_hw_buffer hw_buf;
		unsigned int addr = (unsigned int)
			task_common->mtask_dsp_atod_sharemm.phy_addr;
		if (is_adsp_addr_valid(addr, MEM_ID_HIFI3_SHARED) == false) {
			AUD_LOG_W("%s addr0x%x msgid = %u\n", __func__, addr, ipi_msg->msg_id);
			return false;
		}
		hal_cache_region_operation(HAL_CACHE_TYPE_DATA,
					   HAL_CACHE_OPERATION_INVALIDATE,
					   (void *)addr,
					   task_common->mtask_dsp_atod_sharemm.size);

		get_audiobuf_from_msg(ipi_msg, &hw_buf);
		if (check_audio_hw_buf_valid(hw_buf) == false){
			AUD_LOG_W("%s msg_id[%u] task[%p]\n", __func__, ipi_msg->msg_id, task);
			return false;
		}
		break;
	}
	case AUDIO_DSP_TASK_EINGBUFASHAREMEM:{
		uint64_t *pbuf = (unsigned long long *)ipi_msg->payload;
		// check size
		if (is_ap_addr_valid(pbuf[0], MEM_ID_HIFI3_SHARED) == false) {
			AUD_LOG_W("%s param1 0x%llx\n", __func__, pbuf[0]);
			return false;
		}
		if (is_ap_addr_valid((pbuf[0] + pbuf[1]), MEM_ID_HIFI3_SHARED) == false) {
			AUD_LOG_W("%s param1 0x%llx  param2 0x%llx\n", __func__, pbuf[0], pbuf[1]);
			return false;
		}
		break;
	}
	case OFFLOAD_SETWRITEBLOCK:
	case OFFLOAD_DRAIN:
	case OFFLOAD_WRITEIDX:{
		unsigned long long addr = *(uint64_t *)ipi_msg->payload;
		if (is_ap_addr_valid(addr, MEM_ID_HIFI3_SHARED) == false) {
			AUD_LOG_W("%s addr = 0x%llx msgid = %u\n", __func__, addr, ipi_msg->msg_id);
			return false;
		}
		break;
	}
	default:
		return true;
	}
	return true;
}

/* this function will check message should come from ccorrespond layer */
bool check_msg_layer(uint16_t msg_id, uint8_t source_layer, uint8_t target_layer)
{
	bool ret = true;
	/* check msg come from kernel or hal */
	if (source_layer != AUDIO_IPI_LAYER_FROM_KERNEL &&
	    target_layer == AUDIO_IPI_LAYER_TO_DSP &&
	    msg_layer_from_kernel(msg_id)){
		ret = false;
	} else if (source_layer != AUDIO_IPI_LAYER_FROM_HAL &&
		   target_layer == AUDIO_IPI_LAYER_TO_DSP &&
		   msg_layer_from_hal(msg_id)){
		ret = false;
	}

	if (ret == false) {
		AUD_LOG_W("%s msg_id[%u] source_layer[%u] target_layer[%u]\n", __func__,
			  msg_id, source_layer, target_layer);
	}
	return ret;
}

bool audio_check_common_msg_valid(struct ipi_msg_t *ipi_msg, struct AudioTask *task)
{
	/* check msg if msg come from wrong layer */
	return (check_msg_layer(ipi_msg->msg_id,
				ipi_msg->source_layer,
				ipi_msg->target_layer) &&
		audio_check_common_msgid_valid(ipi_msg, task));
}

/* =========msg check related function end ============  */

void audio_check_buffer(struct audio_task_common *task_common,
			unsigned int flag)
{
	if (!get_aurisys_on())
		return;
	if (flag & AURI_INPUTBUF)
		AUDIO_CHECK(task_common->mtask_input_buf);
	if (flag & AURI_OUTPUTBUF)
		AUDIO_CHECK(task_common->mtask_output_buf);
	if (flag & AURI_REFBUF)
		AUDIO_CHECK(task_common->mtask_ref_buf);
	if (flag & AURI_PROCESSINGBUF)
		AUDIO_CHECK(task_common->mtask_processing_buf);
	if (flag & AURI_LINEAROUT)
		AUDIO_CHECK(task_common->task_aurisys->mLinearOut->p_buffer);
	if (flag & AURI_POOLBUF_DLOUT) {
		AUDIO_CHECK(
			task_common->task_aurisys->mAudioPoolBufDlOut->ringbuf.base);
	}
	if (flag & AURI_POOLBUF_ULOUT) {
		AUDIO_CHECK(
			task_common->task_aurisys->mAudioPoolBufUlOut->ringbuf.base);
	}
	return;
}



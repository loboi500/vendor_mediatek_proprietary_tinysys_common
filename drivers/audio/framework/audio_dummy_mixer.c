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

#include <audio_ringbuf.h>
#include "audio_messenger_ipi.h"
#include "audio_task_attr.h"
#include "audio_task_common.h"

#ifdef AUDIO_DUMMY_SHARE_BUFFER
/* temp using for swmixer share_buffer*/
struct alock_t *g_share_buffer_lock;
static bool task_share_buffer_flag = false;
struct share_buffer_t *sw_mixer_share_buffer[MIXER_BUFFER_SCENE_SIZE];
struct share_buffer_t *task_share_buffer[TASK_SCENE_DATAPROVIDER];

/*
 * when init buffer size is larger than default ,
 * re-init ring buffer size with large value.
 */
static void init_swmixer_buf(struct share_buffer_t *task_share_bufferm,
			     struct sw_mixer_attr_dummy_t *attr)
{
	struct RingBuf *buf = &task_share_bufferm->buf;
	if (task_share_bufferm->buf.bufLen < (attr->bufsize / 2)) {
		AUD_LOG_D("%s() bufLen[%d] bufsize[%d]\n", __func__,
			  task_share_bufferm->buf.bufLen,
			  attr->bufsize);
		if (buf->pBufBase) {
			AUDIO_FREE(buf->pBufBase);
			AUDIO_FREE(task_share_bufferm->outputbuffer);
			buf->pBufBase = NULL;
			task_share_bufferm->outputbuffer = NULL;
			buf->pBufBase = AUDIO_MALLOC(attr->bufsize);
			task_share_bufferm->outputbuffer = AUDIO_MALLOC(attr->bufsize);
			init_ring_buf(buf, buf->pBufBase, attr->bufsize);
			AUD_LOG_D("%s() reinit bufLen[%d] bufsize[%d]\n", __func__,
				  task_share_bufferm->buf.bufLen,
				  attr->bufsize);
		}
	}
	return;
}

bool share_buffer_construct(void)
{
	int i = 0;
	if (!g_share_buffer_lock) {
		g_share_buffer_lock = kal_pvPortMalloc(sizeof(struct alock_t));
		NEW_ALOCK(g_share_buffer_lock);
	}

	LOCK_ALOCK_MS(g_share_buffer_lock, 1000);
	if (task_share_buffer_flag == true)
		return false;

	for (i = 0 ; i < TASK_SCENE_DATAPROVIDER; i++) {
		if (!task_share_buffer[i]) {
			/* allcoate share_buffer_t*/
			task_share_buffer[i] = AUDIO_MALLOC(sizeof(struct share_buffer_t));
			if (!task_share_buffer[i])
				AUD_LOG_E("%s AUDIO_MALLOC fail\n", __func__);
			NEW_ALOCK(task_share_buffer[i]->share_buffer_lock);
			task_share_buffer[i]->target = -1;
			task_share_buffer[i]->source = -1;
			task_share_buffer[i]->task = NULL;
		}
	}

	for (i = 0 ; i < MIXER_BUFFER_SCENE_SIZE; i++) {
		if (!sw_mixer_share_buffer[i]) {
			/* allcoate share_buffer_t*/
			sw_mixer_share_buffer[i] = AUDIO_MALLOC(sizeof(struct share_buffer_t));
			if (!sw_mixer_share_buffer[i])
				AUD_LOG_E("%s AUDIO_MALLOC fail\n", __func__);
			NEW_ALOCK(sw_mixer_share_buffer[i]->share_buffer_lock);
			sw_mixer_share_buffer[i]->target = -1;
			sw_mixer_share_buffer[i]->source = -1;
			sw_mixer_share_buffer[i]->task = NULL;
			sw_mixer_share_buffer[i]->outputbuffer = NULL;

			/* pre allcoate buffer first*/
			sw_mixer_share_buffer[i]->buf.pBufBase = AUDIO_MALLOC(SW_MIXER_DEFAULT_SIZE);
			sw_mixer_share_buffer[i]->outputbuffer = AUDIO_MALLOC(SW_MIXER_DEFAULT_SIZE);
			sw_mixer_share_buffer[i]->outputbuffersize = SW_MIXER_DEFAULT_SIZE;
			init_ring_buf(&sw_mixer_share_buffer[i]->buf,
				      sw_mixer_share_buffer[i]->buf.pBufBase,
				      SW_MIXER_DEFAULT_SIZE);
		}
	}

	task_share_buffer_flag = true;
	UNLOCK_ALOCK(g_share_buffer_lock);
	AUD_LOG_D("%s done\n", __func__);
	return true;
}

struct share_buffer_t *Init_share_buffer(AudioTask *task, int source,
					 int target, unsigned int buffer_size)
{
	struct share_buffer_t *task_share_int = NULL;
	AUD_LOG_D("+%s source = %d target =%d buffer_size [%u]\n", __func__, source,
		  target, buffer_size);

	if (!task) {
		AUD_LOG_E("task = NULL\n");
		goto err_handle;
	}
	if (source >= TASK_SCENE_DATAPROVIDER || target >= TASK_SCENE_DATAPROVIDER) {
		AUD_LOG_E("source = %d target =%d\n", source, target);
		goto err_handle;
	}
	if (!task_share_buffer[source]) {
		AUD_LOG_E("%s source[%d] aleady allocated\n", __func__, source);
		goto err_handle;
	}


	task_share_int = task_share_buffer[source];

	/* allocate buffer*/
	if (task_share_int->buf.pBufBase != NULL)
		task_share_int->buf.pBufBase = AUDIO_MALLOC(buffer_size);
	if (!task_share_int->buf.pBufBase) {
		AUD_LOG_E("%s buf AUDIO_MALLOC fail\n", __func__);
		goto free_task_share_buffer;
	}

	init_ring_buf(&task_share_int->buf, task_share_int->buf.pBufBase, buffer_size);

	task_share_int->task = task->freertos_task;
	task_share_int->source = source;
	task_share_int->target = target;

	AUD_LOG_D("-%s task_share_int->task[%p] \n", __func__, task_share_int->task);

	return task_share_int;

free_task_share_buffer:
	if (task_share_buffer[source]->buf.pBufBase)
		AUDIO_FREE(task_share_buffer[source]->buf.pBufBase);

err_handle:
	AUD_LOG_E("-%s return NULL\n", __func__);
	return NULL;
}

int Set_share_buffer_attr(struct share_buffer_t *buffer, struct buf_attr attr)
{
	if (!buffer) {
		AUD_LOG_E("%s with buffer NULL\n", __func__);
		return -1;
	}
	memcpy(&buffer->attr, &attr, sizeof(struct buf_attr));
	AUD_LOG_D("%s\n", __func__);
	dump_buf_attr(buffer->attr);
	return 0;
}

int Deinit_share_buffer(AudioTask *task, struct share_buffer_t *buffer)
{
	AUD_LOG_E("+%s\n", __func__);

	if (!task || !buffer) {
		AUD_LOG_E("%s with task[%p] buffer[%p]\n", __func__, task, buffer);
		return -1;
	}

	if (buffer->task != task->freertos_task) {
		AUD_LOG_E("%s with buffer->tas[%p] task[%p]\n", __func__, buffer->task,
			  task->freertos_task);
		return -1;
	}

	if (buffer->buf.pBufBase) {
		AUDIO_FREE(buffer->buf.pBufBase);
		buffer->buf.pBufBase = NULL;
	}

	buffer->task = NULL;
	buffer->source = -1;
	buffer->target = -1;

	return 0;
}

/*
    task write to share buffer
    @input task :task handle
    @input buffer : linear buffer with audio data
    @input size : size oflinear buffer
    @return : size of written data
*/

int Write_share_buffer(AudioTask *task, struct share_buffer_t *buffer,
		       char *linear_buf, int size)
{
	int ret = 0;
	int loopcount = 5;
	uint32_t ulNotifiedValue = 0;

#ifdef FAKE_SAHREBUF_WRITE
	int ms = get_bufsize_to_ms(buffer->attr, size);
	AUD_LOG_D("+%s fake write size[%d] ms [%d]\n ", __func__, size, ms);
	aud_task_delay(ms);
	return size;
#endif

	if (!task || !buffer || !linear_buf) {
		AUD_LOG_E("%s with task[%p] buffer[%p] linear_buf[%p]\n", __func__, task,
			  buffer, linear_buf);
		return -1;
	}
	if (buffer->task != task->freertos_task) {
		AUD_LOG_E("%s with buffer->task[%p] task[%p]\n", __func__, buffer->task, task);
		return -1;
	}
	if (!buffer->buf.pBufBase) {
		AUD_LOG_E("%s buffer->buf.pBufBase = NULL\n", __func__);
		return -1;
	}
	if (size > buffer->buf.bufLen) {
		AUD_LOG_E("%s write size[%u] > buffer->buf.bufLen[%u]\n", __func__, size,
			  buffer->buf.bufLen);
		return -1;
	}

	AUD_LOG_D("%s FreeSpace[%u]\n", __func__, RingBuf_getFreeSpace(&buffer->buf));
	LOCK_ALOCK(buffer->share_buffer_lock);

	/* have enough sapce.*/
	if (RingBuf_getFreeSpace(&buffer->buf) >= size) {
		AUD_LOG_D("%s RingBuf_copyFromLinear size[%d] freespace[%u]\n",
			  __func__, size, RingBuf_getFreeSpace(&buffer->buf));
		RingBuf_copyFromLinear(&buffer->buf, linear_buf, size);
		UNLOCK_ALOCK(buffer->share_buffer_lock);
		return size;
	}

	/* wait for loopcount or timeout */
	while ((RingBuf_getFreeSpace(&buffer->buf) <= size) && loopcount--) {
		BaseType_t xResult;
		const TickType_t xMaxBlockTime = pdMS_TO_TICKS(100);
		/* Wait to be notified of an interrupt. */
		UNLOCK_ALOCK(buffer->share_buffer_lock);
		AUD_LOG_D("+ %s xTaskNotifyWait ulNotifiedValue[%u]\n", __func__,
			  ulNotifiedValue);
		xResult = xTaskNotifyWait(pdFALSE,     /* Don't clear bits on entry. */
					  0xffffffff,        /* Clear all bits on exit. */
					  &ulNotifiedValue, /* Stores the notified value. */
					  xMaxBlockTime);
		AUD_LOG_D("- %s xTaskNotifyWait ulNotifiedValue[%u] xResult[%d]\n", __func__,
			  ulNotifiedValue, xResult);
		if (!xResult) {
			AUD_LOG_D("break %s xTaskNotifyWait ulNotifiedValue[%u]\n", __func__,
				  ulNotifiedValue);
			break;
		}
		/* have enough sapce */
		LOCK_ALOCK(buffer->share_buffer_lock);
		if (RingBuf_getFreeSpace(&buffer->buf) >= size) {
			AUD_LOG_D("%s wait RingBuf_copyFromLinear size[%d] freespace[%u] ulNotifiedValue[%u]\n",
				  __func__, size, RingBuf_getFreeSpace(&buffer->buf), ulNotifiedValue);
			RingBuf_copyFromLinear(&buffer->buf, linear_buf, size);
			UNLOCK_ALOCK(buffer->share_buffer_lock);
			ret = size;
			break;
		}
	}

	AUD_LOG_D("-%s\n", __func__);
	return ret;
}

unsigned int Read_share_buffer(struct share_buffer_t *buffer, char *linear_buf,
			       unsigned int size)
{
	int ret = 0;

#ifdef FAKE_SAHREBUF_READ
	AUD_LOG_D("+%s fake read\n", __func__);
	aud_task_delay(3);
	AUD_LOG_D("-%s fake read\n", __func__);
	return size;
#endif
	if (!buffer) {
		AUD_LOG_E("%s with task[%p] buffer NULL\n", __func__,
			  buffer);
		return 0;
	}
	if (!linear_buf) {
		AUD_LOG_E("%s with task[%p] linear_buf NULL\n", __func__,
			  linear_buf);
		return 0;
	}
	if (!buffer->buf.pBufBase) {
		AUD_LOG_E("%s buffer->buf= NULL\n", __func__);
		return 0;
	}

	AUD_LOG_D("+%s read datacount[%u]\n", __func__,
		  RingBuf_getDataCount(&buffer->buf));

	/* lock and read data from ring buffer */
	LOCK_ALOCK_MS(buffer->share_buffer_lock, 1000);
	/* if data is enough ,acquire lock and read from ring buffer */
	if (RingBuf_getDataCount(&buffer->buf) >= size) {
		RingBuf_copyToLinear(linear_buf, &buffer->buf, size);
		ret = size;
		AUD_LOG_D("%s RingBuf_copyToLinear size[%d] datacount[%u]\n", __func__, size,
			  RingBuf_getDataCount(&buffer->buf));
		if (xTaskNotify(buffer->task, 0x1, eSetBits) == pdPASS)
			AUD_LOG_D("%s xTaskNotify pass\n", __func__);
		else
			AUD_LOG_D("%s xTaskNotify fail\n", __func__);
	} else {
		AUD_LOG_W("%s xTaskNotify RingBuf_getDataCount[%d] ret = %d\n",
			  __func__, RingBuf_getDataCount(&buffer->buf), ret);
	}
	UNLOCK_ALOCK(buffer->share_buffer_lock);
	return ret;
}

struct share_buffer_t *get_share_buffer(int source, int target)
{
	int index;

	for (index = 0; index < TASK_SCENE_DATAPROVIDER; index++) {
		if ((task_share_buffer[index]->source == source) &&
		    (task_share_buffer[index]->target == target) &&
		    (task_share_buffer[index]->task != NULL)
		   )
			return task_share_buffer[index];
	}
	AUD_LOG_D("%s return NULL\n", __func__);
	return NULL;
}

/* write this task for source attach*/
void *sw_mixer_source_attach_dummy(const unsigned char id,
				   struct sw_mixer_attr_dummy_t *buffer_attr)
{

	struct share_buffer_t *task_share_buffer = NULL;

	if (id >= MIXER_BUFFER_SCENE_SIZE)
		return NULL;
	LOCK_ALOCK(g_share_buffer_lock);

	task_share_buffer = sw_mixer_share_buffer[id];
	if (task_share_buffer->source != -1)
		AUD_LOG_W("+%s() task_share_buffer->source[%d] \n", __func__,
			  task_share_buffer->source);

	init_swmixer_buf(task_share_buffer, buffer_attr);
	task_share_buffer->source = id;
	RingBuf_Reset(&task_share_buffer->buf);
	memcpy(&task_share_buffer->attr, &buffer_attr->attr, sizeof(struct buf_attr));

	UNLOCK_ALOCK(g_share_buffer_lock);

	return (void *)task_share_buffer;
}

void sw_mixer_source_detach_dummy(void *param)
{
	/* casting */
	struct share_buffer_t *share_buffer = (struct share_buffer_t *)param;

	if (param == NULL) {
		AUD_LOG_W("%s() param == NULL\n", __func__);
		return;
	}
	LOCK_ALOCK(g_share_buffer_lock);

	AUD_LOG_D("%s()\n", __func__);

	if (share_buffer) {
		AUD_LOG_D("%s release task_common->task_share_buffer", __func__);
		share_buffer->source = -1;
	}

	UNLOCK_ALOCK(g_share_buffer_lock);
	return;
}

void sw_mixer_source_write_dummy(void *hdl, void *buffer, uint32_t bytes)
{
	struct share_buffer_t *share_buffer = (struct share_buffer_t *)hdl;
	int ret = 0;

	if (hdl == NULL) {
		AUD_LOG_W("%s hdl = NULL\n", __func__);
		return;
	}
	if (buffer == NULL) {
		AUD_LOG_W("%s buffer = NULL\n", __func__);
		return;
	}
	if (bytes == 0) {
		AUD_LOG_W("%s bytes = 0\n", __func__);
		return;
	}

	/* lock and read data from ring buffer */
	LOCK_ALOCK_MS(share_buffer->share_buffer_lock, 1000);
	/* if data is enough ,acquire lock and read from ring buffer */
	if (RingBuf_getFreeSpace(&share_buffer->buf) >= bytes) {
		RingBuf_copyFromLinear(&share_buffer->buf, buffer, bytes);
		AUD_LOG_D("%s RingBuf_copyFromLinear size[%d] freespace[%u]\n", __func__, bytes,
			  RingBuf_getFreeSpace(&share_buffer->buf));
	} else {
		/* not enough space to write */
		AUD_LOG_W("%s RingBuf_getFreeSpace[%d] ret = %d\n",
			  __func__, RingBuf_getFreeSpace(&share_buffer->buf), ret);
	}
	UNLOCK_ALOCK(share_buffer->share_buffer_lock);

	LOCK_ALOCK(g_share_buffer_lock);
	LOCK_ALOCK_MS(share_buffer->share_buffer_lock, 1000);
	if (share_buffer->sw_mixer_cb) {
		/* enough data to output and outbuffe size is larger thantarget out size */
		while ((RingBuf_getDataCount(&share_buffer->buf) >=
			share_buffer->target_out_size) &&
		       (share_buffer->outputbuffersize >= share_buffer->target_out_size)) {
			RingBuf_copyToLinear(share_buffer->outputbuffer, &share_buffer->buf,
					     share_buffer->target_out_size);
			/* write data with callback function */
			AUD_LOG_D("%s +sw_mixer_cb target_out_size[%d] outputbuffersize[%d]\n",
				  __func__,
				  share_buffer->target_out_size, share_buffer->outputbuffersize);
			share_buffer->sw_mixer_cb(share_buffer->outputbuffer,
						  share_buffer->target_out_size, share_buffer->cbk_arg);
			AUD_LOG_D("%s -sw_mixer_cb\n", __func__);
		}
	}
	UNLOCK_ALOCK(share_buffer->share_buffer_lock);
	UNLOCK_ALOCK(g_share_buffer_lock);

}

/* write this task for swxmier attach*/
void *sw_mixer_target_attach_dummy(const uint8_t id,
				   struct sw_mixer_attr_dummy_t *buffer_attr,
				   sw_mixer_target_cbk_dummy_t cbk,
				   void *arg)
{
	struct share_buffer_t *share_buffer = NULL;
	LOCK_ALOCK(g_share_buffer_lock);
	AUD_LOG_D("%s()\n", __func__);
	if (id >= MIXER_BUFFER_SCENE_SIZE) {
		AUD_LOG_W("%s() id[%d] return NULL]\n", __func__, id);
		goto ERROR;
	}
	if (cbk == NULL) {
		AUD_LOG_W("%s() cbk = NULL\n", __func__);
		goto ERROR;
	}
	if (arg == NULL) {
		AUD_LOG_W("%s() arg = NULL\n", __func__);
		goto ERROR;
	}
	share_buffer = sw_mixer_share_buffer[id];
	init_swmixer_buf(share_buffer, buffer_attr);
	memcpy(&share_buffer->attr_out, &buffer_attr->attr, sizeof(struct buf_attr));
	share_buffer->target_out_size = buffer_attr->bufsize;
	share_buffer->target = id;
	share_buffer->sw_mixer_cb = cbk;
	share_buffer->cbk_arg = arg;
ERROR:
	UNLOCK_ALOCK(g_share_buffer_lock);
	return share_buffer;
}
void sw_mixer_target_detach_dummy(void *hdl)
{
	struct share_buffer_t *share_buffer;
	AUD_LOG_D("%s()\n", __func__);
	if (hdl == NULL) {
		AUD_LOG_W("%s() hdl = NULL\n", __func__);
		return;
	}
	LOCK_ALOCK(g_share_buffer_lock);
	share_buffer = (struct share_buffer_t *)hdl;
	share_buffer->target = -1;
	share_buffer->target_out_size = 0;
	share_buffer->sw_mixer_cb = NULL;
	share_buffer->cbk_arg = NULL;
	UNLOCK_ALOCK(g_share_buffer_lock);
	return;
}

#endif


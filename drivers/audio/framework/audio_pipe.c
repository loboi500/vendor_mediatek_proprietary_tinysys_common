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

#include <audio_type.h>
#include <audio_ringbuf.h>
#include "audio_task_factory.h"
#include "audio_log.h"
#include "audio_hw.h"
#include "audio_lock.h"
#include "audio_task_utility.h"
#include "audio_task_common.h"
#include "audio_dsp_hw_hal.h"
#include "audio_messenger_ipi.h"
#include "audio_task_attr.h"
#include <wrapped_audio.h>
#include <limits.h>
#include <audio_pipe.h>

#include <uthash.h> /* uthash */
#include <utlist.h> /* linked list */

//#define DEBUG_VERBOSE

struct audio_pipe_user {
	void *user;
	struct RingBuf ring_buf;
	UT_hash_handle hh;
};

struct audio_pipe {
	void *user; // user opened handle
	int handle; // handle for open hardware
	int irq_event; // irq_event
	struct audio_hw_buffer hw_buf;
	struct audio_pipe_user *user_list;
	char *read_buf; // copy data from hardware
	unsigned int read_buf_size;
	struct alock_t *pipe_lock;
	UT_hash_handle hh;
};

static struct alock_t *g_pipe_lock;
static int pipe_init = false;
static struct audio_pipe *head = NULL;

static void init_pipe_lock()
{
	if (pipe_init)
		return;
	taskENTER_CRITICAL();
	if (g_pipe_lock == NULL)
		NEW_ALOCK(g_pipe_lock);
	pipe_init = true;
	taskEXIT_CRITICAL();
}

/*
static void deinit_pipe_lock()
{
    if (g_pipe_lock != NULL)
        FREE_ALOCK(g_pipe_lock);
}
*/

static void dump_all_pipe_user(struct audio_pipe *pipe)
{
	struct audio_pipe_user *s, *tmp;
	unsigned int num_users;
	num_users = HASH_COUNT(pipe->user_list);
	HASH_ITER(hh, pipe->user_list, s, tmp) {
		if (s != NULL)
			AUD_LOG_D("%s user[%p]\n", __FUNCTION__, s->user);
	}
}

static void dump_all_pipe(void)
{
	struct audio_pipe *s, *tmp;
	unsigned int num_users;
	num_users = HASH_COUNT(head);
	HASH_ITER(hh, head, s, tmp) {
		if (s != NULL) {
			AUD_LOG_D("handle[%d] user[%p]\n", s->handle, s->user);
			dump_all_pipe_user(s);
		}
	}
}

/*
 * @input void *user : pointer ex: task handle.
 * @input hardware buffer with memeory interface and buffer information
 * @return 0 for NO_ERROR;
 */
int audio_pipe_open(void *user, struct audio_hw_buffer *hw_buf, int irq_event)
{
	int handle;
	int ret = 0;
	struct audio_pipe *tmp = NULL;

	init_pipe_lock();

	AUD_LOG_D("+%s() user[%p]\n", __FUNCTION__, user);

	if (!user || !hw_buf) {
		AUD_LOG_W("%s() user[%p] hw_buf[%p]\n", __FUNCTION__, user, hw_buf);
		return -1;
	}

	LOCK_ALOCK(g_pipe_lock);

	/* open handle */
	handle = audio_dsp_hw_open(user, hw_buf);
	if (handle < 0) {
		AUD_LOG_W("%s() open handle fail\n", __FUNCTION__);
		ret = -1;
		goto ERROR;
	}

	/* allocate memory */
	tmp =  AUDIO_MALLOC(sizeof(struct audio_pipe));
	if (!tmp) {
		AUD_LOG_D("%s() AUDIO_MALLOC fail\n", __FUNCTION__);
		ret = -1;
		goto ERROR;
	}

	/* init value*/
	memset(tmp, 0, sizeof(struct audio_pipe));
	NEW_ALOCK(tmp->pipe_lock);

	/* save handle and user */
	tmp->handle = handle;
	tmp->user = user;
	memcpy(&tmp->hw_buf, hw_buf, sizeof(struct audio_hw_buffer));
	tmp->read_buf_size = get_audio_write_size(&tmp->hw_buf);
	if (tmp->read_buf_size == 0)
		goto ERROR;

	tmp->read_buf = AUDIO_MALLOC(tmp->read_buf_size);
	tmp->irq_event = irq_event;

	/* insert to hash table */
	HASH_ADD_PTR(head, user, tmp);
	UNLOCK_ALOCK(g_pipe_lock);

	AUD_LOG_D("-%s() user[%p] read_buf_size = [%u]\n", __FUNCTION__, user,
		  tmp->read_buf_size);
	dump_all_pipe();

	return handle;

ERROR:
	if (tmp != NULL) {
		AUD_LOG_W("%s() user[%p] ERROR handle read_buf_size[%d]\n",
			  __FUNCTION__, user, tmp->read_buf_size);
	}
	UNLOCK_ALOCK(g_pipe_lock);
	return ret;
}

/*
 * @input void *user : only pipe user oopen can close.
 * @return 0 for NO_ERROR;
 */
int audio_pipe_close(void *user)
{
	int ret = 0;
	struct audio_pipe *tmp = NULL;
	struct audio_pipe_user *current_pipe_user = NULL;
	struct audio_pipe_user *tmp_user = NULL;

	AUD_LOG_D("+%s() user[%p]\n", __FUNCTION__, user);

	if (!user) {
		AUD_LOG_W("%s() user[%p]\n", __FUNCTION__, user);
		ret = -1;
		goto ERROR;
	}

	init_pipe_lock();

	LOCK_ALOCK(g_pipe_lock);
	dump_all_pipe();

	/* find user */
	HASH_FIND_PTR(head, &user, tmp);
	if (!tmp) {
		AUD_LOG_W("%s() find user error\n", __FUNCTION__);
		ret = -1;
		goto ERROR;
	}

	LOCK_ALOCK(tmp->pipe_lock);
	/* close handle */
	audio_dsp_hw_close(tmp->handle);
	tmp->user = NULL;

	if (tmp->read_buf) {
		AUDIO_FREE(tmp->read_buf);
		tmp->read_buf_size = 0;
	}

	if (tmp->user_list != NULL) {
		AUD_LOG_E("%s() but user_list not NULL, free all user\n", __FUNCTION__);
		//AUD_ASSERT(0);
		HASH_ITER(hh, tmp->user_list, current_pipe_user, tmp_user) {
			/* free ring buffer*/
			AUDIO_FREE(current_pipe_user->ring_buf.pBufBase);
			/* free pipe user */
			HASH_DEL(tmp->user_list, current_pipe_user);
			AUDIO_FREE(current_pipe_user);
		}
	}
	UNLOCK_ALOCK(tmp->pipe_lock);

	FREE_ALOCK(tmp->pipe_lock);

	/* release memory */
	HASH_DEL(head, tmp);
	UNLOCK_ALOCK(g_pipe_lock);

	AUDIO_FREE(tmp);
	dump_all_pipe();

	AUD_LOG_D("-%s() user[%p]\n", __FUNCTION__, user);
	return 0;
ERROR:
	AUD_LOG_W("%s() user[%p] ERROR handle\n", __FUNCTION__, user);
	UNLOCK_ALOCK(g_pipe_lock);
	return ret;
}
/*
 * @input void *user : attach user
 * @input int user : which memif
 * @output  struct pipe_attr *p_attr
 * @return 0 for NO_ERROR;
 */
int audio_pipe_attach(void *user, int memif, struct pipe_attr *p_attr)
{
	struct audio_pipe_user *pipe_user;
	struct audio_pipe *current = NULL, *tmp = NULL;
	int buffer_size;

	AUD_LOG_D("+%s() user[%p] memif[%d] p_attr[%p]\n", __FUNCTION__, user, memif,
		  p_attr);

	if (!user) {
		AUD_LOG_W("%s() user [%p] attr[%p]\n", __FUNCTION__, user, p_attr);
		return -1;
	}

	/* find if handle is opened. */
	HASH_ITER(hh, head, current, tmp) {
		if (current->handle == memif)
			break;
	}

	if (!current) {
		AUD_LOG_W("%s() current = %p\n", __FUNCTION__, current);
		return -1;
	}


	LOCK_ALOCK(current->pipe_lock);
	audio_dsp_hw_trigger(current->handle, TRIGGER_START);

	/* can attach to this pipe*/
	pipe_user = AUDIO_MALLOC(sizeof(struct audio_pipe_user));
	pipe_user->user = user;
	HASH_ADD_PTR(current->user_list, user, pipe_user);
	// 4k * 8 = 32K.
	buffer_size = get_audio_write_size(&current->hw_buf) * 8;

	/* return for attach buffer attr */
	if (p_attr != NULL) {
		memcpy(&p_attr->buffer_attr, &current->hw_buf.aud_buffer.buffer_attr,
		       sizeof(struct buf_attr));
		p_attr->buffer_size = buffer_size;
	}

	/* init ring buffer */
	pipe_user->ring_buf.pBufBase = AUDIO_MALLOC(buffer_size);
	init_ring_buf(&pipe_user->ring_buf,  pipe_user->ring_buf.pBufBase,
		      buffer_size);

	UNLOCK_ALOCK(current->pipe_lock);
	dump_all_pipe_user(current);
	AUD_LOG_D("-%s() user[%p] memif[%d]\n", __FUNCTION__, user, memif);

	return 0;
}
/*
 * @input void *user : detach user
 * @input int user : which memif
 * @return 0 for NO_ERROR;
 */
int audio_pipe_detach(void *user, int memif)
{
	struct audio_pipe_user *pipe_user;
	struct audio_pipe *current = NULL, *tmp = NULL;

	AUD_LOG_D("+%s() user[%p] memif[%d]\n", __FUNCTION__, user, memif);

	if (!user) {
		AUD_LOG_W("%s() user [%p]\n", __FUNCTION__, user);
		return -1;
	}

	/* find if handle is opened. */
	HASH_ITER(hh, head, current, tmp) {
		if (current->handle == memif)
			break;
	}

	if (!current) {
		AUD_LOG_W("%s() user %p\n", __FUNCTION__, user);
		return -1;
	}

	dump_all_pipe_user(current);

	LOCK_ALOCK(current->pipe_lock);

	/* find if pipe user */
	HASH_FIND_PTR(current->user_list, &user, pipe_user) ;

	if (pipe_user == NULL) {
		AUD_LOG_W("%s() cannot find pipe_user[%p]\n", __FUNCTION__, user);
		UNLOCK_ALOCK(current->pipe_lock);
		return -1;
	}

	/* free ring buffer*/
	AUDIO_FREE(pipe_user->ring_buf.pBufBase);

	/* free pipe user */
	HASH_DEL(current->user_list, pipe_user);
	AUDIO_FREE(pipe_user);

	if (HASH_COUNT(current->user_list) == 0)
		audio_dsp_hw_trigger(current->handle, TRIGGER_STOP);

	UNLOCK_ALOCK(current->pipe_lock);

	dump_all_pipe_user(current);
	dump_all_pipe();

	AUD_LOG_D("-%s() user[%p] memif[%d]\n", __FUNCTION__, user, memif);

	return 0;
}

/*
 * @input user : attach user
 * @inout buffer : reading buffer
 * @input buffer_size : reading buffer size
 * @return size of reading
 */
int audio_pipe_read(void *user, int memif, void *buffer,
		    unsigned int buffer_size)
{
	struct audio_pipe_user *pipe_user;
	struct audio_pipe *current = NULL, *tmp = NULL;
	unsigned int data_count;

	if (!user || !buffer) {
		AUD_LOG_W("%s() user [%p] buffer[%p]\n", __FUNCTION__, user, buffer);
		return -1;
	}
	init_pipe_lock();
	LOCK_ALOCK(g_pipe_lock);
	/* find if handle is opened. */
	HASH_ITER(hh, head, current, tmp) {
		if (current->handle == memif)
			break;
	}
	if (!current) {
		AUD_LOG_W("%s() current = %p\n", __FUNCTION__, current);
		UNLOCK_ALOCK(g_pipe_lock);
		return -1;
	}

	LOCK_ALOCK(current->pipe_lock);

	/* find if pipe user */
	HASH_FIND_PTR(current->user_list, &user, pipe_user);
	if (pipe_user == NULL) {
		AUD_LOG_W("%s() cannot find pipe_user[%p]\n", __FUNCTION__, user);
		UNLOCK_ALOCK(current->pipe_lock);
		UNLOCK_ALOCK(g_pipe_lock);
		return -1;
	}

	/* copy data to pipe_user linear buffer */
	data_count = RingBuf_getDataCount(&pipe_user->ring_buf);

	if (buffer_size > data_count) {
		AUD_LOG_W("%s() not enough data data_count[%u]\n", __FUNCTION__, data_count);
		UNLOCK_ALOCK(current->pipe_lock);
		UNLOCK_ALOCK(g_pipe_lock);
		return 0;
	}
	RingBuf_copyToLinear(buffer,
			     &pipe_user->ring_buf,
			     buffer_size);
	UNLOCK_ALOCK(current->pipe_lock);
	UNLOCK_ALOCK(g_pipe_lock);
	return buffer_size;
}

/*
 * @input user : attach user
 * @inout int user : which memif
 * @return size of reading buffer
 */
int audio_pipe_get_avail(void *user, int memif)
{
	struct audio_pipe_user *pipe_user;
	struct audio_pipe *current = NULL, *tmp = NULL;
	unsigned int data_count;

	if (!user) {
		AUD_LOG_W("%s() user [%p]\n", __FUNCTION__, user);
		return -1;
	}
	init_pipe_lock();
	LOCK_ALOCK(g_pipe_lock);
	/* find if handle is opened. */
	HASH_ITER(hh, head, current, tmp) {
		if (current->handle == memif)
			break;
	}
	if (!current) {
		AUD_LOG_W("%s() current = %p\n", __FUNCTION__, current);
		UNLOCK_ALOCK(g_pipe_lock);
		return -1;
	}

	LOCK_ALOCK(current->pipe_lock);
	/* find if pipe user */
	HASH_FIND_PTR(current->user_list, &user, pipe_user);
	if (pipe_user == NULL) {
		AUD_LOG_W("%s() cannot find pipe_user[%p]\n", __FUNCTION__, user);
		UNLOCK_ALOCK(current->pipe_lock);
		UNLOCK_ALOCK(g_pipe_lock);
		return -1;
	}

	/* copy data to pipe_user linear buffer */
	data_count = RingBuf_getDataCount(&pipe_user->ring_buf);

	UNLOCK_ALOCK(current->pipe_lock);
	UNLOCK_ALOCK(g_pipe_lock);
	return data_count;
}

static void *audio_get_pipe_by_event(int event)
{
	struct audio_pipe *s, *tmp;
	unsigned int num_users;
	num_users = HASH_COUNT(head);
#ifdef DEBUG_VERBOSE
	AUD_LOG_D("%s users[%u]\n", __FUNCTION__, num_users);
#endif
	HASH_ITER(hh, head, s, tmp) {
		if (s != NULL && s->irq_event == event)
			return s->user;
	}
	return NULL;
}

/*
 * @input int event : message event
 * @input hardware buffer with memeory interface and buffer information
 * @return update pass/fail
 */
int audio_pipe_update_read_size(int event, struct audio_hw_buffer *hw_buf)
{
	void *user = audio_get_pipe_by_event(event);
	struct audio_pipe *current_pipe = NULL;
	if (!user) {
		AUD_LOG_W("%s() cannot find user[%p]\n", __FUNCTION__, user);
		return -1;
	}
	HASH_FIND_PTR(head, &user, current_pipe);
	if (!current_pipe) {
		AUD_LOG_W("%s() find user error\n", __FUNCTION__);
		return -1;
	}
	current_pipe->read_buf_size = get_audio_write_size(hw_buf);
	AUDIO_FREE(current_pipe->read_buf);
	current_pipe->read_buf = AUDIO_MALLOC(current_pipe->read_buf_size);
	AUD_LOG_D("%s() event[%d], new read_buf_size[%d]", __FUNCTION__, event, current_pipe->read_buf_size);
	return 0;
}

static int audio_pipe_irq_event(void *user)
{
	int ret = 0;
	int readcount;
	struct audio_pipe *current_pipe;
	struct audio_pipe_user *current_pipe_user, *tmp;

	if (!user) {
		AUD_LOG_W("%s() user[%p]\n", __FUNCTION__, user);
		return -1;
	}

	/* find user */
	HASH_FIND_PTR(head, &user, current_pipe);
	if (!current_pipe) {
		AUD_LOG_W("%s() find user error\n", __FUNCTION__);
		return -1;
	}

	if (current_pipe->read_buf_size == 0) {
		AUD_LOG_W("%s() read_buf_size cannot be zero!\n", __FUNCTION__);
		return -1;
	}

	/* hw read only block one time */
	readcount = (audio_dsp_hw_getavail(current_pipe->handle) /
		     current_pipe->read_buf_size);
	if (readcount == 0) {
		AUD_LOG_W("%s() users[%p] readcount[%d] getavail[%d] read_buf_size[%d]\n", __FUNCTION__,
			user, readcount, audio_dsp_hw_getavail(current_pipe->handle), current_pipe->read_buf_size);
	}
#ifdef DEBUG_VERBOSE
	AUD_LOG_D("+%s users[%p] readcount[%d]\n", __FUNCTION__, user, readcount);
#endif

	LOCK_ALOCK(current_pipe->pipe_lock);
	if (HASH_COUNT(current_pipe->user_list) == 0) {
		ALOGE("%s(), no user left! user[%p]\n", __FUNCTION__, user);
		UNLOCK_ALOCK(current_pipe->pipe_lock);
		return -1;
	}

	/* read data from hardware */
	while (audio_dsp_hw_getavail(current_pipe->handle) >= 0 &&
		(unsigned int)audio_dsp_hw_getavail(current_pipe->handle) >=
		current_pipe->read_buf_size && (readcount > 0)) {
		ret = audio_dsp_hw_read(current_pipe->handle, current_pipe->read_buf,
					current_pipe->read_buf_size);

		if (ret < 0 || (unsigned int)ret != current_pipe->read_buf_size) {
			AUD_LOG_W("%s() audio_dsp_hw_read ret[%d]\n", __FUNCTION__, ret);
			UNLOCK_ALOCK(current_pipe->pipe_lock);
			return -1;
		}

		/* copy data to attach user */
		HASH_ITER(hh, current_pipe->user_list, current_pipe_user, tmp) {
			if (current_pipe_user) {
				unsigned int freespace = RingBuf_getFreeSpace(&current_pipe_user->ring_buf);
				if (freespace >= current_pipe->read_buf_size) {
					RingBuf_copyFromLinear(&current_pipe_user->ring_buf,
							       current_pipe->read_buf,
							       current_pipe->read_buf_size);
				} else
					AUD_LOG_W("%s() freespace[%d]\n", __FUNCTION__, freespace);
			}
		}
		readcount--;
	}
	UNLOCK_ALOCK(current_pipe->pipe_lock);

#ifdef DEBUG_VERBOSE
	AUD_LOG_D("-%s users[%p]\n", __FUNCTION__, user);
#endif

	return 0;
}

int audio_pipe_irq_trigger(int event)
{
	int ret = 0;
	void *user = audio_get_pipe_by_event(event);
	if (user)
		ret = audio_pipe_irq_event(user);
	return ret;
}



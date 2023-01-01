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
#include "audio_task_event.h"

/* task log event */
void task_init_event(struct task_event_tag *tag, unsigned char scene)
{
	int i = 0;
	if (!tag)
		return;
	tag->scene = scene;
	tag->writeidx = 0;
	for (i = 0; i < TASK_EVENT_TAG_NUM; i++)
		memset(&tag->eventtag[i], 0, sizeof(struct event_tag));
}

/* task log event */
void task_add_event(struct task_event_tag *tag, unsigned int event1,
		    unsigned int event2, const char *name, unsigned int line)
{
	struct event_tag *pevent;
	uint64_t time_ns = get_timestamp_ns();

	if (!tag)
		return;
	pevent = &tag->eventtag[tag->writeidx];
	pevent->event = event1;
	pevent->event2 = event2;
	pevent->line = line;
	if (name)
		pevent->funcname = name;
	pevent->time_ms = TIME_NS_TO_MS(time_ns) % 1000;
	pevent->time_sec = TIME_NS_TO_SEC(time_ns);

#ifdef  EVENT_DEBUG
	AUD_LOG_D("%s event1[%d] event2[%d] [%s] line:%u sec:%d ms:%02d\n",
		  __func__,
		  pevent->event,
		  pevent->event2,
		  pevent->funcname,
		  pevent->line,
		  pevent->time_sec,
		  pevent->time_ms);
#endif
	tag->writeidx++;
	tag->writeidx = tag->writeidx % TASK_EVENT_TAG_NUM;

}

static void print_event(struct event_tag *pevent)
{
	if (!pevent)
		return;
	if (! pevent->event ||  !pevent->funcname)
		return;

	AUD_LOG_D("%s event1[%d] event2[%d] [%s] line:%u sec:%d ms:%02d\n",
		  __func__,
		  pevent->event,
		  pevent->event2,
		  pevent->funcname,
		  pevent->line,
		  pevent->time_sec,
		  pevent->time_ms);
}

static void clear_event(struct event_tag *pevent)
{
	if (!pevent)
		return;
	if (! pevent->event ||  !pevent->funcname)
		return;

	memset(pevent, 0, sizeof(struct event_tag));
}


/* task log event */
void task_print_event(struct task_event_tag *tag, unsigned int event1)
{
	int i = 0;
	if (!tag)
		return;

	AUD_LOG_D("%s scene[%d]\n", __func__, tag->scene);
	/* start to print log */
	for (i = 0; i < TASK_EVENT_TAG_NUM; i++) {
		struct event_tag *pevent = &tag->eventtag[i];
		if (pevent->event == event1)
			print_event(pevent);
	}
}

void task_print_eventall(struct task_event_tag *tag)
{
	int i = 0;
	if (!tag)
		return;

	/*clear certain event */
	for (i = 0; i < TASK_EVENT_TAG_NUM; i++) {
		struct event_tag *pevent = &tag->eventtag[i];
		print_event(pevent);
	}
}

void task_clear_event(struct task_event_tag *tag, unsigned int event1)
{
	int i = 0;
	if (!tag)
		return;

	/* start to print log */
	for (i = 0; i < TASK_EVENT_TAG_NUM; i++) {
		struct event_tag *pevent = &tag->eventtag[i];
		if (pevent->event == event1)
			clear_event(pevent);
	}
}

void task_clear_all(struct task_event_tag *tag)
{
	int i = 0;
	if (!tag)
		return;

	/*clear certain event */
	for (i = 0; i < TASK_EVENT_TAG_NUM; i++) {
		struct event_tag *pevent = &tag->eventtag[i];
		clear_event(pevent);
	}
}


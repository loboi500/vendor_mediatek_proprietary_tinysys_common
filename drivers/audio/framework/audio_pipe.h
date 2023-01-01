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
#ifndef AUDIO_PIPE_H
#define AUDIO_PIPE_H

#include <stdio.h>
#include <audio_type.h>
#include <audio_sw_mixer.h>
#include <uthash.h> /* uthash */
#include <utlist.h> /* linked list */

struct pipe_attr {
	struct buf_attr buffer_attr; /* attach buffer attr */
	int buffer_size; /* buffer size of pipe */
};

/*
 * @input void *user : pointer ex: task handle.
 * @input hardware buffer with memeory interface
 * @irq_event message with irq_event id
 */
int audio_pipe_open(void *user, struct audio_hw_buffer *hw_buf, int irq_event);

/*
 * @input void *user : only pipe user oopen can close.
 */
int audio_pipe_close(void *user);
/*
 * @input void *user : attach user
 * @input int user : which memif
 * @output  struct pipe_attr *p_attr
 * @return 0 for NO_ERROR;
 */
int audio_pipe_attach(void *user, int memif, struct pipe_attr *p_attr);
/*
 * @input void *user : attach user
 * @input int user : which memif
 * @return 0 for NO_ERROR;
 */
int audio_pipe_detach(void *user, int memif);
/*
 * @input user : attach user
 * @input memif : memory interface
 * @inout buffer : reading buffer
 * @input buffer_size : reading buffer size
 */
int audio_pipe_read(void *user, int memif, void *buffer,
		    unsigned int buffer_size);
/*
 * @input user : attach user
 * @inout int user : which memif
 * @return size of reading buffer
 */
int audio_pipe_get_avail(void *user, int memif);
/*
 * @input int event : message event
 * @input hardware buffer with memeory interface and buffer information
 * @return update pass/fail
 */
int audio_pipe_update_read_size(int event, struct audio_hw_buffer *hw_buf);
/*
 * @input int event : message event
 * @return 0 for NO_ERROR, call in task , not in isr.
 */
int audio_pipe_irq_trigger(int event);


#endif // end of AUDIO_PIPE_H

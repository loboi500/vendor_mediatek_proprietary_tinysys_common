/* Copyright Statement:
 *
 * @2015 MediaTek Inc. All rights reserved.
 *
 * This software/firmware and related documentation ("MediaTek Software") are
 * protected under relevant copyright laws. The information contained herein
 * is confidential and proprietary to MediaTek Inc. and/or its licensors.
 * Without the prior written permission of MediaTek Inc. and/or its licensors,
 * any reproduction, modification, use or disclosure of MediaTek Software,
 * and information contained herein, in whole or in part, shall be strictly prohibited.
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
 * MEDIATEK FOR SUCH MEDIATEK SOFTWARE.
 */
#ifndef _PBFR_REC_H
#define _PBFR_REC_H

//Definition of Event Type
#define EV_TYPE_MASK        0xF0
#define EV_TYPE_SYS         0x80	//System event type
#define EV_SYS_REC_START    0x81	//start recording
#define EV_SYS_REC_STOP     0x82	//stop recording
#define EV_SYS_DTS          0x83	//Just DTS
#define EV_SYS_DTS_H        0x84	//High part of DTS to the next event
#define EV_SYS_OS_TICK      0x85	//Log OS tick per x times
#define EV_SYS_COREDUMP     0x86	//Core Dump

#define EV_TYPE_TASK        0x10	//Task event type
#define EV_TASK_SWTCH_1     0x11	//Switch from a running task
#define EV_TASK_SWTCH_2     0x12	//Switch from a blocked task
#define EV_TASK_SWTCH_3     0x13	//Switch from a suspended task
#define EV_TASK_SWTCH_4     0x14	//Switch from a deleted task
#define EV_TASK_CREATE      0x15	//A task has been created
#define EV_TASK_DELETE      0x16	//A task has been deleted
#define EV_TASK_WAKEUP      0x17	//A task has been deleted

#define EV_TYPE_QUEUE       0x20	// Queue event type
#define EV_QUEUE_CREATE     0x21
#define EV_QUEUE_RECV       0x22	/* queue receive successful */
#define EV_QUEUE_RECV_F     0x23	/* queue receive failed */
#define EV_QUEUE_RECV_B     0x24	/* blocking on queue receive */
#define EV_QUEUE_SEND       0x25	/* queue send successful */
#define EV_QUEUE_SEND_F     0x26	/* queue send failed */
#define EV_QUEUE_SEND_B     0x27	/* blocking on queue send */
#define EV_QUEUE_SEND_ISR   0x28	/* queue send from isr successful */
#define EV_QUEUE_SEND_ISR_F 0x29	/* queue send from isr failed */

#define EV_TYPE_SWTIMER         0x30	// Software timer event type
#define EV_SWTIMER_CREATE       0x31	// Create a software timer
#define EV_SWTIMER_CB_START     0x32	// Callback start
#define EV_SWTIMER_CB_END       0x33	// Callback end

#define EV_TYPE_INT         0x40	// Interrupt turn off and on event type
#define EV_INT_OFF          0x41	// Turn off from a task
#define EV_INT_ON           0x42	// Turn on from a task
#define EV_INT_OFF_FROM_ISR 0x43	// Turn off from a ISR
#define EV_INT_ON_FROM_ISR  0x44	// Turn on from a ISR

#define EV_TYPE_ISR         0x50	// Interrupt service routine IN and OUT event type
#define EV_ISR_ENTER        0x51	// IN to a ISR
#define EV_ISR_EXIT         0x52	// OUT from a ISR

struct ev_task_t {
	uint32_t type:8, taskno:8, unused:16;
};

struct ev_rec_t {
	uint32_t type:8, unused:24;
};

struct ev_swt_t {
	uint32_t type:8, timerno:16, unused:8;
};

struct ev_int_t {
	uint32_t type:8, duration_low:24;
};

struct ev_isr_t {
	uint32_t type:8, vec:8, unused:16;
};

struct event_t {
	struct ev_task_t event;
	uint32_t dts;
};

extern void pbfr_rec_add_event(struct ev_rec_t *pevent);
extern int pbfr_rec_queue_create(void *handle, int qtype, int qlen, int dlen);
extern void pbfr_rec_queue_change(int qno, int dlen);
extern void pbfr_rec_queue_event(int evtype, int qno, int dlen);
extern void pbfr_rec_int_on(int evtype);
extern void pbfr_rec_coredump_event(unsigned int data);
#ifdef CFG_PBFR_FLIGHT_REC_DEBUG
extern void pbfr_rec_show_records(int ISR);
#endif				/* CFG_PBFR_FLIGHT_REC_DEBUG */
extern void pbfr_rec_int_off(void);
#endif				//_PBFR_REC_H

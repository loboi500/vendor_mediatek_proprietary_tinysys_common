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

#include "FreeRTOS.h"
#include "task.h"
#include <stdio.h>

#ifdef PBFR_SUPPORT_FLIGHT_REC

#include "pbfr_rec.h"
#include "encoding.h"
#include <dma_api.h>

#if ( (!defined(PBFR_MAX_REC_EVENTS)) || (PBFR_MAX_REC_EVENTS >= 256) )
#error "PBFR_MAX_REC_EVENTS has wrong vaule"
#endif

//Definition of Buffer Status
#define BUF_STS_INITED          (1 << 0)
#define BUF_STS_FULL            (1 << 1)
#define BUF_STS_Q_CREATE_FULL   (1 << 2)
#define BUF_STS_SWT_CREATE_FULL (1 << 3)
#define BUF_LONG_SIZE		100 //long buffer for record more trace

struct rec_buffer_t {
	uint32_t buf_len:8, buf_rp:8, buf_wp:8, buf_status:8;
	uint64_t buf[PBFR_MAX_REC_EVENTS];
};

#ifdef CFG_PBFR_LONG_BUFFER
struct dram_rec_buffer_t {
	uint32_t buf_len;
	uint32_t buf[BUF_LONG_SIZE];
};
#endif

#ifdef PBFR_SUPPORT_REC_TASK
struct ev_task_t event_task;
#endif

struct rec_buffer_t pbfr_rec;
/* long buffer can be put to dram or large sram */
#ifdef CFG_PBFR_LONG_BUFFER
#ifdef PBFR_SUPPORT_REC_IN_DRAM
#define DRAM_REGION_PBFR __attribute__ ((section (".pbfr_in_dram")))
DRAM_REGION_PBFR struct dram_rec_buffer_t dram_pbfr_rec;
#else
__attribute__ ((section (".core0_only_data"))) struct dram_rec_buffer_t dram_pbfr_rec;
#endif
#endif

void pbfr_rec_init_buffer(void);
void pbfr_rec_init_buffer(void)
{
	memset(&pbfr_rec, 0, sizeof(pbfr_rec));
#ifdef CFG_PBFR_LONG_BUFFER
#ifdef PBFR_SUPPORT_REC_IN_DRAM
	dvfs_enable_DRAM_resource_from_isr(PBFR_FLIGHT_REC_ID);
	memset(&dram_pbfr_rec, 0, sizeof(dram_pbfr_rec));
	dvfs_disable_DRAM_resource(PBFR_FLIGHT_REC_ID);
#else
	if (uxPortGetCoreId() == 0)
		memset(&dram_pbfr_rec, 0, sizeof(dram_pbfr_rec));
#endif
#endif
	pbfr_rec.buf_len = PBFR_MAX_REC_EVENTS;
	pbfr_rec.buf_status |= BUF_STS_INITED;
	return;
}

void pbfr_rec_insert_buffer(void *pevent_dts);
void pbfr_rec_insert_buffer(void *pevent_dts)
{
	memcpy(&pbfr_rec.buf[pbfr_rec.buf_wp], pevent_dts, 8);
#ifdef CFG_PBFR_LONG_BUFFER
#ifdef PBFR_SUPPORT_REC_IN_DRAM
	scp_dma_transaction_dram((uint32_t)pevent_dts, (uint32_t)&dram_pbfr_rec.buf[dram_pbfr_rec.buf_len++], 8, PBFR_FLIGHT_REC_ID, NO_RESERVED);
	//dvfs_enable_DRAM_resource_from_isr(PBFR_FLIGHT_REC_ID);
	//memcpy(&dram_pbfr_rec.buf[dram_pbfr_rec.buf_len++], pevent_dts, 8);
	//dvfs_disable_DRAM_resource(PBFR_FLIGHT_REC_ID);
#else
	if (uxPortGetCoreId() == 0)
		memcpy(&dram_pbfr_rec.buf[dram_pbfr_rec.buf_len++], pevent_dts, 8);
#endif
#endif

	pbfr_rec.buf_wp++;
	if (pbfr_rec.buf_wp >= pbfr_rec.buf_len) {
		pbfr_rec.buf_wp = 0;
	}

	if (pbfr_rec.buf_status & BUF_STS_FULL) {
		pbfr_rec.buf_rp = pbfr_rec.buf_wp;
	} else {
		if (pbfr_rec.buf_wp == pbfr_rec.buf_rp) {
			pbfr_rec.buf_status |= BUF_STS_FULL;
		}
	}
	return;
}

extern unsigned long long pbfr_get_timestamp(int isr_ctx);
extern unsigned long long last_switch_ns[portNUM_CPUS];
unsigned long long last_event_ns;
unsigned long long now_ns, dts;
struct event_t ev_dts;
unsigned char freertos_current_task_no;

void pbfr_rec_add_event(struct ev_rec_t *pevent)
{
	if ((pevent->type >= EV_TASK_SWTCH_1)
			&& (pevent->type <= EV_TASK_SWTCH_4)) {
		/* ts in last_switch_ns */
		now_ns = last_switch_ns[uxPortGetHartId()];
	} else {
		now_ns = pbfr_get_timestamp(1);
	}

	dts = now_ns - last_event_ns;

	//Check if DTS is larger than 32-bits
	if (dts >= 0x100000000) {
		ev_dts.event.type = EV_SYS_DTS_H;
		ev_dts.dts = (dts >> 32);
		ev_dts.event.unused = 0;
		pbfr_rec_insert_buffer(&ev_dts);
	}

	memcpy(&ev_dts.event, pevent, 4);
	ev_dts.dts = (uint32_t) (dts & 0x00000000FFFFFFFF);

	pbfr_rec_insert_buffer(&ev_dts);
	last_event_ns = now_ns;
	return;
}


#ifdef PBFR_SUPPORT_REC_QUEUE
/* ============== Queue Recording ============== */
struct queue_info_t {
	uint32_t qtype:8, qlen:8, dlen:8, unused:8;
	void *handle;
};

static struct queue_info_t q_info[FREERTOS_MAX_QUEUES];
static unsigned char freertos_queue_no = 0;

int pbfr_rec_queue_create(void *handle, int qtype, int qlen, int dlen)
{
	if (freertos_queue_no < FREERTOS_MAX_QUEUES) {
		q_info[freertos_queue_no].qtype = qtype;
		q_info[freertos_queue_no].qlen = qlen;
		q_info[freertos_queue_no].dlen = dlen;
		q_info[freertos_queue_no].handle = handle;
	} else {
		pbfr_rec.buf_status |= BUF_STS_Q_CREATE_FULL;
	}
	freertos_queue_no++;
	return freertos_queue_no;
}

void pbfr_rec_queue_change(int qno, int dlen)
{
	q_info[qno].dlen = dlen;
}

struct queue_event_t {
	uint32_t evtype:8, taskno:8, qno:8, dlen:8;
};

void pbfr_rec_queue_event(int evtype, int qno, int dlen)
{
	struct queue_event_t qev;

	if ((evtype != EV_QUEUE_SEND_ISR) && (evtype != EV_QUEUE_SEND_ISR_F)) {
		taskENTER_CRITICAL();
		qev.taskno = freertos_current_task_no;
	} else {
		qev.taskno = read_taken_INT();
		//qev.taskno = 0;
	}

	qev.evtype = evtype;
	qev.qno = qno;
	qev.dlen = dlen;

	pbfr_rec_add_event((struct ev_rec_t *) &qev);

	if ((evtype != EV_QUEUE_SEND_ISR) && (evtype != EV_QUEUE_SEND_ISR_F)) {
		taskEXIT_CRITICAL();
	}
}
#endif

#ifdef PBFR_SUPPORT_REC_SWTIMER
/* ============== Software Timer Recording ============== */
struct ev_swt_t event_swt;
static unsigned int swt_info[FREERTOS_MAX_SWTIMER];
static unsigned char freertos_swt_no = 0;

int pbfr_rec_swt_create(unsigned int pxNewTimer)
{
	if (freertos_swt_no < FREERTOS_MAX_SWTIMER) {
		swt_info[freertos_swt_no] = pxNewTimer;
	} else {
		pbfr_rec.buf_status |= BUF_STS_SWT_CREATE_FULL;
	}
	freertos_swt_no++;
	return freertos_swt_no - 1;
}
#endif

#ifdef PBFR_SUPPORT_REC_INT
/* ============== INT Recording ============== */
struct ev_int_t event_int;
unsigned long long pbfr_rtos_ts = 0;
unsigned long long pbfr_rtos_ts_diff = 0;
char pbfr_rtos_ts_nesting = 0;

void pbfr_rec_int_off(void)
{
	pbfr_rtos_ts = pbfr_get_timestamp(1);
}

void pbfr_rec_int_on(int evtype)
{
	unsigned long long tt = pbfr_get_timestamp(1);
	pbfr_rtos_ts_diff = tt - pbfr_rtos_ts;
	if (pbfr_rtos_ts_diff > configTRACE_INT_MASK_TIME_BOUND_NS) {
		event_int.type = evtype;
		event_int.duration_low =
		    ((pbfr_rtos_ts_diff & 0x00000000ffffff00) >> 8);
		pbfr_rec_add_event((struct ev_rec_t *) &event_int);
	}
}
#endif				/* PBFR_SUPPORT_REC_INT */

#ifdef PBFR_SUPPORT_REC_ISR
struct ev_isr_t event_isr;
#endif

#ifdef PBFR_SUPPORT_REC_OSTICK
struct ev_rec_t event_os_tick;
#endif

#ifdef CFG_PBFR_FLIGHT_REC_DEBUG
void pbfr_rec_show_records(int ISR)
{
	int i;
	unsigned int *add = (unsigned int *) (&(pbfr_rec.buf[0]));
	if (ISR == 0) {
		taskENTER_CRITICAL();
		printf("REC: 0x%x, 0x%x, 0x%x, 0x%x\n",
		       pbfr_rec.buf_len, pbfr_rec.buf_rp, pbfr_rec.buf_wp,
		       pbfr_rec.buf_status);

		taskEXIT_CRITICAL();
		for (i = 0; i < PBFR_MAX_REC_EVENTS; i++) {
			taskENTER_CRITICAL();
			printf(" %02d: 0x%08x 0x%08x\n", i, *add, *(add + 1));
			taskEXIT_CRITICAL();
			add += 2;
		}
	} else {
		printf("REC: 0x%x, 0x%x, 0x%x, 0x%x\n",
		       pbfr_rec.buf_len, pbfr_rec.buf_rp, pbfr_rec.buf_wp,
		       pbfr_rec.buf_status);
		for (i = 0; i < PBFR_MAX_REC_EVENTS; i++) {
			printf(" 0x%02x: 0x%08x 0x%08x\n", i, *add, *(add + 1));
			add += 2;
		}
	}
}
#endif				/* CFG_PBFR_FLIGHT_REC_DEBUG */

void pbfr_rec_coredump_event(unsigned int data)
{
	struct ev_rec_t ev;
	ev.type = EV_SYS_COREDUMP;
	ev.unused = data;
	pbfr_rec_add_event(&ev);
#ifdef CFG_PBFR_FLIGHT_REC_DEBUG
	pbfr_rec_show_records(1);
#endif				/* CFG_PBFR_FLIGHT_REC_DEBUG */
}

#endif				/* PBFR_SUPPORT_FLIGHT_REC */

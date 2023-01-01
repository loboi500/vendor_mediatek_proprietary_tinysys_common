/* Copyright Statement:
 *
 * This software/firmware and related documentation ("MediaTek Software") are
 * protected under relevant copyright laws. The information contained herein
 * is confidential and proprietary to MediaTek Inc. and/or its licensors.
 * Without the prior written permission of MediaTek inc. and/or its licensors,
 * any reproduction, modification, use or disclosure of MediaTek Software,
 * and information contained herein, in whole or in part, shall be strictly
 * prohibited.
 */
/* MediaTek Inc. (C) 2019. All rights reserved.
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
 * THIRD PARTY FOR ANY WARRANTY CLAIM RELATING THERETO. RECEIVER EXPRESSLY
 * ACKNOWLEDGES THAT IT IS RECEIVER\'S SOLE RESPONSIBILITY TO OBTAIN FROM ANY
 * THIRD PARTY ALL PROPER LICENSES CONTAINED IN MEDIATEK SOFTWARE. MEDIATEK
 * SHALL ALSO NOT BE RESPONSIBLE FOR ANY MEDIATEK SOFTWARE RELEASES MADE TO
 * RECEIVER\'S SPECIFICATION OR TO CONFORM TO A PARTICULAR STANDARD OR OPEN
 * FORUM. RECEIVER\'S SOLE AND EXCLUSIVE REMEDY AND MEDIATEK\'S ENTIRE AND
 * CUMULATIVE LIABILITY WITH RESPECT TO THE MEDIATEK SOFTWARE RELEASED HEREUNDER
 * WILL BE, AT MEDIATEK\'S OPTION, TO REVISE OR REPLACE THE MEDIATEK SOFTWARE
 * AT ISSUE, OR REFUND ANY SOFTWARE LICENSE FEES OR SERVICE CHARGE PAID BY
 * RECEIVER TO MEDIATEK FOR SUCH MEDIATEK SOFTWARE AT ISSUE.
 *
 * The following software/firmware and/or related documentation
 * ("MediaTek Software") have been modified by MediaTek Inc. All revisions are
 * subject to any receiver\'s applicable license agreements with MediaTek Inc.
 */

#include <stdio.h>
#include <string.h>
#include <driver_api.h>
#include "FreeRTOS.h"
#include "ipi_id.h"
#include "ipi.h"


static void ipi_isr_cb(unsigned int ipi_id);

/*
 * the function should be implemented by tinysys porting layer
 */
void ipi_notify_receiver(int ipi_id);
void ipi_mdelay(unsigned long ms);

SemaphoreHandle_t mutex_ipi_reg;
ipi_pin_t ipi_pin[IPI_COUNT];


#ifdef IPI_MONITOR
unsigned long long ipi_get_ts(void);

enum ipi_stage {
	SEND_MSG,
	ISR_RECV_MSG,
	RECV_MSG,
	SEND_ACK,
	ISR_RECV_ACK,
	RECV_ACK,
	UNUSED = 0xF,
};

struct ipi_monitor {
	unsigned int stage:4,	/* transmission stage for t0~t5 */
			seqno:28;	/* sequence count of the IPI pin processed */
	unsigned long long ts[3];
};
static struct ipi_monitor ipimon[IPI_COUNT];
static int ipi_lastone;

static void ipi_monitor(unsigned int id, int stage)
{
	switch (stage) {
#ifdef IPI_MONITOR_LOG
	case SEND_MSG:
		ipimon[id].seqno ++;
		ipimon[id].ts[0] = ipi_get_ts();
		ipimon[id].ts[1] = 0;
		ipimon[id].ts[2] = 0;
		PRINTF_I("IPI_%d send msg(#%d)\n", id, ipimon[id].seqno);
		break;
	case ISR_RECV_MSG:
		ipimon[id].seqno ++;
		ipimon[id].ts[0] = ipi_get_ts();
		ipimon[id].ts[1] = 0;
		ipimon[id].ts[2] = 0;
		break;
	case RECV_MSG:
		ipimon[id].ts[1] = ipi_get_ts();
		PRINTF_I("IPI_%d recv msg(#%d)\n", id, ipimon[id].seqno);
		break;
	case SEND_ACK:
		ipimon[id].ts[2] = ipi_get_ts();
		PRINTF_I("IPI_%d send ack(#%d)\n", id, ipimon[id].seqno);
		break;
	case ISR_RECV_ACK:
		ipimon[id].ts[1] = ipi_get_ts();
		break;
	case RECV_ACK:
		ipimon[id].ts[2] = ipi_get_ts();
		PRINTF_I("IPI_%d recv ack(#%d)\n", id, ipimon[id].seqno);
		break;
#else
	case SEND_MSG:
	case ISR_RECV_MSG:
		ipimon[id].seqno ++;
		ipimon[id].ts[0] = ipi_get_ts();
		ipimon[id].ts[1] = 0;
		ipimon[id].ts[2] = 0;
		break;
	case RECV_MSG:
	case ISR_RECV_ACK:
		ipimon[id].ts[1] = ipi_get_ts();
		break;
	case SEND_ACK:
	case RECV_ACK:
		ipimon[id].ts[2] = ipi_get_ts();
		break;
#endif
	default:
		break;
	}

	ipimon[id].stage = stage;
}
#endif

void ipi_init(void)
{
	unsigned int i;

	mutex_ipi_reg = xSemaphoreCreateMutex();

	for (i = 0; i < TOTAL_SEND_PIN; i++) {
		if (mbox_pin_send_table[i].ipi_id >= IPI_COUNT)
			continue;

		mbox_pin_send_table[i].mutex = xSemaphoreCreateMutex();
		ipi_pin[mbox_pin_send_table[i].ipi_id].s_pin =
		    &mbox_pin_send_table[i];
	}

	for (i = 0; i < TOTAL_RECV_PIN; i++) {
		if (mbox_pin_recv_table[i].ipi_id >= IPI_COUNT)
			continue;

		mbox_pin_recv_table[i].notify = xSemaphoreCreateBinary();
		ipi_pin[mbox_pin_recv_table[i].ipi_id].r_pin =
		    &mbox_pin_recv_table[i];
	}

	mbox_isr_hook(ipi_isr_cb);

#ifdef IPI_MONITOR
	for (i = 0; i < IPI_COUNT; i++) {
		ipimon[i].stage = UNUSED;
		ipimon[i].seqno = 0;
	}
	ipi_lastone = -1;
#endif
}

int ipi_register(unsigned int ipi_id, void *cb, void *prdata, void *msg)
{
	int ret;
	struct pin_recv *pin = NULL;

	if (ipi_id >= (unsigned int) IPI_COUNT)
		return IPI_ILLEGAL;

	if (msg == NULL)
		return IPI_NO_MSGBUF;

	pin = ipi_pin[ipi_id].r_pin;
	if (pin == NULL)
		return IPI_UNAVAILABLE;

	ret = xSemaphoreTake(mutex_ipi_reg, portMAX_DELAY);
	if(ret != pdTRUE)
	{
        	PRINTF_E("IPI register fail (%d)\n", ret);
        	return IPI_NO_MEMORY;
	}

	if (pin->pin_buf != NULL) {
		xSemaphoreGive(mutex_ipi_reg);
		return IPI_DUPLEX;
	}
	pin->mbox_pin_cb = cb;
	pin->pin_buf = msg;
	pin->prdata = prdata;

	xSemaphoreGive(mutex_ipi_reg);

	return IPI_ACTION_DONE;
}

int ipi_unregister(unsigned int ipi_id)
{
	int ret;
	struct pin_recv *pin = NULL;

	if (ipi_id >= (unsigned int) IPI_COUNT)
		return IPI_ILLEGAL;

	pin = ipi_pin[ipi_id].r_pin;
	if (pin == NULL)
		return IPI_UNAVAILABLE;

	ret = xSemaphoreTake(mutex_ipi_reg, portMAX_DELAY);
	if(ret != pdTRUE)
	{
        	PRINTF_E("IPI unregister fail (%d)\n", ret);
	        return IPI_NO_MEMORY;
	}

	pin->mbox_pin_cb = NULL;
	pin->pin_buf = NULL;
	pin->prdata = NULL;

	xSemaphoreGive(mutex_ipi_reg);

	return IPI_ACTION_DONE;
}

static int _mbox_send(struct pin_send *pin, void *data,
		      unsigned int slot_len, unsigned long retry)
{
	int ret;

	if (slot_len > pin->msg_size)
		return IPI_NO_MEMORY;
	else if (slot_len == 0)
		slot_len = pin->msg_size;

	/* Confirm this mbox pin is vacanted before send */
	taskENTER_CRITICAL();
	ret = mbox_check_send_irq(pin->mbox, pin->pin_index);
	taskEXIT_CRITICAL();

	while (retry > 0 && ret != 0) {
		ipi_mdelay(1);
		retry--;
		taskENTER_CRITICAL();
		ret = mbox_check_send_irq(pin->mbox, pin->pin_index);
		taskEXIT_CRITICAL();
	}

	if (ret != 0)
		return IPI_PIN_BUSY;

	// TODO: clz before write

	taskENTER_CRITICAL();
	ret = mbox_write(pin->mbox, pin->offset, data, slot_len * MBOX_SLOT_SIZE);
	taskEXIT_CRITICAL();

	if (ret != MBOX_DONE)
		return IPI_MBOX_ERR;

	ipi_notify_receiver(pin->ipi_id);

	taskENTER_CRITICAL();
	ret = mbox_trigger_irq(pin->mbox, 0x1 << pin->pin_index);
	taskEXIT_CRITICAL();

	if (ret != MBOX_DONE)
		return IPI_MBOX_ERR;

	return IPI_ACTION_DONE;
}

static int _ipi_send(struct pin_send *pin, void *data, int len,
			unsigned long retry)
{
	int ret;

	ret = xSemaphoreTake(pin->mutex, portMAX_DELAY);
	if(ret != pdTRUE)
 	{
        	PRINTF_E("IPI send fail (%d)\n", ret);
	        return IPI_NO_MEMORY;
	}

	ret = _mbox_send(pin, data, len, retry);

#ifdef IPI_MONITOR
	if (ret == IPI_ACTION_DONE)
		ipi_monitor(pin->ipi_id, SEND_MSG);
#endif

	return ret;
}

static int _ipi_recv(unsigned int id, portTickType t, enum MBOX_RECV_OPT opt, bool done)
{
	struct pin_recv *pin = NULL;
	int ret;

	pin = ipi_pin[id].r_pin;
	if (pin == NULL)
		return IPI_UNAVAILABLE;

	/* receive the ipi from ISR */
	ret = xSemaphoreTake(pin->notify, t);

	if (ret == pdFALSE)
		return IPI_RECV_TIMEOUT;
	else {
		if (opt == MBOX_RECV_MESSAGE) {
#ifdef IPI_MONITOR
			ipi_monitor(id, RECV_MSG);
#endif
			/* do user callback */
			if ( pin->mbox_pin_cb && pin->cb_ctx_opt == MBOX_CB_IN_TASK)
				pin->mbox_pin_cb(id, pin->prdata, pin->pin_buf, pin->msg_size);
		}
#ifdef IPI_MONITOR
		else {
			ipi_monitor(id, RECV_ACK);
		}
		if (done)
			ipi_lastone = id;
#endif
	}

	return IPI_ACTION_DONE;
}

int ipi_send(unsigned int ipi_id, void *data, int len, unsigned long retry_timeout)
{
	struct pin_send *pin = NULL;
	int ret;
	
	if (ipi_id < IPI_COUNT)
		pin = ipi_pin[ipi_id].s_pin;
	if (pin == NULL)
		return IPI_UNAVAILABLE;

	ret = _ipi_send(pin, data, len, retry_timeout);

#ifdef IPI_MONITOR
	if (ret == IPI_ACTION_DONE)
		ipi_lastone = ipi_id;
#endif
	xSemaphoreGive(pin->mutex);

	return ret;
}

int ipi_recv(unsigned int ipi_id)
{
	return _ipi_recv(ipi_id, portMAX_DELAY, MBOX_RECV_MESSAGE, 1);
}

int ipi_send_compl(unsigned int ipi_id, void *data, int len,
		   unsigned long retry_timeout, unsigned long response_timeout)
{
	struct pin_send *pin;
	portTickType ms_to_tick;
	int ret;

	pin = ipi_pin[ipi_id].s_pin;
	if (pin == NULL)
		return IPI_UNAVAILABLE;

	/* send the IPI message */
	ret = _ipi_send(pin, data, len, retry_timeout);

	if (ret != IPI_ACTION_DONE) {
		xSemaphoreGive(pin->mutex);
		return ret;
	}

	/* wait for response */
	/* not use ' / portTICK_RATE_MS ' due to some tinysys not support float */
	ms_to_tick = response_timeout * configTICK_RATE_HZ / 1000;
	ret = _ipi_recv(ipi_id, ms_to_tick, MBOX_RECV_ACK, 1);

#if defined(IPI_MONITOR) && defined(IPI_MONITOR_LOG)
	if (ret == IPI_RECV_TIMEOUT) {
		PRINTF_E("IPI %d T.O @%lld (last done is IPI %d)\n",
			ipi_id, ipi_get_ts(), ipi_lastone);

		PRINTF_E(" seqno=%d, state=%d, t0=%lld, t4=%lld, t5=%lld\n",
			ipimon[ipi_id].seqno, ipimon[ipi_id].stage,
			ipimon[ipi_id].ts[0], ipimon[ipi_id].ts[1], ipimon[ipi_id].ts[2]);
	}
#endif

	xSemaphoreGive(pin->mutex);

	return ret;
}

int ipi_recv_reply(unsigned int ipi_id, void *reply_data, int len)
{
	struct pin_send *pin;
	int ret;

	pin = ipi_pin[ipi_id].s_pin;
	if (pin == NULL)
		return IPI_UNAVAILABLE;

	/* recvice the IPI message */
	ret = _ipi_recv(ipi_id, portMAX_DELAY, MBOX_RECV_MESSAGE, 0);
	if (ret != IPI_ACTION_DONE)
		return ret;

	/* send the response */
	ret = _mbox_send(pin, reply_data, (reply_data == NULL) ? 0 : len, 0);


	if (ret != IPI_ACTION_DONE)
		PRINTF_E("IPI %d reply fail (%d)\n", ipi_id, ret);
#ifdef IPI_MONITOR
	else {
		ipi_monitor(ipi_id, SEND_ACK);
		ipi_lastone = ipi_id;
	}
#endif

	return ret;
}

static void ipi_isr_cb(unsigned int ipi_id)
{
	static BaseType_t xTaskWoken = 0;
	struct pin_recv *pin = NULL;

	if (ipi_id >= IPI_COUNT)
		return;

	pin = ipi_pin[ipi_id].r_pin;
	if (pin == NULL) {
		PRINTF_E("RECV PIN NOT DEFINED\n");
		return;
	}

	xSemaphoreGiveFromISR(pin->notify, &xTaskWoken);

#ifdef IPI_MONITOR
	if (pin->recv_opt == MBOX_RECV_MESSAGE) {
		// recv msg
		ipi_monitor(ipi_id, ISR_RECV_MSG);
	} else {
		// recv response
		ipi_monitor(ipi_id, ISR_RECV_ACK);
	}
#endif

	portYIELD_FROM_ISR(xTaskWoken);

	return;
}

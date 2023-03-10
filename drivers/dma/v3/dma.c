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

/*   Kernel includes. */
#include "FreeRTOS.h"
#include <stdio.h>
#include <driver_api.h>
#include <dma.h>
#include <string.h>
#include "irq.h"
#ifdef CFG_XGPT_SUPPORT
#include <xgpt.h>
#endif
#ifdef CFG_MPU_DEBUG_SUPPORT
#include "mpu_mtk.h"
#endif

static unsigned int g_dma_base;

static int semaphore_count = 0;
/*
 * global variables
 */
struct dma_channel_t dma_channel[NR_GDMA_CHANNEL];

/*
 * get_emi_semaphore: must get emi resource before access emi
 */
/* Althoug it will access infra bus here.
 * There is no need to enable infra clock here,
 * the infra clock already enable in prevous function call.
 * */
void get_emi_semaphore()
{
#ifdef CFG_MPU_DEBUG_SUPPORT
	disable_dram_protector();
#endif
	semaphore_count++;
}

/*
 * release_emi_semaphore: must release emi resource after emi access
 */
/* Althoug it will access infra bus here.
 * There is no need to enable infra clock here,
 * the infra clock already enable in prevous function call.
 * */
void release_emi_semaphore()
{
	semaphore_count--;
#ifdef CFG_MPU_DEBUG_SUPPORT
	enable_dram_protector();
#endif
}

/*
 * mt_req_gdma: request a general DMA.
 * @chan: specify a channel or not
 * Return channel number for success; return negative error code for failure.
 */
int32_t mt_req_gdma(int32_t chan)
{
	int32_t i;
#ifndef CFG_FREERTOS_SMP
	bool in_isr;
#else
	UBaseType_t uxSavedInterruptStatus;
#endif

	if ((chan < 0) || (chan >= (NR_GDMA_CHANNEL)))
		return DMA_RESULT_INVALID_CH;
#ifndef CFG_FREERTOS_SMP
	in_isr = is_in_isr();
	if (!in_isr)
		taskENTER_CRITICAL();
#else
	uxSavedInterruptStatus = taskENTER_CRITICAL_FROM_ISR();
#endif

	if (dma_channel[chan].in_use) {
		PRINTF_E("[DMA]ch%d in use\n", chan);
#ifndef CFG_FREERTOS_SMP
		if (!is_in_isr())
			taskEXIT_CRITICAL();
#else
		taskEXIT_CRITICAL_FROM_ISR(uxSavedInterruptStatus);
#endif
		return -DMA_RESULT_RUNNING;
	} else {
		i = chan;
		dma_channel[chan].in_use = 1;
		dma_channel[chan].ch_usage_count++;
		dma_channel[chan].task_handle = xTaskGetCurrentTaskHandle();
	}
#ifndef CFG_FREERTOS_SMP
	if (!in_isr)
		taskEXIT_CRITICAL();
#else
	taskEXIT_CRITICAL_FROM_ISR(uxSavedInterruptStatus);
#endif

	mt_reset_gdma_conf(i);
	return i;
}

/*
 * mt_check_dma_channel: check DMA channel whether in use or not
 * @chan: specify a channel or not
 * Return 0 channel no use
 * Return negative value identify error code
 */
int32_t mt_check_dma_channel(int32_t chan)
{
	int32_t i;
#ifndef CFG_FREERTOS_SMP
	bool in_isr;
#else
	UBaseType_t uxSavedInterruptStatus;
#endif

	if ((chan < 0) || (chan >= (NR_GDMA_CHANNEL)))
		return DMA_RESULT_INVALID_CH;
#ifndef CFG_FREERTOS_SMP
	in_isr = is_in_isr();
	if (!in_isr)
		taskENTER_CRITICAL();
#else
	uxSavedInterruptStatus = taskENTER_CRITICAL_FROM_ISR();
#endif

	if (dma_channel[chan].in_use) {
		PRINTF_E("[DMA]ch %d in use\n", chan);
#ifndef CFG_FREERTOS_SMP
		if (!is_in_isr())
			taskEXIT_CRITICAL();
#else
		taskEXIT_CRITICAL_FROM_ISR(uxSavedInterruptStatus);
#endif

		return -DMA_RESULT_RUNNING;
	} else {
		i = chan;
	}
#ifndef CFG_FREERTOS_SMP
	if (!in_isr)
		taskEXIT_CRITICAL();
#else
	taskEXIT_CRITICAL_FROM_ISR(uxSavedInterruptStatus);
#endif

	return i;
}

/*
 * mt_start_gdma: start the DMA stransfer for the specified GDMA channel
 * @channel: GDMA channel to start
 * Return 0 for success; return negative errot code for failure.
 */
int32_t mt_start_gdma(int32_t channel)
{
	uint32_t enable_channel;

	enable_channel = DMA_BASE_CH(channel);

	if ((channel < GDMA_START)
	    || (channel >= (GDMA_START + NR_GDMA_CHANNEL)))
		return DMA_RESULT_INVALID_CH;

	drv_write_reg32(DMA_ACKINT(enable_channel), DMA_ACK_BIT);
	drv_write_reg32(DMA_START(enable_channel), DMA_START_BIT);

	return 0;
}

/*
 * mt_polling_gdma: wait the DMA to finish for the specified GDMA channel
 * @channel: GDMA channel to polling
 * @timeout: polling timeout in ms
 * Return 0 for success; return negative errot code for failure.
 */
int32_t mt_polling_gdma(int32_t channel, uint32_t timeout)
{
	if (channel < GDMA_START)
		return DMA_RESULT_INVALID_CH;

	if (channel >= (GDMA_START + NR_GDMA_CHANNEL))
		return DMA_RESULT_INVALID_CH;

	if (dma_channel[channel].in_use == 0)
		return DMA_RESULT_CH_FREE;

	while (drv_reg32(DMA_RLCT(DMA_BASE_CH(channel))) != 0) {
		timeout--;
		if (!timeout) {
			PRINTF_E("DMA%d polling timeout!!!!\n", channel + 1);
			return 1;
		}
	}

	return 0;
}

/*
 * mt_stop_gdma: stop the DMA stransfer for the specified GDMA channel
 * @channel: GDMA channel to stop
 * Return 0 for success; return negative errot code for failure.
 */
int32_t mt_stop_gdma(int32_t channel)
{
	uint32_t stop_channel;

	stop_channel = DMA_BASE_CH(channel);
	if (channel < GDMA_START)
		return DMA_RESULT_INVALID_CH;

	if (channel >= (GDMA_START + NR_GDMA_CHANNEL))
		return DMA_RESULT_INVALID_CH;

	if (dma_channel[channel].in_use == 0)
		return DMA_RESULT_CH_FREE;

	drv_clr_reg32(DMA_START(stop_channel), DMA_START_CLR_BIT);
	drv_set_reg32(DMA_ACKINT(stop_channel), DMA_ACK_BIT);

	return 0;
}

/*
 * Stop DMA and then clear DMA interrupt.
 */
void mt_cleanup_dma_operations(void)
{
	int32_t channel = 0;

	for (channel = 0; channel < NR_GDMA_CHANNEL; channel++) {
		uint32_t base = DMA_BASE_CH(channel);
		drv_clr_reg32(DMA_START(base), DMA_START_CLR_BIT);
		drv_set_reg32(DMA_ACKINT(base), DMA_ACK_BIT);
	}
}

/*
 * mt_config_gdma: configure the given GDMA channel.
 * @channel: GDMA channel to configure
 * @config: pointer to the mt_gdma_conf structure in which the GDMA configurations store
 * @flag: ALL, SRC, DST, or SRC_AND_DST.
 * Return 0 for success; return negative errot code for failure.
 */
int32_t mt_config_gdma(int32_t channel, struct mt_gdma_conf * config,
		       DMA_CONF_FLAG flag)
{
	uint32_t dma_con = 0x0;
	uint32_t config_channel;
	uint32_t len = 0;

	config_channel = DMA_BASE_CH(channel);

	if ((channel < GDMA_START)
	    || (channel >= (GDMA_START + NR_GDMA_CHANNEL))) {
		PRINTF_E("DMA%d ch err\n", (int) channel);
		return DMA_RESULT_INVALID_CH;
	}

	if (dma_channel[channel].in_use == 0) {
		PRINTF_E("DMA%d in_use=0\n", (int) channel);
		return DMA_RESULT_CH_FREE;
	}

	if (!config) {
		PRINTF_E("DMA%d cfg null\n", (int) channel);
		return DMA_RESULT_ERROR;
	}

	if (config->count > MAX_TRANSFER_LEN) {
		PRINTF_E("DMA%d count:0x%x over 0x%x\n", (int) channel,
			 config->count, MAX_TRANSFER_LEN);
		return DMA_RESULT_ERROR;
	}

	if (config->limiter) {
		PRINTF_E("DMA%d counter over 0x%x\n", (int) channel,
			 MAX_SLOW_DOWN_CNTER);
		return DMA_RESULT_ERROR;
	}

	switch (flag) {
	case ALL:
		/* Wrap src/dst address if accessing range is out of addr[25:0] */
		len = config->count << config->size_per_count;
		/* if address is out of range, addr > addr + len */
		if (!config->wpen && ((config->src & MAX_ADDR_MASK) >
		    ((config->src + len) & MAX_ADDR_MASK))){
			config->wpen = 1;
			config->wpsd = 0;
			config->wpto = (config->src & ~(MAX_ADDR_MASK)) +
				       (0x1 << MAX_ADDR_BIT);
			config->wplen = (config->wpto - config->src) >>
					config->size_per_count;
		} else if (!config->wpen && ((config->dst & MAX_ADDR_MASK) >
			  ((config->dst + len) & MAX_ADDR_MASK))){
			config->wpen = 1;
			config->wpsd = 1;
			config->wpto = (config->dst & ~(MAX_ADDR_MASK)) +
				       (0x1 << MAX_ADDR_BIT);
			config->wplen = (config->wpto - config->dst) >>
					config->size_per_count;
		}

		/* Control Register */
		drv_write_reg32(DMA_SRC(config_channel), config->src);
		drv_write_reg32(DMA_DST(config_channel), config->dst);
		drv_write_reg32(DMA_COUNT(config_channel),
				(config->count & MAX_TRANSFER_LEN));

		if (config->wpen) {
			dma_con |= DMA_CON_WPEN;
		}
		if (config->wpsd) {
			dma_con |= DMA_CON_WPSD;
		}
		if (config->wplen) {
			drv_write_reg32(DMA_WPPT(config_channel),
					config->wplen);
		}
		if (config->wpto) {
			drv_write_reg32(DMA_WPTO(config_channel), config->wpto);
		}

		if (config->iten) {
			dma_channel[channel].isr_cb = config->isr_cb;
			dma_channel[channel].data = config->data;
		} else {
			dma_channel[channel].isr_cb = NULL;
			dma_channel[channel].data = NULL;
		}
		dma_channel[channel].flag = config->flag;

		dma_con |= (config->sinc << DMA_CON_SINC);
		dma_con |= (config->dinc << DMA_CON_DINC);
		dma_con |= (config->size_per_count << DMA_CON_SIZE);
		dma_con |= (config->burst << DMA_CON_BURST);
		dma_con |= (config->iten << DMA_CON_ITEN);

		drv_write_reg32(DMA_CON(config_channel), dma_con);
		break;

	case SRC:
		drv_write_reg32(DMA_SRC(config_channel), config->src);

		break;

	case DST:
		drv_write_reg32(DMA_DST(config_channel), config->dst);
		break;

	case SRC_AND_DST:
		drv_write_reg32(DMA_SRC(config_channel), config->src);
		drv_write_reg32(DMA_DST(config_channel), config->dst);
		break;

	default:
		break;
	}

	return 0;
}

/*
 * mt_free_gdma: free a general DMA.
 * @channel: channel to free
 * Return 0 for success; return negative errot code for failure.
 */
int32_t mt_free_gdma(int32_t chan)
{

#ifndef CFG_FREERTOS_SMP
	bool in_isr;
#else
	UBaseType_t uxSavedInterruptStatus;
#endif

	if (chan < GDMA_START)
		return DMA_RESULT_INVALID_CH;

	if (chan >= (GDMA_START + NR_GDMA_CHANNEL))
		return DMA_RESULT_INVALID_CH;

	if (dma_channel[chan].in_use == 0)
		return DMA_RESULT_CH_FREE;

	mt_stop_gdma(chan);
#ifndef CFG_FREERTOS_SMP
	in_isr = is_in_isr();
	if (!in_isr)
		taskENTER_CRITICAL();

#else
	uxSavedInterruptStatus = taskENTER_CRITICAL_FROM_ISR();
#endif

	dma_channel[chan].isr_cb = NULL;
	dma_channel[chan].data = NULL;
	dma_channel[chan].in_use = 0;
	dma_channel[chan].flag = SYNC;
	dma_channel[chan].task_handle = NULL;

#if DMA_KEEP_AWAKE
	dma_wake_unlock();
#endif
#ifndef CFG_FREERTOS_SMP
	if (!in_isr)
		taskEXIT_CRITICAL();
#else
	taskEXIT_CRITICAL_FROM_ISR(uxSavedInterruptStatus);
#endif

	return 0;
}

/*
 * mt_dump_gdma: dump registers for the specified GDMA channel
 * @channel: GDMA channel to dump registers
 * Return 0 for success; return negative errot code for failure.
 */
int32_t mt_dump_gdma(int32_t channel)
{
	uint32_t i, reg;
	uint32_t j = 0;

	for (i = 0; i <= GDMA_REG_BANK_SIZE; i += 4) {
		reg = drv_reg32(DMA_BASE_CH(channel) + i);
		PRINTF_D("0x%x ", reg);

		if (j++ >= 3)
			j = 0;
	}

	return 0;
}

/*
 * mt_request_dma_channel: request free DMA channel,
 * Return 0~X for get DMA free channel;
 * Return DMA_RESULT_NO_FREE_CH info. no free channel;
 */
int32_t mt_request_dma_channel(int8_t dma_id)
{
	int32_t i;
	int32_t free_channel;
#ifndef CFG_FREERTOS_SMP
	bool in_isr;
#else
	UBaseType_t uxSavedInterruptStatus;
#endif

	free_channel = DMA_RESULT_NO_FREE_CH;
	for (i = NR_GDMA_CHANNEL - 1; i >= 0; i--) {
		/*skip reserved channel */
		if (RESERVED_DMA_CHANNEL & 0x1 << i)
			continue;
#ifndef CFG_FREERTOS_SMP
		in_isr = is_in_isr();
		if (!in_isr)
			taskENTER_CRITICAL();
#else
		uxSavedInterruptStatus = taskENTER_CRITICAL_FROM_ISR();
#endif

		/*get free channel */
		if (dma_channel[i].in_use == 0) {
			free_channel = i;
			dma_channel[i].dma_id = dma_id;
			dma_channel[i].in_use = 1;
			dma_channel[i].ch_usage_count++;
			dma_channel[i].task_handle = xTaskGetCurrentTaskHandle();
		}
#ifndef CFG_FREERTOS_SMP
		if (!in_isr)
			taskEXIT_CRITICAL();
#else
		taskEXIT_CRITICAL_FROM_ISR(uxSavedInterruptStatus);
#endif

		if (free_channel >= 0) {
			/*some dma id keep channel, keep scp awake */
#if DMA_KEEP_AWAKE
			dma_wake_lock();
#endif
			break;
		}
	}

	return free_channel;
}

/*
 * mt_reset_gdma_conf: reset the config of the specified DMA channel
 * @iChannel: channel number of the DMA channel to reset
 */
void mt_reset_gdma_conf(const uint32_t iChannel)
{
	struct mt_gdma_conf conf;

	memset(&conf, 0, sizeof (struct mt_gdma_conf));
	if (mt_config_gdma(iChannel, &conf, ALL) != 0)
		return;

	return;
}

static unsigned int mt_dma_irqhandler(void *data)
{
	BaseType_t xHigherPriorityTaskWoken;
	int32_t channel;
	uint64_t duration;

	xHigherPriorityTaskWoken = pdFALSE;

	/*scan all channel status */
	for (channel = NR_GDMA_CHANNEL - 1; channel >= 0; channel--) {
		if (drv_reg32(DMA_INSTSTA(DMA_BASE_CH(channel)))) {
			if (dma_channel[channel].isr_cb) {
#ifdef CFG_XGPT_SUPPORT
				dma_channel[channel].last_enter =
				    read_xgpt_stamp_ns();
#endif
				dma_channel[channel].
				    isr_cb(dma_channel[channel].data);
#ifdef CFG_XGPT_SUPPORT
				dma_channel[channel].last_exit =
				    read_xgpt_stamp_ns();
#endif
				duration =
				    dma_channel[channel].last_exit -
				    dma_channel[channel].last_enter;
				/* handle the xgpt overflow case
				 * discard the duration time when exit time < enter time
				 * */
				if (dma_channel[channel].last_exit >
				    dma_channel[channel].last_enter) {
					if (duration >
					    dma_channel[channel].max_duration)
						dma_channel[channel].
						    max_duration = duration;
				}
			}

			if (dma_channel[channel].flag == SYNC) {
				vTaskNotifyGiveFromISR(dma_channel[channel].task_handle,
						       &xHigherPriorityTaskWoken);
			}
			mt_free_gdma(channel);
#ifdef CFG_DMA_STRIDE_MODE_SUPPORT
			/* clean stride dma cfg */
			drv_write_reg32(DMA_STRIDE_CFG(DMA_BASE_CH(channel)), 0);
			drv_write_reg32(DMA_STRIDE(DMA_BASE_CH(channel)), 0);
#endif
		}
	}

	portYIELD_FROM_ISR(xHigherPriorityTaskWoken);

	return 0;
}

/*
 * mt_addr_check: If dma accessing range of src/dst are both out of addr[25:0],
 * divided transaction into 2 part. First half will finish in this function,
 * second half will back to origin dma_transaction.
 */
void mt_addr_check(uint32_t *dst_addr, uint32_t *src_addr, uint32_t *len,
		   int8_t dma_id, int32_t ch)
{
	unsigned int wpto = 0, wplen = 0;

	if (((*src_addr + *len) & MAX_ADDR_MASK) == 0 ||
	    (((*src_addr + *len) & MAX_ADDR_MASK) > (*src_addr & MAX_ADDR_MASK)))
		return;

	if (((*dst_addr + *len) & MAX_ADDR_MASK) == 0 ||
	    (((*dst_addr + *len) & MAX_ADDR_MASK) > (*dst_addr & MAX_ADDR_MASK)))
		return;

	/* accessing range of src/dst are both out of addr[25:0],
	 * using valid src range as the fisrt part of dma_transaction
	 */
	wpto = (*src_addr & ~(MAX_ADDR_MASK)) + (0x1 << MAX_ADDR_BIT);
	wplen = wpto - *src_addr;
	/* never execute isr_cb function before whole transaction finished,
	 * isr_cb must be null and flag must be SYNC to ensure order
	 */
	dma_transaction(*dst_addr, *src_addr, wplen, dma_id, ch, NULL, SYNC);
	*len -= wplen;
	*dst_addr += wplen;
	*src_addr += wplen;

	return;
}

/*
 * dma_transaction: dma transaction function
 * @dst_addr: to the destination address
 * @src_addr: from the source address
 * @len: data lengh
 * @dma_id: user id
 * @ch: revserved channel or NO_RESERVED
 * @isr_cb: isr callback function
 * @flag: sync or async
 * Return 0 for DMA_DONE, 1 for DMA_RUNNING; return negative error code for failure.
 */
DMA_RESULT dma_transaction(uint32_t dst_addr, uint32_t src_addr, uint32_t len,
			   int8_t dma_id, int32_t ch, void (*isr_cb) (void *),
			   DMA_SYNC flag)
{
	struct mt_gdma_conf conf;
	DMA_RESULT ret;
	int32_t size_per_count;
	int32_t dma_ch;

	if (len == 0) {
		PRINTF_E("DMA len=0\n");
		ret = DMA_RESULT_ERROR;
		goto _exit;
	}

	if (len > MAX_DMA_TRAN_SIZE) {
		PRINTF_E("DMA len>MAX\n");
		ret = DMA_RESULT_ERROR;
		goto _exit;
	}

	if ((dma_id < 0) || (dma_id >= NUMS_DMA_ID)) {
		PRINTF_E("DMA ID err\n");
		ret = DMA_RESULT_ERROR;
		goto _exit;
	}

	if (is_in_isr() && flag == SYNC) {
		PRINTF_E("DMA id:%d can't in isr\n", dma_id);
		configASSERT(0);
	}

	mt_addr_check(&dst_addr, &src_addr, &len, dma_id, ch);

	memset(&conf, 0, sizeof (struct mt_gdma_conf));

	/*get free dma channel */
	if (ch != NO_RESERVED) {
		/*specific ch */
		dma_ch = mt_req_gdma(ch);
	} else {
		dma_ch = mt_request_dma_channel(dma_id);
		while (dma_ch < 0) {
			dma_ch = mt_request_dma_channel(dma_id);
			/*in isr or critical context */
			if (is_in_isr() || xGetCriticalNesting() > 0) {
				ret = DMA_RESULT_NO_FREE_CH;
				goto _exit;
			}
		}
	}

	if (dma_ch < 0) {
		PRINTF_E("DMA req ch:%d fail\n", dma_ch);
		ret = DMA_RESULT_INVALID_CH;
		goto _exit;
	}

	/* set iten before enter dma critical section */
#if DMA_INTERRUPT_MODE
	/*in isr or critical context */
	if (is_in_isr() || xGetCriticalNesting() > 0)
		conf.iten = DMA_FALSE;
	else
		conf.iten = DMA_TRUE;
#else
	conf.iten = DMA_FALSE;
#endif
	taskENTER_CRITICAL();

	/* check count value */
	if (dst_addr & 0x1 || src_addr & 0x1 || len & 0x1) {
		conf.size_per_count = DMA_CON_SIZE_1BYTE;
		size_per_count = 1;
		conf.burst = DMA_CON_BURST_BEAT;
	} else if (dst_addr & 0x2 || src_addr & 0x2 || len & 0x2) {
		conf.size_per_count = DMA_CON_SIZE_2BYTE;
		size_per_count = 2;
		conf.burst = DMA_CON_BURST_BEAT;
	} else {
		conf.size_per_count = DMA_CON_SIZE_4BYTE;
		size_per_count = 4;
		conf.burst = DMA_CON_BURST_BEAT;
	}
	conf.count = len / size_per_count;
	conf.src = src_addr;
	conf.dst = dst_addr;
	conf.isr_cb = isr_cb;
	conf.data = (void *) dma_ch;
	conf.sinc = DMA_TRUE;
	conf.dinc = DMA_TRUE;
	conf.limiter = 0;
	conf.wpen = 0;
	conf.wpsd = 0;
	conf.flag = flag;

	if (mt_config_gdma(dma_ch, &conf, ALL) != 0) {
		PRINTF_E("DMA cfg err\n");
		ret = DMA_RESULT_ERROR;
		mt_free_gdma(dma_ch);
		taskEXIT_CRITICAL();
		goto _exit;
	}

	mt_start_gdma(dma_ch);
	taskEXIT_CRITICAL();

#if DMA_INTERRUPT_MODE
	if (is_in_isr() || xGetCriticalNesting() > 0) {
		/*in isr or critical context no need get sema */
		while (mt_polling_gdma(dma_ch, 0x10000)) {
			PRINTF_E("DMA ch:%d poll timeout\n", dma_ch);
		}
		mt_stop_gdma(dma_ch);
		ret = DMA_RESULT_DONE;
	} else {
		if (flag == SYNC) {
			ulTaskNotifyTake(pdTRUE, (TickType_t)10000);
			ret = DMA_RESULT_DONE;
		} else
			ret = DMA_RESULT_RUNNING;
	}
#else
	while (mt_polling_gdma(dma_ch, 0x10000)) {
		PRINTF_E("DMA ch:%d poll timeout\n", dma_ch);
	}
	mt_stop_gdma(dma_ch);
	ret = DMA_RESULT_DONE;
#endif

#if DMA_INTERRUPT_MODE
	/*in isr or critical context need to free ch */
	if (is_in_isr() || xGetCriticalNesting() > 0) {
		mt_free_gdma(dma_ch);
	}
#else
	mt_free_gdma(dma_ch);
#endif

_exit:
	return ret;
}

#ifdef CFG_DMA_STRIDE_MODE_SUPPORT
/*
 * mt_polling_stride_gdma: wait the DMA to finish for the specified GDMA channel
 * @channel: GDMA channel to polling
 * @timeout: polling timeout in ms
 * Return 0 for success; return negative errot code for failure.
 */
int32_t mt_polling_stride_gdma(int32_t channel, uint32_t timeout)
{
	if (channel < GDMA_START)
		return DMA_RESULT_INVALID_CH;

	if (channel >= (GDMA_START + NR_GDMA_CHANNEL))
		return DMA_RESULT_INVALID_CH;

	if (dma_channel[channel].in_use == 0)
		return DMA_RESULT_CH_FREE;

	while (drv_reg32(DMA_STATE(DMA_BASE_CH(channel))) & 0x1) {
		timeout--;
		if (!timeout) {
			PRINTF_E("DMA%d polling timeout!!!!\n", channel + 1);
			return 1;
		}
	}

	return 0;
}


/*
 * dma_transaction_stride_mode: dma transaction stride mode function
 * @src_addr: from the source address
 * @dst_addr: to the destination address
 * @len: data lengh
 * @src_stride: source stride lengh, 0 means normal mode
 * @src_stride_count: source stride count, 0 means normal mode
 * @dst_stride: destination stride lengh, 0 means normal mode
 * @dst_stride_count: destination stride count, 0 means normal mode
 * @dma_id: user id
 * @ch: revserved channel or NO_RESERVED
 * @isr_cb: isr callback function
 * @flag: sync or async
 * Return 0 for DMA_DONE, 1 for DMA_RUNNING; return negative error code for failure.
 */
DMA_RESULT dma_transaction_stride_mode(
	uint32_t src_addr,
	uint32_t dst_addr,
	uint32_t len,
	uint32_t src_stride,
	uint32_t src_stride_count,
	uint32_t dst_stride,
	uint32_t dst_stride_count,
	int32_t dma_id,
	int32_t ch,
	void (*isr_cb)(void*),
	DMA_SYNC flag)
{
	struct mt_gdma_conf conf;
	int32_t retry = 0x10000;
	int32_t dma_ch = DMA_RESULT_NO_FREE_CH;
	uint32_t val;

	if ((dma_id < 0) || (dma_id >= NUMS_DMA_ID)) {
		PRINTF_E("DMA ID err\n");
		return DMA_RESULT_ERROR;
	}

	if (is_in_isr() && flag == SYNC) {
		PRINTF_E("DMA id:%d can't in isr\n", dma_id);
		configASSERT(0);
	}

	do {
		/*specific ch */
		if (ch != NO_RESERVED) {
			dma_ch = mt_req_gdma(ch);
		} else {
			dma_ch = mt_request_dma_channel(dma_id);
		}
		retry--;
	} while (dma_ch < 0 && retry > 0);

	if (dma_ch < 0 || dma_ch >= NR_GDMA_CHANNEL) {
		PRINTF_E("DMA_RESULT_NO_FREE_CH\n");
		return DMA_RESULT_NO_FREE_CH;
	}

	/* set iten before enter dma critical section */
#if DMA_INTERRUPT_MODE
	/*in isr or critical context */
	if (is_in_isr() || xGetCriticalNesting() > 0) {
		conf.iten = DMA_CON_INTEN_DISABLED;
	} else {
		if (flag == ASYNC) {
			conf.iten = DMA_CON_INTEN_ENABLED;
		} else {
			conf.iten = DMA_CON_INTEN_DISABLED;
		}
	}
#else
	conf.iten = DMA_CON_INTEN_DISABLED;
#endif
	taskENTER_CRITICAL();

	conf.src = src_addr;
	conf.dst = dst_addr;
	conf.count = len;
	conf.size_per_count = DMA_CON_SIZE_1BYTE;
	conf.burst = DMA_CON_BURST_BEAT;
	conf.sinc = DMA_CON_SRC_BURST;
	conf.dinc = DMA_CON_DST_BURST;
	conf.wplen = 0;
	conf.wpto = 0;
	conf.wpsd = 0;
	conf.wpen = 0;
	conf.limiter = 0;
	conf.isr_cb = isr_cb;
	conf.flag = flag;
	conf.data = (void*)dma_ch;;
	if (mt_config_gdma(dma_ch, &conf, ALL) != 0) {
		PRINTF_E("DMA cfg err\n");
		mt_free_gdma(dma_ch);
		taskEXIT_CRITICAL();
		return DMA_RESULT_ERROR;
	}

	val = 0;
	if (src_stride > 0) {
		val = DMA_STRIDE_CFG_SRC_STRIDE;
	}
	if (dst_stride > 0) {
		val |= (0x1 << DMA_STRIDE_CFG_DST_BIT_SHIFT);
	}
	if (src_stride_count > 0 || dst_stride_count > 0) {
		if (src_stride_count > dst_stride_count) {
			val |= (src_stride_count << DMA_STRIDE_CFG_STRIDE_CNT_BIT_SHIFT);
		} else {
			val |= (dst_stride_count << DMA_STRIDE_CFG_STRIDE_CNT_BIT_SHIFT);
		}
	}
	drv_write_reg32(DMA_STRIDE_CFG(DMA_BASE_CH(dma_ch)), val);
	val = (src_stride << DMA_STRIDE_SRC_STRIDE_BIT_SHIFT) | (dst_stride << DMA_STRIDE_DST_STRIDE_BIT_SHIFT) ;
	drv_write_reg32(DMA_STRIDE(DMA_BASE_CH(dma_ch)), val);

	if (mt_start_gdma(dma_ch) != 0) {
		PRINTF_E("DMA start err\n");
		mt_stop_gdma(dma_ch);
		mt_free_gdma(dma_ch);
		taskEXIT_CRITICAL();
		return DMA_RESULT_ERROR;
	}
	taskEXIT_CRITICAL();

#if DMA_INTERRUPT_MODE
	if (is_in_isr() || xGetCriticalNesting() > 0) {
		while (mt_polling_stride_gdma(dma_ch, 0x10000)) {
			PRINTF_E("DMA ch:%d poll timeout\n", dma_ch);
		}
		mt_stop_gdma(dma_ch);
		mt_free_gdma(dma_ch);

		/* clean stride dma cfg */
		drv_write_reg32(DMA_STRIDE_CFG(DMA_BASE_CH(dma_ch)), 0);
		drv_write_reg32(DMA_STRIDE(DMA_BASE_CH(dma_ch)), 0);
		return DMA_RESULT_DONE;
	} else {
		if (flag == SYNC) {
			ulTaskNotifyTake(pdTRUE, (TickType_t)10000);
			return DMA_RESULT_DONE;
		} else {
			return DMA_RESULT_RUNNING;
		}
	}
#else
	while (mt_polling_stride_gdma(dma_ch, 0x10000)) {
		PRINTF_E("DMA ch:%d poll timeout\n", dma_ch);
	}
	mt_stop_gdma(dma_ch);
	mt_free_gdma(dma_ch);

	/* clean stride dma cfg */
	drv_write_reg32(DMA_STRIDE_CFG(DMA_BASE_CH(dma_ch)), 0);
	drv_write_reg32(DMA_STRIDE(DMA_BASE_CH(dma_ch)), 0);

	return DMA_RESULT_DONE;
#endif

}
#endif


/*
 * mt_dump_dma_struct: dump DMA struct info
 */
void mt_dump_dma_struct(void)
{
	uint32_t i;

	for (i = 0; i < NR_GDMA_CHANNEL; i++)
		if (dma_channel[i].ch_usage_count > 0)
			PRINTF_E("dma ch:%u,use:%d,count:%u,flag:%d\n", i,
				 dma_channel[i].in_use,
				 dma_channel[i].ch_usage_count,
				 dma_channel[i].flag);

	return;
}

/*
 * mt_init_dma: initialize DMA.
 * Always return 0.
 */
int32_t mt_init_dma(void)
{
	int32_t i;
	g_dma_base = DMA_BASE_REG;

	mt_cleanup_dma_operations();

	for (i = 0; i < NR_GDMA_CHANNEL; i++) {
		dma_channel[i].in_use = 0;
		dma_channel[i].dma_id = -1;
		dma_channel[i].last_enter = 0;
		dma_channel[i].last_exit = 0;
		dma_channel[i].max_duration = 0;
		dma_channel[i].ch_usage_count = 0;
		dma_channel[i].task_handle = NULL;
		dma_channel[i].flag = SYNC;
		mt_reset_gdma_conf(i);
	}

	intc_irq_request(&INTC_IRQ_DMA, mt_dma_irqhandler, (void *) 0);
#if DMA_KEEP_AWAKE
	dma_wake_lock_init();
#endif

	return 0;
}

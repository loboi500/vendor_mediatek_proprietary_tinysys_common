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
/* MediaTek Inc. (C) 2021. All rights reserved.
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

#include "FreeRTOS.h"
#include "task.h"
#include "mt_alloc.h"
#include "driver_api.h"
#include "portmacro.h"
#include "mt_printf.h"

/* portBYTE_ALIGNMENT should greater than 8 for watermark checking */
void *aligned_malloc(size_t nbytes, size_t aligned)
{
	StackType_t aptr;
	void *ret;

	if ((aligned == 0) || ((aligned & (portBYTE_ALIGNMENT-1)) != 0)) {
		PRINTF_E("Err: Invalid alignment value, %d\n", aligned);
		configASSERT(0);
	}

	ret = pvPortMalloc(nbytes + aligned);
	/* Pick next aligned address to user even if aptr is aligned */
	aptr = ((StackType_t)ret + aligned) & ~(aligned - 1);
	/* Save original aptr to previous 4 bytes for free API */
	writel(aptr - 4, (StackType_t)ret);
	writel(aptr - 8, ALIGNED_MAL_WATERMARK);

	return (void *)aptr;

}

void aligned_free(void *aptr)
{
	StackType_t free_aptr;

	if (readl((StackType_t)aptr - 8) != ALIGNED_MAL_WATERMARK) {
		PRINTF_E("Err: algined allocate API mismatch\n");
		configASSERT(0);
	}

	/* Get original aptr to free */
	free_aptr = readl((StackType_t)aptr - 4);
	vPortFree((void *)free_aptr);
}

// dram malloc API
/* portBYTE_ALIGNMENT should greater than 8 for watermark checking */
#ifdef CFG_DRAM_HEAP_SUPPORT
void *aligned_dram_malloc(size_t nbytes, size_t aligned)
{
	StackType_t aptr;
	void *ret;

	if ((aligned == 0) || ((aligned & (portBYTE_ALIGNMENT-1)) != 0)) {
		PRINTF_E("Err: Invalid alignment value, %d\n", aligned);
		configASSERT(0);
	}

	ret = pvPortDramMalloc(nbytes + aligned);
	/* Pick next aligned address to user even if aptr is aligned */
	aptr = ((StackType_t)ret + aligned) & ~(aligned - 1);
	/* Save original aptr to previous 4 bytes for free API */
	writel(aptr - 4, (StackType_t)ret);
	writel(aptr - 8, ALIGNED_MAL_WATERMARK);

	return (void *)aptr;

}

void aligned_dram_free(void *aptr)
{
	StackType_t free_aptr;

	if (readl((StackType_t)aptr - 8) != ALIGNED_MAL_WATERMARK) {
		PRINTF_E("Err: algined allocate API mismatch\n");
		configASSERT(0);
	}

	/* Get original aptr to free */
	free_aptr = readl((StackType_t)aptr - 4);
	vPortDramFree((void *)free_aptr);
}
#endif


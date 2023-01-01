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

#include "audio.h"

#include <audio_type.h>
#include <audio_ringbuf.h>

#include <audio_task_factory.h>
#include <audio_messenger_ipi.h>
#include <audio_irq.h>


#include <audio_hw_hal.h>
#ifdef CFG_MTK_AUDIODSP_SUPPORT
#include <audio_sw_mixer.h>

#if defined(C2C_IPC_SUPPORT) && !defined(VOIP_SEPARATE_CORE)
#include "audio_voip_c2c_server.h"
#include "audio_voip_c2c_client.h"
#endif
#endif

void audio_init(void)
{
	AUD_LOG_D("+%s(), Heap free/total = %d/%d\n",
		  __func__, xPortGetFreeHeapSize(), configTOTAL_HEAP_SIZE);

	/* init task manager & create tasks */
	audio_task_factory_init();

	/* register IPI */
	audio_messenger_ipi_init();

	/* register IRQ */
	audio_irq_init();
	/* init dsp audio ops*/
	init_dsp_audio_ops();
#ifdef CFG_MTK_AUDIODSP_SUPPORT
	init_audio_sw_mixer();

#if defined(C2C_IPC_SUPPORT) && !defined(VOIP_SEPARATE_CORE)
	/* init voip c2c thread */
	audio_voip_c2c_server_init();
	audio_voip_c2c_client_init();
#endif
#endif
	AUD_LOG_D("-%s(), Heap free/total = %d/%d\n",
		  __func__, xPortGetFreeHeapSize(), configTOTAL_HEAP_SIZE);
}


void audio_deinit(void)
{
	audio_task_factory_deinit();
}


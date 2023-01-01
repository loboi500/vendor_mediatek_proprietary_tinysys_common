/* Copyright Statement:
 *
 * This software/firmware and related documentation ("MediaTek Software") are
 * protected under relevant copyright laws. The information contained herein
 * is confidential and proprietary to MediaTek Inc. and/or its licensors.
 * Without the prior written permission of MediaTek inc. and/or its licensors,
 * any reproduction, modification, use or disclosure of MediaTek Software,
 * and information contained herein, in whole or in part, shall be strictly prohibited.
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
 * THIRD PARTY FOR ANY WARRANTY CLAIM RELATING THERETO. RECEIVER EXPRESSLY ACKNOWLEDGES
 * THAT IT IS RECEIVER\'S SOLE RESPONSIBILITY TO OBTAIN FROM ANY THIRD PARTY ALL PROPER LICENSES
 * CONTAINED IN MEDIATEK SOFTWARE. MEDIATEK SHALL ALSO NOT BE RESPONSIBLE FOR ANY MEDIATEK
 * SOFTWARE RELEASES MADE TO RECEIVER\'S SPECIFICATION OR TO CONFORM TO A PARTICULAR
 * STANDARD OR OPEN FORUM. RECEIVER\'S SOLE AND EXCLUSIVE REMEDY AND MEDIATEK\'S ENTIRE AND
 * CUMULATIVE LIABILITY WITH RESPECT TO THE MEDIATEK SOFTWARE RELEASED HEREUNDER WILL BE,
 * AT MEDIATEK\'S OPTION, TO REVISE OR REPLACE THE MEDIATEK SOFTWARE AT ISSUE,
 * OR REFUND ANY SOFTWARE LICENSE FEES OR SERVICE CHARGE PAID BY RECEIVER TO
 * MEDIATEK FOR SUCH MEDIATEK SOFTWARE AT ISSUE.
 *
 * The following software/firmware and/or related documentation ("MediaTek Software")
 * have been modified by MediaTek Inc. All revisions are subject to any receiver\'s
 * applicable license agreements with MediaTek Inc.
 */

#ifndef __SRAMRC__
#define __SRAMRC__

#include "FreeRTOS.h"
#include "sramrc_reg.h"

/* Default sramrc read/write/remap functions which could be overriden by platforms
 * Add SRAMRC_PLATFORM_REMAP C preprocessor define to replace default implementation
 */
unsigned int sramrc_remap (unsigned int addr);
unsigned int sramrc_read(unsigned int addr);
unsigned int sramrc_write(unsigned int addr, unsigned int val);




/* read/write mask/shift API*/
unsigned int sramrc_config_interface(unsigned int reg_num, unsigned int val,
				unsigned int mask, unsigned int shift);
unsigned int sramrc_read_interface(unsigned int reg_num, unsigned int *val,
				unsigned int mask, unsigned int shift);

/* GPU API */
void sramrc_set_gpu_passive_mask(unsigned int val);
unsigned int sramrc_get_gpu_passive_mask(void);
void sramrc_mask_passive_gpu(void);
void sramrc_unmask_passive_gpu(void);
void sramrc_set_gpu_threshold(unsigned int sram_threshold,
			unsigned int vlogic_high, unsigned int vlogic_low);
unsigned int sramrc_get_gpu_threshold(void);
void sramrc_set_gpu_timeout(unsigned int timeout_us);
unsigned int sramrc_get_gpu_timeout(void);
void sramrc_retry_gpu(void);
unsigned int sramrc_get_gpu_bound_level(void);

/* ISP API */
void sramrc_set_isp_passive_mask(unsigned int val);
unsigned int sramrc_get_isp_passive_mask(void);
void sramrc_mask_passive_isp(void);
void sramrc_unmask_passive_isp(void);
void sramrc_set_isp_threshold(unsigned int sram_threshold, unsigned int vlogic_high,
				unsigned int vlogic_low);
unsigned int sramrc_get_isp_threshold(void);
void sramrc_set_isp_timeout(unsigned int timeout_us);
unsigned int sramrc_get_isp_timeout(void);
void sramrc_retry_isp(void);
unsigned int sramrc_get_isp_bound_level(void);

/* VCORE API */
void sramrc_set_vcore_passive_mask(unsigned int val);
unsigned int sramrc_get_vcore_passive_mask(void);
void sramrc_mask_passive_vcore(void);
void sramrc_unmask_passive_vcore(void);
void sramrc_set_vcore_threshold(unsigned int sram_threshold, unsigned int vlogic_high,
				unsigned int vlogic_low);
unsigned int sramrc_get_vcore_threshold(void);
void sramrc_set_vcore_timeout(unsigned int timeout_us);
unsigned int sramrc_get_vcore_timeout(void);
void sramrc_retry_vcore(void);
unsigned int sramrc_get_vcore_bound_level(void);
unsigned int sramrc_level_to_uV(unsigned int level);
/* SRAMRC common  */
struct sramrc_dump {
	unsigned int timeout_sta;
	unsigned int sram_sta;
	unsigned int sram_sta2;
};

int sramrc_register_init(void);
int sramrc_get_safe_vlogic(void);
int sramrc_dump_status(struct sramrc_dump *sramrc_dump);
unsigned int sramrc_get_longest_timeout(void);
unsigned int sramrc_get_sram_cur_level(void);
unsigned int sramrc_get_sram_bound(void);
unsigned int sramrc_passive_ack(unsigned int ack_reg);
void sramrc_sram_request(unsigned int sram_voltage_step);
int sramrc_is_swreq_done(void);
int sramrc_is_timeout(unsigned int *real);
void sramrc_clear_timeout(void);
void sramrc_set_sram_timeout(unsigned int timeout_us);
unsigned int sramrc_get_sram_timeout(void);

#endif

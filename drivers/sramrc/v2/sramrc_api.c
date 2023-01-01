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

#include <FreeRTOS.h>
#include <driver_api.h>
#include <sramrc.h>
#include <mt_printf.h>
#ifdef CFG_XGPT_SUPPORT
#include <xgpt.h>
#endif
#ifdef CFG_TIMER_SUPPORT
#include <ostimer.h>
#endif

unsigned int profile_timestamp[2];

#ifndef SRAMRC_PLATFORM_REMAP
/* Default sramrc read/write/remap functions which could be overriden by platforms */
unsigned int sramrc_remap (unsigned int addr)
{
	return addr;
}

unsigned int sramrc_read(unsigned int addr)
{
	return DRV_Reg32(sramrc_remap(addr));
}

unsigned int sramrc_write(unsigned int addr, unsigned int val)
{
	return DRV_WriteReg32(sramrc_remap(addr), val);
}
#endif

/* read MASK/shift wrapper API*/
unsigned int sramrc_config_interface(unsigned int reg_num, unsigned int val,
				unsigned int mask, unsigned int shift)
{
	unsigned int reg;

	reg = sramrc_read(reg_num);

	reg &= ~(mask << shift);
	reg |= (val << shift);

	sramrc_write(reg_num, reg);

	return reg;
}

unsigned int sramrc_read_interface(unsigned int reg_num, unsigned int *val,
				unsigned int mask, unsigned int shift)
{
	unsigned int reg;

	reg = sramrc_read(reg_num);

	reg &= (mask << shift);
	*val = (reg  >> shift);

	return 0;
}

#ifdef SRAMRC_PROFILE_TIMING_SUPPORT
static unsigned int get_sramrc_timestamp(void)
{
#if defined (CFG_TIMER_SUPPORT)
	int isr_mode = is_in_isr();
#endif
	unsigned long long ts = 0;
	unsigned int ret = 0;

#if defined (CFG_XGPT_SUPPORT)
	ts = read_sys_timestamp_ns();
	ret = (unsigned int)((ts / 1000) & 0x7FFFFFFF); //ns to us
#elif defined (CFG_TIMER_SUPPORT)
	ts = ts_ostimer_read_isr(isr_mode);
	ret = (unsigned int)(ts/13); //us
#endif

	return ret;
}
#endif

/* SRAMRC APU API */
void sramrc_set_apu_passive_mask(unsigned int val)
{
	sramrc_write(SRAMRC_VAPU_MASK, (val & 0x3));
}

unsigned int sramrc_get_apu_passive_mask(void)
{
	return sramrc_read(SRAMRC_VAPU_MASK);
}

void sramrc_set_apu_active_mask(unsigned int val)
{
	sramrc_write(SRAMRC_APU_MASK, (val & 0x3));
}

unsigned int sramrc_get_apu_active_mask(void)
{
	return sramrc_read(SRAMRC_APU_MASK);
}

void sramrc_mask_passive_apu(void)
{
	sramrc_write(SRAMRC_VAPU_MASK, 0x3);
}

void sramrc_unmask_passive_apu(void)
{
	sramrc_write(SRAMRC_VAPU_MASK, 0x0);
}

void sramrc_mask_active_apu(void)
{
	sramrc_write(SRAMRC_APU_MASK, 0x3);
}

void sramrc_unmask_active_apu(void)
{
	sramrc_write(SRAMRC_APU_MASK, 0x0);
}

void sramrc_set_apu_threshold(unsigned int sram_threshold, unsigned int vlogic_high,
				unsigned int vlogic_low)
{
	unsigned int val;

	val = ((sram_threshold & 0xff) << 16) |
		  ((vlogic_high    & 0xff) << 8) |
		  ((vlogic_low     & 0xff) << 0);
	sramrc_write(SRAMRC_VAPU_SET, val);
}

unsigned int sramrc_get_apu_threshold(void)
{
	return sramrc_read(SRAMRC_VAPU_SET);
}

void sramrc_set_apu_timeout(unsigned int timeout_us)
{
	unsigned int timeout_step = (timeout_us / 10) & 0xFFFF;

	sramrc_config_interface(TIMEOUT_3_RG_VAPU_REQ_TIMEOUT_ADDR, timeout_step,
	TIMEOUT_3_RG_VAPU_REQ_TIMEOUT_MASK, TIMEOUT_3_RG_VAPU_REQ_TIMEOUT_SHIFT);
}

unsigned int sramrc_get_apu_timeout(void)
{
	unsigned int val;

	sramrc_read_interface(TIMEOUT_3_RG_VAPU_REQ_TIMEOUT_ADDR, &val,
	TIMEOUT_3_RG_VAPU_REQ_TIMEOUT_MASK, TIMEOUT_3_RG_VAPU_REQ_TIMEOUT_SHIFT);

	return val;
}

void sramrc_retry_apu(void)
{
	sramrc_write(SRAMRC_VAPU_RETRY, 0);
	sramrc_write(SRAMRC_VAPU_RETRY, 1);
}

unsigned int sramrc_get_apu_bound_level(void)
{
	unsigned int sram_bound, apu_setting;

	sram_bound = sramrc_get_sram_bound();
	apu_setting = sramrc_read(SRAMRC_VAPU_SET);

	/* return apu vlogic_high if sram_bound > apu_threshold*/
	if (sram_bound >= ((apu_setting >> 16) & 0xff))
		return (apu_setting >> 8) & 0xff;

	return apu_setting & 0xff;
}

/* SRAMRC GPU API */
void sramrc_set_gpu_passive_mask(unsigned int val)
{
	sramrc_write(SRAMRC_VGPU_MASK, (val & 0x3));
}

unsigned int sramrc_get_gpu_passive_mask(void)
{
	return sramrc_read(SRAMRC_VGPU_MASK);
}

void sramrc_set_gpu_active_mask(unsigned int val)
{
	sramrc_write(SRAMRC_GPU_MASK, (val & 0x3));
}

unsigned int sramrc_get_gpu_active_mask(void)
{
	return sramrc_read(SRAMRC_GPU_MASK);
}

void sramrc_mask_passive_gpu(void)
{
	sramrc_write(SRAMRC_VGPU_MASK, 0x3);
}

void sramrc_unmask_passive_gpu(void)
{
	sramrc_write(SRAMRC_VGPU_MASK, 0x0);
}

void sramrc_mask_active_gpu(void)
{
	sramrc_write(SRAMRC_GPU_MASK, 0x3);
}

void sramrc_unmask_active_gpu(void)
{
	sramrc_write(SRAMRC_GPU_MASK, 0x0);
}

void sramrc_set_gpu_threshold(unsigned int sram_threshold, unsigned int vlogic_high,
				unsigned int vlogic_low)
{
	unsigned int val;

	val = ((sram_threshold & 0xff) << 16) |
		  ((vlogic_high    & 0xff) << 8) |
		  ((vlogic_low     & 0xff) << 0);
	sramrc_write(SRAMRC_VGPU_SET, val);
}

unsigned int sramrc_get_gpu_threshold(void)
{
	return sramrc_read(SRAMRC_VGPU_SET);
}

void sramrc_set_gpu_timeout(unsigned int timeout_us)
{
	unsigned int timeout_step = (timeout_us / 10) & 0xFFFF;

	sramrc_config_interface(TIMEOUT_1_RG_VGPU_REQ_TIMEOUT_ADDR, timeout_step,
	TIMEOUT_1_RG_VGPU_REQ_TIMEOUT_MASK, TIMEOUT_1_RG_VGPU_REQ_TIMEOUT_SHIFT);
}

unsigned int sramrc_get_gpu_timeout(void)
{
	unsigned int val;

	sramrc_read_interface(TIMEOUT_1_RG_VGPU_REQ_TIMEOUT_ADDR, &val,
	TIMEOUT_1_RG_VGPU_REQ_TIMEOUT_MASK, TIMEOUT_1_RG_VGPU_REQ_TIMEOUT_SHIFT);

	return val;
}

void sramrc_retry_gpu(void)
{
	sramrc_write(SRAMRC_VGPU_RETRY, 0);
	sramrc_write(SRAMRC_VGPU_RETRY, 1);
}

unsigned int sramrc_get_gpu_bound_level(void)
{
	unsigned int sram_bound, gpu_setting;

	sram_bound = sramrc_get_sram_bound();
	gpu_setting = sramrc_read(SRAMRC_VGPU_SET);

	/* return gpu vlogic_high if sram_bound > gpu_threshold*/
	if (sram_bound >= ((gpu_setting >> 16) & 0xff))
		return (gpu_setting >> 8) & 0xff;

	return gpu_setting & 0xff;
}

/* SRAMRC ISP API*/
void sramrc_set_isp_passive_mask(unsigned int val)
{
	sramrc_write(SRAMRC_VISP_MASK, (val & 0x3));
}

unsigned int sramrc_get_isp_passive_mask(void)
{
	return sramrc_read(SRAMRC_VISP_MASK);
}

void sramrc_mask_passive_isp(void)
{
	sramrc_write(SRAMRC_VISP_MASK, 0x3);
}

void sramrc_unmask_passive_isp(void)
{
	sramrc_write(SRAMRC_VISP_MASK, 0x0);
}

void sramrc_set_isp_threshold(unsigned int sram_threshold, unsigned int vlogic_high,
				unsigned int vlogic_low)
{
	unsigned int val;

	val = ((sram_threshold & 0xff) << 16) |
		  ((vlogic_high    & 0xff) << 8) |
		  ((vlogic_low     & 0xff) << 0);
	sramrc_write(SRAMRC_VISP_SET, val);
}

unsigned int sramrc_get_isp_threshold(void)
{
	return sramrc_read(SRAMRC_VISP_SET);
}

void sramrc_retry_isp(void)
{
	sramrc_write(SRAMRC_VISP_RETRY, 0);
	sramrc_write(SRAMRC_VISP_RETRY, 1);
}

void sramrc_set_isp_timeout(unsigned int timeout_us)
{
	unsigned int timeout_step = (timeout_us / 10) & 0xFFFF;

	sramrc_config_interface(TIMEOUT_2_RG_VISP_REQ_TIMEOUT_ADDR, timeout_step,
	TIMEOUT_2_RG_VISP_REQ_TIMEOUT_MASK, TIMEOUT_2_RG_VISP_REQ_TIMEOUT_SHIFT);
}

unsigned int sramrc_get_isp_timeout(void)
{
	unsigned int val;

	sramrc_read_interface(TIMEOUT_2_RG_VISP_REQ_TIMEOUT_ADDR, &val,
	TIMEOUT_2_RG_VISP_REQ_TIMEOUT_MASK, TIMEOUT_2_RG_VISP_REQ_TIMEOUT_SHIFT);

	return val;
}

unsigned int sramrc_get_isp_bound_level(void)
{
	unsigned int sram_bound, isp_setting;

	sram_bound = sramrc_get_sram_bound();
	isp_setting = sramrc_read(SRAMRC_VISP_SET);

	/* return isp vlogic_high if sram_bound > isp_threshold*/
	if (sram_bound >= ((isp_setting >> 16) & 0xff))
		return (isp_setting >> 8) & 0xff;

	return isp_setting & 0xff;
}

/* SRAMRC VCORE API */
void sramrc_set_vcore_passive_mask(unsigned int val)
{
	sramrc_write(SRAMRC_VCORE_MASK, (val & 0x3));
}

unsigned int sramrc_get_vcore_passive_mask(void)
{
	return sramrc_read(SRAMRC_VCORE_MASK);
}

void sramrc_mask_passive_vcore(void)
{
	sramrc_write(SRAMRC_VCORE_MASK, 0x3);
}

void sramrc_unmask_passive_vcore(void)
{
	sramrc_write(SRAMRC_VCORE_MASK, 0x0);
}

void sramrc_set_vcore_threshold(unsigned int sram_threshold, unsigned int vlogic_high,
				unsigned int vlogic_low)
{
	unsigned int val;

	val = ((sram_threshold & 0xff) << 16) |
		  ((vlogic_high    & 0xff) << 8) |
		  ((vlogic_low     & 0xff) << 0);
	sramrc_write(SRAMRC_VCORE_SET, val);
}

unsigned int sramrc_get_vcore_threshold(void)
{
	return sramrc_read(SRAMRC_VCORE_SET);
}

void sramrc_retry_vcore(void)
{
	sramrc_write(SRAMRC_VCORE_RETRY, 0);
	sramrc_write(SRAMRC_VCORE_RETRY, 1);
}

void sramrc_set_vcore_timeout(unsigned int timeout_us)
{
	unsigned int timeout_step = (timeout_us / 10) & 0xFFFF;

	sramrc_config_interface(TIMEOUT_2_RG_VCORE_REQ_TIMEOUT_ADDR, timeout_step,
	TIMEOUT_2_RG_VCORE_REQ_TIMEOUT_MASK, TIMEOUT_2_RG_VCORE_REQ_TIMEOUT_SHIFT);
}

unsigned int sramrc_get_vcore_timeout(void)
{
	unsigned int val;

	sramrc_read_interface(TIMEOUT_2_RG_VCORE_REQ_TIMEOUT_ADDR, &val,
	TIMEOUT_2_RG_VCORE_REQ_TIMEOUT_MASK, TIMEOUT_2_RG_VCORE_REQ_TIMEOUT_SHIFT);

	return val;
}

unsigned int sramrc_get_vcore_bound_level(void)
{
	unsigned int sram_bound, vcore_setting;

	sram_bound = sramrc_get_sram_bound();
	vcore_setting = sramrc_read(SRAMRC_VCORE_SET);

	/* return vcore vlogic_high if sram_bound > vcore_threshold*/
	if (sram_bound >= ((vcore_setting >> 16) & 0xff))
		return (vcore_setting >> 8) & 0xff;

	return vcore_setting & 0xff;
}

/* SSPM API */
void sramrc_set_sspm_timeout(unsigned int timeout_us)
{
	unsigned int timeout_step = (timeout_us / 10) & 0xFFFF;

	sramrc_config_interface(TIMEOUT_3_RG_VSRAM_REQ_TIMEOUT_ADDR, timeout_step,
	TIMEOUT_3_RG_VSRAM_REQ_TIMEOUT_MASK, TIMEOUT_3_RG_VSRAM_REQ_TIMEOUT_SHIFT);
}

unsigned int sramrc_get_sspm_timeout(void)
{
	unsigned int val;

	sramrc_read_interface(TIMEOUT_3_RG_VSRAM_REQ_TIMEOUT_ADDR, &val,
	TIMEOUT_3_RG_VSRAM_REQ_TIMEOUT_MASK, TIMEOUT_3_RG_VSRAM_REQ_TIMEOUT_SHIFT);

	return val;
}

/* SRARMC COMMON API*/
#define SRAMRC_INIT_NUM (14)

static unsigned int  _sram_init_data[] = {
	SRAMRC_VAPU_SET, 0x806000,
	SRAMRC_VGPU_SET, 0x806000,
	SRAMRC_VISP_SET, 0x806000,
	SRAMRC_VCORE_SET, 0x800100,
	SRAMRC_VSRAM_MASK, 0x0,
	SRAMRC_VGPU_MASK, 0x3,
	SRAMRC_VISP_MASK, 0x3,
	SRAMRC_VAPU_MASK, 0x3,
	SRAMRC_VCORE_MASK, 0x0,
	SRAMRC_APU_MASK, 0x3,
	SRAMRC_GPU_MASK, 0x3,
	SRAMRC_TIMEOUT_1, 0xFFFFFFFF,
	SRAMRC_TIMEOUT_2, 0xFFFFFFFF,
	SRAMRC_TIMEOUT_3, 0xFFFFFFFF,
	SRAMRC_IRQ_SET, 0x1,
	SRAMRC_FORCE_CUR, 0x0,
	SRAMRC_FORCE_SET, 0x1,
	SRAMRC_FORCE_SET, 0x0,
	SRAMRC_BASIC_SET, 0x90013,
};
int sramrc_register_init(void)
{
	int i;
	unsigned int *tmp = _sram_init_data;

	for (i = 0; i < SRAMRC_INIT_NUM ;i++) {
		sramrc_write(*tmp, *(tmp+1));
		tmp = tmp + 2;
	}
	return 0;
}

static const int safe_vlogic_level = 0x60;
int sramrc_get_safe_vlogic(void)
{
	return safe_vlogic_level;
}

int sramrc_dump_status(struct sramrc_dump *sramrc_dump)
{
	unsigned int val;

	sramrc_dump->timeout_sta = sramrc_read(SRAMRC_TIMEOUT_STA) & 0xFFFFF ;
	sramrc_dump->timeout_sta |= (sramrc_read(SRAMRC_VGPU_MASK) & 0x3) << 16;
	sramrc_dump->timeout_sta |= (sramrc_read(SRAMRC_VISP_MASK) & 0x3) << 18;
	sramrc_dump->timeout_sta |= (sramrc_read(SRAMRC_VCORE_MASK) & 0x3) << 20;
	sramrc_dump->timeout_sta |= (sramrc_read(SRAMRC_VAPU_MASK) & 0x3) << 22;
	sramrc_dump->timeout_sta |= (sramrc_read(SRAMRC_APU_MASK) & 0x3) << 24;
	sramrc_dump->timeout_sta |= (sramrc_read(SRAMRC_GPU_MASK) & 0x3) << 26;

	val = sramrc_read(SRAMRC_VSRAM_STA);
	sramrc_dump->sram_sta = (( val & 0xFF) | (val & 0xFF0000) >> 8);
	sramrc_dump->sram_sta |= (sramrc_read(SRAMRC_SRAM_SW_REQ1) << 16);
	sramrc_dump->sram_sta2 = sramrc_read(SRAMRC_VSRAM_STA2);

	return 0;
}
unsigned int sramrc_get_longest_timeout(void)
{
	unsigned int val = 0, tmp;

	tmp = sramrc_get_vcore_timeout();
	val = (tmp > val) ? tmp : val;
	tmp = sramrc_get_sspm_timeout();
	val = (tmp > val) ? tmp : val;
	tmp = sramrc_get_apu_timeout();
	val = (tmp > val) ? tmp : val;
	tmp = sramrc_get_gpu_timeout();
	val = (tmp > val) ? tmp : val;
	tmp = sramrc_get_isp_timeout();
	val = (tmp > val) ? tmp : val;
	/* this refer to passsive user timeout + sspm timeout */
	tmp = sramrc_get_sram_timeout();
	val = (tmp > val) ? tmp : val;
	/* return value in us */
	return val * 10;
}

unsigned int sramrc_get_sram_cur_level(void)
{
	unsigned int val;

	sramrc_read_interface(VSRAM_STA_CUR_VSRAM_ADDR, &val,
		VSRAM_STA_CUR_VSRAM_MASK, VSRAM_STA_CUR_VSRAM_SHIFT);

	return val;
}

unsigned int sramrc_get_sram_bound(void)
{
	unsigned int val, bound;

	val = sramrc_read(SRAMRC_VSRAM_STA);

	bound = (val >> VSRAM_STA_TAR_VSRAM_REQ_SHIFT) & VSRAM_STA_TAR_VSRAM_REQ_MASK;
	if (bound > 0)
		return bound;

	bound = (val >> VSRAM_STA_CUR_VSRAM_SHIFT) & VSRAM_STA_CUR_VSRAM_MASK;
	return bound;
}

unsigned int sramrc_passive_ack(unsigned int ack_reg)
{
	sramrc_write(ack_reg, 0);
	sramrc_write(ack_reg, 1);
	return 0;
}

void sramrc_sram_request(unsigned int sram_voltage_step)
{
#ifdef SRAMRC_PROFILE_TIMING_SUPPORT
	profile_timestamp[0] = get_sramrc_timestamp();
#endif
	sramrc_write(SRAMRC_SRAM_SW_REQ1, sram_voltage_step);
}

int sramrc_is_swreq_done(void)
{
	unsigned int val;

	/* if target buffer != 0, sram request still on going*/
	val = sramrc_read(SRAMRC_VSRAM_STA) & VSRAM_STA_TAR_VSRAM_REQ_MASK;

	if (val != 0)
		return 0;

#ifdef SRAMRC_PROFILE_TIMING_SUPPORT
	profile_timestamp[1] = get_sramrc_timestamp();
	PRINTF_I("%s latency = %d usec\n", __func__, profile_timestamp[1] - profile_timestamp[0]);
#endif

	return 1;
}


int sramrc_is_timeout(unsigned int *real)
{
	unsigned int val;

	val = sramrc_read(SRAMRC_TIMEOUT_STA);

	/* sram_to bit indicates whether sramrc really timeouts*/
	if (val & 0x1)
		*real = 1;

	return val;
}

void sramrc_clear_timeout(void)
{
	sramrc_write(SRAMRC_IRQ_SET, 0x3);
	sramrc_write(SRAMRC_IRQ_SET, 0x1);
}

void sramrc_set_sram_timeout(unsigned int timeout_us)
{
	unsigned int timeout_step = (timeout_us / 10) & 0xFFFF;

	sramrc_config_interface(TIMEOUT_1_RG_REQ_TIMEOUT_ADDR, timeout_step,
	TIMEOUT_1_RG_REQ_TIMEOUT_MASK, TIMEOUT_1_RG_REQ_TIMEOUT_SHIFT);
}

unsigned int sramrc_level_to_uV(unsigned int level)
{
	return level * 6250;
}
/* return timeout setting in us */
unsigned int sramrc_get_sram_timeout(void)
{
	unsigned int val;

	sramrc_read_interface(TIMEOUT_1_RG_REQ_TIMEOUT_ADDR, &val,
	TIMEOUT_1_RG_REQ_TIMEOUT_MASK, TIMEOUT_1_RG_REQ_TIMEOUT_SHIFT);

	return val;
}

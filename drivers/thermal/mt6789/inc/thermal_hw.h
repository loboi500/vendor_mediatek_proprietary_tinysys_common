/* Copyright Statement:
 *
 * This software/firmware and related documentation ("MediaTek Software") are
 * protected under relevant copyright laws. The information contained herein
 * is confidential and proprietary to MediaTek Inc. and/or its licensors.
 * Without the prior written permission of MediaTek inc. and/or its licensors,
 * any reproduction, modification, use or disclosure of MediaTek Software,
 * and information contained herein, in whole or in part, shall be strictly prohibited.
 */
/* MediaTek Inc. (C) 2017. All rights reserved.
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

#ifndef _THERMAL_HW_H
#define _THERMAL_HW_H
/*=============================================================
 * Thermal Configuration
 *=============================================================*/

/*
 * AP_LITTLE(TS1,TS2,TS3,TS4)		LVTS1-0, LVTS1-1, LVTS1-2, LVTS1-3
 * AP_BIG(TS5,TS6,TS7,TS8			LVTS2-0, LVTS2-1, LVTS2-2, LVTS2-3
 * GPU(TS9,TS10)					LVTS3-0, LVTS3-1
 * SOC(TS11,TS12)					LVTS3-2, LVTS3-3
 * MD(TS13)							LVTS4-0
 */

/* private thermal sensor enum */
enum lvts_sensor_enum {
	L_TS_LVTS1_0 = 0,	/* LVTS1-0 1(CPU_1_L)*/
	L_TS_LVTS1_1,		/* LVTS1-1 2(CPU_2_L)*/
	L_TS_LVTS1_2,		/* LVTS1-2 3(CPU_3_L)*/
	L_TS_LVTS1_3,		/* LVTS1-3 4(CPU_4_L)*/
	L_TS_LVTS2_0,		/* LVTS2-0 5(CPU_5_B)*/
	L_TS_LVTS2_1,		/* LVTS2-1 6(CPU_6_B)*/
	L_TS_LVTS2_2,		/* LVTS2-2 7(CPU_7_B)*/
	L_TS_LVTS2_3,		/* LVTS2-3 8(CPU_8_B)*/
	L_TS_LVTS3_0,		/* LVTS3-0 9(GPU)*/
	L_TS_LVTS3_1,		/* LVTS3-1 10(GPU)*/
	L_TS_LVTS3_2,		/* LVTS3-2 11(SOC)*/
	L_TS_LVTS3_3,		/* LVTS3-3 12(SOC)*/
	L_TS_LVTS4_0,		/* LVTS4-0 13(MD)*/
	L_TS_LVTS_NUM
};

enum lvts_tc_enum {
	LVTS_AP_CONTROLLER0 = 0,/* LVTSMONCTL0_0 */
	LVTS_AP_CONTROLLER1,	/* LVTSMONCTL0_1 */
	LVTS_AP_CONTROLLER2,	/* LVTSMONCTL0_2 */
	LVTS_AP_CONTROLLER3,	/* LVTSMONCTL0_3 */
	LVTS_AP_CONTROLLER_NUM
};

#define DEFAULT_TTJ    (95000)

/*=============================================================
 * Thermal Controller Registers / Coef
 *=============================================================*/
#define LVTS_CTRL_BASE              (0x1100B000)

#define LVTS_MSR0_OFFSET            (0x90)
#define LVTS_TC_OFFSET              (0x100)
#define LVTS_MSR_REG(base, tc_num, order) \
        (base + tc_num * LVTS_TC_OFFSET + LVTS_MSR0_OFFSET + 4 * order)

#define LVTS_COEFF_A_X_1000         (-250460)
#define LVTS_COEFF_B_X_1000         (250460)

/*=============================================================
 * Efuse
 *=============================================================*/
#define GOLDEN_TEMP_EFUSE_ADDR      (0X11C101A4)
#define DEFAULT_EFUSE_GOLDEN_TEMP   (60)
#define read_golden_temp(addr)      ((thermal_read(addr) & _BITMASK_(31:24)) >> 24)

/*=============================================================
 * Thermal SRAM
 *=============================================================*/
#define THERMAL_CSRAM_BASE              (0x00114000)
#define CPU_TEMP_BASE_ADDR              (THERMAL_CSRAM_BASE)
#define CPU_HEADROOM_BASE_ADDR          (THERMAL_CSRAM_BASE + 0x20)
#define CPU_HEADROOM_RATIO_BASE_ADDR    (THERMAL_CSRAM_BASE + 0x40)
#define CPU_PREDICT_TEMP_BASE_ADDR      (THERMAL_CSRAM_BASE + 0x60)

/*TTJ*/
#define CPU_TTJ_BASE_ADDR      (THERMAL_CSRAM_BASE + 0x100)
#define GPU_TTJ_BASE_ADDR      (THERMAL_CSRAM_BASE + 0x104)
#define APU_TTJ_BASE_ADDR      (THERMAL_CSRAM_BASE + 0x108)

/*power budget*/
#define CPU_PB_BASE_ADDR      (THERMAL_CSRAM_BASE + 0x110)
#define GPU_PB_BASE_ADDR      (THERMAL_CSRAM_BASE + 0x114)
#define APU_PB_BASE_ADDR      (THERMAL_CSRAM_BASE + 0x118)

/*min opp hint*/
#define CPU_MIN_OPP_BASE_ADDR      (THERMAL_CSRAM_BASE + 0x120)

#define CPU_ACTIVE_BASE_ADDR      (THERMAL_CSRAM_BASE + 0x130)

#define CPU_JATM_SUSPEND_ADDR      (THERMAL_CSRAM_BASE + 0x140)
#define GPU_JATM_SUSPEND_ADDR      (THERMAL_CSRAM_BASE + 0x144)

#define MIN_THROTTLE_FREQ_BASE_ADDR      (THERMAL_CSRAM_BASE + 0x14C)

#define GPU_MAX_TEMP_BASE_ADDR      (THERMAL_CSRAM_BASE + 0x180)
#define GPU_LIMIT_FREQ_BASE_ADDR    (THERMAL_CSRAM_BASE + 0x184)
#define GPU_CURR_FREQ_BASE_ADDR     (THERMAL_CSRAM_BASE + 0x188)

#define APU_MAX_TEMP_BASE_ADDR      (THERMAL_CSRAM_BASE + 0x190)
#define APU_LIMIT_OPP_BASE_ADDR     (THERMAL_CSRAM_BASE + 0x194)
#define APU_CURR_OPP_BASE_ADDR      (THERMAL_CSRAM_BASE + 0x198)

#define VCORE_MIN_TEMP_BASE_ADDR    (THERMAL_CSRAM_BASE + 0x1A0)

#define CPU_EMUL_TEMP_BASE_ADDR     (THERMAL_CSRAM_BASE + 0x1B0)
#define GPU_EMUL_TEMP_BASE_ADDR     (THERMAL_CSRAM_BASE + 0x1B4)
#define APU_EMUL_TEMP_BASE_ADDR     (THERMAL_CSRAM_BASE + 0x1B8)
#define VCORE_EMUL_TEMP_BASE_ADDR   (THERMAL_CSRAM_BASE + 0x1BC)

#define CPU_LIMIT_FREQ_BASE_ADDR       (THERMAL_CSRAM_BASE + 0x200)
#define CPU_CURR_FREQ_BASE_ADDR        (THERMAL_CSRAM_BASE + 0x210)
#define CPU_MAX_LOG_TEMP_BASE_ADDR     (THERMAL_CSRAM_BASE + 0x220)

#define CPU_TIMESTAMP_BASE_ADDR        (THERMAL_CSRAM_BASE + 0x230)
#define GPU_TIMESTAMP_BASE_ADDR        (THERMAL_CSRAM_BASE + 0x250)
#define APU_TIMESTAMP_BASE_ADDR        (THERMAL_CSRAM_BASE + 0x254)
#define VCORE_TIMESTAMP_BASE_ADDR      (THERMAL_CSRAM_BASE + 0x258)

#define CPU_LIMIT_OPP_BASE_ADDR        (THERMAL_CSRAM_BASE + 0x260)

#define UTC_COUNT_ADDR                 (THERMAL_CSRAM_BASE + 0x27C)

#endif /*_THERMAL_HW_H */

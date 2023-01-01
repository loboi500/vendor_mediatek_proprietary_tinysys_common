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
 * MCU_BIG(T1,T2)(core6, core7)	LVTS1-0, LVTS1-1(BB)
 * MCU_BIG(T3,T4)(core4, core5)	LVTS2-0, LVTS2-1
 * MCU_LITTLE(T5,T6,T7,T8)	LVTS3-0, LVTS3-1, LVTS3-2, LVTS3-3
 * VPU_MLDA(T9,T10)		LVTS4-0, LVTS4-1
 * GPU(T11,T12)			LVTS5-0, LVTS5-1
 * INFA(T13)			LVTS6-0
 * CAMSYS(T18)			LVTS6-1
 * MDSYS(T14,T15,T20)		LVTS7-0, LVTS7-1, LVTS7-2
 */

/* private thermal sensor enum */
enum lvts_sensor_enum {
	L_TS_LVTS1_0 = 0,	/* LVTS1-0 1*/
	L_TS_LVTS1_1,		/* LVTS1-1 2(BB)*/
	L_TS_LVTS2_0,		/* LVTS2-0 3*/
	L_TS_LVTS2_1,		/* LVTS2-1 4*/
	L_TS_LVTS3_0,		/* LVTS3-0 5*/
	L_TS_LVTS3_1,		/* LVTS3-1 6*/
	L_TS_LVTS3_2,		/* LVTS3-2 7*/
	L_TS_LVTS3_3,		/* LVTS3-3 8*/
	L_TS_LVTS4_0,		/* LVTS4-0 9*/
	L_TS_LVTS4_1,		/* LVTS4-1 10*/
	L_TS_LVTS5_0,		/* LVTS5-0 11*/
	L_TS_LVTS5_1,		/* LVTS5-1 12*/
	L_TS_LVTS6_0,		/* LVTS6-0 13*/
	L_TS_LVTS6_1,		/* LVTS6-1 14*/
	L_TS_LVTS7_0,		/* LVTS7-0 15*/
	L_TS_LVTS7_1,		/* LVTS7-1 16*/
	L_TS_LVTS7_2,		/* LVTS7-2 17*/
	L_TS_LVTS_NUM
};

enum lvts_mcu_tc_enum {
	LVTS_MCU_CONTROLLER0 = 0,/* LVTSMONCTL0 */
	LVTS_MCU_CONTROLLER1,	/* LVTSMONCTL0_1 */
	LVTS_MCU_CONTROLLER2,	/* LVTSMONCTL0_2 */
	LVTS_MCU_CONTROLLER_NUM
};

enum lvts_tc_enum {
	LVTS_AP_CONTROLLER0 = 0,/* LVTSMONCTL0 */
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
#define LVTS_MCU_CTRL_BASE          (0x11278000)

#define LVTS_MSR0_OFFSET            (0x90)
#define LVTS_TC_OFFSET              (0x100)
#define LVTS_MSR_REG(base, tc_num, order) \
        (base + tc_num * LVTS_TC_OFFSET + LVTS_MSR0_OFFSET + 4 * order)

#define LVTS_COEFF_A_X_1000         (-252500)
#define LVTS_COEFF_B_X_1000         (252500)

/*=============================================================
 * Efuse
 *=============================================================*/
#define GOLDEN_TEMP_EFUSE_ADDR      (0X11C101C0)
#define DEFAULT_EFUSE_GOLDEN_TEMP   (50)
#define read_golden_temp(addr)      ((thermal_read(addr) & _BITMASK_(31:24)) >> 24)

/*=============================================================
 * Thermal SRAM
 *=============================================================*/
#define THERMAL_CSRAM_BASE              (0x0010f000)
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

#endif /*_THERMAL_HW_H */

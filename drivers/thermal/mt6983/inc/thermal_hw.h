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
 * module            LVTS Plan
 *=====================================================
 * MCU_BIG         LVTS1-0, LVTS1-1, LVTS1-2, LVTS1-3
 * MCU_BIG         LVTS2-0, LVTS2-1, LVTS2-2, LVTS2-3
 * MCU_LITTLE    LVTS3-0, LVTS3-1, LVTS3-2, LVTS3-3
 * ADCT         LVTS4-0, LVTS4-1, LVTS4-2, LVTS4-3
 * GPU            LVTS5-0, LVTS5-1
 * VPU            LVTS6-0, LVTS6-1, LVTS6-2, LVTS6-3
 * DRAM CH0     LVTS7-0,
 * DRAM CH1     LVTS7-1,
 * SOC TOP      LVTS7-2,
 * DRAM CH3     LVTS8-0,
 * DRAM CH4     LVTS8-1,
 * SOC MM       LVTS8-2,
 * MD-4G        LVTS9-0
 * MD-5G        LVTS9-1
 * MD-3G        LVTS9-2
 * ptp_therm_ctrl_AP  Base address: (+0x1031_5000), 0x1031_5100,
 *                                 0x1031_5200, 0x1031_5300, 0x1031_5400
 * ptp_therm_ctrl_MCU Base address: (+0x1031_6000), 0x1031_6100,
 *                                 0x1031_6200, 0x1031_6300
 */


/* private thermal sensor enum */
enum lvts_sensor_enum {
	L_TS_LVTS1_0 = 0,    /* LVTS1-0 Big */
	L_TS_LVTS1_1,        /* LVTS1-1 Big */
	L_TS_LVTS1_2,        /* LVTS1-1 Big */
	L_TS_LVTS1_3,        /* LVTS1-1 Big */
	L_TS_LVTS2_0,        /* LVTS2-0 Big */
	L_TS_LVTS2_1,        /* LVTS2-1 Big */
	L_TS_LVTS2_2,        /* LVTS2-2 Big */
	L_TS_LVTS2_3,        /* LVTS2-3 Big */
	L_TS_LVTS3_0,        /* LVTS3-0 Little */
	L_TS_LVTS3_1,        /* LVTS3-1 Little */
	L_TS_LVTS3_2,        /* LVTS3-2 Little */
	L_TS_LVTS3_3,        /* LVTS3-3 Little */
	L_TS_LVTS4_0,        /* LVTS4-0 ADCT */
	L_TS_LVTS4_1,        /* LVTS4-1 ADCT */
	L_TS_LVTS4_2,        /* LVTS4-2 ADCT */
	L_TS_LVTS4_3,        /* LVTS4-3 ADCT */
	L_TS_LVTS5_0,        /* LVTS5-0 GPU */
	L_TS_LVTS5_1,        /* LVTS5-1 GPU */
	L_TS_LVTS6_0,        /* LVTS6-0 VPU */
	L_TS_LVTS6_1,        /* LVTS6-1 VPU */
	L_TS_LVTS6_2,        /* LVTS6-2 VPU */
	L_TS_LVTS6_3,        /* LVTS6-3 VPU */
	L_TS_LVTS7_0,        /* LVTS7-0 DRAM CH0 */
	L_TS_LVTS7_1,        /* LVTS7-1 DRAM CH1 */
	L_TS_LVTS7_2,        /* LVTS7-2 SOC TOP */
	L_TS_LVTS8_0,        /* LVTS8-0 DRAM CH3 */
	L_TS_LVTS8_1,        /* LVTS8-1 DRAM CH4 */
	L_TS_LVTS8_2,        /* LVTS8-2 SOC MM */
	L_TS_LVTS9_0,        /* LVTS9-0 MD-4G */
	L_TS_LVTS9_1,        /* LVTS9-1 MD-5G */
	L_TS_LVTS9_2,        /* LVTS9-2 MD-3G */
	L_TS_LVTS_NUM
};


enum lvts_mcu_tc_enum {
	LVTS_MCU_CONTROLLER0 = 0,/* LVTSMONCTL0 */
	LVTS_MCU_CONTROLLER1,	/* LVTSMONCTL0_1 */
	LVTS_MCU_CONTROLLER2,	/* LVTSMONCTL0_2 */
	LVTS_MCU_CONTROLLER3,	/* LVTSMONCTL0_3 */
	LVTS_MCU_CONTROLLER_NUM
};

enum lvts_tc_enum {
	LVTS_AP_CONTROLLER0 = 0,/* LVTSMONCTL0 */
	LVTS_AP_CONTROLLER1,	/* LVTSMONCTL0_1 */
	LVTS_AP_CONTROLLER2,	/* LVTSMONCTL0_2 */
	LVTS_AP_CONTROLLER3,	/* LVTSMONCTL0_3 */
	LVTS_AP_CONTROLLER4,	/* LVTSMONCTL0_4 */
	LVTS_AP_CONTROLLER_NUM
};

#define DEFAULT_TTJ    (95000)

/*=============================================================
 * Thermal Controller Registers / Coef
 *=============================================================*/
#define LVTS_CTRL_BASE              (0x10315000)
#define LVTS_MCU_CTRL_BASE          (0x10316000)

#define LVTS_MSR0_OFFSET            (0x90)
#define LVTS_TC_OFFSET              (0x100)
#define LVTS_MSR_REG(base, tc_num, order) \
        (base + tc_num * LVTS_TC_OFFSET + LVTS_MSR0_OFFSET + 4 * order)

#define LVTS_COEFF_A_X_1000           (191740)
#define LVTS_COEFF_A_X_1000_HT        (246740)

#define LVTS_COF_T_SLP_GLD            (199410)
#define LVTS_COF_COUNT_R_GLD          (12052)
#define LVTS_COF_T_CONST_OFS          (280000)

#define LVTS_COF_T_SLP_GLD_HT         (254410)
#define LVTS_COF_COUNT_R_GLD_HT       (15380)
#define LVTS_COF_T_CONST_OFS_HT       (170000)
/*=============================================================
 * Efuse
 *=============================================================*/
#define GOLDEN_TEMP_EFUSE_ADDR          (0x11ee01c4)
#define DEFAULT_EFUSE_GOLDEN_TEMP       (60)
#define DEFAULT_EFUSE_GOLDEN_TEMP_HT    (170)
#define DEFAULT_EFUSE_COUNT             (12084)

#define read_golden_temp_ht(addr)      ((thermal_read(addr) & _BITMASK_(15:8)) >> 8)
#define read_golden_temp(addr)      (thermal_read(addr) & _BITMASK_(7:0))
#define read_cali_mode(addr)      (thermal_read(addr) & _BIT_(31))

#define LVTS1_0_COUNT_R_EFUSE_ADDR      (0x11ee01c8)
#define LVTS5_0_COUNT_R_EFUSE_ADDR      (0x11ee01e8)
#define LVTS6_0_COUNT_R_EFUSE_ADDR      (0x11ee01ec)
#define LVTS7_2_COUNT_R_EFUSE_ADDR      (0x11ee01f8)
#define LVTS8_1_COUNT_R_EFUSE_ADDR      (0x11ee01fc)
#define LVTS8_2_COUNT_R_EFUSE_ADDR      (0x11ee01fc)

#define LVTS_COUNT_R_EFUSE(addr, mask, shift) ((thermal_read(addr) >> shift) & mask)
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

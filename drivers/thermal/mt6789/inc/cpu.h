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

#ifndef _CPU_H
#define _CPU_H

#include "thermal_utils.h"

/*=============================================================
 * Thermal Configuration
 *=============================================================*/
/*
 * module  			        LVTS Plan
 *=====================================================
 * AP_LITTLE(TS1,TS2,TS3,TS4)		LVTS1-0, LVTS1-1, LVTS1-2, LVTS1-3
 * AP_BIG(TS5,TS6,TS7,TS8			LVTS2-0, LVTS2-1, LVTS2-2, LVTS2-3
 * GPU(TS9,TS10)					LVTS3-0, LVTS3-1
 * SOC(TS11,TS12)					LVTS3-2, LVTS3-3
 * MD(TS13)							LVTS4-0
 */

/* private thermal sensor enum */
enum cpu_lvts_sensor_enum {
	CPU_TS_LVTS1_0 = 0,	/* LVTS1-0 1(CPU_1_L)*/
	CPU_TS_LVTS1_1,		/* LVTS1-1 2(CPU_2_L)*/
	CPU_TS_LVTS1_2 ,	/* LVTS1-2 3(CPU_3_L)*/
	CPU_TS_LVTS1_3,		/* LVTS1-3 4(CPU_4_L)*/
	CPU_TS_LVTS2_0,		/* LVTS2-0 5(CPU_5_B)*/
	CPU_TS_LVTS2_1,		/* LVTS2-1 6(CPU_6_B)*/
	CPU_TS_LVTS2_2,		/* LVTS2-2 7(CPU_7_B)*/
	CPU_TS_LVTS2_3,		/* LVTS2-3 8(CPU_8_B)*/
	CPU_TS_LVTS_NUM
};

enum cpu_id_enum {
	CPU0 = 0,
	CPU1,
	CPU2,
	CPU3,
	CPU4,
	CPU5,
	CPU6,
	CPU7,
	CPU_NUM
};

enum cluster_id_enum {
	L_CLUSTER = 0,
	B_CLUSTER,
	CLUSTER_NUM
};


extern struct lvts_config cpu_lvts_cfg;
extern int max_cpu_temp[CPU_NUM];

#endif /*_CPU_H */

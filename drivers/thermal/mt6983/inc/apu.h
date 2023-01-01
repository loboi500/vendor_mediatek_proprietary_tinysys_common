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

#ifndef _APU_H
#define _APU_H
/*=============================================================
 * Thermal Configuration
 *=============================================================*/
#define USE_APU_MBOX	(1)

#ifdef USE_APU_MBOX
#define APU_MBOX_BASE           (0x190E1000)
#define APU_MBOX_TTJ_ADDR       (APU_MBOX_BASE + 0x700)
#define APU_MBOX_PB_ADDR        (APU_MBOX_BASE + 0x704)
#define APU_MBOX_MAX_TEMP_ADDR  (APU_MBOX_BASE + 0x708)
#define APU_MBOX_LIMIT_OPP_ADDR (APU_MBOX_BASE + 0x70C)
#define APU_MBOX_CUR_OPP_ADDR   (APU_MBOX_BASE + 0x710)
#define APU_MBOX_EMUL_TEMP_ADDR (APU_MBOX_BASE + 0x714)
#define APU_MBOX_RESERVED_ADDR  (APU_MBOX_BASE + 0x718)
#endif

enum apu_lvts_sensor_enum {
    APU_TS_LVTS6_0 = 0,	/* LVTS6-0*/
    APU_TS_LVTS6_1,		/* LVTS6-1*/
    APU_TS_LVTS6_2,		/* LVTS6-2*/
    APU_TS_LVTS6_3,		/* LVTS6-3*/
    APU_TS_LVTS_NUM
};

extern struct lvts_config apu_lvts_cfg;

#endif /*_APU_H */

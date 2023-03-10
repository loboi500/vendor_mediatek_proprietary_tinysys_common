/* Copyright Statement:
 *
 * This software/firmware and related documentation ("MediaTek Software") are
 * protected under relevant copyright laws. The information contained herein
 * is confidential and proprietary to MediaTek Inc. and/or its licensors.
 * Without the prior written permission of MediaTek inc. and/or its licensors,
 * any reproduction, modification, use or disclosure of MediaTek Software,
 * and information contained herein, in whole or in part, shall be strictly prohibited.
 */
/* MediaTek Inc. (C) 2018. All rights reserved.
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

#ifndef MTK_ARSI_LIBRARY_ENTRY_POINTS_H
#define MTK_ARSI_LIBRARY_ENTRY_POINTS_H

#include <string.h>

#include <arsi_api.h>

#ifdef __cplusplus
extern "C" {
#endif


/*
 * =============================================================================
 *                     Need 3rd-party to implement the libraries
 * =============================================================================
 */

#if 0 /* For HAL */
/*
 * ONLY for dynamic link, like libXXX.so in HAL (including parsing lib.so)
 * NEVER implement this function for libXXX.a in FreeRTOS
 */
void dynamic_link_arsi_assign_lib_fp(AurisysLibInterface *lib);


#else /* For FreeRTOS */
/*
 * For static link, like libXXX.a in FreeRTOS
 * However, it's fine to implement it for libXXX.so
 */
void demo_arsi_assign_lib_fp(AurisysLibInterface *lib); /* DEMO only */

#ifdef CFG_MTK_AUDIODSP_SUPPORT
void iir_arsi_assign_lib_fp(AurisysLibInterface *lib);
void mtk_sp_team_arsi_assign_lib_fp(AurisysLibInterface *lib);
void besloudness_arsi_assign_lib_fp(AurisysLibInterface *lib);
void dcremoval_arsi_assign_lib_fp(AurisysLibInterface *lib);
#else
#define iir_arsi_assign_lib_fp demo_arsi_assign_lib_fp
#define mtk_sp_team_arsi_assign_lib_fp demo_arsi_assign_lib_fp
#define besloudness_arsi_assign_lib_fp demo_arsi_assign_lib_fp
#define dcremoval_arsi_assign_lib_fp demo_arsi_assign_lib_fp
#endif
#ifdef AURISYS_CPP_SUPPORT
void democpp_arsi_assign_lib_fp(AurisysLibInterface *lib);
#else
#define democpp_arsi_assign_lib_fp demo_arsi_assign_lib_fp
#endif

#if MT6660_SUPPORT
void mt6660_arsi_assign_lib_fp(AurisysLibInterface *lib);
#else
#define mt6660_arsi_assign_lib_fp demo_arsi_assign_lib_fp
#endif

#if RT5512_SUPPORT
void rt5512_arsi_assign_lib_fp(AurisysLibInterface *lib);
#else
#define rt5512_arsi_assign_lib_fp demo_arsi_assign_lib_fp
#endif

#if NXP_SMARTPA_SUPPORT
void tfadsp_link_arsi_assign_lib_fp(AurisysLibInterface *lib);
#else
#define tfadsp_link_arsi_assign_lib_fp demo_arsi_assign_lib_fp
#endif

#if GOODIXSPEECH_LIB_SUPPORT
void lvve18_arsi_assign_lib_fp(AurisysLibInterface *lib);
#else
#define lvve18_arsi_assign_lib_fp demo_arsi_assign_lib_fp
#endif

#if !NXPSPEECH_DUMMY_LIB
void lvve_arsi_assign_lib_fp(AurisysLibInterface *lib);
#else
#define lvve_arsi_assign_lib_fp demo_arsi_assign_lib_fp
#endif

#if !NXPRECORD_DUMMY_LIB
void lvim_arsi_assign_lib_fp(AurisysLibInterface *lib);
#else
#define lvim_arsi_assign_lib_fp demo_arsi_assign_lib_fp
#endif

#if !FV_DUMMY_LIB
void FV_arsi_assign_lib_fp(AurisysLibInterface *lib);
#else
#define FV_arsi_assign_lib_fp demo_arsi_assign_lib_fp
#endif

#if DIRAC_SUPPORT
void dirac_arsi_assign_lib_fp(AurisysLibInterface *lib);
#else
#define dirac_arsi_assign_lib_fp demo_arsi_assign_lib_fp
#endif
void smartpa_arsi_assign_lib_fp(AurisysLibInterface *lib);

#if MTKSPEECH_LIB_SUPPORT
void mtk_sp_arsi_assign_lib_fp(AurisysLibInterface *lib);
#else
#define mtk_sp_arsi_assign_lib_fp demo_arsi_assign_lib_fp
#endif

#if GAMEVOIVE_LIB_SUPPORT
void game_voice_eng_arsi_assign_lib_fp(AurisysLibInterface *lib);
#else
#define game_voice_eng_arsi_assign_lib_fp demo_arsi_assign_lib_fp
#endif


#define AURISYS_LINK_LIB_NAME_TO_API(name, api) \
	do { \
		if (!name || !api) { \
			break; \
		} \
		if (!strcmp(name, "aurisys_demo")) { \
			demo_arsi_assign_lib_fp(api); \
		} else if (!strcmp(name, "smartpa_mt6660")) {\
			mt6660_arsi_assign_lib_fp(api); \
		} else if (!strcmp(name, "smartpa_rt5512")) {\
			rt5512_arsi_assign_lib_fp(api); \
		} else if (!strcmp(name, "smartpa_tfaxxxx")) {\
			tfadsp_link_arsi_assign_lib_fp(api); \
		} else if (!strcmp(name, "mtk_bessound")) {\
			besloudness_arsi_assign_lib_fp(api); \
		} else if (!strcmp(name, "mtk_dcrflt")) {\
			dcremoval_arsi_assign_lib_fp(api); \
		} else if (!strcmp(name, "mtk_democpp")) {\
			democpp_arsi_assign_lib_fp(api); \
		} else if (!strcmp(name, "fv_speech")) {\
			FV_arsi_assign_lib_fp(api); \
		} else if (!strcmp(name, "goodix_speech")) {\
			lvve18_arsi_assign_lib_fp(api); \
		} else if (!strcmp(name, "nxp_speech")) {\
			lvve_arsi_assign_lib_fp(api); \
		} else if (!strcmp(name, "nxp_record")) {\
			lvim_arsi_assign_lib_fp(api); \
		} else if (!strcmp(name, "dirac")) {\
			dirac_arsi_assign_lib_fp(api); \
		} else if (!strcmp(name, "mtk_sp")) { \
			mtk_sp_arsi_assign_lib_fp(api); \
		} else if(!strcmp(name, "game_voice_enh")) { \
			game_voice_eng_arsi_assign_lib_fp(api); \
		} \
	} while (0)


#endif




#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif /* end of MTK_ARSI_LIBRARY_ENTRY_POINTS_H */


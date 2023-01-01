/* Copyright Statement:
 *
 * This software/firmware and related documentation ("MediaTek Software") are
 * protected under relevant copyright laws. The information contained herein is
 * confidential and proprietary to MediaTek Inc. and/or its licensors. Without
 * the prior written permission of MediaTek inc. and/or its licensors, any
 * reproduction, modification, use or disclosure of MediaTek Software, and
 * information contained herein, in whole or in part, shall be strictly
 * prohibited.
 *
 * MediaTek Inc. (C) 2019. All rights reserved.
 *
 * BY OPENING THIS FILE, RECEIVER HEREBY UNEQUIVOCALLY ACKNOWLEDGES AND AGREES
 * THAT THE SOFTWARE/FIRMWARE AND ITS DOCUMENTATIONS ("MEDIATEK SOFTWARE")
 * RECEIVED FROM MEDIATEK AND/OR ITS REPRESENTATIVES ARE PROVIDED TO RECEIVER
 * ON AN "AS-IS" BASIS ONLY. MEDIATEK EXPRESSLY DISCLAIMS ANY AND ALL
 * WARRANTIES, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE OR
 * NONINFRINGEMENT. NEITHER DOES MEDIATEK PROVIDE ANY WARRANTY WHATSOEVER WITH
 * RESPECT TO THE SOFTWARE OF ANY THIRD PARTY WHICH MAY BE USED BY,
 * INCORPORATED IN, OR SUPPLIED WITH THE MEDIATEK SOFTWARE, AND RECEIVER AGREES
 * TO LOOK ONLY TO SUCH THIRD PARTY FOR ANY WARRANTY CLAIM RELATING THERETO.
 * RECEIVER EXPRESSLY ACKNOWLEDGES THAT IT IS RECEIVER'S SOLE RESPONSIBILITY TO
 * OBTAIN FROM ANY THIRD PARTY ALL PROPER LICENSES CONTAINED IN MEDIATEK
 * SOFTWARE. MEDIATEK SHALL ALSO NOT BE RESPONSIBLE FOR ANY MEDIATEK SOFTWARE
 * RELEASES MADE TO RECEIVER'S SPECIFICATION OR TO CONFORM TO A PARTICULAR
 * STANDARD OR OPEN FORUM. RECEIVER'S SOLE AND EXCLUSIVE REMEDY AND MEDIATEK'S
 * ENTIRE AND CUMULATIVE LIABILITY WITH RESPECT TO THE MEDIATEK SOFTWARE
 * RELEASED HEREUNDER WILL BE, AT MEDIATEK'S OPTION, TO REVISE OR REPLACE THE
 * MEDIATEK SOFTWARE AT ISSUE, OR REFUND ANY SOFTWARE LICENSE FEES OR SERVICE
 * CHARGE PAID BY RECEIVER TO MEDIATEK FOR SUCH MEDIATEK SOFTWARE AT ISSUE.
 *
 * The following software/firmware and/or related documentation ("MediaTek
 * Software") have been modified by MediaTek Inc. All revisions are subject to
 * any receiver's applicable license agreements with MediaTek Inc.
 */

#include "pmic_wrap.h"
#include "irq.h"

/************** For timeout API *********************/
#define PWRAP_NO_TIMEOUT
#ifdef PWRAP_NO_TIMEOUT
static unsigned int _pwrap_get_current_time(void) {
    return 0;
}

static int _pwrap_timeout_ns(unsigned int start_time, unsigned int elapse_time) {
    return 0;
}

static unsigned int _pwrap_time2ns(unsigned int time_us) {
    return 0;
}
#else
static unsigned int _pwrap_get_current_time(void) {
    portTickType tick_in_ns = (1000 / portTICK_RATE_MS) * 1000;
    return (xTaskGetTickCount() * tick_in_ns);
}

static int _pwrap_timeout_ns (unsigned int start_time, unsigned int elapse_time) {
    unsigned int cur_time = 0 ;
    portTickType tick_in_ns = (1000 / portTICK_RATE_MS) * 1000;
    cur_time = (xTaskGetTickCount() * tick_in_ns);
    return (cur_time > (start_time + elapse_time));
}

static unsigned int _pwrap_time2ns (unsigned int time_us) {
    return (time_us * 1000);
}

#endif

/****************  channel API *****************/
typedef unsigned int (*loop_condition_fp)(unsigned int);

static inline unsigned int wait_for_fsm_idle(unsigned int x) {
    return (GET_SWINF_TINYSYS_FSM( x ) != 0 );
}
static inline unsigned int wait_for_fsm_vldclr(unsigned int x) {
    return (GET_SWINF_TINYSYS_FSM( x ) != 6);
}

static inline unsigned int  wait_for_state_idle(loop_condition_fp fp, unsigned int timeout_us, void *wacs_register,
            void *wacs_vldclr_register, unsigned int *read_reg) {
    unsigned long long start_time_ns = 0, timeout_ns = 0;
    unsigned int reg_rdata;

    start_time_ns = _pwrap_get_current_time();
    timeout_ns = _pwrap_time2ns(timeout_us);

    do {
        if (_pwrap_timeout_ns(start_time_ns, timeout_ns)) {
            PWRAPERR("wait_for_idle timeout\n");
            return E_PWR_WAIT_IDLE_TIMEOUT;
        }
        reg_rdata = DRV_Reg32(wacs_register);
        if (GET_SWINF_TINYSYS_INIT_DONE(reg_rdata) != WACS_INIT_DONE) {
            PWRAPERR("init not finish\n");
            return E_PWR_NOT_INIT_DONE;
        }
    } while (fp(reg_rdata));

    if (read_reg)
        *read_reg = reg_rdata;

    return 0;
}

static inline unsigned int wait_for_state_ready(loop_condition_fp fp, unsigned int timeout_us, void *wacs_register,
            unsigned int *read_reg) {

    unsigned long long start_time_ns = 0, timeout_ns = 0;
    unsigned int reg_rdata;

    start_time_ns = _pwrap_get_current_time();
    timeout_ns = _pwrap_time2ns(timeout_us);

    do {
        if (_pwrap_timeout_ns(start_time_ns, timeout_ns)) {
            PWRAPERR("wait_for_ready timeout\n");
            return E_PWR_WAIT_IDLE_TIMEOUT;
        }
        reg_rdata = DRV_Reg32(wacs_register);
        if (GET_SWINF_TINYSYS_INIT_DONE(reg_rdata) != WACS_INIT_DONE) {
            PWRAPERR("init not finish\n");
            return E_PWR_NOT_INIT_DONE;
        }
    } while (fp(reg_rdata));

    if(read_reg)
        *read_reg = reg_rdata;

    return 0;
}

static signed int pwrap_swinf_tinysys(unsigned int cmd, unsigned int write, unsigned int pmifid, unsigned int slvid,
	unsigned int addr, unsigned int bytecnt, unsigned int wdata, unsigned int *rdata)
{
        unsigned int reg_rdata = 0;
        unsigned int ret = 0;
#ifdef CFG_FREERTOS_SMP
        UBaseType_t uxSavedInterruptStatus;
#else
        bool in_isr_and_cs = false;
#endif

	/* Check argument validation */
	if ((cmd & ~(0x3)) != 0)
		return E_PWR_INVALID_CMD;
	if ((write & ~(0x1)) != 0)
		return E_PWR_INVALID_RW;
	if ((pmifid & ~(0x3)) != 0)
		return E_PWR_INVALID_PMIFID;
	if ((slvid & ~(0xf)) != 0)
		return E_PWR_INVALID_SLVID;
	if ((addr & ~(0xffff)) != 0)
		return E_PWR_INVALID_ADDR;
	if ((bytecnt & ~(0x1)) != 0)
		return E_PWR_INVALID_BYTECNT;
	if ((wdata & ~(0xffff)) != 0)
		return E_PWR_INVALID_WDAT;

        /* check C.S. */
#ifdef CFG_FREERTOS_SMP
	uxSavedInterruptStatus = taskENTER_CRITICAL_FROM_ISR();
#else
        in_isr_and_cs = is_in_isr();
        if(!in_isr_and_cs) {
            taskENTER_CRITICAL();
        }
#endif

	/* Check whether INIT_DONE is set */
	if (pmifid == 0)
		reg_rdata = DRV_Reg32(PMIF_SPI_PMIF_SWINF_TINYSYS_STA);

	if (GET_SWINF_TINYSYS_INIT_DONE(reg_rdata) != 0x1)
		return E_PWR_NOT_INIT_DONE;

	/* Wait for Software Interface FSM state to be IDLE */
	ret = wait_for_state_idle(wait_for_fsm_idle, TIMEOUT_WAIT_IDLE,
			(UINT32P)PMIF_SPI_PMIF_SWINF_TINYSYS_STA,
			(UINT32P)PMIF_SPI_PMIF_SWINF_TINYSYS_VLD_CLR, 0);
	if (ret != 0) {
		PWRAPERR("wait_fsm_idle fail, ret=%x\n", ret);
		goto FAIL;
	}

	/* Set the write data */
	if (write == 1)
		DRV_WriteReg32(PMIF_SPI_PMIF_SWINF_TINYSYS_WDATA_31_0, wdata);

	/* Send the command */
	DRV_WriteReg32(PMIF_SPI_PMIF_SWINF_TINYSYS_ACC,
			(cmd << 30) | (write << 29) | (slvid << 24) | (bytecnt << 16) | addr);

	if (write == 0) {
		if (NULL == rdata) {
			PWRAPERR("rdata null\n");
			ret= E_PWR_INVALID_ARG;
			goto FAIL;
		}

		/* Wait for FSM to be WFVLDCLR, get the read data and clear the valid flag */
		ret = wait_for_state_ready(wait_for_fsm_vldclr, TIMEOUT_READ,
				(UINT32P)(PMIF_SPI_PMIF_SWINF_TINYSYS_STA), &reg_rdata);
		if (ret != 0) {
			PWRAPERR("wait_fsm_vldclr fail,ret=%x\n", ret);
			ret += 1;
			goto FAIL;
		}

		*rdata = DRV_Reg32(PMIF_SPI_PMIF_SWINF_TINYSYS_RDATA_31_0);

		DRV_WriteReg32(PMIF_SPI_PMIF_SWINF_TINYSYS_VLD_CLR, 0x1);
	}

    FAIL:
        /* check C.S. */
#ifdef CFG_FREERTOS_SMP
        taskEXIT_CRITICAL_FROM_ISR(uxSavedInterruptStatus);
#else
        if(!in_isr_and_cs) {
            taskEXIT_CRITICAL();
        }
#endif
        if (ret != 0) {
            PWRAPERR("pwrap_swinf_tinysys fail, ret=%x\n", ret);
        }

    return ret;
}

signed int pwrap_tinysys_read(unsigned int adr, unsigned int *rdata) {
    return pwrap_swinf_tinysys(DEFAULT_CMD, 0, PMIF_SPI_PMIFID,
                            DEFAULT_SLVID, adr, DEFAULT_BYTECNT, 0x0, rdata);
}

signed int pwrap_tinysys_write(unsigned int adr, unsigned int wdata) {
    return pwrap_swinf_tinysys(DEFAULT_CMD, 1, PMIF_SPI_PMIFID,
                            DEFAULT_SLVID, adr, DEFAULT_BYTECNT, wdata, 0x0);
}

/******* For channel UT ********/
#ifdef PMIC_WRAP_DEBUG
static unsigned int pwrap_read_test(void) {
    unsigned int rdata = 0;
    unsigned int ret = 0;

    ret = pwrap_tinysys_read(DEW_READ_TEST, &rdata);
    if (rdata != DEFAULT_VALUE_READ_TEST) {
        PWRAPERR("Read fail, rdata=0x%x, exp=0x5aa5, ret=0x%x\n", rdata, ret);
        return E_PWR_READ_TEST_FAIL;
    } else {
        PWRAPLOG("Read pass, ret=%x\n", ret);
    }

    return 0;
}

static unsigned int pwrap_write_test(void) {
    unsigned int rdata = 0;
    unsigned int ret = 0;
    unsigned int ret1 = 0;

    ret = pwrap_tinysys_write(DEW_WRITE_TEST, PWRAP_WRITE_TEST_VALUE);
    ret1 = pwrap_tinysys_read(DEW_WRITE_TEST, &rdata);
    if ((rdata != PWRAP_WRITE_TEST_VALUE) || (ret != 0) || (ret1 != 0)) {
        PWRAPERR("Write fail, rdata=0x%x, exp=0xa55a, ret=0x%x, ret1=0x%x\n",
            rdata, ret, ret1);
        return E_PWR_INIT_WRITE_TEST;
    } else {
        PWRAPLOG("Write pass\n");
    }
    return 0;
}
#endif
signed int pmic_wrap_init(void) {

#ifdef PMIC_WRAP_CG
    pmic_wrap_cg_enable();
#endif

    /* TINYSYS Read/Write Test */
#ifdef PMIC_WRAP_DEBUG
    pwrap_read_test();
    pwrap_write_test();
#endif

    PWRAPLOG("init done\n");

    return 0;
}

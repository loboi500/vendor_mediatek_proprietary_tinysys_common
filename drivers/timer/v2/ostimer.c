/* Copyright Statement:
 *
 * This software/firmware and related documentation ("MediaTek Software") are
 * protected under relevant copyright laws. The information contained herein
 * is confidential and proprietary to MediaTek Inc. and/or its licensors.
 * Without the prior written permission of MediaTek inc. and/or its licensors,
 * any reproduction, modification, use or disclosure of MediaTek Software,
 * and information contained herein, in whole or in part, shall be strictly prohibited.
 */
/* MediaTek Inc. (C) 2010. All rights reserved.
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

#include <tinysys_reg.h>
#include "driver_api.h"
#include "timer.h"
#include "ostimer.h"
#include "irq.h"

#include <FreeRTOS.h>
#include <task.h>
#ifdef CFG_INTERNAL_TIMER_SUPPORT
#include "timers.h"
#elif defined(CFG_CSR_XGPT_SUPPORT)
#include <mt_printf.h>
#include "encoding.h"
#else
#include <debug.h>
#endif

#define MBOX_SLOT_SIZE	4   // 4 bytes

#ifdef CFG_ONDIEMET_SUPPORT
#include "ondiemet.h"
#endif

#define OSTIMER_PM_SUSPEND  (0)
#define OSTIMER_PM_RESUME   (1)

#define OSTIMER_TICK_TO_NS     (76) /* 13MHz */
#define OSTIMER_MAX_TICK_COUNT (0xFFFFFFFF)

#define OSTIMER_32BITS_TO_64BITS(val_h, val_l) \
    (((unsigned long long)val_h << 32) & 0xFFFFFFFF00000000) | \
    ((unsigned long long)val_l & 0x00000000FFFFFFFF)

#define OSTIMER_64BITS_TO_32BITS_H(val) \
    (unsigned int)((val >> 32) & 0x00000000FFFFFFFF)

#define OSTIMER_64BITS_TO_32BITS_L(val) \
    (unsigned int)(val & 0x00000000FFFFFFFF)

struct clock_data {
    unsigned long long epoch_ns;
    unsigned long long epoch_cyc;
    unsigned int mult;
    unsigned int shift;
    bool insuspend;

    /* time sync v2 */
    unsigned long long epoch_ns_raw;
    unsigned char      epoch_freeze;
    unsigned char      epoch_ver;
    unsigned int       mbox_base;
#ifdef CFG_INTERNAL_TIMER_SUPPORT
    TickType_t         xTickCount;
#endif
};

static struct clock_data cd = {
#ifdef CFG_FPGA
    /* 6000000's mult and shift*/
    .mult = 349525333,
    .shift = 21,
#else
    /*
     * AP System Timer uses 13MHz while AP is awake.
     * Use 13000000's mult and shift.
     */
    .mult = 161319385,
    .shift = 21,
#endif
    .insuspend = 0,

    /* time sync v2 */
    .epoch_ns = 0,
    .epoch_cyc = 0,
    .epoch_freeze = 0,
    .mbox_base = 0,

    /*
     * ap initial version is 0, thus set initial version in
     * co-processor as 0xFF to ensure synchronization will be
     * started.
     */
    .epoch_ver = 0xFF,
};

#define ts_timesync_get_ver_in_reg_h(val) \
    ((((val) & TIMESYNC_HEADER_VER_MASK)) >> TIMESYNC_HEADER_VER_OFS)

#define ts_timesync_get_freeze_in_reg_h(val) \
    (((val) & TIMESYNC_HEADER_FREEZE) ? 1 : 0)

static unsigned int ts_timesync_mbox_read(unsigned int id);
static unsigned int ts_timesync_mbox_write(unsigned int id, unsigned int val);

#ifndef CFG_INTERNAL_TIMER_SUPPORT
#ifdef CFG_TIMER_QUIRK_READ_OSTIMER_TWICE
/*
 * ts_ostimer_read_isr (legacy version)
 *
 * Read low/high twice to cover boundary hw issue.
 */
unsigned long long ts_ostimer_read_isr(int isr_mode)
{
    unsigned long long val;
    unsigned long high_1, high_2, low_1, low_2;

    /*
     * must ensure atomicity from reading ostimer to calculating "val"
     * to prevent timestamp drift issue in print log.
     */
    if (0 == isr_mode)
        taskENTER_CRITICAL();

#ifdef CFG_CSR_XGPT_SUPPORT
    low_1 = (unsigned long)mrv_read_csr(CSR_TIME);
    high_1 = (unsigned long)mrv_read_csr(CSR_TIMEH);
    low_2 = (unsigned long)mrv_read_csr(CSR_TIME);
    high_2 = (unsigned long)mrv_read_csr(CSR_TIMEH);
#else
    low_1 = DRV_Reg32(OSTIMER_CUR_LOW);
    high_1 = DRV_Reg32(OSTIMER_CUR_HIGH);
    low_2 = DRV_Reg32(OSTIMER_CUR_LOW);
    high_2 = DRV_Reg32(OSTIMER_CUR_HIGH);
#endif

    if (low_2 < low_1) {
        /*
         * boundary condition
         */
        high_1 = high_2;
        low_1 = low_2;
    }

    val = (((unsigned long long)high_1 << 32) & 0xFFFFFFFF00000000) |
            ((unsigned long long)low_1 & 0x00000000FFFFFFFF);

    if (0 == isr_mode)
        taskEXIT_CRITICAL();

    return val;
}
#else
unsigned long long ts_ostimer_read_isr(int isr_mode)
{
    unsigned long long val;
    unsigned long high = 0, low = 0;

    /*
     * must ensure atomicity from reading ostimer to calculating "val"
     * to prevent timestamp drift issue in print log.
     */

    if (0 == isr_mode)
        taskENTER_CRITICAL();

#ifdef CFG_CSR_XGPT_SUPPORT
    low = (unsigned long)mrv_read_csr(CSR_TIME);
    high = (unsigned long)mrv_read_csr(CSR_TIMEH);
#else
    low = DRV_Reg32(OSTIMER_CUR_LOW);
    high = DRV_Reg32(OSTIMER_CUR_HIGH);
#endif

    val = (((unsigned long long)high << 32) & 0xFFFFFFFF00000000) |
            ((unsigned long long)low & 0x00000000FFFFFFFF);

    if (0 == isr_mode)
        taskEXIT_CRITICAL();

    return val;
}
#endif

unsigned long long ostimer_read(void)
{
    return ts_ostimer_read_isr(0);
}

unsigned long long ostimer_get_ns(void)
{
    return (ostimer_read() * cd.mult) >> cd.shift;
}

unsigned long ostimer_low_cur_to_ns(void)
{
#ifdef CFG_CSR_XGPT_SUPPORT
    return (unsigned long) (mrv_read_csr(CSR_TIME) * cd.mult) >> cd.shift;
#else
    return (unsigned long) (DRV_Reg32(OSTIMER_CUR_LOW) * cd.mult) >> cd.shift;
#endif
}

static inline unsigned long long ts_cyc_to_ns(unsigned long long cyc, unsigned int mult, unsigned int shift)
{
    return (cyc * mult) >> shift;
}

#ifdef CFG_ONDIEMET_SUPPORT
static unsigned long long ts_sched_clock_common(unsigned long long base_ns, unsigned long long base_clk, bool isr_mode,
        unsigned long long* ret_clk)
{
    unsigned long long epoch_ns;
    unsigned long long epoch_cyc;
    unsigned long long cyc;

    if (0 == isr_mode)
        taskENTER_CRITICAL();

    if (0 == base_ns || 0 == base_clk) {
        epoch_cyc = cd.epoch_cyc;
        epoch_ns = cd.epoch_ns;
    } else {
        epoch_cyc = base_clk;
        epoch_ns = base_ns;
    }

    if (0 == isr_mode)
        taskEXIT_CRITICAL();

    cyc = ts_ostimer_read_isr(isr_mode);

    if (ret_clk)
        *ret_clk = cyc;

    cyc = cyc - epoch_cyc;
    return epoch_ns + ts_cyc_to_ns(cyc, cd.mult, cd.shift);
}

unsigned long long met_sched_clock(unsigned long long base_ns, unsigned long long base_clk, bool isr_mode,
                                   unsigned long long* ret_clk)
{
    return ts_sched_clock_common(base_ns, base_clk, isr_mode, ret_clk);
}

unsigned long long ts_get_ap_time_from_sys_timer(unsigned long long sys_timer_cyc)
{
	return cd.epoch_ns + ts_cyc_to_ns(sys_timer_cyc - cd.epoch_cyc, cd.mult, cd.shift);
}
#endif

long long ts_gpt_diff(unsigned long long gpt1, unsigned long long gpt2)
{
    if (gpt1 > gpt2)
        return (long long)ts_cyc_to_ns(gpt1 - gpt2, cd.mult, cd.shift);
    else
        return -(long long)ts_cyc_to_ns(gpt2 - gpt1, cd.mult, cd.shift);
}
#endif

void ts_apmcu_sync(unsigned long long ns, unsigned long long clk) {};

static int ts_timesync_update_base(bool in_isr)
{
    unsigned long tick_h, tick_l, ts_h, ts_l, tick_ver, ts_ver;
    int restart = 0;
    unsigned char fz;

restart:

    restart = 0;

    if (!in_isr)
        taskENTER_CRITICAL();

    tick_h = ts_timesync_mbox_read(TIMESYNC_MBOX_TICK_H);
    tick_l = ts_timesync_mbox_read(TIMESYNC_MBOX_TICK_L);

    tick_ver = ts_timesync_get_ver_in_reg_h(tick_h);
    fz = (unsigned char)ts_timesync_get_freeze_in_reg_h(tick_h);
    tick_h &= ~TIMESYNC_HEADER_MASK;

    cd.epoch_cyc = OSTIMER_32BITS_TO_64BITS(tick_h, tick_l);

    ts_l = ts_timesync_mbox_read(TIMESYNC_MBOX_TS_L);
    ts_h = ts_timesync_mbox_read(TIMESYNC_MBOX_TS_H);

    ts_ver = ts_timesync_get_ver_in_reg_h(ts_h);

    cd.epoch_ns_raw = OSTIMER_32BITS_TO_64BITS(ts_h, ts_l);

    ts_h &= ~TIMESYNC_HEADER_MASK;

    cd.epoch_ns = OSTIMER_32BITS_TO_64BITS(ts_h, ts_l);

    if (tick_ver != ts_ver) {
        restart = 1;
        goto exit_cs;
    }

    cd.epoch_ver = tick_ver;
    cd.epoch_freeze = fz;

exit_cs:

    if (!in_isr)
        taskEXIT_CRITICAL();

    if (restart)
        goto restart;

    return 0;
}

static int ts_timesync_if_need_base_update(void)
{
    unsigned long val;

    val = ts_timesync_mbox_read(TIMESYNC_MBOX_TS_H);

    /* in case mbox is not initialized */
    if (val == 0)
        return 0;

    if (val != (unsigned long)((cd.epoch_ns_raw >> 32) & 0xffffffff))
        return 1;

    return 0;
}

static int ts_get_ap_time(struct sys_time_t* ap_ts, bool isr_mode)
{
    unsigned long long clk;

#ifdef CFG_INTERNAL_TIMER_SUPPORT
    if (!isr_mode)
        clk = xTaskGetTickCount();
    else
        clk = xTaskGetTickCountFromISR();
#else
    clk = ts_ostimer_read_isr(isr_mode);
#endif

    if (ts_timesync_if_need_base_update()) {
        ts_timesync_update_base(isr_mode);
#ifdef CFG_INTERNAL_TIMER_SUPPORT
        if (!isr_mode)
            cd.xTickCount = xTaskGetTickCount();
        else
            cd.xTickCount = clk;
#endif
    }

    if (cd.epoch_freeze) {
        ap_ts->ts = cd.epoch_ns;
        ap_ts->clk = clk;
#ifdef CFG_INTERNAL_TIMER_SUPPORT
        if (!isr_mode)
            ap_ts->off_ns = (unsigned long long)(xTaskGetTickCount() - cd.xTickCount) *
                            portTICK_PERIOD_MS * 1000000U;
        else
            ap_ts->off_ns = (clk - cd.xTickCount) *
                            portTICK_PERIOD_MS * 1000000U;
#else
        ap_ts->off_ns = ts_cyc_to_ns(clk - cd.epoch_cyc, cd.mult, cd.shift);
#endif
        ap_ts->insuspend = 1;
    } else {
#ifdef CFG_INTERNAL_TIMER_SUPPORT
        if (!isr_mode)
            ap_ts->ts = cd.epoch_ns + (((unsigned long long)xTaskGetTickCount() - cd.xTickCount) *
                        portTICK_PERIOD_MS * 1000000U);
        else
            ap_ts->ts = cd.epoch_ns + ((clk - cd.xTickCount) *
                        portTICK_PERIOD_MS * 1000000U);
#else
        ap_ts->ts = cd.epoch_ns + ts_cyc_to_ns(clk - cd.epoch_cyc, cd.mult, cd.shift);
#endif
        ap_ts->clk = clk;
        ap_ts->off_ns = 0;
        ap_ts->insuspend = 0;
    }

    return ap_ts->insuspend;
}

extern unsigned int mbox_get_base(unsigned int mbox);
static unsigned int ts_timesync_mbox_init(void)
{
    if (!cd.mbox_base) {
        cd.mbox_base = mbox_get_base(TIMESYNC_MBOX);

        /* base will be 0 if mbox is not initialized */
        if (!cd.mbox_base)
            return 1;
        else {
            /*
             * reset base to avoid infinite retry in ts_timesync_update_base
             *
             * note here we assume this api will be executed before ap's sys_timer init
             */
            ts_timesync_mbox_write(TIMESYNC_MBOX_TICK_H, 0);
            ts_timesync_mbox_write(TIMESYNC_MBOX_TICK_L, 0);
            ts_timesync_mbox_write(TIMESYNC_MBOX_TS_H, 0);
            ts_timesync_mbox_write(TIMESYNC_MBOX_TS_L, 0);
        }
    }

    return 0;
}

static unsigned int ts_timesync_mbox_read(unsigned int id)
{
    unsigned int val;
    unsigned int slot_ofs;

    if (ts_timesync_mbox_init())
        return 0;

    slot_ofs = MBOX_SLOT_SIZE * id;

    memcpy(&val, (void *)(cd.mbox_base + slot_ofs), MBOX_SLOT_SIZE);

    return val;
}

static unsigned int ts_timesync_mbox_write(unsigned int id, unsigned int val)
{
    unsigned int slot_ofs;

    if (ts_timesync_mbox_init())
        return 0;

    slot_ofs = MBOX_SLOT_SIZE * id;

    memcpy((void *)(cd.mbox_base + slot_ofs), &val, MBOX_SLOT_SIZE);

    return MBOX_SLOT_SIZE;
}

#ifndef CFG_INTERNAL_TIMER_SUPPORT
void ts_timesync_print_base(void)
{
    PRINTF_E("[ts]tick=0x%lx,0x%lx\n",
        (unsigned long)((cd.epoch_cyc >> 32) & 0xffffffff),
        (unsigned long)(cd.epoch_cyc & 0xffffffff));
    PRINTF_E("[ts]ts=%llu.%llu\n",
        cd.epoch_ns / 1000000000,
        (cd.epoch_ns / 1000) % 1000000);

    PRINTF_E("[ts]ver=%u,fz=%u\n", (unsigned int)cd.epoch_ver, (unsigned int)cd.epoch_freeze);
}

void ts_timesync_verify(void)
{
    struct sys_time_t ap_ts;
    unsigned long td_1, td_2;

    td_1 = ostimer_low_cur_to_ns();

    ts_apmcu_time(&ap_ts);

    td_2 = ostimer_low_cur_to_ns();

    ts_timesync_mbox_write(TIMESYNC_MBOX_DEBUG_TS_H,
        OSTIMER_64BITS_TO_32BITS_H(ap_ts.ts));

    ts_timesync_mbox_write(TIMESYNC_MBOX_DEBUG_TS_L,
        OSTIMER_64BITS_TO_32BITS_L(ap_ts.ts));
}
#else
unsigned long long ostimer_get_ns(void)
{
    return 0;
}
#endif

void ts_suspend_handle(unsigned long long ap_ts, unsigned long long ap_clk)
{
    return;
}

void ts_resume_handle(unsigned long long ap_ts, unsigned long long ap_clk)
{
    return;
}

/*
 * ts_apmcu_time()/ts_apmcu_time_isr():
 * return 1: apmcu in suspend state
 * return 0: apmcu in operation state
 *
 * if ap in suspend mode:
 * .ts = ap's latest suspend timestamp
 * .off_ns = nano second offset start from latest suspend timestamp
 * .insuspend = 1
 *
 * if ap in operation mode:
 * .ts = timestamp align with ap
 * .off_ns = 0
 * .insuspend = 0
 */
int ts_apmcu_time(struct sys_time_t* ap_ts)
{
    return ts_get_ap_time(ap_ts, 0);
}

int ts_apmcu_time_isr(struct sys_time_t* ap_ts)
{
    return ts_get_ap_time(ap_ts, 1);
}

#define GPT_BIT_MASK_L 0x00000000FFFFFFFF
#define GPT_BIT_MASK_H 0xFFFFFFFF00000000

#ifndef CFG_INTERNAL_TIMER_SUPPORT
static unsigned int ts_ostimer_read_low_tick(void)
{
#ifdef CFG_CSR_XGPT_SUPPORT
    return (unsigned int)mrv_read_csr(CSR_TIME);
#else
    return DRV_Reg32(OSTIMER_CUR_LOW);
#endif
}

static int ts_check_timeout_tick(unsigned int start_tick, unsigned int timeout_tick)
{
    unsigned int cur_tick;
    unsigned int elapse_tick;

    cur_tick = ts_ostimer_read_low_tick();

    if (start_tick <= cur_tick)
        elapse_tick = cur_tick - start_tick;
    else
        elapse_tick = (OSTIMER_MAX_TICK_COUNT - start_tick) + cur_tick;

    /*check if timeout*/
    if (timeout_tick <= elapse_tick) {
        return 1;
    }

    return 0;
}

static unsigned int ts_time2tick_us(unsigned int time_us)
{
    return TIME_TO_TICK_US(time_us);
}

static unsigned int ts_time2tick_ms(unsigned int time_ms)
{
    return TIME_TO_TICK_MS(time_ms);
}

//==============================================
// busy wait
//==============================================
static void ts_busy_wait_us(unsigned int timeout_us)
{
    unsigned int start_tick, timeout_tick;

    // get timeout tick
    timeout_tick = ts_time2tick_us(timeout_us);
    start_tick = ts_ostimer_read_low_tick();

    // wait for timeout
    while (!ts_check_timeout_tick(start_tick, timeout_tick));
}

static void ts_busy_wait_ms(unsigned int timeout_ms)
{
    unsigned int start_tick, timeout_tick;

    // get timeout tick
    timeout_tick = ts_time2tick_ms(timeout_ms);
    start_tick = ts_ostimer_read_low_tick();

    // wait for timeout
    while (!ts_check_timeout_tick(start_tick, timeout_tick));
}

/* delay msec mseconds, can't delay time bigger than 2.5 min */
void mdelay(unsigned int msec)
{
    ts_busy_wait_ms(msec);
}

/* delay usec useconds, can't delay time bigger than 2.5 min */
void udelay(unsigned int usec)
{
#ifdef _OSTIMER_32kHZ_
    unsigned int usec_t;

    if (usec < US_LIMIT) {
        ts_busy_wait_us(1);
    } else {
        usec_t = usec / 31 + 1;
        ts_busy_wait_us(usec_t);
    }

#else
    ts_busy_wait_us(usec);
#endif
}
#endif

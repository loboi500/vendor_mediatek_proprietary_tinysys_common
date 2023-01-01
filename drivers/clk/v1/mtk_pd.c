/*
 * Copyright (c) 2021 MediaTek Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files
 * (the "Software"), to deal in the Software without restriction,
 * including without limitation the rights to use, copy, modify, merge,
 * publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#include <FreeRTOS.h>
#include <semphr.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <mt_printf.h>
#if defined(CFG_TIMER_SUPPORT)
#include <ostimer.h>
#elif defined(CFG_XGPT_SUPPORT)
#include <xgpt.h>
#elif defined(ISP_CCU)
#include <ccu_utility.h>
#else
#error "no timer defined"
#endif
#include <ccf_common.h>
#include <clk.h>
#include <mtk_clk_provider.h>
#include <mtk_pd.h>
#include <mtk_pd_provider.h>
#include <hwccf_provider.h>
#include "hwccf_port.h"

#define PD_DEBUG  0

#if PD_DEBUG
#define PD_LOG(fmt, arg...) PRINTF_E(fmt, ##arg)
#else
#define PD_LOG(...)
#endif

#if CCF_DEBUG
#define SEMA_TIMEOUT_MS 500
#else
#define SEMA_TIMEOUT_MS 100
#endif

#define PWR_RST_B_BIT			BIT(0)
#define PWR_ISO_BIT			BIT(1)
#define PWR_ON_BIT			BIT(2)
#define PWR_ON_2ND_BIT			BIT(3)
#define PWR_CLK_DIS_BIT			BIT(4)
#define PWR_SRAM_CLKISO_BIT		BIT(5)
#define PWR_SRAM_ISOINT_B_BIT		BIT(6)
#define SRAM_PDN_BIT			BIT(8)
#define SRAM_SLP_BIT			BIT(9)
#define SRAM_PDN_ACK_BIT		BIT(12)
#define SRAM_SLP_ACK_BIT		BIT(13)
#define PWR_STATUS_BIT			((uint32_t)BIT(30))
#define PWR_STATUS_2ND_BIT		((uint32_t)BIT(31))

#define MTK_SCPD_CAPS(_pdc, _x)	((_pdc)->caps & (_x))
#define BIT(nr)				(1 << (nr))

static SemaphoreHandle_t xSemaphore_pd;
static bool mutex_initialized = false;
static uint32_t pdck_pvd_id;
static uint32_t pdsck_pvd_id;
struct mtk_pd_provider pd_providers[NR_PD_PROVIDER] = {{0}};

uint32_t hwccf_remap(uint32_t addr);
void hwccf_write(uint32_t addr, uint32_t value);
uint32_t hwccf_read(uint32_t addr);

static void mtk_pd_lock(void)
{
	/* PD_LOG("%s\n", __func__); */

	if (is_in_isr())
		return;

	if (!mutex_initialized) {
		xSemaphore_pd = xSemaphoreCreateMutex();
		mutex_initialized = true;
	}

	if (xSemaphoreTake(xSemaphore_pd, pdMS_TO_TICKS(SEMA_TIMEOUT_MS)) != pdTRUE) {
		PRINTF_E("xSemaphoreTake(xSemaphore_pd) fail\n");
		configASSERT(0);
	}
}

static void mtk_pd_unlock(void)
{
	/* PD_LOG("%s\n", __func__); */

	if (is_in_isr())
		return;

	if (xSemaphoreGive(xSemaphore_pd) != pdTRUE) {
		PRINTF_E("xSemaphoreGive(xSemaphore_pd) fail\n");
		configASSERT(0);
	}
}

#define mtk_pd_wait_pwr_on(pd) \
	mtk_pd_wait_for_state(pd, 0, 0, mtk_pd_is_pwr_on)
#define mtk_pd_wait_pwr_off(pd) \
	mtk_pd_wait_for_state(pd, 0, 0, mtk_pd_is_pwr_off)
#define mtk_pd_wait_bus_prot_set(reg, mask) \
	mtk_pd_wait_for_state(NULL, reg, mask, mtk_pd_is_state_set)
#define mtk_pd_wait_bus_prot_clr(reg, mask) \
	mtk_pd_wait_for_state(NULL, reg, mask, mtk_pd_is_state_clr)
#define mtk_pd_wait_sram_pwr_on(reg, mask) \
	mtk_pd_wait_for_state(NULL, reg, mask, mtk_pd_is_state_clr)
#define mtk_pd_wait_sram_pwr_down(reg, mask) \
	mtk_pd_wait_for_state(NULL, reg, mask, mtk_pd_is_state_set)

static int mtk_pd_is_on(struct mtk_pd *pd)
{
	const struct mtk_pd_ctrl *pd_ctrl = pdc_from_mpd(pd);
	uint32_t pwr_sta_addr = pd->base + pd_ctrl->ctl_offs;
	uint32_t status = hwccf_read(pwr_sta_addr) & PWR_STATUS_BIT;
	uint32_t status2 = hwccf_read(pwr_sta_addr) & PWR_STATUS_2ND_BIT;

	/*
	 * A domain is on when both status bits are set. If only one is set
	 * return an error. This happens while powering up a domain
	 */

	if (status && status2)
		return true;
	if (!status && !status2)
		return false;

	return -EINVAL;
}

static inline bool mtk_pd_is_state_set(struct mtk_pd *pd, uint32_t reg, uint32_t mask)
{
	PD_LOG("%s\n", __func__);
	return ((hwccf_read(reg) & mask) == mask);
}

static inline bool mtk_pd_is_state_clr(struct mtk_pd *pd, uint32_t reg, uint32_t mask)
{
	PD_LOG("%s\n", __func__);
	return (!(hwccf_read(reg) & mask));
}

static inline bool mtk_pd_is_pwr_on(struct mtk_pd *pd, uint32_t reg, uint32_t mask)
{
	PD_LOG("%s\n", __func__);
	return mtk_pd_is_on(pd);
}

static inline bool mtk_pd_is_pwr_off(struct mtk_pd *pd, uint32_t reg, uint32_t mask)
{
	PD_LOG("%s\n", __func__);
	return (!mtk_pd_is_on(pd));
}

static int mtk_pd_wait_for_state(struct mtk_pd *pd, uint32_t reg, uint32_t mask,
		bool (*fp)(struct mtk_pd *, uint32_t, uint32_t))
{
	int times = 0;

	PD_LOG("%s\n", __func__);

	do {
		if (fp(pd, reg, mask))
			return 0;

		udelay(10);
		times++;
		if (times > 100)
			return -ETIMEDOUT;
	} while (1);
}

static int mtk_pd_set_bus_prot(const struct bus_prot *bp_table, uint8_t steps)
{
	int i, ret;
	uint32_t reg_set, mask, reg_sta;

	PD_LOG("%s\n", __func__);

	for (i = 0; i < steps; i++) {
		reg_set = hwccf_remap(bp_table[i].reg->base + bp_table[i].reg->set_ofs);
		mask = bp_table[i].mask;
		reg_sta = hwccf_remap(bp_table[i].reg->base + bp_table[i].reg->sta_ofs);

		hwccf_write(reg_set, mask);

		ret = mtk_pd_wait_bus_prot_set(reg_sta, mask);
		if (ret) {
			PRINTF_E("%s timeout: reg_sta 0x%x = 0x%x\n", __func__,
					reg_sta, hwccf_read(reg_sta));
			return ret;
		}
	}

	return 0;
}

static int mtk_pd_clear_bus_prot(const struct bus_prot *bp_table, uint8_t steps)
{
	int i;
	uint32_t reg_clr, mask, reg_sta;

	PD_LOG("%s\n", __func__);

	for (i = steps -1; i >= 0; i--) {
		reg_clr = hwccf_remap(bp_table[i].reg->base + bp_table[i].reg->clr_ofs);
		mask = bp_table[i].mask;
		reg_sta = hwccf_remap(bp_table[i].reg->base + bp_table[i].reg->sta_ofs);

		hwccf_write(reg_clr, mask);
	}

	return 0;
}

int mtk_pd_clk_enable(int id, uint32_t clk_pvd);
int mtk_pd_clk_enable(int id, uint32_t clk_pvd)
{
	struct clk *clk;
	int clk_id;
	int ret = 0;

	PD_LOG("%s(%d, %d)\n", __func__, id, clk_pvd);

	clk_id = CLK_ID(clk_pvd, id);

	clk = clk_get(clk_id);
	if (is_err_or_null_clk(clk))
		return (int)clk;

	ret = clk_enable(clk);
	if (ret)
		clk_disable(clk);

	return ret;
}

void mtk_pd_clk_disable(int id, uint32_t clk_pvd);
void mtk_pd_clk_disable(int id, uint32_t clk_pvd)
{
	struct clk *clk;
	int clk_id;

	PD_LOG("%s(%d, %d)\n", __func__, id, clk_pvd);

	clk_id = CLK_ID(clk_pvd, id);
	PD_LOG("%s: clk_id=0x%X\n", __func__, clk_id);

	clk = clk_get(clk_id);
	if (is_err_or_null_clk(clk))
		return;

	clk_disable(clk);
}

static int scpsys_sram_enable(struct mtk_pd *pd, uint32_t ctl_addr)
{
	const struct mtk_pd_ctrl *pd_ctrl = pdc_from_mpd(pd);
	struct mtk_pd_sram_ops *sram_ops = pd->sram_ops;
	uint32_t ack_sta;
	uint32_t val;
	int ret = 0;

	PD_LOG("%s(0x%x)\n", __func__, ctl_addr);

	if (MTK_SCPD_CAPS(pd_ctrl, MTK_SCPD_SRAM_SLP))
		val = hwccf_read(ctl_addr) | SRAM_SLP_BIT;
	else
		val = hwccf_read(ctl_addr) & ~SRAM_PDN_BIT;

	hwccf_write(ctl_addr, val);

	if (sram_ops && sram_ops->sram_on) {
		ret = sram_ops->sram_on(pd);
	} else {
		/* Either wait until SRAM_PDN_ACK all 1 or 0 */
		if (MTK_SCPD_CAPS(pd_ctrl, MTK_SCPD_SRAM_SLP))
			ack_sta = SRAM_SLP_ACK_BIT;
		else
			ack_sta = SRAM_PDN_BIT;

		ret = mtk_pd_wait_sram_pwr_on(ctl_addr, ack_sta);
		if (ret)
			PRINTF_E("%s: wait %s sram_ack timeout, ctl_addr 0x%x = 0x%x\n",
				"mtk_pd_on", pd->name, ctl_addr, hwccf_read(ctl_addr));
	}

	if (MTK_SCPD_CAPS(pd_ctrl, MTK_SCPD_SRAM_ISO)) {
		val = hwccf_read(ctl_addr) | PWR_SRAM_ISOINT_B_BIT;
		hwccf_write(ctl_addr, val);
		udelay(1);
		val &= ~PWR_SRAM_CLKISO_BIT;
		hwccf_write(ctl_addr, val);
	}

	return ret;
}

static int scpsys_sram_disable(struct mtk_pd *pd, uint32_t ctl_addr)
{
	const struct mtk_pd_ctrl *pd_ctrl = pdc_from_mpd(pd);
	struct mtk_pd_sram_ops *sram_ops = pd->sram_ops;
	uint32_t ack_sta;
	uint32_t val;
	int ret;

	PD_LOG("%s(0x%x)\n", __func__, ctl_addr);

	if (MTK_SCPD_CAPS(pd_ctrl, MTK_SCPD_SRAM_ISO)) {
		val = hwccf_read(ctl_addr) | PWR_SRAM_CLKISO_BIT;
		hwccf_write(ctl_addr, val);
		val &= ~PWR_SRAM_ISOINT_B_BIT;
		hwccf_write(ctl_addr, val);
		udelay(1);
	}

	if (sram_ops && sram_ops->sram_off) {
		ret = sram_ops->sram_off(pd);
	} else {
		if (MTK_SCPD_CAPS(pd_ctrl, MTK_SCPD_SRAM_SLP)) {
			ack_sta = SRAM_SLP_ACK_BIT;
			val = hwccf_read(ctl_addr) & ~SRAM_SLP_BIT;
		} else {
			ack_sta = SRAM_PDN_ACK_BIT;
			val = hwccf_read(ctl_addr) | SRAM_PDN_BIT;
		}
		hwccf_write(ctl_addr, val);

		/* wait until SRAM_PDN_ACK all 1 */
		ret = mtk_pd_wait_sram_pwr_down(ctl_addr, ack_sta);
		if (ret)
			PRINTF_E("%s: wait %s sram_ack timeout, ctl_addr 0x%x = 0x%x\n",
				"mtk_pd_off", pd->name, ctl_addr, hwccf_read(ctl_addr));
	}

	return ret;
}

static int mtk_pd_on(struct mtk_pd *pd)
{
	const struct mtk_pd_ctrl *pd_ctrl = pdc_from_mpd(pd);
	uint32_t ctl_addr = pd->base + pd_ctrl->ctl_offs;
	uint32_t val;
	int ret = 0;

	PD_LOG("%s: %s\n", __func__, pd->name?pd->name:"");

	val = hwccf_read(ctl_addr);
	val |= PWR_ON_BIT;
	hwccf_write(ctl_addr, val);
	val |= PWR_ON_2ND_BIT;
	hwccf_write(ctl_addr, val);

	/* wait until PWR_ACK = 1 */
	ret = mtk_pd_wait_pwr_on(pd);
	if (ret) {
		PRINTF_E("%s: wait %s pwr_status timeout\n", __func__, pd->name);
		goto err;
	}

	val &= ~PWR_CLK_DIS_BIT;
	hwccf_write(ctl_addr, val);

	val &= ~PWR_ISO_BIT;
	hwccf_write(ctl_addr, val);

	val |= PWR_RST_B_BIT;
	hwccf_write(ctl_addr, val);

#ifdef CLK_PVD_PDSCK
	ret = mtk_pd_clk_enable(PD_IDX(pd->id), pdsck_pvd_id);
	if (ret)
		goto err;
#endif

	/* wait until SRAM_PDN_ACK all 0 */
	ret = scpsys_sram_enable(pd, ctl_addr);
	if (ret)
		goto err;

	ret = mtk_pd_clear_bus_prot(pd_ctrl->bp_table, pd_ctrl->bp_step);
	if (ret) {
		PRINTF_E("%s: clear %s bus protection error, ret = %d\n", __func__, pd->name, ret);
		goto err;
	}

	return 0;

err:
	PRINTF_E("%s: Failed to power on mtcmos %s\n", __func__, pd->name);
	return ret;
}

static int mtk_pd_off(struct mtk_pd *pd)
{
	const struct mtk_pd_ctrl *pd_ctrl = pdc_from_mpd(pd);
	uint32_t ctl_addr = pd->base + pd_ctrl->ctl_offs;
	uint32_t val;
	int ret = 0;

	PD_LOG("%s: %s\n", __func__, pd->name?pd->name:"");

	ret = mtk_pd_set_bus_prot(pd_ctrl->bp_table, pd_ctrl->bp_step);
	if (ret) {
		PRINTF_E("%s: set %s bus protection error, ret = %d\n", __func__, pd->name, ret);
		goto err;
	}

	ret = scpsys_sram_disable(pd, ctl_addr);
	if (ret)
		goto err;

#ifdef CLK_PVD_PDSCK
	mtk_pd_clk_disable(PD_IDX(pd->id), pdsck_pvd_id);
#endif

	val = hwccf_read(ctl_addr);
	val |= PWR_ISO_BIT;
	hwccf_write(ctl_addr, val);

	val &= ~PWR_RST_B_BIT;
	hwccf_write(ctl_addr, val);

	val |= PWR_CLK_DIS_BIT;
	hwccf_write(ctl_addr, val);

	val &= ~PWR_ON_BIT;
	hwccf_write(ctl_addr, val);

	val &= ~PWR_ON_2ND_BIT;
	hwccf_write(ctl_addr, val);

	ret = mtk_pd_wait_pwr_off(pd);
	if (ret) {
		PRINTF_E("%s: wait %s pwr_status timeout\n", __func__, pd->name);
		goto err;
	}

	return 0;

err:
	PRINTF_E("%s: Failed to power off mtcmos %s\n", __func__, pd->name);
	return ret;
}

struct pd *mtk_pd_get(int id)
{
	struct mtk_pd_data *pd_data;
	uint32_t pvd_id, pd_id;

	PD_LOG("%s(0x%x)\n", __func__, id);

	pvd_id = PVD_PD_IDX(id);
	pd_id = PD_IDX(id);

	PD_LOG("mtk_pd_get: power domain ID (%d %d)\n", pvd_id, pd_id);

	if (pvd_id >= NR_PD_PROVIDER) {
		PRINTF_E("mtk_pd_get: power domain ID invalid!(0x%x)\n", id);
		return (struct pd *) -EINVAL;
	}

	pd_data = pd_providers[pvd_id].data;
	if (!pd_data) {
		PRINTF_E("pd_get: provider not ready\n");
		return (struct pd *) -EDEFER;
	}

	if (pd_id >= pd_data->npds) {
		PRINTF_E("pd_get: invalid index 0x%x\n", id);
		return (struct pd *) -EINVAL;
	}

	return pd_from_mpd(pd_data->mpds[pd_id]);
}

void mtk_pd_disable_unused(void)
{
	struct mtk_pd *pd;
	int i;

	PD_LOG("%s\n", __func__);

	for (i = NR_PD; i > 0; i--) {
		pd = mpd_from_pd(mtk_pd_get(i - 1));
		if (!pd)
			continue;

		if ((pd->enable_cnt == 0) && (mtk_pd_is_on(pd) == true)) {
			mtk_pd_on(pd);
			mtk_pd_off(pd);
		}
	}
}

bool is_err_or_null_pd(struct pd *pd)
{
	return ((int)pd <= 0);
}

struct mtk_pd *mtk_pd_from_name(const char *name)
{
	struct mtk_pd *pd;
	int i;

	for (i = 0; i < NR_PD; i++) {
		pd = mpd_from_pd(mtk_pd_get(i));
		if (!pd)
			break;

		if (strcmp(name, pd->name) == 0)
			return pd;
	}

	return NULL;
}

void mtk_pd_register_pwr_ops(struct mtk_pd *pd,
		struct mtk_pd_ops *ops)
{
	mtk_pd_lock();

	pd->ops->is_on = ops->is_on;
	pd->ops->power_on = ops->power_on;
	pd->ops->power_off = ops->power_off;

	mtk_pd_unlock();

	return;
}

void mtk_pd_register_sram_ops(struct mtk_pd *pd,
		struct mtk_pd_sram_ops *sram_ops)
{
	mtk_pd_lock();

	pd->sram_ops->sram_on = sram_ops->sram_on;
	pd->sram_ops->sram_off = sram_ops->sram_off;

	mtk_pd_unlock();

	return;
}

int mtk_pd_is_enabled(struct pd *pd)
{
	struct mtk_pd *mpd = NULL;

	PD_LOG("%s\n", __func__);

	if (!pd) {
		configASSERT(0);
		return -EINVAL;
	}

	mpd = mpd_from_pd(pd);
	if (!mpd) {
		configASSERT(0);
		return -EINVAL;
	}

	PD_LOG("%s: %s, enable_cnt=%d\n",
			__func__,
			mpd->name?mpd->name:"null",
			mpd->enable_cnt);

	if (mpd->enable_cnt > 0)
		return 1;

	return 0;
}

static int mtk_pd_enable_nolock(struct pd *pd)
{
	struct mtk_pd *mpd = NULL;
	int ret = 0;
	int i;

	PD_LOG("%s\n", __func__);

	if (!pd) {
		configASSERT(0);
		return -EINVAL;
	}

	mpd = mpd_from_pd(pd);
	if (!mpd) {
		configASSERT(0);
		return -EINVAL;
	}

	PD_LOG("%s: num_masters=%d\n", mpd->name?mpd->name:"", mpd->num_masters);
	for (i = 0; i < mpd->num_masters; i++) {
		struct pd *master_pd;

		master_pd = mtk_pd_get(mpd->masters[i]);
		ret = mtk_pd_enable_nolock(master_pd);
		if (ret)
			goto OUT;
	}

	mpd->enable_cnt++;
	PD_LOG("%s: enable_cnt: %d\n", mpd->name?mpd->name:"", mpd->enable_cnt);
	if (mpd->enable_cnt != 1)
		return 0;

#if defined(CLK_PDCK_NR_CLK) && (CLK_PDCK_NR_CLK > 0)
	ret = mtk_pd_clk_enable(PD_IDX(mpd->id), pdck_pvd_id);
	if (ret)
		goto OUT;
#endif

	if (mpd->hv_id != HW_VOTER_NONE) {
		struct pd *hv_pd;
		struct mtk_pd *hv_mpd;

		hv_pd = mtk_pd_get(mpd->hv_id);
		hv_mpd = mpd_from_pd(hv_pd);
		if (hv_mpd->ops && hv_mpd->ops->power_on) {
			ret = hv_mpd->ops->power_on(hv_mpd);
			if (ret)
				goto OUT;
		} else
			PRINTF_E("hv power_on ops is NULL\n");
	} else {
		if (mpd->ops && mpd->ops->power_on) {
			ret = mpd->ops->power_on(mpd);
			if (ret)
				goto OUT;
		} else
			PRINTF_E("power_on ops is NULL\n");
	}

	return ret;

OUT:
	PRINTF_E("%s: %s fail %d\n", __func__, mpd->name?mpd->name:"", ret);
	return ret;
}

int mtk_pd_enable(struct pd *pd)
{
	int ret = 0;

	PD_LOG("%s\n", __func__);

	mtk_pd_lock();
	ret = mtk_pd_enable_nolock(pd);
	mtk_pd_unlock();

	return ret;
}

int mtk_pd_disable_nolock(struct pd *pd);
int mtk_pd_disable_nolock(struct pd *pd)
{
	struct mtk_pd *mpd = NULL;
	int ret = 0;
	int i;

	PD_LOG("%s\n", __func__);

	if (!pd) {
		configASSERT(0);
		return -EINVAL;
	}

	mpd = mpd_from_pd(pd);
	if (!mpd) {
		configASSERT(0);
		return -EINVAL;
	}

	/* check PD is already ON before power down it */
	if ( !mtk_pd_is_enabled(pd) ) {
		PRINTF_E("ERR: %s already off!",
			mpd->name? mpd->name:"");
		ret = -EINVSTEP;
		goto OUT;
	}

	mpd->enable_cnt--;
	PD_LOG("%s: enable_cnt: %d\n", mpd->name, mpd->enable_cnt);

	if (mpd->enable_cnt == 0) {
		/* check whether all slaves are PD OFF by enable_cnt */
		PD_LOG("num_slaves = %d\n", mpd->num_slaves);
		for (i = 0; i < mpd->num_slaves; i++) {
			struct pd *slave_pd;

			slave_pd = mtk_pd_get(mpd->slaves[i]);
			if ( mtk_pd_is_enabled(slave_pd) ) {
				struct mtk_pd *slave_mpd = mpd_from_pd(slave_pd);

				PRINTF_E("ERR: %s not off\n",
						slave_mpd->name?slave_mpd->name:"");
				ret = -EINVSTEP;
				goto OUT;
			}
		}

		if (mpd->hv_id != HW_VOTER_NONE) {
			struct pd *hv_pd;
			struct mtk_pd *hv_mpd;

			hv_pd = mtk_pd_get(mpd->hv_id);
			hv_mpd = mpd_from_pd(hv_pd);
			if (hv_mpd->ops && hv_mpd->ops->power_off) {
				PD_LOG("%s HWCCF PD OFF\n", mpd->name?mpd->name:"");
				ret = hv_mpd->ops->power_off(hv_mpd);
				if (ret) {
					PD_LOG("%s power_off fail\n", mpd->name?mpd->name:"");
					goto OUT;
				}
			} else
				PRINTF_E("hv power_off ops is NULL\n");
		} else {
			if (mpd->ops && mpd->ops->power_off) {
				PD_LOG("%s SWCCF PD OFF\n", mpd->name?mpd->name:"");
				ret = mpd->ops->power_off(mpd);
				if (ret) {
					PD_LOG("%s power_off fail\n", mpd->name?mpd->name:"");
					goto OUT;
				}
			} else
				PRINTF_E("power_off ops is NULL\n");
		}

#if defined(CLK_PDCK_NR_CLK) && (CLK_PDCK_NR_CLK > 0)
		mtk_pd_clk_disable(PD_IDX(mpd->id), pdck_pvd_id);
#endif
	}

	PD_LOG("%s: num_masters=%d\n", mpd->name?mpd->name:"", mpd->num_masters);
	for (i = 0; i < mpd->num_masters; i++) {
		struct pd *master_pd;

		master_pd = mtk_pd_get(mpd->masters[i]);
		ret = mtk_pd_disable_nolock(master_pd);
		if (ret)
			goto OUT;
	}

	return 0;

OUT:
	PRINTF_E("%s: err %d\n", __func__, ret);
	configASSERT(0);
	return ret;
}

int mtk_pd_disable(struct pd *pd)
{
	int ret = 0;

	/* PD_LOG("%s\n", __func__); */

	mtk_pd_lock();
	ret = mtk_pd_disable_nolock(pd);
	mtk_pd_unlock();

	return ret;
}

struct mtk_pd_ops pd_ops = {
	.is_on = mtk_pd_is_on,
	.power_on = mtk_pd_on,
	.power_off = mtk_pd_off,
};

struct mtk_pd_data *mtk_alloc_pd_data(unsigned int pd_num)
{
	struct mtk_pd_data *pd_data;

	pd_data = pvPortMalloc(sizeof(*pd_data));
	if (!pd_data) {
		PRINTF_E("%s: pvPortMalloc fail, size=%u\n",
				__func__, (uint32_t)(sizeof(*pd_data)));
		configASSERT(0);
		return (struct mtk_pd_data *) -ENOMEM;
	} else {
		PD_LOG("%s: pvPortMalloc size=%u\n",
				__func__, (uint32_t)(sizeof(*pd_data)));
	}

	pd_data->mpds = pvPortMalloc(pd_num * sizeof(*pd_data->mpds));
	if (!pd_data->mpds) {
		PRINTF_E("%s: pvPortMalloc fail, size=%u\n",
				__func__, (uint32_t)(pd_num * sizeof(*pd_data->mpds)));
		configASSERT(0);
		goto err_out;
	} else {
		PD_LOG("%s: pvPortMalloc size=%u\n",
				__func__, (uint32_t)(pd_num * sizeof(*pd_data->mpds)));
	}

	pd_data ->npds = pd_num;

	return pd_data;

err_out:
	vPortFree(pd_data);
	return (struct mtk_pd_data *) -ENOMEM;
}

void mtk_pd_set_pd_ck_id(uint32_t id, uint32_t type)
{
	if (type == MAINCLK)
		pdck_pvd_id = id;
	else if (type == SUBCLK)
		pdsck_pvd_id = id;
	else
		return;
}

int mtk_pds_add_provider(int pvd_id, struct mtk_pd_data *pd_data, int num_pds,
		const struct mtk_pd_link  link[], int num_links, uint32_t base)
{
	struct mtk_pd_provider *mpp = &pd_providers[pvd_id];
	int i, j;
	int ret = 0;

	PD_LOG("%s\n", __func__);

	if (num_pds > NR_PD) {
		PRINTF_E("excede max pd number!\n");
		return -EINVAL;
	}

	if (!pd_data) {
		PRINTF_E("pd_data is NULL\n");
		return -EINVAL;
	}

	/* pd links create by PD SW set only */
	/* init each pd_data's link and ops */
	for (i = 0; i < num_pds; i++) {
		struct mtk_pd *pd;
		unsigned int m_cnt = 0;
		unsigned int s_cnt = 0;

		if (!pd_data->mpds[i] || !pd_data->mpds[i]->name)
			continue;

		pd = pd_data->mpds[i];

		for (j = 0; j < num_links; j++) {
			if (link[j].sub == i) {
				pd->masters[m_cnt] = link[j].origin;
				m_cnt++;
			}

			if (link[j].origin == i) {
				pd->slaves[s_cnt] = link[j].sub;
				s_cnt++;
			}
		}

		pd->num_masters = m_cnt;
		pd->num_slaves = s_cnt;
		pd->base = hwccf_remap(base);
	}

	mpp->data = pd_data;
	mpp->base = hwccf_remap(base);

	return ret;
}


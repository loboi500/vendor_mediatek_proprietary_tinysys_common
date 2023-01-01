/*
 * Copyright (c) 2020 MediaTek Inc.
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
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <limits.h>
#include <mt_printf.h>
#ifndef CFG_INTERNAL_TIMER_SUPPORT
#include <ostimer.h>
#endif
#include <ccf_common.h>
#include <vip_clk_provider.h>
#include <vip_clk.h>
#include <hwccf.h>

#define CCF_DEBUG  0

#if CCF_DEBUG
#define CCF_LOG(fmt, arg...) PRINTF_E(fmt, ##arg)
#else
#define CCF_LOG(...)
#endif

#define NR_VIP_PROVIDER	1

uint32_t hwccf_remap(uint32_t addr);
void hwccf_write(uint32_t addr, uint32_t value);
uint32_t hwccf_read(uint32_t addr);

static struct vip_clk_provider vip_clk_providers[NR_VIP_PROVIDER] = {{0}};
static struct vip_hwccf *vip_clks;
static uint32_t voter_base;

uint64_t profile_tick[7];
uint8_t pll_on_step, pll_off_step;

struct vclk *vip_clk_get(int id)
{
	struct vip_clk_data *clk_data;
	uint32_t pvd_id = PVD_IDX(id);
	uint32_t clk_id = CLK_IDX(id);

	CCF_LOG("vip_clk_get: pvd_id=%d, clk_id=%d\n", pvd_id, clk_id);

	if (pvd_id >= NR_VIP_PROVIDER) {
		CCF_LOG("clk_get: invalid clock pvd idx %u\n", pvd_id);
		return (struct vclk *) -EINVPVD;
	}

	clk_data = vip_clk_providers[pvd_id].data;
	if (!clk_data) {
		CCF_LOG("vip_clk_get: clock pvd %u not rdy\n", pvd_id);
		return (struct vclk *) -ENODATA;
	}

	if (clk_id >= clk_data->nclks) {
		CCF_LOG("vip_clk_get: invalid clock idx %d >= %d\n", clk_id, clk_data->nclks);
		return (struct vclk *) -EINVCKD;
	}

	return clk_from_vck(clk_data->vcks[clk_id]);
}

struct vip_clk_data *vip_alloc_clk_data(unsigned int clk_num)
{
	uint32_t i;
	struct vip_clk_data *clk_data;

	CCF_LOG("vip_alloc_clk_data\n");

	clk_data = pvPortMalloc(sizeof(*clk_data));
	if (!clk_data) {
		PRINTF_E("pvPortMalloc fail, size=%u\n",
				(uint32_t)(sizeof(*clk_data)));
		configASSERT(0);
		return (struct vip_clk_data *) -ENOMEM;
	} else {
		CCF_LOG("pvPortMalloc size=%u\n",
				(uint32_t)(sizeof(*clk_data)));
	}

	clk_data->vcks = pvPortMalloc(clk_num * sizeof(*clk_data->vcks));
	if (!clk_data->vcks) {
		PRINTF_E("pvPortMalloc fail, size=%u\n",
				(uint32_t)(clk_num * sizeof(*clk_data->vcks)));
		configASSERT(0);
		goto err_out;
	} else {
		CCF_LOG("pvPortMalloc size=%u\n",
				(uint32_t)(clk_num * sizeof(*clk_data->vcks)));
	}

	clk_data->nclks = clk_num;

	for (i = 0; i < clk_num; i++)
		clk_data->vcks[i] = NULL;

	return clk_data;

err_out:
	vPortFree(clk_data);
	return (struct vip_clk_data *) -ENOMEM;
}

int vip_clk_add_provider(int pvd_id, const char *name, struct vip_clk_data *clk_data, uint32_t base)
{
	struct vip_clk_provider *mcp = &vip_clk_providers[pvd_id];

	CCF_LOG("vip_clk_add_provider\n");

	if (pvd_id > NR_VIP_PROVIDER) {
		PRINTF_E("pvd_id(%d) > NR_VIP_PROVIDER(%d)\n",
				pvd_id, (int)NR_VIP_PROVIDER);
		configASSERT(0);
	}

	mcp->name = name;
	mcp->data = clk_data;
	if (base)
		mcp->base = hwccf_remap(base);

	return 0;
}

static int32_t vip_clk_get_base(struct vip_clk *vck)
{
	int pvd_id = PVD_IDX(vck->id);

	CCF_LOG("vip_clk_get_base\n");

	if (vck->id < 0 || pvd_id >= NR_VIP_PROVIDER)
		return -EINVAL;

	return vip_clk_providers[pvd_id].base;
}

bool vip_is_err_or_null_clk(struct vclk *clk)
{
	return ((int)clk <= 0);
}

/* PLL support */

#define CON0_BASE_EN		BIT(0)
#define CON0_PWR_ON		BIT(0)
#define CON0_ISO_EN		BIT(1)

int vip_pll_ctrl(struct vip_clk *vck, bool enable)
{
	struct vip_pll *pll = pll_from_hwccf_clk(vck);
	int isr_mode = is_in_isr();
	uint32_t pwr_addr;
	uint32_t en_addr;
	uint32_t tuner_en_addr;
	uint32_t rst_addr;
	int32_t base;
	uint32_t r;

	CCF_LOG("vip_pll_ctrl\n");

#ifdef CFG_TIMER_SUPPORT
	profile_tick[0] = ts_ostimer_read_isr(isr_mode);
#endif
	pll_on_step = 0;
	pll_off_step = 0;
	base = vip_clk_get_base(vck);
	if (base < 0)
		return base;

	pwr_addr = base + pll->pwr_ofs;
	en_addr = base + pll->en_ofs;
	tuner_en_addr = base + pll->tuner_en_ofs;
	rst_addr = base + pll->rst_bar_ofs;

	if (enable) {
#ifdef CFG_TIMER_SUPPORT
		profile_tick[1] = ts_ostimer_read_isr(isr_mode);
#endif
		pll_on_step = 1;
		r = hwccf_read(pwr_addr) | CON0_PWR_ON;
		hwccf_write(pwr_addr, r);

		udelay(1);

#ifdef CFG_TIMER_SUPPORT
		profile_tick[2] = ts_ostimer_read_isr(isr_mode);
#endif
		pll_on_step = 2;
		hwccf_write(pwr_addr, hwccf_read(pwr_addr) & ~CON0_ISO_EN);

		udelay(1);

#ifdef CFG_TIMER_SUPPORT
		profile_tick[3] = ts_ostimer_read_isr(isr_mode);
#endif
		pll_on_step = 3;
		hwccf_write(en_addr, hwccf_read(en_addr) | CON0_BASE_EN);

#ifdef CFG_TIMER_SUPPORT
		profile_tick[4] = ts_ostimer_read_isr(isr_mode);
#endif
		pll_on_step = 4;
		if (pll->tuner_en_ofs)
			hwccf_write(tuner_en_addr, hwccf_read(tuner_en_addr) | BIT(pll->tuner_en_bit));

		udelay(20);

#ifdef CFG_TIMER_SUPPORT
		profile_tick[5] = ts_ostimer_read_isr(isr_mode);
#endif
		pll_on_step = 5;
		if (pll->rst_bar_ofs)
			hwccf_write(rst_addr, hwccf_read(rst_addr) | pll->rst_bar_mask);

#ifdef CFG_TIMER_SUPPORT
		profile_tick[6] = ts_ostimer_read_isr(isr_mode);
#endif
		pll_on_step = 6;
	} else {
		if (pll->rst_bar_ofs) {
			r = hwccf_read(rst_addr);
			r &= ~pll->rst_bar_mask;
			hwccf_write(rst_addr, r);
		}

#ifdef CFG_TIMER_SUPPORT
		profile_tick[1] = ts_ostimer_read_isr(isr_mode);
#endif
		pll_off_step = 1;

		if (pll->tuner_en_ofs) {
			r = hwccf_read(tuner_en_addr);
			r &= ~BIT(pll->tuner_en_bit);
			hwccf_write(tuner_en_addr, r);
		}

#ifdef CFG_TIMER_SUPPORT
		profile_tick[2] = ts_ostimer_read_isr(isr_mode);
#endif
		pll_off_step = 2;
		r = hwccf_read(en_addr);
		r &= ~CON0_BASE_EN;
		hwccf_write(en_addr, r);

#ifdef CFG_TIMER_SUPPORT
		profile_tick[3] = ts_ostimer_read_isr(isr_mode);
#endif
		pll_off_step = 3;
		r = hwccf_read(pwr_addr) | CON0_ISO_EN;
		hwccf_write(pwr_addr, r);

#ifdef CFG_TIMER_SUPPORT
		profile_tick[4] = ts_ostimer_read_isr(isr_mode);
#endif
		pll_off_step = 4;
		r = hwccf_read(pwr_addr) & ~CON0_PWR_ON;
		hwccf_write(pwr_addr, r);

#ifdef CFG_TIMER_SUPPORT
		profile_tick[5] = ts_ostimer_read_isr(isr_mode);
#endif
		pll_off_step = 5;
	}

	return r;
}

int vip_pll_enable(struct vip_clk *vck)
{
	CCF_LOG("vip_pll_enable\n");
	return vip_pll_ctrl(vck, true);
}

int vip_pll_disable(struct vip_clk *vck)
{
	CCF_LOG("vip_pll_disable\n");
	return vip_pll_ctrl(vck, false);
}

int vip_pll_is_enabled(struct vip_clk *vck)
{
	struct vip_pll *pll = pll_from_hwccf_clk(vck);
	uint32_t en_addr;
	int32_t base;


	CCF_LOG("vip_pll_is_enabled\n");

	base = vip_clk_get_base(vck);
	if (base < 0)
		return base;

	en_addr = base + pll->en_ofs;

	return (hwccf_read(en_addr) & CON0_BASE_EN) != 0;
}

int vip_clk_ctrl(struct vclk *clk, bool enable)
{
	struct vip_clk *vck;
	int r = 0;

	if (!clk) {
		PRINTF_E("vip_clk_ctrl: clk NULL\n");
		return -EINVAL;
	}

	vck = vck_from_clk(clk);
	CCF_LOG("%s: %s\n",
			vck->name?vck->name:"", enable?"enable":"disable");

	if (enable && vck->ops->enable)
		r = vck->ops->enable(vck);
	else if (!enable && vck->ops->disable)
		r = vck->ops->disable(vck);

	return r;
}

int vip_clk_enable(struct vclk *clk)
{
	CCF_LOG("vip_clk_enable\n");
	return vip_clk_ctrl(clk, true);
}

int vip_clk_disable(struct vclk *clk)
{
	CCF_LOG("vip_clk_disable\n");
	return vip_clk_ctrl(clk, false);
}

const struct vip_clk_ops vip_pll_ops = {
	.enable		= vip_pll_enable,
	.disable	= vip_pll_disable,
	.is_enabled	= vip_pll_is_enabled,
};

int vip_clk_subsys_ctrl(uint8_t id, bool flag, bool enable)
{
	struct voter_map *vm = get_voter_map(vip_clks[id].id);
	uint32_t addr;
	uint32_t done_addr = 0;
	uint32_t cg_addr = 0;
	uint32_t val, val2;
	uint8_t i = 0;

	CCF_LOG("vip_clk_subsys_ctrl(%d, %d, %d)\n", id, flag, enable);

	if (flag) {
		if (enable)
			addr = hwccf_remap(vm->map_base) + vm->set_addr;
		else
			addr = hwccf_remap(vm->map_base) + vm->clr_addr;
	} else {
		done_addr = hwccf_remap(CCF_VIP_CG_DONE(vip_clks[id].id));
		if (enable) {
			addr = hwccf_remap(SSPM_VOTER_SET(vip_clks[id].id));
			cg_addr = hwccf_remap(vm->map_base + vm->sta_addr);
		} else
			addr = hwccf_remap(SSPM_VOTER_CLR(vip_clks[id].id));
	}

	hwccf_write(addr, vip_clks[id].msk);
	udelay(3);
	do {
		if (done_addr)
			val = hwccf_read(done_addr);
		if (cg_addr)
			val2 = hwccf_read(cg_addr);
		if (i > 200)
			return -ETIMEDOUT;
		else if ((!done_addr || (val & vip_clks[id].msk) != vip_clks[id].msk)
				&& (!cg_addr || (val2 & vip_clks[id].msk) != vip_clks[id].msk))
			break;
		else
			udelay(5);
		i++;
	} while(1);

	return 0;
}

int vip_clk_subsys_init(const struct vip_hwccf clks[], uint32_t base, uint8_t num)
{
	uint8_t i;

	CCF_LOG("vip_clk_subsys_init\n");

	vip_clks = pvPortMalloc(num * sizeof(struct vip_hwccf));
	if (!vip_clks) {
		PRINTF_E("pvPortMalloc fail, size=%u\n",
				(uint32_t)(num * sizeof(struct vip_hwccf)));
		configASSERT(0);
		return -ENOMEM;
	} else {
		CCF_LOG("pvPortMalloc size=%u\n",
				(uint32_t)(num * sizeof(struct vip_hwccf)));
	}

	for (i = 0; i < num; i++)
		vip_clks[i] = clks[i];

	voter_base = hwccf_remap(base);

	return 0;
}

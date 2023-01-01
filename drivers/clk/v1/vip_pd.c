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
#include <tinysys_reg.h>
#include <mt_printf.h>
#ifndef CFG_INTERNAL_TIMER_SUPPORT
#include <ostimer.h>
#endif
#include <ccf_common.h>
//#include <mtk_pd.h>
#include <vip_pd_provider.h>
#include <vip_clk.h>
#include <vip_pd.h>

#define PD_DEBUG  0

#if PD_DEBUG
#define PD_LOG(fmt, arg...) PRINTF_E(fmt, ##arg)
#else
#define PD_LOG(...)
#endif

#if PD_DEBUG
#define SEMA_TIMEOUT_MS 100
#else
#define SEMA_TIMEOUT_MS 10
#endif

#define ON_CTRL				(true)
#define OFF_CTRL				(false)
#define MAX_PD_STEPS			6
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

#define VIP_SCPD_CAPS(_scpd, _x)	((_scpd)->data->caps & (_x))
#define BIT(nr)				(1 << (nr))

#define OSTIMER_TICK_TO_NS     (76) /* 13MHz */

static SemaphoreHandle_t xSemaphore_pd;
static bool mutex_initialized = false;
static uint32_t scpsys_base;
static uint32_t voter_base;
static bool init_flag = false;
uint8_t pd_step_start;
uint8_t pd_step_done;

uint32_t hwccf_remap(uint32_t addr);
void hwccf_write(uint32_t addr, uint32_t value);
uint32_t hwccf_read(uint32_t addr);

struct vip_pd_cb *vip_pd_cb_head;
const struct vip_sram_table *vip_sram_table_p;

static int registered_num_pds;

static void vip_pd_lock(void)
{
	if (!mutex_initialized) {
		xSemaphore_pd = xSemaphoreCreateMutex();
		mutex_initialized = true;
	}

	if (xSemaphoreTake(xSemaphore_pd, pdMS_TO_TICKS(SEMA_TIMEOUT_MS)) != pdTRUE) {
		PRINTF_E("xSemaphoreTake timeout\n");
		configASSERT(0);
	}
}

static void vip_pd_unlock(void)
{
	if (xSemaphoreGive(xSemaphore_pd) != pdTRUE) {
		PRINTF_E("xSemaphoreGive(xSemaphore_clk) fail\n");
		configASSERT(0);
	}
}

#define vip_pd_wait_pwr_ctrl(vpd, onoff) \
	vip_pd_wait_for_state(vpd, 0, 0, onoff, vip_pd_is_onoff_pwr_state)
#define vip_pd_wait_bus_prot_ctrl(reg, mask, onoff) \
	vip_pd_wait_for_state(NULL, reg, mask, onoff, vip_pd_is_setclr_state)
#define vip_pd_wait_sram_pwr_ctrl(reg, mask, onoff) \
	vip_pd_wait_for_state(NULL, reg, mask, onoff, vip_pd_is_setclr_state)

struct vip_pd *vip_pds;

static void timeout_error_handle(const char *name, bool on,
		uint32_t ctl_addr, const char *type)
{
	PRINTF_E("%s %s %s timeout, addr 0x%x(0x%x)\n",
		name?name:"", on? "on": "off", type, ctl_addr, hwccf_read(ctl_addr));
}

static struct vip_pd *get_vip_pds(void)
{
	return vip_pds;
}

void set_init_flag(bool flag)
{
	init_flag = flag;
}

int vip_pd_is_on(struct vip_pd *pd)
{
	const struct vip_pd_data *pd_data = pd->data;
	uint32_t pwr_sta_addr = scpsys_base + pd_data->ctl_offs;
	uint32_t status = hwccf_read(pwr_sta_addr) & PWR_STATUS_BIT;
	uint32_t status2 = hwccf_read(pwr_sta_addr) & PWR_STATUS_2ND_BIT;

	//PD_LOG("vip_pd_is_on\n");

	/*
	 * A domain is on when both status bits are set. If only one is set
	 * return an error. This happens while powering up a domain
	 */

	if (status && status2) {
		PD_LOG("%s is ON\n", pd_data->name?pd_data->name:"");
		return true;
	}

	if (!status && !status2) {
		PD_LOG("%s is OFF\n", pd_data->name?pd_data->name:"");
		return false;
	}

	PD_LOG("vip_pd_is_on: %s ERR\n", pd_data->name?pd_data->name:"");

	return -EINVAL;
}

static inline bool vip_pd_is_setclr_state(struct vip_pd *pd, uint32_t reg, uint32_t mask, bool set)
{
	PD_LOG("vip_pd_is_setclr_state\n");

	if (set)
		return ((hwccf_read(reg) & mask) == mask);
	else
		return (!(hwccf_read(reg) & mask));
}

static inline bool vip_pd_is_onoff_pwr_state(struct vip_pd *pd, uint32_t reg, uint32_t mask, bool on)
{
	//PD_LOG("vip_pd_is_onoff_pwr_state\n");

	if (on)
		return vip_pd_is_on(pd);
	else
		return (!vip_pd_is_on(pd));
}

static int vip_pd_wait_for_state(struct vip_pd *vpd, uint32_t reg, uint32_t mask, bool onoff,
		bool (*fp)(struct vip_pd *, uint32_t, uint32_t, bool))
{
	int time = 0;

	PD_LOG("vip_pd_wait_for_state\n");

	do {
		if (fp(vpd, reg, mask, onoff))
			return 0;

		udelay(10);
		time++;
		if (time > 100)
			return -ETIMEDOUT;
	} while (1);
}

static int vip_pd_bus_prot_ctrl(const char *name, const struct vip_bus_prot bp_table, bool set)
{
	uint32_t ctrl_ofs, mask, reg_en, reg_sta;
	int ret = 0;

	PD_LOG("vip_pd_bus_prot_ctrl\n");

	if (!bp_table.mask)
		return 0;

	if (set)
		ctrl_ofs = hwccf_remap(bp_table.reg->base + bp_table.reg->set_ofs);
	else
		ctrl_ofs = hwccf_remap(bp_table.reg->base + bp_table.reg->clr_ofs);

	mask = bp_table.mask;
	reg_sta = hwccf_remap(bp_table.reg->base + bp_table.reg->sta_ofs);
	hwccf_write(ctrl_ofs, mask);

	if (set) {
		ret = vip_pd_wait_bus_prot_ctrl(reg_sta, mask, set);
		if (ret) {
			reg_en = hwccf_remap(bp_table.reg->base + bp_table.reg->en_ofs);
			timeout_error_handle(name, set, reg_en, "bus_prot");
			PRINTF_E("sta: %x\n", hwccf_read(reg_sta));
		}
	}

	PD_LOG("%s sta 0x%x(0x%x)\n", set? "set" : "clr",
				reg_sta, hwccf_read(reg_sta));
	return ret;
}

static int vip_pd_setclr_bus_prot(const char *name, const struct vip_bus_prot *bp_table, bool set)
{
	int step_idx;
	int end_step = VIP_MAX_STEPS - 1;
	int ret;
	int i;

	PD_LOG("vip_pd_setclr_bus_prot\n");

	for (i = 0; i < VIP_MAX_STEPS; i++) {
		if (set)
			step_idx = i;
		else
			step_idx = end_step - i;

		ret = vip_pd_bus_prot_ctrl(name, bp_table[step_idx], set);
		if (ret) {
			vip_pd_bus_prot_ctrl(name, bp_table[step_idx], !set);
			return ret;
		}
	}

	return 0;
}

int vip_pd_clk_ctrl(const struct vip_pd_data *pd, bool enable)
{
	int ret = 0;
	int i;

	PD_LOG("vip_pd_clk_ctrl(%s, %d)\n", pd->name?pd->name:"", enable);

	for (i = 0; i < VIP_MAX_VOTER_CLKS; i++) {
		if (pd->subsys_clks_id[i] != CLK_NULL) {
			ret = vip_clk_subsys_ctrl(pd->subsys_clks_id[i], init_flag, enable);
			if (ret)
				return ret;
		}
	}

	return ret;
}

static void vip_pd_sram_iso_ctrl(struct vip_pd *pd, uint32_t ctl_addr, bool pdn_enable)
{
	uint32_t val;

	PD_LOG("vip_pd_sram_iso_ctrl(%s, 0x%x, %d)\n",
			pd->data->name?pd->data->name:"",
			ctl_addr, pdn_enable);

	if (VIP_SCPD_CAPS(pd, VIP_SCPD_SRAM_ISO)) {
		if (pdn_enable) {
			val = hwccf_read(ctl_addr) | PWR_SRAM_CLKISO_BIT;
			hwccf_write(ctl_addr, val);
			val &= ~PWR_SRAM_ISOINT_B_BIT;
			hwccf_write(ctl_addr, val);
			udelay(1);
		} else {
			val = hwccf_read(ctl_addr) | PWR_SRAM_ISOINT_B_BIT;
			hwccf_write(ctl_addr, val);
			udelay(1);
			val &= ~PWR_SRAM_CLKISO_BIT;
			hwccf_write(ctl_addr, val);
		}
	}
}

static int vip_pd_sram_pdn_ctrl(struct vip_pd *pd, uint32_t ctl_addr,
		uint32_t ctrl_bits, bool pdn_enable, uint8_t wait_ack)
{
	struct vip_pd_sram_ops *sram_ops = pd->sram_ops;
	uint32_t first_ctrl_bits = 0;
	uint32_t val;
	int ret = 0;

	PD_LOG("vip_pd_sram_pdn_ctrl(%s, 0x%x, %d)\n",
		pd->data->name?pd->data->name:"",
		ctl_addr, pdn_enable);

	if (!ctrl_bits) {
		if (VIP_SCPD_CAPS(pd, VIP_SCPD_SRAM_SLP)) {
			ctrl_bits = SRAM_SLP_BIT;
			first_ctrl_bits = SRAM_PDN_BIT;
		} else
			ctrl_bits = SRAM_PDN_BIT;
	}

	if (pdn_enable && sram_ops && sram_ops->sram_off) {
		ret = sram_ops->sram_off(pd);
	} else if (!pdn_enable && sram_ops && sram_ops->sram_on) {
		ret = sram_ops->sram_on(pd);
	} else {
		/* Either wait until SRAM_PDN_ACK all 1 or 0 */
		uint32_t ack_sta;
		int ret;

		/* sram iso enable */
		if (pdn_enable)
			vip_pd_sram_iso_ctrl(pd, ctl_addr, pdn_enable);

		if (VIP_SCPD_CAPS(pd, VIP_SCPD_SRAM_SLP)) {
			ack_sta = SRAM_SLP_ACK_BIT;
			if (pdn_enable)
				val = hwccf_read(ctl_addr) & ~ctrl_bits;
			else if (first_ctrl_bits && (hwccf_read(ctl_addr) & first_ctrl_bits != 0))
				val = hwccf_read(ctl_addr) & ~first_ctrl_bits;
			else
				val = hwccf_read(ctl_addr) | ctrl_bits;
		} else {
			ack_sta = SRAM_PDN_ACK_BIT;
			if (pdn_enable)
				val = hwccf_read(ctl_addr) | ctrl_bits;
			else
				val = hwccf_read(ctl_addr) & ~ctrl_bits;
		}
		hwccf_write(ctl_addr, val);

		if (wait_ack) {
			if (VIP_SCPD_CAPS(pd, VIP_SCPD_SRAM_SLP))
				ret = vip_pd_wait_sram_pwr_ctrl(ctl_addr, ack_sta, !pdn_enable);
			else
				ret = vip_pd_wait_sram_pwr_ctrl(ctl_addr, ack_sta, pdn_enable);
			if (ret)
				timeout_error_handle(pd->data->name, pdn_enable, ctl_addr, "sram_pdn");
		}

		if (!pdn_enable)
			vip_pd_sram_iso_ctrl(pd, ctl_addr, pdn_enable);
	}

	return ret;
}

static int vip_pd_sram_table_ctrl(struct vip_pd *pd, bool pdn_enable)
{
	int ret = 0;
	int i;

	for (i = 0; i < VIP_MAX_SRAM_STEPS; i++) {
		if (vip_sram_table_p[i].ofs) {
			uint32_t ctl_addr = scpsys_base + vip_sram_table_p[i].ofs;
			uint8_t wait_ack = vip_sram_table_p[i].wait_ack;

			if (VIP_SCPD_CAPS(pd, VIP_SCPD_SRAM_SLP))
				ret = vip_pd_sram_pdn_ctrl(pd, ctl_addr,
					vip_sram_table_p[i].slp_msk, pdn_enable, wait_ack);
			else
				ret = vip_pd_sram_pdn_ctrl(pd, ctl_addr,
					vip_sram_table_p[i].pdn_msk, pdn_enable, wait_ack);
			if (ret)
				return ret;
		}
	}

	return ret;
}

static void vip_pd_power_misc_ctrl(uint32_t ctl_addr, bool on)
{
	uint32_t val;

	PD_LOG("vip_pd_power_misc_ctrl(0x%x, %d)\n", ctl_addr, on);

	val = hwccf_read(ctl_addr);
	if (on) {
		val &= ~PWR_CLK_DIS_BIT;
		hwccf_write(ctl_addr, val);
		val &= ~PWR_ISO_BIT;
		hwccf_write(ctl_addr, val);
		val |= PWR_RST_B_BIT;
		hwccf_write(ctl_addr, val);
	} else {
		val |= PWR_ISO_BIT;
		hwccf_write(ctl_addr, val);
		val &= ~PWR_RST_B_BIT;
		hwccf_write(ctl_addr, val);
		val |= PWR_CLK_DIS_BIT;
		hwccf_write(ctl_addr, val);
	}
}

static int vip_pd_power_ctrl(struct vip_pd *pd, uint32_t ctl_addr,
		const char *name, bool on)
{
	uint32_t val;
	int ret = 0;
	const struct vip_pd_data *pd_data = pd->data;

	PD_LOG("vip_pd_power_ctrl(%s, 0x%x, %d)\n",
		pd_data->name?pd_data->name:"", ctl_addr, on);

	val = hwccf_read(ctl_addr);
	if (on) {
		val |= PWR_ON_BIT;
		hwccf_write(ctl_addr, val);
		val |= PWR_ON_2ND_BIT;
		hwccf_write(ctl_addr, val);
	} else {
		val &= ~PWR_ON_BIT;
		hwccf_write(ctl_addr, val);
		val &= ~PWR_ON_2ND_BIT;
		hwccf_write(ctl_addr, val);
	}

	/* wait until PWR_ACK = 1 */
	ret = vip_pd_wait_pwr_ctrl(pd, on);
	if (ret)
		timeout_error_handle(pd->data->name, on, ctl_addr, "pwr_sta");

	return ret;
}

static int vip_pd_onoff_ctrl(struct vip_pd *pd, bool on)
{
	const struct vip_pd_data *pd_data = pd->data;
	uint32_t ctl_addr = scpsys_base + pd_data->ctl_offs;
	int step_idx;
	int end_step = MAX_PD_STEPS - 1;
	int ret = 0;
	int i;

	//PD_LOG("vip_pd_onoff_ctr\n");

	PD_LOG("vip_pd_onoff_ctrl(%s, %d)\n",
			pd_data->name?pd_data->name:"", on);

	for (i = 0; i < MAX_PD_STEPS; i++) {
		if (on)
			step_idx = i;
		else
			step_idx = end_step - i;

		pd_step_start = step_idx;

		if (step_idx == 0) {
			ret = vip_pd_power_ctrl(pd, ctl_addr, pd_data->name, on);
			if (ret)
				goto err;
		} else if (step_idx == 1) {
			vip_pd_power_misc_ctrl(ctl_addr, on);
		} else if (step_idx == 2) {
			if (!VIP_SCPD_CAPS(pd, VIP_SCPD_BYPASS_CLK)) {
				ret = vip_pd_clk_ctrl(pd_data, on);
				if (ret)
					goto err;
			}
		} else if (step_idx == 3) {
			/* wait until SRAM_PDN_ACK all 0 */
			if (VIP_SCPD_CAPS(pd, VIP_SCPD_L2TCM_SRAM)) {
				ret = vip_pd_sram_table_ctrl(pd, !on);
				if (ret)
					goto err;
			}
			ret = vip_pd_sram_pdn_ctrl(pd, ctl_addr, 0, !on, true);
			if (ret)
				goto err;
		} else if (step_idx == 4) {
			ret = vip_pd_setclr_bus_prot(pd_data->name, pd_data->bp_table, !on);
			if (ret)
				goto err;
		} else if (step_idx == 5) {
			if (VIP_SCPD_CAPS(pd, VIP_SCPD_LP_CLK))
				vip_pd_clk_ctrl(pd_data, !on);
		} else {
			ret = -EINVSTEP;
			goto err;
		}
		pd_step_done = step_idx;
	}

err:
	return ret;
}

static int vip_pd_on(struct vip_pd *pd)
{
	//PD_LOG("vip_pd_on\n");

	return vip_pd_onoff_ctrl(pd, ON_CTRL);
}

static int vip_pd_off(struct vip_pd *pd)
{
	//PD_LOG("vip_pd_off\n");

	return vip_pd_onoff_ctrl(pd, OFF_CTRL);
}

struct vip_pd_ops vip_pd_ops = {
	.is_on = vip_pd_is_on,
	.power_on = vip_pd_on,
	.power_off = vip_pd_off,
};

static int vip_alloc_pd_data(uint32_t pd_num)
{
	//PD_LOG("vip_alloc_pd_data(%d)\n", pd_num);

	vip_pds = pvPortMalloc(pd_num* sizeof(*vip_pds));
	if (!vip_pds) {
		PRINTF_E("pvPortMalloc fail, size=%u\n",
				(uint32_t)(pd_num* sizeof(*vip_pds)));
		configASSERT(0);
		return -ENOMEM;
	} else {
		PD_LOG("pvPortMalloc size=%u\n",
				(uint32_t)(pd_num* sizeof(*vip_pds)));
	}

	return 0;
}

static struct clk **__vip_alloc_clk_data(uint32_t clk_num)
{
	struct clk **clks;

	//PD_LOG("__vip_alloc_clk_data(%d)\n", clk_num);

	clks = pvPortMalloc(clk_num * sizeof(*clks));
	if (!clks) {
		PRINTF_E("pvPortMalloc fail, size=%u\n",
				(uint32_t)(clk_num * sizeof(*clks)));
		configASSERT(0);
		return (struct clk **) -ENOMEM;
	} else {
		PD_LOG("pvPortMalloc size=%u\n",
				(uint32_t)(clk_num * sizeof(*clks)));
	}

	return clks;
}

int vip_pds_register(const struct vip_pd_data *pds, int num_pds, uint32_t base)
{
	int i, j;

	PD_LOG("vip_pds_register(%s, %d, 0x%x\n",
			pds->name?pds->name:"", num_pds, base);

	if (num_pds >= NR_VPD) {
		PRINTF_E("excede max pd number!\n");
		return -EINVAL;
	}

	registered_num_pds = num_pds;
	vip_alloc_pd_data(num_pds);

	if (base)
		scpsys_base = hwccf_remap(base);

	for (i = 0; i < num_pds; i++) {
		struct vip_pd *vpd = &vip_pds[i];

		vpd->data = &pds[i];
		vpd->ops = &vip_pd_ops;
	}

	return 0;
}

struct vpd *vip_pd_get(int id)
{
	PD_LOG("vip_pd_get(%d)\n", id);

	if (id >= registered_num_pds || id < 0) {
		return (struct vpd *) -EINVPD;
	}

	if (!vip_pds[id].data) {
		return (struct vpd *) -EPDNORDY;
	}

	return pd_from_vpd(&vip_pds[id]);
}

bool vip_is_err_or_null_pd(struct vpd *pd)
{
	return ((int)pd <= 0);
}

struct vip_pd *vip_pd_from_name(const char *name)
{
	int i;
	struct vip_pd *pd;

	PD_LOG("vip_pd_from_name(%s)\n", name?name:"null");

	for (i = 0; i < NR_VPD; i++) {
		pd = &vip_pds[i];

		if (!pd->data)
			break;

		if (strcmp(name, pd->data->name) == 0)
			return pd;
	}

	return NULL;
}

void vip_pd_register_pwr_ops(struct vip_pd *pd,
		struct vip_pd_ops *ops)
{
	PD_LOG("vip_pd_register_pwr_ops\n");

	vip_pd_lock();

	pd->ops->is_on = ops->is_on;
	pd->ops->power_on = ops->power_on;
	pd->ops->power_off = ops->power_off;

	vip_pd_unlock();

	return;
}

void vip_pd_register_sram_ops(struct vip_pd *pd,
		struct vip_pd_sram_ops *sram_ops)
{
	PD_LOG("vip_pd_register_sram_ops\n");

	vip_pd_lock();

	pd->sram_ops->sram_on = sram_ops->sram_on;
	pd->sram_ops->sram_off = sram_ops->sram_off;

	vip_pd_unlock();

	return;
}

void vip_pd_register_sram_table_data(const struct vip_sram_table *st_data)
{
	PD_LOG("vip_pd_register_sram_table_ops\n");

	vip_pd_lock();

	vip_sram_table_p = st_data;

	vip_pd_unlock();

	return;
}

int vip_pd_enable(struct vpd *pd)
{
	struct vip_pd *vpd = vpd_from_pd(pd);
	int ret = -1;

	PD_LOG("vip_pd_enable: %s\n",
		vpd->data->name?vpd->data->name:"");

	if (vip_pd_is_on(vpd))
		return 0;

	if (!vpd->ops)
		PRINTF_E("vpd->ops NULL\n");

	if (!vpd->ops->power_on)
		PRINTF_E("vpd->ops->power_on NULL\n");

	if (vpd->ops && vpd->ops->power_on) {
		ret = vpd->ops->power_on(vpd);
		if (ret)
			goto OUT;
		else
			return 0;
	}

OUT:
	return ret;
}

int vip_pd_disable(struct vpd *pd)
{
	struct vip_pd *vpd = vpd_from_pd(pd);
	int ret;

	PD_LOG("vip_pd_disable: %s\n",
		vpd->data->name?vpd->data->name:"");

	if (!vip_pd_is_on(vpd))
		return 0;

	if (vpd->ops && vpd->ops->power_off) {
		ret = vpd->ops->power_off(vpd);
		if (ret)
			goto OUT;
		else
			return 0;
	}

OUT:
	return ret;
}

int vip_pd_is_enabled(struct vpd *pd)
{
	struct vip_pd *vpd = vpd_from_pd(pd);

	PD_LOG("vip_pd_is_enabled: %s\n",
		vpd->data->name?vpd->data->name:"");

	return vip_pd_is_on(vpd);
}

/* MTCMOS backup/restore callback register API */
int vip_pd_cb_register(struct vip_pd_cb *node)
{
	struct vip_pd_cb *node_list = NULL;

	PD_LOG("vip_pd_cb_register\n");

	if (!node) {
		PD_LOG("node is NULL\n");
		return -EINVAL;
	}

	if (node->id < 0 || node->id > NR_VPD) {
		PD_LOG("invalid ID %d\n", node->id);
		return -EINVAL;
	}

	if (!node->name) {
		PD_LOG("callback name NULL\n");
		return -EINVAL;
	}

	PD_LOG("callback register:%s, %d\n",
			node->name, node->id);

	if (!node->pd_backup && !node->pd_restore) {
		PD_LOG("callback api NULL\n");
		return -EINVAL;
	}

	taskENTER_CRITICAL();

	/* add node to link list */
	if (!vip_pd_cb_head) {
		node->next = NULL;
		node->prev = NULL;
		vip_pd_cb_head = node;
	} else {
		node_list = vip_pd_cb_head;

		/* search if same node already exist */
		while (strcmp(node_list->name, node->name) && node_list->next)
			node_list = node_list->next;

		if (strcmp(node_list->name, node->name)) {
			/* add node to link list */
			node->next = NULL;
			node->prev = node_list;
			node_list->next = node;
		} else {
			PRINTF_E("ERR: %s exist\n",node->name);
			return -EINVAL;
		}
	}

	taskEXIT_CRITICAL();

	return 0;
}

/* MTCMOS backup/restore callback unregister API */
int vip_pd_cb_unregister(struct vip_pd_cb *node)
{
	struct vip_pd_cb *node_list = NULL;

	PD_LOG("vip_pd_cb_unregister\n");

	if (!node) {
		PD_LOG("node is NULL\n");
		return -EINVAL;
	}

	if (!node->name) {
		PD_LOG("callback name NULL\n");
		return -EINVAL;
	}

	PD_LOG("callback unregister: %s\n", node->name);

	taskENTER_CRITICAL();

	/* remove node from link list */
	if (vip_pd_cb_head) {
		node_list = vip_pd_cb_head;

		/* search node from link list */
		while ((strcmp(node_list->name, node->name) != 0) &&
			   (node_list->next != NULL))
			node_list = node_list->next;

		if (!strcmp(node_list->name, node->name)) {
			struct vip_pd_cb *prev_node = NULL;
			struct vip_pd_cb *next_node = NULL;

			/* remove node from link list */
			if (!node_list->prev)
				vip_pd_cb_head = node_list->next;
			else {
				prev_node = node_list->prev;
				next_node = node_list->next;
				prev_node->next = next_node;
				next_node->prev = prev_node;
			}
		} else {
			PD_LOG("not find %s\n", node->name);
			return -ENODEV;
		}
	} else {
		PD_LOG("not find %s\n", node->name);
		return -ENODEV;
	}

	taskEXIT_CRITICAL();

	return 0;
}

static uint32_t get_tick_diff(uint64_t start_tick, uint64_t stop_tick)
{
	PD_LOG("get_tick_diff\n");

	if (start_tick > stop_tick)
		return (uint32_t)((UINT64_MAX - start_tick) + stop_tick);
	else
		return (uint32_t)(stop_tick - start_tick);
}

/* MTCMOS backup API */
void vip_pd_backup(int id)
{
	struct vip_pd_cb *node = vip_pd_cb_head;
	int isr_mode = is_in_isr();
#ifdef CFG_TIMER_SUPPORT
	uint64_t start_tick, cur_tick;
	uint32_t spend_time_ns;
#endif

	PD_LOG("vip_pd_backup(%d)\n", id);

	if (0 == isr_mode)
        taskENTER_CRITICAL();

	while (node) {
		if (id == node->id && node->pd_backup) {
#ifdef CFG_TIMER_SUPPORT
    		start_tick = ts_ostimer_read_isr(isr_mode);
#endif

			PD_LOG("%s: backup\n", node->name);
			node->pd_backup();

#ifdef CFG_TIMER_SUPPORT
			cur_tick = ts_ostimer_read_isr(isr_mode);
    		spend_time_ns = get_tick_diff(start_tick, cur_tick) * OSTIMER_TICK_TO_NS;
    		PD_LOG("%s: backup time = %uns\n", node->name, spend_time_ns);

			if (node->backup_timeout_ns &&
				spend_time_ns > node->backup_timeout_ns) {
				PRINTF_E("%s: backup timeout %uns>%uns\n",
						node->name, spend_time_ns, node->backup_timeout_ns);
				configASSERT(0);
			}
#endif
		}

		node = node->next;
	}

	if (0 == isr_mode)
        taskEXIT_CRITICAL();
}

/* MTCMOS restore API */
void vip_pd_restore(int id)
{
	struct vip_pd_cb *node = vip_pd_cb_head;
	int isr_mode = is_in_isr();
#ifdef CFG_TIMER_SUPPORT
	uint64_t start_tick, cur_tick;
	uint32_t spend_time_ns;
#endif

	PD_LOG("vip_pd_restore(%d)\n", id);

	if (0 == isr_mode)
        taskENTER_CRITICAL();

	while (node) {
		if (id == node->id && node->pd_restore) {
#ifdef CFG_TIMER_SUPPORT
    		start_tick = ts_ostimer_read_isr(isr_mode);
#endif

			PD_LOG("%s: restore\n", node->name);
			node->pd_restore();

#ifdef CFG_TIMER_SUPPORT
			cur_tick = ts_ostimer_read_isr(isr_mode);
    		spend_time_ns = get_tick_diff(start_tick, cur_tick) * OSTIMER_TICK_TO_NS;
    		PD_LOG("%s: restore time %uns\n", node->name, spend_time_ns);

			if (node->restore_timeout_ns &&
				spend_time_ns > node->restore_timeout_ns) {
				PRINTF_E("%s: restore timeout %uns>%uns\n",
						node->name, spend_time_ns, node->restore_timeout_ns);
				configASSERT(0);
			}
#endif
		}

		node = node->next;
	}

	if (0 == isr_mode)
        taskEXIT_CRITICAL();
}


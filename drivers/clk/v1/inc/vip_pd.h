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

#ifndef _VIP_PD_H_
#define _VIP_PD_H_

#include <stdint.h>
#include <sys/types.h>
#include <vip_pd_provider.h>

#define CLK_NULL			0xff

struct vpd;
struct vip_pd;

struct vip_pd_ops {
	int (*is_on)(struct vip_pd *pd);
	int (*power_on)(struct vip_pd *pd);
	int (*power_off)(struct vip_pd *pd);
} __attribute__((packed));

struct vip_pd_sram_ops {
	int (*sram_on)(struct vip_pd *pd);
	int (*sram_off)(struct vip_pd *pd);
	int (*sram_table_ops)(struct vip_pd *pd, bool pdn_en);
} __attribute__((packed));

struct vip_pd_data {
	struct vip_bus_prot bp_table[VIP_MAX_STEPS];	/* bus protect table */
	const char *name;
	uint16_t sta_offs;
	uint16_t sta2_offs;
	uint16_t ctl_offs;
	uint8_t sta_bit;
	uint8_t subsys_clks_id[VIP_MAX_VOTER_CLKS];
	uint8_t caps;
};

struct vip_pd {
	struct vpd pd;
	const struct vip_pd_data *data;
	struct vip_pd_ops *ops;
	struct vip_pd_sram_ops *sram_ops;
};

struct vip_pd_cb {
	const char *name;
	int id;
	void (*pd_backup)(void);
	void (*pd_restore)(void);
	uint32_t backup_timeout_ns;
	uint32_t restore_timeout_ns;
	struct vip_pd_cb *next;
	struct vip_pd_cb *prev;
};

#define vpd_from_pd(p)	container_of(p, struct vip_pd, pd)
#define pd_from_vpd(v)	((v) ? &(v)->pd : NULL)

/**
 * get_mtk_pd_ops - get operands for pd handling
 * Returns operands for pd.
 */
bool vip_is_err_or_null_pd(struct vpd *pd);
void set_init_flag(bool flag);
int vip_pd_clk_ctrl(const struct vip_pd_data *pd, bool enable);
struct vpd *vip_pd_get(int id);
int vip_pd_enable(struct vpd *pd);
int vip_pd_disable(struct vpd *pd);
int vip_pd_is_enabled(struct vpd *pd);
int vip_pds_register(const struct vip_pd_data *pd, int num_pds, uint32_t base);
void vip_pd_init(void);

/* PD backup/restore callback */
int vip_pd_cb_register(struct vip_pd_cb *node);
int vip_pd_cb_unregister(struct vip_pd_cb *node);
void vip_pd_backup(int id);
void vip_pd_restore(int id);

#endif /* _VIP_H_ */

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

#ifndef _VIP_CLK_PROVIDER_H_
#define _VIP_CLK_PROVIDER_H_

#include <stdint.h>
#include <sys/types.h>

#define PVD_ID_MASK			GENMASK(14, 9)
#define PVD_ID_SHIFT			9
#define PVD_IDX(id)			(((id) & PVD_ID_MASK) >> PVD_ID_SHIFT) /* remove clk_id */
#define CLK_ID_MASK			GENMASK(8, 0)
#define CLK_ID_SHIFT			0
#define CLK_ID(pvd, idx)		(((pvd) << PVD_ID_SHIFT) | (idx)) /* add pvd_id */
#define CLK_IDX(id)			(((id) & CLK_ID_MASK) >> CLK_ID_SHIFT) /* remove pvd_id */

#define INIT_VIP_CLK_DATA(ckdt, dvcks) \
	do { \
		uint32_t i; \
		for (i = 0; i < ARRAY_SIZE(dvcks); i++) \
			ckdt->vcks[CLK_IDX(dvcks[i].vck.id)] = &dvcks[i].vck; \
	} while (0)

struct vclk {
	/* nothing here */
};

struct vip_clk;

struct vip_clk_ops {
	int		(*enable)(struct vip_clk *vck);
	int		(*disable)(struct vip_clk *vck);
	int		(*is_enabled)(struct vip_clk *vck);
} __attribute__((packed));

struct vip_clk {
	struct vclk clk;
	int id;
	const char *name;
	uint32_t flags;
	const struct vip_clk_ops *ops;
} __attribute__((packed));

#define vck_from_clk(c)	container_of(c, struct vip_clk, clk)
#define clk_from_vck(v)	((v) ? &(v)->clk : NULL)

struct vip_clk_data {
	struct vip_clk **vcks;
	uint16_t nclks;
} __attribute__((packed));

struct vip_clk_provider {
	const char *name;
	struct vip_clk_data *data;
	uint32_t base;
} __attribute__((packed));

struct vip_clk_data *vip_alloc_clk_data(unsigned int clk_num);
int vip_clk_add_provider(int pvd_id, const char *name, struct vip_clk_data *clk_data, uint32_t base);

#endif /* _VIP_CLK_PROVIDER_H_ */

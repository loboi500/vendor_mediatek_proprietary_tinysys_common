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

#ifndef _VIP_CLK_H_
#define _VIP_CLK_H_

#include <stdint.h>
#include <stdbool.h>

#include <vip_clk_provider.h>

struct vip_pll {
	struct vip_clk			vck;
	uint32_t			pll_base;
	uint32_t			rst_bar_mask;
	uint16_t			en_ofs;
	uint16_t			pwr_ofs;
	uint16_t			tuner_ofs;
	uint16_t			tuner_en_ofs;
	uint16_t			rst_bar_ofs;
	uint8_t				tuner_en_bit;
	/*uint8_t			flags;*/
} __attribute__((packed));

#define pll_from_hwccf_clk(h)	container_of(h, struct vip_pll, vck)
#define hwccf_clk_from_pll(p)	((p) ? &(p)->vck : NULL)

extern const struct vip_clk_ops vip_pll_ops;

#define PLL_VIP(_id, _name, _en_ofs, _pwr_ofs,		\
			_flags, _rst_bar_ofs,		\
			_rst_bar_mask, _tuner_ofs,		\
			_tuner_en_ofs, _tuner_en_bit) {		\
		.vck.id = _id,						\
		.vck.name = _name,					\
		.vck.ops = &vip_pll_ops,				\
		.en_ofs = _en_ofs,					\
		.pwr_ofs = _pwr_ofs,					\
		.vck.flags = (_flags | PLL_CFLAGS),				\
		.rst_bar_ofs = _rst_bar_ofs,				\
		.rst_bar_mask = _rst_bar_mask,				\
		.tuner_ofs = _tuner_ofs,				\
		.tuner_en_ofs = _tuner_en_ofs,			\
		.tuner_en_bit = _tuner_en_bit,				\
	}

struct vip_hwccf {
	uint32_t msk;
	uint8_t id;
};

#define HWCCF_VIP(_id, _msk)	{	\
		.id = _id,		\
		.msk = _msk,		\
	}

struct vclk *vip_clk_get(int id);
int vip_clk_enable(struct vclk *clk);
int vip_clk_disable(struct vclk *clk);
int vip_clk_subsys_ctrl(uint8_t id, bool flag, bool enable);
bool vip_is_err_or_null_clk(struct vclk *clk);
void vip_clk_init(void);
int vip_clk_subsys_init(const struct vip_hwccf clks[], uint32_t base, uint8_t num);
#endif /* _VIP_CLK_H_ */

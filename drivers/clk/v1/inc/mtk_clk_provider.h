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

#ifndef _MTK_CLK_PROVIDER_H_
#define _MTK_CLK_PROVIDER_H_

#include <stdint.h>
#include <sys/types.h>

#define MAX_MUX_IDX			128
#define NR_PROVIDER			32
#define MAINCLK				0
#define SUBCLK				1

#define PVD_ID_MASK			GENMASK(14, 9)
#define PVD_ID_SHIFT			9
#define PVD_IDX(id)			(((id) & PVD_ID_MASK) >> PVD_ID_SHIFT) /* remove clk_id */
#define CLK_ID_MASK			GENMASK(8, 0)
#define CLK_ID_SHIFT			0
#define CLK_ID(pvd, idx)		(((pvd) << PVD_ID_SHIFT) | (idx)) /* add pvd_id */
#define CLK_IDX(id)			(((id) & CLK_ID_MASK) >> CLK_ID_SHIFT) /* remove pvd_id */

#define INIT_CLK_DATA(ckdt, dmcks) \
	do { \
		uint32_t i; \
		for (i = 0; i < ARRAY_SIZE(dmcks); i++) \
			ckdt->mcks[CLK_IDX(dmcks[i].mck.id)] = &dmcks[i].mck; \
	} while (0)

struct mtk_clk;

struct mtk_clk_ops {
	int		(*prepare)(struct mtk_clk *mck);
	int		(*unprepare)(struct mtk_clk *mck);
	int		(*is_prepared)(struct mtk_clk *mck);
	int		(*enable)(struct mtk_clk *mck);
	int		(*disable)(struct mtk_clk *mck);
	int		(*is_enabled)(struct mtk_clk *mck);
	unsigned long	(*recalc_rate)(struct mtk_clk *mck, unsigned long parent_rate);
	long		(*round_rate)(struct mtk_clk *mck, unsigned long rate, unsigned long *parent_rate);
	int		(*set_parent)(struct mtk_clk *mck, uint8_t index);
	uint8_t		(*get_parent)(struct mtk_clk *mck);
	int		(*set_rate)(struct mtk_clk *mck, unsigned long rate, unsigned long parent_rate);
} __attribute__((packed));

/* definition for mck.flags */
#define CLK_SET_RATE_PARENT	BIT(0)	/* propagate rate change up one level */
#define MTK_CLK_MUX_INDEX_BIT BIT(1)
#define HAVE_RST_BAR	BIT(2)
#define PLL_AO			BIT(3)

struct clk {
	/* nothing here */
};

struct mtk_clk {
	struct clk clk;
	int id;
	int hc_id;
	const char *name;
	uint32_t flags;

	union {
		struct mtk_clk *parent;
		struct mtk_clk **parents;
	} p;

	union {
		const char *parent_name;
		const char * const *parent_names;
	} pn;

	const struct mtk_clk_ops *ops;
	int8_t prepare_cnt;
	int8_t enable_cnt;
	uint8_t num_parents;
} __attribute__((packed));

#define mck_from_clk(c)	container_of(c, struct mtk_clk, clk)
#define clk_from_mck(m)	((m) ? &(m)->clk : NULL)

struct mtk_clk_data {
	struct mtk_clk **mcks;
	uint16_t nclks;
} __attribute__((packed));

struct mtk_clk_provider {
	const char *name;
	struct mtk_clk_data *data;
	uint32_t base;
} __attribute__((packed));

struct mtk_clk_data *mtk_alloc_clk_data(unsigned int clk_num);
struct clk *mtk_clk_register_clk(struct mtk_clk *mck);
int mtk_clk_add_provider(int pvd_id, const char *name, struct mtk_clk_data *clk_data, uint32_t base);

struct mtk_clk_fixed_rate {
	struct mtk_clk		mck;
	unsigned long		rate;
} __attribute__((packed));

#define fixed_from_mtk_clk(m)	container_of(m, struct mtk_clk_fixed_rate, mck)
#define mtk_clk_from_fixed(f)	((f) ? &(f)->mck : NULL)

extern const struct mtk_clk_ops mtk_clk_fixed_rate_ops;

#define FIXED_CLK(_id, _name, _parent, _rate) {		\
	.mck.id = _id,					\
	.mck.name = _name,				\
	.mck.pn.parent_name = _parent,			\
	.mck.num_parents =  (_parent != NULL) ? 1: 0,		\
	.mck.ops = &mtk_clk_fixed_rate_ops,		\
	.mck.enable_cnt = 0,		\
	.rate = _rate,					\
}

struct mtk_clk_fixed_factor {
	struct mtk_clk		mck;
	uint8_t		mult;
	uint16_t	div;
} __attribute__((packed));

#define factor_from_mtk_clk(m)	container_of(m, struct mtk_clk_fixed_factor, mck)
#define mtk_clk_from_factor(f)	((f) ? &(f)->mck : NULL)

extern const struct mtk_clk_ops mtk_clk_fixed_factor_ops;

#define FACTOR(_id, _name, _parent, _mult, _div) {	\
	.mck.id = _id,					\
	.mck.name = _name,				\
	.mck.pn.parent_name = _parent,			\
	.mck.num_parents = 1,	   \
	.mck.ops = &mtk_clk_fixed_factor_ops,		\
	.mck.enable_cnt = 0,		\
	.mult = _mult,					\
	.div = _div,					\
}

struct mtk_clk_gate_regs {
	uint16_t		sta_ofs;
	uint16_t		clr_ofs;
	uint16_t		set_ofs;
};

struct mtk_clk_gate {
	struct mtk_clk			mck;
	const struct mtk_clk_gate_regs	*regs;
	uint8_t				shift;
} __attribute__((packed));

#define gate_from_mtk_clk(m)	container_of(m, struct mtk_clk_gate, mck)
#define mtk_clk_from_gate(g)	((g) ? &(g)->mck : NULL)

extern const struct mtk_clk_ops mtk_clk_gate_ops;
extern const struct mtk_clk_ops mtk_clk_gate_inv_ops;
extern const struct mtk_clk_ops mtk_clk_gate_setclr_ops;
extern const struct mtk_clk_ops mtk_clk_gate_setclr_inv_ops;

#define GATE(_id, _name, _parent, _shift, _regs, _ops) {	\
	.mck.id = _id,						\
	.mck.name = _name,					\
	.mck.pn.parent_name = _parent,				\
	.mck.num_parents =  1,			\
	.mck.ops = &_ops,   		\
	.mck.enable_cnt = 0,		\
	.regs = &_regs,		\
	.shift = _shift,		 \
}

struct mtk_clk_mux {
	struct mtk_clk mck;
	uint16_t mux_ofs;
	uint16_t mux_set_ofs;
	uint16_t mux_clr_ofs;
	uint16_t gate_set_ofs;
	uint16_t gate_clr_ofs;
	int8_t upd_ofs;

	uint8_t mux_shift;
	uint8_t mux_width;
	int8_t gate_shift;
	int8_t upd_shift;

	/*uint8_t flags;*/
} __attribute__((packed));

#define mux_from_mtk_clk(m)	container_of(m, struct mtk_clk_mux, mck)
#define mtk_clk_from_mux(x)	((x) ? &(x)->mck : NULL)

extern const struct mtk_clk_ops mtk_clk_mux_ops;
extern const struct mtk_clk_ops mtk_clk_mux_clr_set_upd_ops;

#define MUX(_id, _name, _parents, _ofs, _shift, _width) {	\
	.mck.id = _id,						\
	.mck.name = _name,					\
	.mck.pn.parent_names = _parents,			\
	.mck.num_parents = ARRAY_SIZE(_parents),		\
	.mck.ops = &mtk_clk_mux_ops,				\
	.mck.enable_cnt = 0,		\
	.mux_ofs = _ofs,					\
	.mux_shift = _shift,					\
	.mux_width = _width,					\
}

#define MUX_FLAGS(_id, _name, _parents, _ofs, _shift, _width, _mux_flags) {	\
	.mck.id = _id,						\
	.mck.name = _name,					\
	.mck.pn.parent_names = _parents,			\
	.mck.num_parents = ARRAY_SIZE(_parents),		\
	.mck.ops = &mtk_clk_mux_ops,				\
	.mck.enable_cnt = 0,		\
	.mux_ofs = _ofs,					\
	.mux_shift = _shift,					\
	.mux_width = _width,					\
	.mck.flags = _mux_flags,			\
}

#define CLR_SET_UPD_FLAGS(_id, _hc_id, _name, _parents, _mux_ofs, _mux_set_ofs,\
			_mux_clr_ofs, _shift, _width, _gate,		\
			_upd_ofs, _upd, _mux_flags) {			\
	.mck.id = _id,						\
	.mck.hc_id = _hc_id,						\
	.mck.name = _name,					\
	.mck.pn.parent_names = _parents,			\
	.mck.num_parents = ARRAY_SIZE(_parents),		\
	.mck.ops = &mtk_clk_mux_clr_set_upd_ops,				\
	.mck.enable_cnt = 0,		\
	.mux_ofs = _mux_ofs,					\
	.mux_set_ofs = _mux_set_ofs,				\
	.mux_clr_ofs = _mux_clr_ofs,				\
	.upd_ofs = _upd_ofs,					\
	.mux_shift = _shift,					\
	.mux_width = _width,					\
	.gate_shift = _gate,					\
	.upd_shift = _upd,					\
	.mck.flags = _mux_flags,			\
}

#define MUX_CLR_SET_UPD(_id, _hc_id, _name, _parents, _mux_ofs, _mux_set_ofs,\
			_mux_clr_ofs, _shift, _width, _gate,		\
			_upd_ofs, _upd)				\
		CLR_SET_UPD_FLAGS(_id, _hc_id, _name, _parents,	\
			_mux_ofs, _mux_set_ofs,			\
			_mux_clr_ofs, _shift, _width, _gate,		\
			_upd_ofs, _upd, 0)

#define MUX_CLR_SET_UPD_AO(_id, _hc_id, _name, _parents, _mux_ofs, _mux_set_ofs,\
			_mux_clr_ofs, _shift, _width,		\
			_upd_ofs, _upd)				\
	.mck.id = _id,						\
	.mck.hc_id = _hc_id,						\
	.mck.name = _name,					\
	.mck.pn.parent_names = _parents,			\
	.mck.num_parents = ARRAY_SIZE(_parents),		\
	.mck.ops = &mtk_clk_mux_ops,				\
	.mck.enable_cnt = 0,		\
	.mux_ofs = _mux_ofs,					\
	.mux_set_ofs = _mux_set_ofs,				\
	.mux_clr_ofs = _mux_clr_ofs,				\
	.upd_ofs = _upd_ofs,					\
	.mux_shift = _shift,					\
	.mux_width = _width,					\
	.upd_shift = _upd,					\
}

struct mtk_pll_div_table {
	uint32_t div;
	unsigned long freq;
} __attribute__((packed));

struct mtk_clk_pll {
	struct mtk_clk			mck;
	unsigned long			fmax;
	unsigned long			fmin;
	uint32_t			rst_bar_mask;
	uint16_t			en_ofs;
	uint16_t			pwr_ofs;
	uint16_t			pcw_ofs;
	uint16_t				pcw_chg_ofs;
	uint16_t			pd_ofs;
	uint16_t			rst_bar_ofs;
	uint16_t			tuner_ofs;
	uint16_t			tuner_en_ofs;
	uint8_t				tuner_en_bit;
	int8_t			en_bit;
	int8_t			pwron_bit;
	int8_t			iso_bit;
	uint8_t				pcw_shift;
	uint8_t				pd_shift;
	uint8_t				pcwbits;
	uint8_t				pcwibits;
	/*uint8_t			flags;*/
} __attribute__((packed));

#define PLL(_id, _hc_id, _name, _en_ofs, _en_bit, _pwr_ofs,		\
			_iso_bit, _pwron_bit, _flags, _rst_bar_ofs,	\
			_rst_bar_mask, _pd_ofs, _pd_shift, _tuner_ofs,	\
			_tuner_en_ofs, _tuner_en_bit, _pcw_ofs,		\
			_pcw_shift, _pcwbits, _pcwibits) {		\
		.mck.id = _id,						\
		.mck.hc_id = _hc_id,					\
		.mck.name = _name,					\
		.mck.pn.parent_name = "clk26m",			\
		.mck.num_parents = 1,					\
		.mck.ops = &mtk_clk_pll_ops,				\
		.en_ofs = _en_ofs,					\
		.en_bit = _en_bit,					\
		.pwr_ofs = _pwr_ofs,					\
		.iso_bit = _iso_bit,					\
		.pwron_bit = _pwron_bit,				\
		.mck.flags = (_flags | PLL_CFLAGS),				\
		.rst_bar_ofs = _rst_bar_ofs,				\
		.rst_bar_mask = _rst_bar_mask,				\
		.fmax = PLL_FREQ_MAX,					\
		.fmin = PLL_FREQ_MIN,					\
		.pd_ofs = _pd_ofs,					\
		.pd_shift = _pd_shift,					\
		.tuner_ofs = _tuner_ofs,				\
		.tuner_en_ofs = _tuner_en_ofs,				\
		.tuner_en_bit = _tuner_en_bit,				\
		.pcw_ofs = _pcw_ofs,					\
		.pcw_shift = _pcw_shift,				\
		.pcwbits = _pcwbits,					\
		.pcwibits = _pcwibits,					\
	}

#define pll_from_mtk_clk(m)	container_of(m, struct mtk_clk_pll, mck)
#define mtk_clk_from_pll(p)	((p) ? &(p)->mck : NULL)

extern const struct mtk_clk_ops mtk_clk_pll_ops;

#if MT_CCF_PLL_DISABLE
#define PLL_CFLAGS		PLL_AO
#else
#define PLL_CFLAGS		(0)
#endif

struct mtk_clk_div {
	struct mtk_clk		mck;
	uint16_t		div_ofs;
	uint8_t			div_shift;
	uint8_t			div_width;
} __attribute__((packed));

#define div_from_mtk_clk(m)	container_of(m, struct mtk_clk_div, mck)
#define mtk_clk_from_div(d)	((d) ? &(d)->mck : NULL)

extern const struct mtk_clk_ops mtk_clk_div_ops;

#define DIV(_id, _name, _parent, _ofs, _shift, _width) {	\
	.mck.id = _id,						\
	.mck.name = _name,					\
	.mck.pn.parent_name = _parent,				\
	.mck.num_parents = 1,			\
	.mck.ops = &mtk_clk_div_ops,				\
	.mck.enable_cnt = 0,		\
	.div_ofs = _ofs,					\
	.div_shift = _shift,					\
	.div_width = _width,					\
}

struct mtk_clk_pdck {
	struct mtk_clk	mck;
	uint32_t	*clks;
	uint8_t 	id;
	uint16_t	clk_num;
} __attribute__((packed));

#define pdck_from_mtk_clk(m)	container_of(m, struct mtk_clk_pdck, mck)
#define mtk_clk_from_pdck(p)	((p) ? &(p)->mck : NULL)

extern const struct mtk_clk_ops mtk_clk_pdck_ops;

#define PDCK(_id, _name, _clks) {		\
	.mck.id = _id,					\
	.mck.name = _name,				\
	.mck.num_parents =  0,				\
	.mck.ops = &mtk_clk_pdck_ops,			\
	.mck.enable_cnt = 0,		\
	.clks = &_clks[0],				\
	.clk_num = (_clks)? ARRAY_SIZE(_clks) : 0,		\
}

struct clk *clk_from_name(const char *name);
void mtk_clk_disable_unused(void);
struct mtk_clk_provider *get_mtk_clk_pvds(void);
void clk_init(void);

#endif /* _MTK_CLK_PROVIDER_H_ */

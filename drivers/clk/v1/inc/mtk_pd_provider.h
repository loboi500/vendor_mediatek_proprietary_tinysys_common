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

#ifndef _MTK_PD_PROVIDER_H_
#define _MTK_PD_PROVIDER_H_

#include <stdint.h>
#include <sys/types.h>

#define MAX_STEPS			5
#define MAX_WAY_STEPS			2
#define MAX_CLKS			3
#define MAX_SUBSYS_CLKS			5
#define MAX_LINK			10

#define POWER_ON			1
#define POWER_OFF		0

#define MTK_SCPD_ACTIVE_WAKEUP		BIT(0)
#define MTK_SCPD_FWAIT_SRAM		BIT(1)
#define MTK_SCPD_SRAM_ISO		BIT(2)
#define MTK_SCPD_MD_OPS			BIT(3)
#define MTK_SCPD_ALWAYS_ON		BIT(4)
#define MTK_SCPD_SRAM_SLP		BIT(6)

#define NR_PD_PROVIDER			2
#define NR_PD	64

#define PVD_PD_ID_MASK			GENMASK(14, 9)
#define PVD_PD_ID_SHIFT			9
#define PVD_PD_IDX(id)			(((id) & PVD_PD_ID_MASK) >> PVD_PD_ID_SHIFT) /* remove clk_id */
#define PD_ID_MASK			GENMASK(8, 0)
#define PD_ID_SHIFT			0
#define PD_ID(pvd, idx)		(((pvd) << PVD_PD_ID_SHIFT) | (idx)) /* add pvd_id */
#define PD_IDX(id)			(((id) & PD_ID_MASK) >> PD_ID_SHIFT) /* remove pvd_id */

#define INIT_PD_DATA(pddt, dmpds) \
	do { \
		uint32_t i; \
		for (i = 0; i < ARRAY_SIZE(dmpds); i++) \
			pddt->mpds[PD_IDX(dmpds[i].mpd.id)] = &dmpds[i].mpd; \
	} while (0)

#define BUS_PROT_REG(_base, _set_ofs,  _clr_ofs,			\
			_en_ofs, _sta_ofs) {	\
		.base = _base,			\
		.en_ofs = _en_ofs,		\
		.set_ofs = _set_ofs,		\
		.clr_ofs = _clr_ofs,		\
		.sta_ofs = _sta_ofs,		\
	}

#define BUS_PROT(_reg, _mask) {	\
		.reg = &_reg,			\
		.mask = _mask,			\
	}

#define BUS_WAY_REG(_base, _en_ofs) {	\
		.base = _base,			\
		.en_ofs = _en_ofs,		\
	}

#define BUS_WAY(_reg, _mask, _stat) {	\
		.reg = &_reg,			\
		.mask = _mask,			\
		.state = _stat,			\
	}

struct bus_prot_reg {
	uint32_t base;
	uint16_t en_ofs;
	uint16_t set_ofs;
	uint16_t clr_ofs;
	uint16_t sta_ofs;
};

struct bus_prot {
	const struct bus_prot_reg *reg;
	uint32_t mask;
};

struct bus_way_reg {
	uint32_t base;
	uint16_t en_ofs;
};

struct bus_way {
	const struct bus_way_reg *reg;
	uint32_t mask;
	uint8_t state;
};

struct pd {
	/* nothing here */
};

struct mtk_pd;

struct mtk_pd_ops {
	int (*is_on)(struct mtk_pd *pd);
	int (*power_on)(struct mtk_pd *pd);
	int (*power_off)(struct mtk_pd *pd);
} __attribute__((packed));

struct mtk_pd_sram_ops {
	int (*sram_on)(struct mtk_pd *pd);
	int (*sram_off)(struct mtk_pd *pd);
} __attribute__((packed));

#define pdc_from_mpd(m)	container_of(m, struct mtk_pd_ctrl, mpd)
#define mpd_from_pdc(x)	((x) ? &(x)->mpd : NULL)

struct mtk_pd {
	struct pd pd;
	uint32_t base;
	int id;
	int hv_id;
	const char *name;
	int enable_cnt;
	struct mtk_pd_ops *ops;
	struct mtk_pd_sram_ops *sram_ops;
	uint8_t masters[MAX_LINK];
	uint8_t slaves[MAX_LINK];
	uint8_t num_masters;
	uint8_t num_slaves;
	uint32_t flags;
} __attribute__((packed));

#define mpd_from_pd(p)	container_of(p, struct mtk_pd, pd)
#define pd_from_mpd(m)	((m) ? &(m)->pd : NULL)

struct mtk_pd_link {
	int origin;
	int sub;
};

struct mtk_pd_ctrl {
	struct mtk_pd mpd;
	const struct bus_prot *bp_table;	/* bus protect table */

	const char *name;
	uint32_t hw_voter_id;

	uint16_t ctl_offs;

	uint8_t caps;
	uint8_t bp_step;
} __attribute__((packed));

struct mtk_pd_data {
	struct mtk_pd **mpds;
	uint16_t npds;
} __attribute__((packed));

struct mtk_pd_provider {
	struct mtk_pd_data *data;
	uint32_t base;
} __attribute__((packed));

extern struct mtk_pd_ops pd_ops;

#define PD_CTL(_id, _name, _ctl_offs, _bp_table) {	\
		.mpd.id = _id,	\
		.mpd.name = _name,		\
		.mpd.hv_id = HW_VOTER_NONE,	\
		.mpd.ops = &pd_ops,	\
		.ctl_offs = _ctl_offs,		\
		.bp_table = &_bp_table[0],		\
		.bp_step = ARRAY_SIZE(_bp_table),		\
	}

#define PD_NO_BUS(_id, _name, _ctl_offs) {	\
		.mpd.id = _id,	\
		.mpd.name = _name,		\
		.mpd.hv_id = HW_VOTER_NONE,	\
		.mpd.ops = &pd_ops,	\
		.ctl_offs = _ctl_offs,		\
		.bp_table = NULL,		\
		.bp_step = 0,		\
	}

#define PD_VOTER(_id, _name, _hv_id) {	\
		.mpd.id = _id,	\
		.mpd.name = _name,		\
		.mpd.hv_id = _hv_id,	\
	}

struct mtk_pd_data *mtk_alloc_pd_data(unsigned int pd_num);

/**
 * get_mtk_pd_ops - get operands for pd handling
 * Returns operands for pd.
 */
const struct mtk_pd_ops *get_mtk_pd_ops(void);
struct mtk_pd *mtk_pd_from_name(const char *name);
void mtk_pd_set_pd_ck_id(uint32_t id, uint32_t type);
int mtk_pds_add_provider(int pvd_id, struct mtk_pd_data *pd_data, int num_pds,
		const struct mtk_pd_link  link[], int num_links, uint32_t base);
void pd_init(void);

#endif /* _MTK_PD_PROVIDER_H_ */

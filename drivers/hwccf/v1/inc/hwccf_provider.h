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

#ifndef _HWCCF_PROVIDER_H_
#define _HWCCF_PROVIDER_H_

#include <stdint.h>
#include <sys/types.h>
#include <mtk_clk_provider.h>
#include <mtk_pd_provider.h>

#define HW_VOTER_NONE		(0xFFFF)

struct hwccf_clk_voter {
	struct mtk_clk		mck;
	int16_t			sta_ofs;
	int16_t			clr_ofs;
	int16_t			set_ofs;
	uint8_t			shift;
	/*uint8_t			flags;*/
} __attribute__((packed));

#define voter_from_hwccf_clk(h)	container_of(h, struct hwccf_clk_voter, mck)
#define hwccf_clk_from_voter(v)	((v) ? &(v)->mck : NULL)

struct hwccf_pd_voter {
	struct mtk_pd		mpd;
	uint32_t		base;
	uint32_t		hw_voter_id;
	int16_t			sta_ofs;
	int16_t			clr_ofs;
	int16_t			set_ofs;
	uint8_t			shift;
	/*uint8_t			flags;*/
} __attribute__((packed));

#define voter_from_hwccf_pd(h)	container_of(h, struct hwccf_pd_voter, mpd)
#define hwccf_pd_from_voter(v)	((v) ? &(v)->mpd : NULL)

extern const struct mtk_clk_ops hwccf_voter_clk_ops;
extern const struct mtk_clk_ops hwccf_voter_pll_ops;
extern struct mtk_pd_ops hwccf_voter_pd_ops;

#define VOTER_CLK(_id, _name, _set_ofs, _clr_ofs, _sta_ofs, _shift, _flags) {	\
	.mck.id = _id,						\
	.mck.hc_id = INV_ID,					\
	.mck.name = _name,					\
	.mck.ops = &hwccf_voter_clk_ops,   		\
	.mck.enable_cnt = 0,		\
	.set_ofs = _set_ofs,		\
	.clr_ofs = _clr_ofs,		\
	.sta_ofs = _sta_ofs,		\
	.shift = _shift,		 \
	.mck.flags = _flags,		\
}

#define VOTER_PLL(_id, _name, _set_ofs, _clr_ofs, _sta_ofs, _shift, _flags) {	\
	.mck.id = _id,						\
	.mck.hc_id = INV_ID,					\
	.mck.name = _name,					\
	.mck.ops = &hwccf_voter_pll_ops,   		\
	.mck.prepare_cnt = 0,		\
	.set_ofs = _set_ofs,		\
	.clr_ofs = _clr_ofs,		\
	.sta_ofs = _sta_ofs,		\
	.shift = _shift,		 \
	.mck.flags = _flags,		\
}

#define VOTER_PD(_id, _name, _set_ofs, _clr_ofs, _sta_ofs, _shift, _flags) {	\
	.mpd.id = _id,					\
	.mpd.hv_id = HW_VOTER_NONE,					\
	.mpd.name = _name,   				\
	.mpd.ops = &hwccf_voter_pd_ops,   		\
	.mpd.enable_cnt = 0,		\
	.set_ofs = _set_ofs,		\
	.clr_ofs = _clr_ofs,		\
	.sta_ofs = _sta_ofs,		\
	.shift = _shift,		 \
	.mpd.flags = _flags,		\
}

void hwccf_disable_unused(void);
void hwccf_init(uint32_t base);

#endif /* _HWCCF_PROVIDER_H_ */

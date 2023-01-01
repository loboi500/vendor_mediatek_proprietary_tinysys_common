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

#ifndef _VIP_PD_PROVIDER_H_
#define _VIP_PD_PROVIDER_H_

#include <stdint.h>
#include <sys/types.h>

#define VIP_MAX_VOTER_CLKS			2
#define VIP_MAX_STEPS				8
#define VIP_MAX_SRAM_STEPS			4

#define VIP_SCPD_SRAM_ISO		BIT(0)
#define VIP_SCPD_NO_SRAM		BIT(1)
#define VIP_SCPD_SRAM_SLP		BIT(2)
#define VIP_SCPD_L2TCM_SRAM		BIT(3)
#define VIP_SCPD_BYPASS_CLK		BIT(4)
#define VIP_SCPD_LP_CLK		BIT(5)

#define NR_VPD	32

#define INIT_VIP_PD_DATA(pddt, dvpds) \
	do { \
		uint32_t i; \
		for (i = 0; i < ARRAY_SIZE(dvpds); i++) \
			pddt->vpds[dvpds[i].vpd.id] = &dvpds[i].vpd; \
	} while (0)

#define VIP_BUS_PROT_REG(_base, _set_ofs,  _clr_ofs,			\
			_en_ofs, _sta_ofs) {	\
		.base = _base,			\
		.en_ofs = _en_ofs,		\
		.set_ofs = _set_ofs,		\
		.clr_ofs = _clr_ofs,		\
		.sta_ofs = _sta_ofs,		\
	}

#define VIP_BUS_PROT(_reg, _mask) {	\
		.reg = &_reg,			\
		.mask = _mask,			\
	}

#define VIP_SRAM_TABLE(_ofs, _slp_msk, _pdn_msk, _wait_ack) {	\
		.ofs = _ofs,		\
		.slp_msk = _slp_msk,	\
		.pdn_msk = _pdn_msk,		\
		.wait_ack = _wait_ack,	\
	}

struct vip_bus_prot_reg {
	uint32_t base;
	uint16_t en_ofs;
	uint16_t set_ofs;
	uint16_t clr_ofs;
	uint16_t sta_ofs;
};

struct vip_bus_prot {
	const struct vip_bus_prot_reg *reg;
	uint32_t mask;
};

struct vip_sram_table {
	uint32_t slp_msk;
	uint32_t pdn_msk;
	uint16_t ofs;
	uint8_t wait_ack;
};

struct vpd {
	/* nothing here */
};

void vip_pd_register_sram_table_data(const struct vip_sram_table *st_data);

#endif /* _VIP_PD_PROVIDER_H_ */

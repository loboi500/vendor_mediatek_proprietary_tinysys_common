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
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <mt_printf.h>
#if defined(CFG_TIMER_SUPPORT)
#include <ostimer.h>
#elif defined(CFG_XGPT_SUPPORT)
#include <xgpt.h>
#else
#error "no timer defined"
#endif
#include <clk.h>
#include "FreeRTOS_CLI.h"

#include "ccf_common.h"
#include "mtk_clk_provider.h"
#include "mtk_clkdbg.h"
#include "mtk_pd_provider.h"
#include "mtk_pd.h"

/*
 * clkdbg ops
 */

static const struct clkdbg_ops *clkdbg_ops;

uint32_t hwccf_remap(uint32_t addr);
void hwccf_write(uint32_t addr, uint32_t value);
uint32_t hwccf_read(uint32_t addr);

void set_clkdbg_ops(const struct clkdbg_ops *ops)
{
	clkdbg_ops = ops;
}

static const struct fmeter_clk *get_all_fmeter_clks(void)
{
	if (!clkdbg_ops || !clkdbg_ops->get_all_fmeter_clks)
		return NULL;

	return clkdbg_ops->get_all_fmeter_clks();
}

static void *prepare_fmeter(void)
{
	if (!clkdbg_ops || !clkdbg_ops->prepare_fmeter)
		return NULL;

	return clkdbg_ops->prepare_fmeter();
}

static void unprepare_fmeter(void *data)
{
	if (!clkdbg_ops || !clkdbg_ops->unprepare_fmeter)
		return;

	clkdbg_ops->unprepare_fmeter(data);
}

static uint32_t fmeter_freq(const struct fmeter_clk *fclk)
{
	if (!clkdbg_ops || !clkdbg_ops->fmeter_freq)
		return 0;

	return clkdbg_ops->fmeter_freq(fclk);
}

static const struct regname *get_all_regnames(void)
{
	if (!clkdbg_ops || !clkdbg_ops->get_all_regnames)
		return NULL;

	return clkdbg_ops->get_all_regnames();
}

static const char * const *get_all_clk_names(void)
{
	if (!clkdbg_ops || !clkdbg_ops->get_all_clk_names)
		return NULL;

	return clkdbg_ops->get_all_clk_names();
}

static const char * const *get_pwr_names(void)
{
	static const char * const default_pwr_names[32] = {""};

	if (!clkdbg_ops || !clkdbg_ops->get_pwr_names)
		return default_pwr_names;

	return clkdbg_ops->get_pwr_names();
}

static void setup_provider(struct provider *pvd)
{
	if (!clkdbg_ops || !clkdbg_ops->setup_provider)
		return;

	clkdbg_ops->setup_provider(pvd);
}

/*
 * clkdbg fmeter
 */

typedef void (*fn_fclk_freq_proc)(const struct fmeter_clk *fclk,
					uint32_t freq);

static void proc_all_fclk_freq(fn_fclk_freq_proc proc)
{
	void *fmeter_data;
	const struct fmeter_clk *fclk;

	fclk = get_all_fmeter_clks();

	if (!fclk || !proc)
		return;

	fmeter_data = prepare_fmeter();

	for (; fclk->type; fclk++) {
		uint32_t freq;

		freq = fmeter_freq(fclk);
		proc(fclk, freq);
	}

	unprepare_fmeter(fmeter_data);
}

static void print_fclk_freq(const struct fmeter_clk *fclk,
				uint32_t freq)
{
	FreeRTOS_CLIPrintf("%2d: %-29s: %u\r\n", fclk->id, fclk->name, freq);
}

static int clkdbg_fmeter(int argc, char ** argv)
{
	proc_all_fclk_freq(print_fclk_freq);

	return 0;
}

/*
 * clkdbg dump_regs / dump_regs2
 */

static void proc_all_regname()
{
	const struct regname *rn = get_all_regnames();

	if (!rn)
		return;

	for (; rn->base; rn++)
		FreeRTOS_CLIPrintf("%-24s: [0x%08x] = 0x%08x\r\n",
			rn->name, PHYSADDR(rn), hwccf_read(hwccf_remap(PHYSADDR(rn))));
}

static int clkdbg_dump_regs(int argc, char ** argv)
{
	proc_all_regname();

	return 0;
}

/*
 * clkdbg dump_clks / dump_state
 */

static struct provider *get_all_providers(void)
{
	static struct provider providers[NR_PROVIDER];
	struct mtk_clk_provider *cp;
	int n = 0;

	if (providers[0].mcks)
		return providers;

	for (cp = get_mtk_clk_pvds(); cp->data != NULL; cp++) {
		struct mtk_clk_data *clk_data = cp->data;
		struct provider *pc = &providers[n++];

		pc->provider_name = cp->name;
		pc->mcks = clk_data->mcks;
		pc->nclks = clk_data->nclks;
		setup_provider(pc);
	}

	return providers;
}

static int mck_is_on(struct mtk_clk *mck)
{
	const struct mtk_clk_ops *ops = mck->ops;

	if (ops->is_enabled)
		return ops->is_enabled(mck);

	return mck->enable_cnt > 0;
}

static bool mck_pwr_is_on(struct mtk_clk *mck,
			uint32_t spm_pwr_status, uint32_t pwr_mask)
{
	if ((spm_pwr_status & pwr_mask) != pwr_mask)
		return false;

	return mck_is_on(mck);
}

static bool pvd_pwr_is_on(struct provider *pvd, uint32_t spm_pwr_status, int i)
{
	return mck_pwr_is_on(pvd->mcks[i], spm_pwr_status, pvd->pwr_mask);
}

static bool pvd_is_on(struct provider *pvd, int i)
{
	uint32_t spm_pwr_status = 0;

	if (pvd->pwr_mask)
		spm_pwr_status = hwccf_read(hwccf_remap(0x10006180));

	return pvd_pwr_is_on(pvd, spm_pwr_status, i);
}

static const char *mck_state(struct mtk_clk *mck)
{
	if (mck->enable_cnt > 0)
		return "enabled";

	return "disabled";
}

static void dump_provider_clk(struct provider *pvd, int i)
{
	struct mtk_clk *mck = pvd->mcks[i];
	struct clk *p = clk_get_parent(clk_from_mck(mck));
	const char *pn = p ? mck_from_clk(p)->name : NULL;

	FreeRTOS_CLIPrintf("[%10s: %-17s: %3s, %3d, %10lu, %17s]\r\n",
		pvd->provider_name ? pvd->provider_name : "/ ",
		mck->name,
		pvd_is_on(pvd, i) ? "ON" : "off",
		mck->enable_cnt,
		clk_get_rate(clk_from_mck(mck)),
		pn);
}

static void dump_provider_clks(struct provider *pvd)
{
	int i;

	for (i = 0; i < pvd->nclks; i++)
		dump_provider_clk(pvd, i);
}

static int clkdbg_dump_provider_clks(int argc, char ** argv)
{
	struct provider *pvd = get_all_providers();

	for (; pvd->mcks; pvd++)
		dump_provider_clks(pvd);

	return 0;
}

static void dump_clk_state(const char *clkname)
{
	struct clk *c = clk_from_name(clkname);
	struct clk *p = c ? clk_get_parent(c) : NULL;
	struct mtk_clk *mck = mck_from_clk(c);

	if (c == NULL) {
		FreeRTOS_CLIPrintf("[%17s: NULL]\r\n", clkname);
		return;
	}

	FreeRTOS_CLIPrintf("[%-17s: %8s, %3d, %10lu, %17s]\r\n",
		mck->name,
		mck_state(mck),
		mck->enable_cnt,
		clk_get_rate(c),
		p ? mck_from_clk(p)->name : "- ");
}

static int clkdbg_dump_state(int argc, char ** argv)
{
	const char * const *ckn = get_all_clk_names();

	if (!ckn)
		return clkdbg_dump_provider_clks(argc, argv);

	for (; *ckn; ckn++)
		dump_clk_state(*ckn);

	return 0;
}

/*
 * clkdbg dump_muxes
 */

static void dump_provider_mux(struct provider *pvd)
{
	int i, j;

	for (i = 0; i < pvd->nclks; i++) {
		struct mtk_clk *mck = pvd->mcks[i];
		unsigned int np = mck->num_parents;

		if (np <= 1)
			continue;

		dump_provider_clk(pvd, i);

		for (j = 0; j < np; j++) {
			struct clk *p = clk_from_name(mck->pn.parent_names[j]);
			struct mtk_clk *mck_p = mck_from_clk(p);

			if (p == NULL)
				continue;

			FreeRTOS_CLIPrintf("\t\t\t(%2d: %-17s: %8s, %10ld)\r\n",
				j,
				mck_p->name,
				mck_state(mck_p),
				clk_get_rate(p));
		}
	}
}

static int clkdbg_dump_muxes(int argc, char ** argv)
{
	struct provider *pvd = get_all_providers();

	for (; pvd->mcks; pvd++)
		dump_provider_mux(pvd);

	return 0;
}

/*
 * clkdbg pwr_status
 */

static int dump_pwr_status(uint32_t spm_pwr_status)
{
	int i;
	const char * const *pwr_name = get_pwr_names();

	FreeRTOS_CLIPrintf("SPM_PWR_STATUS: 0x%08x\n\r\n", spm_pwr_status);

	for (i = 0; i < 32; i++) {
		const char *st = (spm_pwr_status & BIT(i)) ? "ON" : "off";

		FreeRTOS_CLIPrintf("[%2d]: %3s: %s\r\n", i, st, pwr_name[i]);
	}

	return 0;
}

static int clkdbg_pwr_status(int argc, char ** argv)
{
	return dump_pwr_status(hwccf_read(hwccf_remap(0x10006180)));
}

/*
 * clkdbg enable / disable / set_parent / set_rate
 */

static int clkop_int_ckname(int (*clkop)(struct clk *clk),
			const char *clkop_name, const char *clk_name,
			struct clk *ck)
{
	struct clk *clk;

	if (ck != NULL) {
		clk = ck;
	} else {
		clk = clk_from_name(clk_name);
		if (clk == NULL) {
			FreeRTOS_CLIPrintf("clk_from_name(%s): NULL\r\n", clk_name);
			return -EINVAL;
		}
	}

	return clkop(clk);
}

static int clkdbg_clkop_int_ckname(int (*clkop)(struct clk *clk),
			const char *clkop_name, const char *clk_name)
{
	int r = 0;
	int i;

	if (!clk_name)
		return 0;

	if (strcmp(clk_name, "all") == 0) {
		struct provider *pvd = get_all_providers();

		for (; pvd->mcks; pvd++) {
			for (i = 0; i < pvd->nclks; i++)
				r |= clkop_int_ckname(clkop, clkop_name, NULL,
							clk_from_mck(pvd->mcks[i]));
		}

		FreeRTOS_CLIPrintf("%s(%s): %d\r\n", clkop_name, clk_name, r);

		return r;
	}

	r = clkop_int_ckname(clkop, clkop_name, clk_name, NULL);
	FreeRTOS_CLIPrintf("%s(%s): %d\r\n", clkop_name, clk_name, r);

	return r;
}

static int clkdbg_enable(int argc, char ** argv)
{
	if (argc < 2) {
		FreeRTOS_CLIPrintf("clk_enable: invalid arguments\r\n");
		return -EINVAL;
	}

	return clkdbg_clkop_int_ckname(clk_enable,
					"clk_enable", argv[1]);
}

static int clkdbg_disable(int argc, char ** argv)
{
	if (argc < 2) {
		FreeRTOS_CLIPrintf("clk_disable: invalid arguments\r\n");
		return -EINVAL;
	}

	return clkdbg_clkop_int_ckname(clk_disable,
					"clk_disable", argv[1]);
}

static int clkdbg_set_parent(int argc, char ** argv)
{
	char *clk_name;
	char *parent_name;
	struct clk *clk;
	struct clk *parent;
	int r;

	if (argc < 3) {
		FreeRTOS_CLIPrintf("clk_set_parent: invalid arguments\r\n");
		return -EINVAL;
	}

	clk_name = argv[1];
	parent_name = argv[2];

	clk = clk_from_name(clk_name);
	if (clk == NULL) {
		FreeRTOS_CLIPrintf("clk_from_name(): 0x%p\r\n", clk);
		return -EINVAL;
	}

	parent = clk_from_name(parent_name);
	if (parent == NULL) {
		FreeRTOS_CLIPrintf("clk_from_name(): 0x%p\r\n", parent);
		return -EINVAL;
	}

	r = clk_enable(clk);
	if (r) {
		FreeRTOS_CLIPrintf("clk_enable(): %d\r\n", r);
		return r;
	}

	r = clk_set_parent(clk, parent);
	FreeRTOS_CLIPrintf("clk_set_parent(%s, %s): %d\r\n", clk_name, parent_name, r);

	clk_disable(clk);

	return r;
}

static int clkdbg_set_rate(int argc, char ** argv)
{
	char *clk_name;
	struct clk *clk;
	unsigned long rate;
	int r;

	if (argc < 3) {
		FreeRTOS_CLIPrintf("clk_set_rate: invalid arguments\r\n");
		return -EINVAL;
	}

	clk_name = argv[1];
	rate = strtoul(argv[2], NULL, 0);

	clk = clk_from_name(clk_name);
	if (clk == NULL) {
		FreeRTOS_CLIPrintf("clk_from_name(): 0x%p\r\n", clk);
		return -EINVAL;
	}

	r = clk_set_rate(clk, rate);
	FreeRTOS_CLIPrintf("clk_set_rate(%s, %lu): %d\r\n", clk_name, rate, r);

	return r;
}

/*
 * clkdbg reg_read / reg_write / reg_set / reg_clr
 */

static int clkdbg_reg_read(int argc, char ** argv)
{
	unsigned long addr;

	if (argc < 2) {
		FreeRTOS_CLIPrintf("clkdbg_reg_read: invalid arguments\r\n");
		return -EINVAL;
	}

	addr = hwccf_remap(strtoul(argv[1], NULL, 0));

	FreeRTOS_CLIPrintf("read(0x%08x): 0x%08x\r\n", (uint32_t)addr, hwccf_read(addr));

	return 0;
}

static int clkdbg_reg_write(int argc, char ** argv)
{
	unsigned long addr;
	unsigned long val;

	if (argc < 3) {
		FreeRTOS_CLIPrintf("clkdbg_reg_write: invalid arguments\r\n");
		return -EINVAL;
	}

	addr = hwccf_remap(strtoul(argv[1], NULL, 0));
	val = strtoul(argv[2], NULL, 0);

	FreeRTOS_CLIPrintf("write(0x%08x, 0x%08x)\r\n", (uint32_t)addr, (uint32_t)val);

	hwccf_write(addr, val);
	FreeRTOS_CLIPrintf("read(0x%08x): 0x%08x\r\n", (uint32_t)addr, hwccf_read(addr));

	return 0;
}

static int clkdbg_reg_set(int argc, char ** argv)
{
	unsigned long addr;
	unsigned long val;

	if (argc < 3) {
		FreeRTOS_CLIPrintf("clkdbg_reg_set: invalid arguments\r\n");
		return -EINVAL;
	}

	addr = hwccf_remap(strtoul(argv[1], NULL, 0));
	val = strtoul(argv[2], NULL, 0);

	FreeRTOS_CLIPrintf("set(0x%08x, 0x%08x)\r\n", (uint32_t)addr, (uint32_t)val);

	val = hwccf_read(addr) | val;
	hwccf_write(addr, val);

	FreeRTOS_CLIPrintf("read(0x%08x): 0x%08x\r\n", (uint32_t)addr, hwccf_read(addr));

	return 0;
}

static int clkdbg_reg_clr(int argc, char ** argv)
{
	unsigned long addr;
	unsigned long val;

	if (argc < 3) {
		FreeRTOS_CLIPrintf("clkdbg_reg_set: invalid arguments\r\n");
		return -EINVAL;
	}

	addr = hwccf_remap(strtoul(argv[1], NULL, 0));
	val = strtoul(argv[2], NULL, 0);

	FreeRTOS_CLIPrintf("clr(0x%08x, 0x%08x)\r\n", (uint32_t)addr, (uint32_t)val);

	val = hwccf_read(addr) & ~val;
	hwccf_write(addr, val);

	FreeRTOS_CLIPrintf("read(0x%08x): 0x%08x\r\n", (uint32_t)addr, hwccf_read(addr));

	return 0;
}

/*
 * clkdbg dump_pds / pd_enable / pd_disable / pd_on / pd_off
 */

static int clkdbg_dump_pds(int argc, char ** argv)
{
	const struct mtk_pd_ops *ops = get_mtk_pd_ops();
	struct mtk_pd *pd = get_mtk_pds();

	for (; pd->data != NULL; pd++)
		FreeRTOS_CLIPrintf("[%-9s: %3s, %2d]\r\n",
			pd->data->name, ops->is_on(pd) ? "ON" : "off",
			pd->enable_cnt);

	return 0;
}

static int pd_op(const char *op_name, const char *pd_name)
{
	struct mtk_pd *pd;
	int r = 0;
	int (*op)(struct mtk_pd *pd);

	if (strcmp(op_name, "mtk_pd_enable") == 0)
		op = mtk_pd_enable;
	else if (strcmp(op_name, "mtk_pd_disable") == 0)
		op = mtk_pd_disable;
	else if (strcmp(op_name, "mtk_pd_on") == 0)
		op = get_mtk_pd_ops()->power_on;
	else if (strcmp(op_name, "mtk_pd_off") == 0)
		op = get_mtk_pd_ops()->power_off;
	else {
		FreeRTOS_CLIPrintf("unknown op: %s\r\n", op_name);
		return 0;
	}

	if (!pd_name)
		return 0;

	if (strcmp(pd_name, "all") == 0) {
		pd = get_mtk_pds();

		for (; pd->data != NULL; pd++)
			r |= op(pd);

		FreeRTOS_CLIPrintf("%s(%s): %d\r\n", op_name, pd_name, r);

		return 0;
	}

	pd = mtk_pd_from_name(pd_name);

	if (pd == NULL)
		FreeRTOS_CLIPrintf("%s(%s): NULL\r\n", op_name, pd_name);
	else {
		r = op(pd);
		FreeRTOS_CLIPrintf("%s(%s): %d\r\n", op_name, pd_name, r);
	}

	return 0;
}

static int clkdbg_pd_enable(int argc, char ** argv)
{
	if (argc < 2) {
		FreeRTOS_CLIPrintf("clkdbg_pd_enable: invalid arguments\r\n");
		return -EINVAL;
	}

	return pd_op("mtk_pd_enable", argv[1]);
}

static int clkdbg_pd_disable(int argc, char ** argv)
{
	if (argc < 2) {
		FreeRTOS_CLIPrintf("clkdbg_pd_disable: invalid arguments\r\n");
		return -EINVAL;
	}

	return pd_op("mtk_pd_disable", argv[1]);
}

static int clkdbg_pd_on(int argc, char ** argv)
{
	if (argc < 2) {
		FreeRTOS_CLIPrintf("clkdbg_pd_on: invalid arguments\r\n");
		return -EINVAL;
	}

	return pd_op("mtk_pd_on", argv[1]);
}

static int clkdbg_pd_off(int argc, char ** argv)
{
	if (argc < 2) {
		FreeRTOS_CLIPrintf("clkdbg_pd_off: invalid arguments\r\n");
		return -EINVAL;
	}

	return pd_op("mtk_pd_off", argv[1]);
}

/*
 * clkdbg cmds
 */

static const struct cmd_fn *custom_cmds;

void set_custom_cmds(const struct cmd_fn *cmds)
{
	custom_cmds = cmds;
}

static int clkdbg_cmds(int argc, char ** argv);

static const struct cmd_fn common_cmds[] = {
	CMDFN("dump_regs", clkdbg_dump_regs),
	CMDFN("dump_regs2", clkdbg_dump_regs),
	CMDFN("dump_state", clkdbg_dump_state),
	CMDFN("dump_clks", clkdbg_dump_provider_clks),
	CMDFN("dump_muxes", clkdbg_dump_muxes),
	CMDFN("fmeter", clkdbg_fmeter),
	CMDFN("pwr_status", clkdbg_pwr_status),
	CMDFN("enable", clkdbg_enable),
	CMDFN("disable", clkdbg_disable),
	CMDFN("set_parent", clkdbg_set_parent),
	CMDFN("set_rate", clkdbg_set_rate),
	CMDFN("reg_read", clkdbg_reg_read),
	CMDFN("reg_write", clkdbg_reg_write),
	CMDFN("reg_set", clkdbg_reg_set),
	CMDFN("reg_clr", clkdbg_reg_clr),
	CMDFN("dump_pds", clkdbg_dump_pds),
	CMDFN("pd_enable", clkdbg_pd_enable),
	CMDFN("pd_disable", clkdbg_pd_disable),
	CMDFN("pd_on", clkdbg_pd_on),
	CMDFN("pd_off", clkdbg_pd_off),
	CMDFN("pwr_on", clkdbg_pd_on),
	CMDFN("pwr_off", clkdbg_pd_off),
	CMDFN("cmds", clkdbg_cmds),
	{}
};

static int clkdbg_cmds(int argc, char ** argv)
{
	const struct cmd_fn *cf;

	for (cf = common_cmds; cf->cmd; cf++)
		FreeRTOS_CLIPrintf("%s\r\n", cf->cmd);

	for (cf = custom_cmds; cf && cf->cmd; cf++)
		FreeRTOS_CLIPrintf("%s\r\n", cf->cmd);

	FreeRTOS_CLIPrintf("\r\n");

	return 0;
}

int clkdbg_run(int argc, char ** argv)
{
	const struct cmd_fn *cf;

	if (argc < 1) {
		FreeRTOS_CLIPrintf("clkdbg: invalid arguments\r\n");
		return -EINVAL;
	}

	for (cf = custom_cmds; cf && cf->cmd; cf++) {
		if (strcmp(cf->cmd, argv[0]) == 0)
			return cf->fn(argc, argv);
	}

	for (cf = common_cmds; cf->cmd; cf++) {
		if (strcmp(cf->cmd, argv[0]) == 0)
			return cf->fn(argc, argv);
	}

	return 0;
}

#define CLKDBG_MAX_ARGS	3
static int cli_clkdbg( char *pcWriteBuffer, size_t xWriteBufferLen, const char *pcCommandString )
{
	int i, para_len;
	char *para = (char*)pcCommandString;
	int argc;
	const char *argv[CLKDBG_MAX_ARGS + 1];

	argc = FreeRTOS_CLIGetNumberOfParameters(pcCommandString);
	if (argc < 1) {
		FreeRTOS_CLIPrintf("clkdbg: invalid arguments\r\n");
		return 0;
	}

	FreeRTOS_CLIPrintf("%s\r\n", pcCommandString);

	for (i = 1; i <= argc; i++)
		argv[i-1] = FreeRTOS_CLIGetParameter(pcCommandString, i, &para_len);

	for(; *para != 0x00 ; para++) {
		if (*para == ' ')
			*para = 0;
	}

	clkdbg_run(argc, (char **)argv);

	return 0;
}

static const CLI_Command_Definition_t cli_cmd_clkdbg =
{
	"clkdbg",
	"\r\nclkdbg:\r\n Commands to debug clock/mtcmos."
	"\r\n use \"clkdbg cmds\" to show all available commands\r\r\n",
	cli_clkdbg,
	-1
};

void cli_clkdbg_register(void)
{
	FreeRTOS_CLIRegisterCommand(&cli_cmd_clkdbg);
}

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
#include <semphr.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <limits.h>
#include <mt_printf.h>
#if defined(CFG_TIMER_SUPPORT)
#include <ostimer.h>
#elif defined(CFG_XGPT_SUPPORT)
#include <xgpt.h>
#elif defined(ISP_CCU)
#include <ccu_utility.h>
#else
#error "no timer defined"
#endif
#include <ccf_common.h>
#include <clk.h>
#include <mtk_clk_provider.h>
#include "hwccf_port.h"

#define CCF_DEBUG  0

#if CCF_DEBUG
#define CCF_LOG(fmt, arg...) PRINTF_E(fmt, ##arg)
#else
#define CCF_LOG(...)
#endif

#if CCF_DEBUG
#define SEMA_TIMEOUT_MS 500
#else
#define SEMA_TIMEOUT_MS 100
#endif

static SemaphoreHandle_t xSemaphore_clk;
static bool mutex_initialized = false;

static void mtk_clk_lock(void)
{
	/* CCF_LOG("%s\n", __func__); */

	if (is_in_isr())
		return;

	if (!mutex_initialized) {
		xSemaphore_clk = xSemaphoreCreateMutex();
		mutex_initialized = true;
	}

	if (xSemaphoreTake(xSemaphore_clk, pdMS_TO_TICKS(SEMA_TIMEOUT_MS)) != pdTRUE) {
		PRINTF_E("xSemaphoreTake(xSemaphore_clk) fail\n");
		configASSERT(0);
	}
}

static void mtk_clk_unlock(void)
{
	/* CCF_LOG("%s\n", __func__); */

	if (is_in_isr())
		return;

	if (xSemaphoreGive(xSemaphore_clk) != pdTRUE) {
		PRINTF_E("xSemaphoreGive(xSemaphore_clk) fail\n");
		configASSERT(0);
	}
}

struct mtk_clk_provider mtk_clk_providers[NR_PROVIDER] = {{0}};

struct mtk_clk_provider *get_mtk_clk_pvds(void)
{
	return mtk_clk_providers;
}

static struct mtk_clk *clk_from_name_nolock(const char *name)
{
	uint32_t i, j;

	CCF_LOG("%s(%s)\n", __func__, name?name:"null");

	if (!name)
		return NULL;

	for (i = 0; i < NR_PROVIDER; i++) {
		struct mtk_clk_data *clk_data = mtk_clk_providers[i].data;

		if (!clk_data)
			continue;

		for (j = 0; j < clk_data->nclks; j++) {
			struct mtk_clk *mck = clk_data->mcks[j];

			if (!mck || !mck->name)
				continue;

			if (!strcmp(name, mck->name))
				return mck;
		}
	}

	return NULL;
}

struct clk *clk_from_name(const char *name)
{
	struct mtk_clk *mck;

	mtk_clk_lock();
	mck = clk_from_name_nolock(name);
	mtk_clk_unlock();

	return clk_from_mck(mck);
}

struct clk *clk_get(int id)
{
	struct mtk_clk_data *clk_data;
	uint32_t pvd_id, clk_id;

	pvd_id = PVD_IDX(id);
	clk_id = CLK_IDX(id);

	CCF_LOG("clk_get: pvd_id=%d, clk_id=%d\n", pvd_id, clk_id);

	if (pvd_id >= NR_PROVIDER) {
		CCF_LOG("clk_get: invalid clock pvd idx %u\n", pvd_id);
		return (struct clk *) -EINVAL;
	}

	clk_data = mtk_clk_providers[pvd_id].data;
	if (!clk_data) {
		CCF_LOG("clk_get: clock pvd %u not rdy\n", pvd_id);
		return (struct clk *) -EDEFER;
	}

	if (clk_id >= clk_data->nclks) {
		CCF_LOG("clk_get: invalid clock idx %d >= %d\n", clk_id, clk_data->nclks);
		return (struct clk *) -EINVAL;
	}

	return clk_from_mck(clk_data->mcks[clk_id]);
}

void clk_put(struct clk *clk)
{
	// Do nothing.
}

bool is_err_or_null_clk(struct clk *clk)
{
	return ((int)clk <= 0);
}

static void mtk_clk_parents_init(struct mtk_clk *mck)
{
	int i;

	CCF_LOG("%s\n", __func__);

	if (mck->num_parents < 1)
		return;

	if (mck->num_parents == 1) {
		if (mck->p.parent)
			return;

		mck->p.parent = clk_from_name_nolock(mck->pn.parent_name);

		return;
	}

	if (mck->num_parents > 1) {
		if (!mck->p.parents) {
			mck->p.parents = pvPortMalloc(sizeof(*mck->p.parents) * mck->num_parents);
			if (mck->p.parents) {
				CCF_LOG("%s: pvPortMalloc size=%u\n",
						__func__, (uint32_t)(sizeof(*mck->p.parents) * mck->num_parents));
				memset(mck->p.parents, 0, (sizeof(*mck->p.parents) * mck->num_parents));
			} else {
				PRINTF_E("%s: pvPortMalloc fail, size=%u\n",
						__func__, (uint32_t)(sizeof(*mck->p.parents) * mck->num_parents));
				configASSERT(0);
				return;
			}
		}

		for (i = 0; i < mck->num_parents; i++) {
			if (!mck->pn.parent_names[i] || mck->p.parents[i])
				continue;

			mck->p.parents[i] = clk_from_name_nolock(
						mck->pn.parent_names[i]);
		}
	}
}

static struct mtk_clk *mtk_clk_get_parent_nolock(struct mtk_clk *mck)
{
	uint8_t pi;

	CCF_LOG("%s: %s\n", __func__, mck->name?mck->name:"");

	if (mck->num_parents < 1)
		return NULL;

	mtk_clk_parents_init(mck);

	if (mck->num_parents == 1)
		return mck->p.parent;

	/* mck->num_parents > 1 */

	if (!mck->ops || !mck->ops->get_parent)
		return NULL;

	pi = mck->ops->get_parent(mck);

	if (pi >= mck->num_parents || !mck->p.parents)
		return NULL;

	return mck->p.parents[pi];
}

struct clk *clk_get_parent(struct clk *clk)
{
	struct mtk_clk *mck, *mck_p;

	CCF_LOG("%s\n", __func__);

	if (!clk)
		return NULL;

	mtk_clk_lock();

	mck = mck_from_clk(clk);
	mck_p = mtk_clk_get_parent_nolock(mck);

	mtk_clk_unlock();

	return clk_from_mck(mck_p);
}

static unsigned long mtk_clk_get_rate_nolock(struct mtk_clk *mck)
{
	struct mtk_clk *clk_p = mtk_clk_get_parent_nolock(mck);
	unsigned long parent_rate =
			clk_p ? mtk_clk_get_rate_nolock(clk_p) : 0;

	CCF_LOG("%s: %s\n", __func__, mck->name?mck->name:"");

	if (mck->ops->recalc_rate)
		return mck->ops->recalc_rate(mck, parent_rate);

	return parent_rate;
}

unsigned long clk_get_rate(struct clk *clk)
{
	unsigned long r;

	CCF_LOG("%s\n", __func__);

	if (!clk)
		return 0;

	mtk_clk_lock();
	r = mtk_clk_get_rate_nolock(mck_from_clk(clk));
	mtk_clk_unlock();

	return r;
}

static int mtk_clk_set_rate_nolock(struct mtk_clk *mck, unsigned long rate)
{
	struct mtk_clk *clk_p = mtk_clk_get_parent_nolock(mck);
	unsigned long parent_rate =
			clk_p ? mtk_clk_get_rate_nolock(clk_p) : 0;
	unsigned long new_prate = parent_rate;
	long rounded = rate;
	int r = EINVAL;

	CCF_LOG("%s: %s, %u\n", __func__, mck->name?mck->name:"", rate);

	if (rate == mtk_clk_get_rate_nolock(mck))
		return 0;

	if (mck->ops->round_rate)
		rounded = mck->ops->round_rate(mck, rate, &new_prate);
	else
		new_prate = rate;

	if (clk_p && new_prate != parent_rate)
		r = mtk_clk_set_rate_nolock(clk_p, new_prate);

	if (mck->ops->set_rate && !(mck->flags & CLK_SET_RATE_PARENT))
		return mck->ops->set_rate(mck, rounded, new_prate);

	return r;
}

static int mtk_clk_set_rate(struct mtk_clk *mck, unsigned long rate)
{
	int r;

	//CCF_LOG("%s: %s, %u\n", __func__, mck->name?mck->name:"", rate);

	mtk_clk_lock();
	r = mtk_clk_set_rate_nolock(mck, rate);
	mtk_clk_unlock();

	return r;
}

int clk_set_rate(struct clk *clk, unsigned long rate)
{
	int r;
	struct mtk_clk *mck;

	if (!clk) {
		CCF_LOG("%s: clk null\n", __func__);
		return 0;
	}

	mck = mck_from_clk(clk);
	CCF_LOG("%s: %s: %u\n", __func__, mck->name?mck->name:"", rate);

	if (mck->flags & CLK_SET_RATE_PARENT)
		r = clk_set_rate(clk_get_parent(clk), rate);
	else
		r = mtk_clk_set_rate(mck, rate);

	return r;
}

static int mtk_clk_enable_nolock(struct mtk_clk *mck)
{
	struct mtk_clk *clk_p;

	CCF_LOG("%s: %s\n", __func__, mck->name? mck->name:"");

	if ((mck->flags & PLL_AO) == PLL_AO) {
		CCF_LOG("%s: %s: bypass\n", __func__, mck->name? mck->name:"");
		return 0;
	}

	mck->enable_cnt++;
	CCF_LOG("\n%s: %s: enable_cnt++ %d => %d\n\n", __func__,
			mck->name? mck->name:"", mck->enable_cnt - 1, mck->enable_cnt);
	if (mck->enable_cnt != 1)
		return 0;

	clk_p = mtk_clk_get_parent_nolock(mck);
	if (clk_p)
		mtk_clk_enable_nolock(clk_p);

	if (mck->hc_id && mck->hc_id != INV_ID) {
		struct clk *clk;
		int ret;

		clk = clk_get(mck->hc_id);
		if (is_err_or_null_clk(clk)) {
			PRINTF_E("[%s] get clk[%d] fail\n", mck->name, mck->hc_id);
			return -EINVAL;
		}

		ret = mtk_clk_enable_nolock(mck_from_clk(clk));
		if (ret)
			PRINTF_E("[%s] enable clk[%d] fail\n", mck->name, mck->hc_id);

		return ret;
	}

	if (mck->ops && mck->ops->enable)
		return mck->ops->enable(mck);

	return 0;
}

int clk_enable(struct clk *clk)
{
	int r;

	CCF_LOG("%s\n", __func__);

	if (!clk)
		return -EINVAL;

	mtk_clk_lock();
	r = mtk_clk_enable_nolock(mck_from_clk(clk));
	mtk_clk_unlock();

	return r;
}

static int mtk_clk_disable_nolock(struct mtk_clk *mck)
{
	struct mtk_clk *clk_p;
	int ret = 0;

	if ((mck->flags & PLL_AO) == PLL_AO) {
		CCF_LOG("%s: %s: bypass\n", __func__, mck->name? mck->name:"");
		return 0;
	}

	if (mck->enable_cnt <= 0) {
		PRINTF_E("%s: %s: enable_cnt(%d) <= 0\n", __func__,
			mck->name? mck->name:"", mck->enable_cnt);
		configASSERT(0);
		return -EINVAL;
	}

	mck->enable_cnt--;
	CCF_LOG("\n%s: %s: enable_cnt-- %d => %d\n\n", __func__,
			mck->name? mck->name:"null", mck->enable_cnt+1, mck->enable_cnt);
	if (mck->enable_cnt != 0)
		return 0;

	if (mck->hc_id && mck->hc_id != INV_ID) {
		struct clk *clk;

		clk = clk_get(mck->hc_id);
		if (is_err_or_null_clk(clk)) {
			PRINTF_E("[%s] get clk[%d] fail\n", mck->name, mck->hc_id);
			return -EINVAL;
		}

		ret = mtk_clk_disable_nolock(mck_from_clk(clk));
		if (ret)
			PRINTF_E("[%s] disable clk[%d] fail\n", mck->name, mck->hc_id);
	} else if (mck->ops && mck->ops->disable)
		mck->ops->disable(mck);

	clk_p = mtk_clk_get_parent_nolock(mck);
	if (clk_p)
		mtk_clk_disable_nolock(clk_p);

	return ret;
}

int clk_disable(struct clk *clk)
{
	int r;

	CCF_LOG("%s\n", __func__);

	if (!clk)
		return -EINVAL;

	mtk_clk_lock();
	r = mtk_clk_disable_nolock(mck_from_clk(clk));
	mtk_clk_unlock();

	return r;
}

static int mtk_clk_prepare_nolock(struct mtk_clk *mck)
{
	struct mtk_clk *clk_p;

	if ((mck->flags & PLL_AO) == PLL_AO) {
		CCF_LOG("%s: %s: bypass\n", __func__, mck->name? mck->name:"");
		return 0;
	}

	mck->prepare_cnt++;
	CCF_LOG("\n%s: %s: prepare_cnt++ %d => %d\n\n", __func__,
			mck->name? mck->name:"", mck->prepare_cnt - 1, mck->prepare_cnt);
	if (mck->prepare_cnt != 1)
		return 0;

	clk_p = mtk_clk_get_parent_nolock(mck);
	if (clk_p)
		mtk_clk_prepare_nolock(clk_p);

	if (mck->hc_id && mck->hc_id != INV_ID) {
		struct clk *clk;
		int ret;

		clk = clk_get(mck->hc_id);
		if (is_err_or_null_clk(clk)) {
			PRINTF_E("[%s] get clk[%d] fail\n", mck->name, mck->hc_id);
			return -EINVAL;
		}

		ret = mtk_clk_prepare_nolock(mck_from_clk(clk));
		if (ret)
			PRINTF_E("[%s] prepare clk[%d] fail\n", mck->name, mck->hc_id);

		return ret;
	}

	if (mck->ops && mck->ops->prepare)
		return mck->ops->prepare(mck);

	return 0;
}

int clk_prepare(struct clk *clk)
{
	int r;

	CCF_LOG("%s\n", __func__);

	if (!clk)
		return -EINVAL;

	mtk_clk_lock();
	r = mtk_clk_prepare_nolock(mck_from_clk(clk));
	mtk_clk_unlock();

	return r;
}

static int mtk_clk_unprepare_nolock(struct mtk_clk *mck)
{
	struct mtk_clk *clk_p;
	int ret = 0;

	if ((mck->flags & PLL_AO) == PLL_AO) {
		CCF_LOG("%s: %s: bypass\n", __func__, mck->name? mck->name:"");
		return 0;
	}

	if (mck->prepare_cnt <= 0) {
		PRINTF_E("%s: %s prepare_cnt %d <= 0\n", __func__,
				mck->name? mck->name:"", mck->prepare_cnt);
		configASSERT(0);
		return -EINVAL;
	}

	mck->prepare_cnt--;
	CCF_LOG("\n%s: %s prepare_cnt-- %d => %d\n\n", __func__,
			mck->name?mck->name:"", mck->prepare_cnt+1, mck->prepare_cnt);
	if (mck->prepare_cnt != 0)
		return 0;

	if (mck->hc_id && mck->hc_id != INV_ID) {
		struct clk *clk;

		clk = clk_get(mck->hc_id);
		if (is_err_or_null_clk(clk)) {
			PRINTF_E("[%s] get clk[%d] fail\n", mck->name, mck->hc_id);
			return -EINVAL;
		}

		ret = mtk_clk_unprepare_nolock(mck_from_clk(clk));
		if (ret)
			PRINTF_E("[%s] unprepare clk[%d] fail\n", mck->name, mck->hc_id);
	} else if (mck->ops && mck->ops->unprepare)
		mck->ops->unprepare(mck);

	clk_p = mtk_clk_get_parent_nolock(mck);
	if (clk_p)
		mtk_clk_unprepare_nolock(clk_p);

	return ret;
}

int clk_unprepare(struct clk *clk)
{
	int r;

	CCF_LOG("%s\n", __func__);

	if (!clk)
		return -EINVAL;

	mtk_clk_lock();
	r = mtk_clk_unprepare_nolock(mck_from_clk(clk));
	mtk_clk_unlock();

	return r;
}

int clk_prepare_enable(struct clk *clk)
{
	int r = 0;

	CCF_LOG("%s\n", __func__);

	r = clk_prepare(clk);
	if (r)
		return r;
	r = clk_enable(clk);

	return r;
}

int clk_disable_unprepare(struct clk *clk)
{
	int r = 0;

	CCF_LOG("%s\n", __func__);

	r = clk_disable(clk);
	if (r)
		return r;
	r = clk_unprepare(clk);

	return r;
}

static int mtk_clk_set_parent_nolock(struct mtk_clk *mck, struct mtk_clk *mck_parent)
{
	int i;
	struct mtk_clk *clk_p;

	if (!mck || !mck_parent) {
		PRINTF_E("%s: mck or mck_parent NULL\n", __func__);
		return -EINVAL;
	}

	CCF_LOG("%s(%s, %s)\n", __func__,
				mck->name?mck->name:"",
				mck_parent->name?mck_parent->name:"");

	if (!mck->ops->set_parent) {
		PRINTF_E("%s: mck->ops->set_parent NULL\n", __func__);
		return -EINVAL;
	}

	mtk_clk_parents_init(mck);

	if (mck->num_parents == 1) {
		if (mck->p.parent != mck_parent) {
			PRINTF_E("%s: mck->p.parent != mck_parent\n", __func__);
			return -EINVAL;
		}
		return 0;
	}

	/* if new parent == current parent, return */
	clk_p = mtk_clk_get_parent_nolock(mck);
	CCF_LOG("%s: current parent %s\n", __func__, clk_p->name?clk_p->name:"");
	if (clk_p && (clk_p == mck_parent)) {
		CCF_LOG("%s: parent %s already exits. no need redo set parent.\n",
				__func__, mck_parent->name?mck_parent->name:"");
		return 0;
	}

	for (i = 0;
		i < mck->num_parents && mck_parent != mck->p.parents[i];
		i++) {}

	if (i < mck->num_parents) {
		int r;

		if (mck->prepare_cnt > 0)
			mtk_clk_prepare_nolock(mck_parent);

		if (mck->enable_cnt > 0)
			mtk_clk_enable_nolock(mck_parent);

		CCF_LOG("%s: mck->ops->set_parent\n", __func__);
		r = mck->ops->set_parent(mck, i);

		if (mck->enable_cnt > 0 && clk_p)
			mtk_clk_disable_nolock(clk_p);

		if (mck->prepare_cnt > 0 && clk_p)
			mtk_clk_unprepare_nolock(clk_p);

		return r;
	}

	CCF_LOG("%s: return -EINVAL\n", __func__);
	return -EINVAL;
}

int clk_set_parent(struct clk *clk, struct clk *clk_parent)
{
	int r;

	CCF_LOG("%s\n", __func__);

	if (!clk || !clk_parent) {
		CCF_LOG("clk:%s or clk_parent:%s\n", __func__,
			clk?"not null":"null",
			clk_parent?"not null":"null");
		return -EINVAL;
	}

	mtk_clk_lock();
	r = mtk_clk_set_parent_nolock(mck_from_clk(clk),
			mck_from_clk(clk_parent));
	mtk_clk_unlock();

	return r;
}

struct mtk_clk_data *mtk_alloc_clk_data(unsigned int clk_num)
{
	uint32_t i;
	struct mtk_clk_data *clk_data;

	clk_data = pvPortMalloc(sizeof(*clk_data));
	if (!clk_data) {
		PRINTF_E("%s: pvPortMalloc fail, size=%u\n",
				__func__, (uint32_t)(sizeof(*clk_data)));
		configASSERT(0);
		return (struct mtk_clk_data *) -ENOMEM;
	} else {
		CCF_LOG("%s: pvPortMalloc size=%u\n",
				__func__, (uint32_t)(sizeof(*clk_data)));
	}

	clk_data->mcks = pvPortMalloc(clk_num * sizeof(*clk_data->mcks));
	if (!clk_data->mcks) {
		PRINTF_E("%s: pvPortMalloc fail, size=%u\n",
				__func__, (uint32_t)(clk_num * sizeof(*clk_data->mcks)));
		configASSERT(0);
		goto err_out;
	} else {
		CCF_LOG("%s: pvPortMalloc size=%u\n",
				__func__, (uint32_t)(clk_num * sizeof(*clk_data->mcks)));
	}

	clk_data->nclks = clk_num;

	for (i = 0; i < clk_num; i++)
		clk_data->mcks[i] = NULL;

	return clk_data;

err_out:
	vPortFree(clk_data);
	return (struct mtk_clk_data *) -ENOMEM;
}

int mtk_clk_add_provider(int pvd_id, const char *name, struct mtk_clk_data *clk_data, uint32_t base)
{
	struct mtk_clk_provider *mcp = &mtk_clk_providers[pvd_id];

	if (pvd_id >= NR_PROVIDER) {
		PRINTF_E("excede max provider!\n");
		return -EINVAL;
	}

	mcp->name = name;
	mcp->data = clk_data;
	if (base)
		mcp->base = hwccf_remap(base);

	return 0;
}

static int32_t mtk_clk_get_base(struct mtk_clk *mck)
{
	unsigned int pvd_id = PVD_IDX(mck->id);

	if (mck->id < 0 || pvd_id >= NR_PROVIDER)
		return -EINVAL;

	return mtk_clk_providers[pvd_id].base;
}

void mtk_clk_disable_unused(void)
{
	int i, j;

	CCF_LOG("%s\n", __func__);

	for (i = NR_PROVIDER; i > 0; i--) {
		struct mtk_clk_data *clk_data = mtk_clk_providers[i - 1].data;

		if (!clk_data)
			continue;

		for (j = clk_data->nclks; j > 0; j--) {
			struct mtk_clk *mck = clk_data->mcks[j - 1];
			int is_enabled = 0;

			if (!mck)
				continue;

			if (mck->ops->is_enabled != NULL)
				is_enabled = mck->ops->is_enabled(mck);

			if (is_enabled > 0 && mck->enable_cnt == 0 &&
					mck->ops->disable)
				mck->ops->disable(mck);
		}
	}
}

/* fixed-rate clock support */

static unsigned long
mtk_clk_fixed_rate_recalc_rate(struct mtk_clk *mck, unsigned long parent_rate)
{
	struct mtk_clk_fixed_rate *fixed = fixed_from_mtk_clk(mck);

	return fixed->rate;
}

const struct mtk_clk_ops mtk_clk_fixed_rate_ops = {
	.recalc_rate	= mtk_clk_fixed_rate_recalc_rate,
};

/* fixed-factor clock support */

static unsigned long
mtk_clk_fixed_factor_recalc_rate(struct mtk_clk *mck, unsigned long parent_rate)
{
	struct mtk_clk_fixed_factor *factor = factor_from_mtk_clk(mck);
	unsigned long long r = (unsigned long long)parent_rate * factor->mult;

	r /= factor->div;

	return (unsigned long)r;
}

static long mtk_clk_fixed_factor_round_rate(struct mtk_clk *mck, unsigned long rate,
			unsigned long *parent_rate)
{
	struct mtk_clk_fixed_factor *factor = factor_from_mtk_clk(mck);

	*parent_rate = (rate / factor->mult) * factor->div;

	return *parent_rate / factor->div * factor->mult;
}

static int mtk_clk_fixed_factor_set_rate(struct mtk_clk *mck, unsigned long rate,
			unsigned long parent_rate)
{
	return 0;
}

const struct mtk_clk_ops mtk_clk_fixed_factor_ops = {
	.recalc_rate	= mtk_clk_fixed_factor_recalc_rate,
	.round_rate	= mtk_clk_fixed_factor_round_rate,
	.set_rate	= mtk_clk_fixed_factor_set_rate,
};

/* clock gate support */

static int mtk_clk_gate_set(struct mtk_clk_gate *cg, bool has_set)
{
	int32_t base;

	if (cg == NULL || cg->regs == NULL)
		return -EINVAL;

	base = mtk_clk_get_base(mtk_clk_from_gate(cg));
	if (base < 0)
		return base;

	if (has_set)
		hwccf_write(base + cg->regs->set_ofs, BIT(cg->shift));
	else {
		uint32_t addr = base + cg->regs->sta_ofs;

		hwccf_write(addr, hwccf_read(addr) | BIT(cg->shift));
	}

	return 0;
}

static int mtk_clk_gate_clr(struct mtk_clk_gate *cg, bool has_clr)
{
	int32_t base;

	if (cg == NULL || cg->regs == NULL)
		return -EINVAL;

	base = mtk_clk_get_base(mtk_clk_from_gate(cg));
	if (base < 0)
		return base;

	if (has_clr)
		hwccf_write(base + cg->regs->clr_ofs, BIT(cg->shift));
	else {
		uint32_t addr = base + cg->regs->sta_ofs;

		hwccf_write(addr, hwccf_read(addr)& ~BIT(cg->shift));
	}

	return 0;
}

static bool mtk_clk_gate_is_set(struct mtk_clk_gate *cg)
{
	int32_t base;
	uint32_t r;

	if (cg == NULL || cg->regs == NULL)
		return -EINVAL;

	base = mtk_clk_get_base(mtk_clk_from_gate(cg));
	if (base < 0)
		return false;

	r = hwccf_read(base + cg->regs->sta_ofs);

	return !!(r & BIT(cg->shift));
}

static int mtk_clk_gate_enable(struct mtk_clk *mck)
{
	return mtk_clk_gate_clr(gate_from_mtk_clk(mck), false);
}

static int mtk_clk_gate_disable(struct mtk_clk *mck)
{
	return mtk_clk_gate_set(gate_from_mtk_clk(mck), false);
}

static int mtk_clk_gate_is_enabled(struct mtk_clk *mck)
{
	return !mtk_clk_gate_is_set(gate_from_mtk_clk(mck));
}

static int mtk_clk_gate_inv_enable(struct mtk_clk *mck)
{
	return mtk_clk_gate_set(gate_from_mtk_clk(mck), false);
}

static int mtk_clk_gate_inv_disable(struct mtk_clk *mck)
{
	return mtk_clk_gate_clr(gate_from_mtk_clk(mck), false);
}

static int mtk_clk_gate_inv_is_enabled(struct mtk_clk *mck)
{
	return mtk_clk_gate_is_set(gate_from_mtk_clk(mck));
}

static int mtk_clk_gate_setclr_enable(struct mtk_clk *mck)
{
	return mtk_clk_gate_clr(gate_from_mtk_clk(mck), true);
}

static int mtk_clk_gate_setclr_disable(struct mtk_clk *mck)
{
	return mtk_clk_gate_set(gate_from_mtk_clk(mck), true);
}

static int mtk_clk_gate_setclr_inv_enable(struct mtk_clk *mck)
{
	return mtk_clk_gate_set(gate_from_mtk_clk(mck), true);
}

static int  mtk_clk_gate_setclr_inv_disable(struct mtk_clk *mck)
{
	return mtk_clk_gate_clr(gate_from_mtk_clk(mck), true);
}

const struct mtk_clk_ops mtk_clk_gate_ops = {
	.enable		= mtk_clk_gate_enable,
	.disable	= mtk_clk_gate_disable,
	.is_enabled	= mtk_clk_gate_is_enabled,
};

const struct mtk_clk_ops mtk_clk_gate_inv_ops = {
	.enable		= mtk_clk_gate_inv_enable,
	.disable	= mtk_clk_gate_inv_disable,
	.is_enabled	= mtk_clk_gate_inv_is_enabled,
};

const struct mtk_clk_ops mtk_clk_gate_setclr_ops = {
	.enable		= mtk_clk_gate_setclr_enable,
	.disable	= mtk_clk_gate_setclr_disable,
	.is_enabled	= mtk_clk_gate_is_enabled,
};

const struct mtk_clk_ops mtk_clk_gate_setclr_inv_ops = {
	.enable		= mtk_clk_gate_setclr_inv_enable,
	.disable	= mtk_clk_gate_setclr_inv_disable,
	.is_enabled	= mtk_clk_gate_inv_is_enabled,
};

/* MUX support */
static int mtk_clk_mux_enable_setclr(struct mtk_clk *mck)
{
	struct mtk_clk_mux *mux = mux_from_mtk_clk(mck);
	uint32_t mux_clr_addr;
	int32_t base;
	uint32_t val;

	CCF_LOG("%s: %s\n", __func__, mck->name?mck->name:"");

	if (mux->gate_shift == INV_BIT) {
		CCF_LOG("pdn bit is INV_BIT\n");
		return 0;
	}

	base = mtk_clk_get_base(mck);
	if (base < 0)
		return base;

	mux_clr_addr = base + mux->mux_clr_ofs;

	val = BIT(mux->gate_shift);
	hwccf_write(mux_clr_addr, val);

	/* For AOC 2.0, set UPDATE bit after MUX enable
	   even though mux_parent no change */
	if (mux->upd_shift >= 0)
		hwccf_write(base + mux->upd_ofs, BIT(mux->upd_shift));

	return 0;
}

static int mtk_clk_mux_disable_setclr(struct mtk_clk *mck)
{
	struct mtk_clk_mux *mux = mux_from_mtk_clk(mck);
	uint32_t mux_set_addr;
	int32_t base;
	uint32_t val;

	CCF_LOG("%s: %s\n", __func__, mck->name?mck->name:"");

	if (mux->gate_shift == INV_BIT) {
		CCF_LOG("pdn bit is INV_BIT\n");
		return 0;
	}

	base = mtk_clk_get_base(mck);
	if (base < 0)
		return base;

	mux_set_addr = base + mux->mux_set_ofs;

	val = BIT(mux->gate_shift);
	hwccf_write(mux_set_addr, val);

	return 0;
}

static int mtk_clk_mux_is_enabled(struct mtk_clk *mck)
{
	struct mtk_clk_mux *mux = mux_from_mtk_clk(mck);
	uint32_t mux_addr;
	int32_t base;
	uint32_t val;

	CCF_LOG("%s\n", __func__);

	base = mtk_clk_get_base(mck);
	if (base < 0)
		return base;

	mux_addr = base + mux->mux_ofs;

	if (mux->gate_shift < 0)
		return true;

	val = hwccf_read(mux_addr);

	return (val & BIT(mux->gate_shift)) == 0;
}

static int mtk_clk_mux_set_parent(struct mtk_clk *mck, uint8_t index)
{
	struct mtk_clk_mux *mux = mux_from_mtk_clk(mck);
	uint32_t mask = GENMASK(mux->mux_width - 1, 0);
	uint32_t val, orig;
	uint32_t mux_addr;
	int32_t base;

	CCF_LOG("%s(%s, %d)\n", __func__, mck->name?mck->name:"", index);

	base = mtk_clk_get_base(mck);
	if (base < 0)
		return base;

	mux_addr = base + mux->mux_ofs;

	if (index >= mck->num_parents) {
		CCF_LOG("index(%d) >= mck->num_parents(%d)\n", index, mck->num_parents);
		return -EINVAL;
	}

	if (mck->flags & MTK_CLK_MUX_INDEX_BIT)
		index = 1 << index;

	val = hwccf_read(mux_addr);
	orig = val;

	val &= ~(mask << mux->mux_shift);
	val |= index << mux->mux_shift;

	if (val != orig)
		hwccf_write(mux_addr, val);

	return 0;
}

static int mtk_clk_mux_set_parent_setclr(struct mtk_clk *mck, uint8_t index)
{
	struct mtk_clk_mux *mux = mux_from_mtk_clk(mck);
	uint32_t mask = GENMASK(mux->mux_width - 1, 0);
	uint32_t val, orig;
	int32_t base;

	CCF_LOG("%s(%s, %d)\n", __func__, mck->name?mck->name:"", index);

	base = mtk_clk_get_base(mck);
	if (base < 0)
		return base;

	val = hwccf_read(base + mux->mux_ofs);
	orig = val;
	val &= ~(mask << mux->mux_shift);
	val |= index << mux->mux_shift;

	if (val != orig) {
		val = (mask << mux->mux_shift);
		hwccf_write(base + mux->mux_clr_ofs, val);

		val = (index << mux->mux_shift);
		hwccf_write(base + mux->mux_set_ofs, val);
	}

	/* For AOC 2.0, set UPDATE bit after MUX enable
	   even though mux_parent no change */
	if (mux->upd_shift >= 0)
		hwccf_write(base + mux->upd_ofs, BIT(mux->upd_shift));

	return 0;
}

static uint8_t mtk_clk_mux_get_parent(struct mtk_clk *mck)
{
	struct mtk_clk_mux *mux = mux_from_mtk_clk(mck);
	uint32_t mask = GENMASK(mux->mux_width - 1, 0);
	uint32_t mux_addr;
	int32_t base;
	uint32_t val;

	CCF_LOG("%s: %s\n", __func__, mck->name?mck->name:"");

	base = mtk_clk_get_base(mck);
	if (base < 0)
		return base;

	mux_addr = base + mux->mux_ofs;

	val = hwccf_read(mux_addr);
	val >>= mux->mux_shift;
	val &= mask;
	CCF_LOG("%s: val=0x%x\n", __func__, val);

	if (val && (mck->flags & MTK_CLK_MUX_INDEX_BIT)) {
		val = ffs(val) - 1;
		CCF_LOG("val = ffs(val) - 1 => 0x%x\n", __func__, val);
	}

	if (val >= mck->num_parents)
		return -EEXCEDE;

	return val;
}

const struct mtk_clk_ops mtk_clk_mux_ops = {
	.set_parent	= mtk_clk_mux_set_parent,
	.get_parent	= mtk_clk_mux_get_parent,
};

const struct mtk_clk_ops mtk_clk_mux_clr_set_upd_ops = {
	.enable = mtk_clk_mux_enable_setclr,
	.disable = mtk_clk_mux_disable_setclr,
	.is_enabled = mtk_clk_mux_is_enabled,
	.set_parent = mtk_clk_mux_set_parent_setclr,
	.get_parent = mtk_clk_mux_get_parent,
};

/* PLL support */

#define CON0_BASE_EN		BIT(0)
#define CON0_PWR_ON		BIT(0)
#define CON0_ISO_EN		BIT(1)
#define CON1_PCW_CHG		BIT(31)

#define POSTDIV_MASK		0x7
#define INTEGER_BITS		7

static int mtk_clk_pll_enable(struct mtk_clk *mck)
{
	struct mtk_clk_pll *pll = pll_from_mtk_clk(mck);
	uint32_t pwr_addr;
	uint32_t en_addr;
	uint32_t tuner_en_addr;
	uint32_t rst_addr;
	int32_t base;
	int r;

	CCF_LOG("%s\n", __func__, mck->name?mck->name:"");

	base = mtk_clk_get_base(mck);
	if (base < 0)
		return base;

	pwr_addr = base + pll->pwr_ofs;
	en_addr = base + pll->en_ofs;
	tuner_en_addr = base + pll->tuner_en_ofs;
	rst_addr = base + pll->rst_bar_ofs;

	if (pll->pwron_bit > 0 && pll->pwron_bit <= 31) {
		r = hwccf_read(pwr_addr) | BIT(pll->pwron_bit);
		hwccf_write(pwr_addr, r);
	} else {
		r = hwccf_read(pwr_addr) | CON0_PWR_ON;
		hwccf_write(pwr_addr, r);
	}
	udelay(1);

	if (pll->iso_bit > 0 && pll->iso_bit <= 31) {
		r = hwccf_read(pwr_addr) & ~BIT(pll->iso_bit);
		hwccf_write(pwr_addr, r);
	} else {
		r = hwccf_read(pwr_addr) & ~CON0_ISO_EN;
		hwccf_write(pwr_addr, r);
	}
	udelay(1);

	r = hwccf_read(en_addr);
	r |= BIT(pll->en_bit);
	hwccf_write(en_addr, r);

	if (pll->tuner_en_ofs) {
		r = hwccf_read(tuner_en_addr);
		r |= BIT(pll->tuner_en_bit);
		hwccf_write(tuner_en_addr, r);
	}

	udelay(20);

	if (pll->rst_bar_ofs) {
		r = hwccf_read(rst_addr);
		r |= pll->rst_bar_mask;
		hwccf_write(rst_addr, r);
	}

	return 0;
}

static int mtk_clk_pll_disable(struct mtk_clk *mck)
{
	struct mtk_clk_pll *pll = pll_from_mtk_clk(mck);
	uint32_t pwr_addr;
	uint32_t en_addr;
	uint32_t tuner_en_addr;
	uint32_t rst_addr;
	int32_t base;
	uint32_t r;

	CCF_LOG("%s\n", __func__, mck->name?mck->name:"");

	base = mtk_clk_get_base(mck);
	if (base < 0)
		return base;

	pwr_addr = base + pll->pwr_ofs;
	en_addr = base + pll->en_ofs;
	tuner_en_addr = base + pll->tuner_en_ofs;
	rst_addr = base + pll->rst_bar_ofs;

	if (pll->rst_bar_ofs) {
		r = hwccf_read(rst_addr);
		r &= ~pll->rst_bar_mask;
		hwccf_write(rst_addr, r);
	}

	if (pll->tuner_en_ofs) {
		r = hwccf_read(tuner_en_addr);
		r &= ~BIT(pll->tuner_en_bit);
		hwccf_write(tuner_en_addr, r);
	}

	r = hwccf_read(en_addr);
	r &= ~BIT(pll->en_bit);
	hwccf_write(en_addr, r);

	if (pll->iso_bit > 0 && pll->iso_bit <= 31) {
		r = hwccf_read(pwr_addr) | BIT(pll->iso_bit);
		hwccf_write(pwr_addr, r);
	} else {
		r = hwccf_read(pwr_addr) | CON0_ISO_EN;
		hwccf_write(pwr_addr, r);
	}

	if (pll->pwron_bit > 0 && pll->pwron_bit <= 31) {
		r = hwccf_read(pwr_addr) & ~BIT(pll->pwron_bit);
		hwccf_write(pwr_addr, r);
	} else {
		r = hwccf_read(pwr_addr) & ~CON0_PWR_ON;
		hwccf_write(pwr_addr, r);
	}

	return 0;
}

static int mtk_clk_pll_is_enabled(struct mtk_clk *mck)
{
	struct mtk_clk_pll *pll = pll_from_mtk_clk(mck);
	uint32_t en_addr;
	int32_t base;

	base = mtk_clk_get_base(mck);
	if (base < 0)
		return base;

	en_addr = base + pll->en_ofs;

	return (hwccf_read(en_addr) & BIT(pll->en_bit)) != 0;
}

static unsigned long __mtk_pll_recalc_rate(struct mtk_clk_pll *pll, uint32_t fin,
		uint32_t pcw, int postdiv)
{
	int pcwbits = pll->pcwbits;
	int pcwfbits;
	int ibits;
	uint64_t vco;
	uint8_t c = 0;

	/* The fractional part of the PLL divider. */
	ibits = pll->pcwibits? pll->pcwibits : INTEGER_BITS;
	pcwfbits = pcwbits > ibits ? pcwbits - ibits : 0;

	vco = (uint64_t)fin * pcw;

	if (pcwfbits && (vco & GENMASK(pcwfbits - 1, 0)))
		c = 1;

	vco >>= pcwfbits;

	if (c)
		vco++;

	return ((unsigned long)vco + postdiv - 1) / postdiv;
}

static void mtk_pll_set_rate_regs(struct mtk_clk_pll *pll, uint32_t pcw,
		int postdiv, uint32_t base)
{
	uint32_t val, chg;
	uint32_t tuner_en = 0;
	uint32_t tuner_en_mask = BIT(pll->tuner_en_bit);
	uint32_t tuner_en_addr = base + pll->tuner_en_ofs;
	uint32_t pd_addr = base + pll->pd_ofs;
	uint32_t pcw_addr = base + pll->pcw_ofs;
	uint32_t pcw_chg_addr = base + pll->pcw_chg_ofs;

	/* disable tuner */
	if (pll->tuner_en_ofs) {
		val = hwccf_read(tuner_en_addr);
		tuner_en = val & tuner_en_mask;

		if (tuner_en) {
			val &= ~tuner_en_mask;
			hwccf_write(tuner_en_addr, val);
		}
	}

	/* set postdiv */
	val = hwccf_read(pd_addr);
	val &= ~(POSTDIV_MASK << pll->pd_shift);
	val |= (ffs(postdiv) - 1) << pll->pd_shift;

	/* postdiv and pcw need to set at the same time if on same register */
	if (pll->pd_ofs != pll->pcw_ofs) {
		hwccf_write(pd_addr, val);
		val = hwccf_read(pcw_addr);
	}

	/* set pcw */
	val &= ~GENMASK(pll->pcw_shift + pll->pcwbits - 1,
			pll->pcw_shift);
	val |= pcw << pll->pcw_shift;

	if (pll->tuner_ofs)
		hwccf_write(tuner_en_addr, val + 1);

	if (pll->pcw_chg_ofs) {
		chg = hwccf_read(pcw_chg_addr);
		chg |= (uint32_t)CON1_PCW_CHG;
		hwccf_write(pcw_addr, val);
		hwccf_write(pcw_chg_addr, chg);
	} else {
		val |= (uint32_t)CON1_PCW_CHG;
		hwccf_write(pcw_addr, val);
	}

	/* restore tuner_en */
	if (pll->tuner_en_ofs && tuner_en) {
		val = hwccf_read(tuner_en_addr);
		val |= tuner_en_mask;
		hwccf_write(tuner_en_addr, val);
	}

	udelay(20);
}

/*
 * mtk_pll_calc_values - calculate good values for a given input frequency.
 * @pll:	The pll
 * @pcw:	The pcw value (output)
 * @postdiv:	The post divider (output)
 * @freq:	The desired target frequency
 * @fin:	The input frequency
 *
 */
static void
mtk_pll_calc_values(struct mtk_clk_pll *pll, uint32_t *pcw, uint32_t *postdiv,
		uint32_t freq, uint32_t fin)
{
	unsigned long fmin = pll->fmin ? pll->fmin : 1000 * MHZ;
	uint64_t _pcw;
	uint32_t val;
	int ibits;

	CCF_LOG("%s(freq=%u)\n", __func__, freq);

	if (freq > pll->fmax)
		freq = pll->fmax;

	for (val = 0; val < 5; val++) {
		*postdiv = 1 << val;
		if ((uint64_t)freq * *postdiv >= fmin)
			break;
	}

	/* _pcw = freq * postdiv / fin * 2^pcwfbits */
	ibits = pll->pcwibits ? pll->pcwibits : INTEGER_BITS;
	_pcw = ((uint64_t)freq << val) << (pll->pcwbits - ibits);
	_pcw /= fin;	// do_div(_pcw, fin);

	*pcw = (uint32_t)_pcw;
}

static unsigned long mtk_clk_pll_recalc_rate(struct mtk_clk *mck, unsigned long parent_rate)
{
	struct mtk_clk_pll *pll = pll_from_mtk_clk(mck);
	uint32_t postdiv;
	uint32_t pcw;
	uint32_t pd_addr;
	uint32_t pcw_addr;
	int32_t base;

	CCF_LOG("%s: %s: parent_rate=%u\n",
			__func__, mck->name?mck->name:"", parent_rate);

	base = mtk_clk_get_base(mck);
	if (base < 0)
		return base;

	pd_addr = base + pll->pd_ofs;
	pcw_addr = base + pll->pcw_ofs;

	postdiv = hwccf_read(pd_addr);
	postdiv = (postdiv >> pll->pd_shift) & POSTDIV_MASK;
	postdiv = 1 << postdiv;

	pcw = hwccf_read(pcw_addr);
	pcw = pcw >> pll->pcw_shift;
	pcw &= GENMASK(pll->pcwbits - 1, 0);

	return __mtk_pll_recalc_rate(pll, parent_rate, pcw, postdiv);
}

static long mtk_clk_pll_round_rate(struct mtk_clk *mck, unsigned long rate,
			unsigned long *parent_rate)
{
	struct mtk_clk_pll *pll = pll_from_mtk_clk(mck);
	uint32_t pcw = 0;
	uint32_t postdiv;

	mtk_pll_calc_values(pll, &pcw, &postdiv, rate, *parent_rate);

	return __mtk_pll_recalc_rate(pll, *parent_rate, pcw, postdiv);
}

static int mtk_clk_pll_set_rate(struct mtk_clk *mck, unsigned long rate,
			unsigned long parent_rate)
{
	struct mtk_clk_pll *pll = pll_from_mtk_clk(mck);
	uint32_t pcw = 0;
	uint32_t postdiv;
	int32_t base;

	base = mtk_clk_get_base(mck);
	if (base < 0)
		return base;

	if (rate == 0)
		return -EINVAL;

	if (parent_rate == 0)
		parent_rate = 26000000;

	mtk_pll_calc_values(pll, &pcw, &postdiv, rate, parent_rate);
	mtk_pll_set_rate_regs(pll, pcw, postdiv, base);

	return 0;
}

const struct mtk_clk_ops mtk_clk_pll_ops = {
	.prepare	= mtk_clk_pll_enable,
	.unprepare	= mtk_clk_pll_disable,
	.is_prepared	= mtk_clk_pll_is_enabled,
	.recalc_rate	= mtk_clk_pll_recalc_rate,
	.round_rate	= mtk_clk_pll_round_rate,
	.set_rate	= mtk_clk_pll_set_rate,
};

/* divider support */

#define DIV_ROUND_UP(n,d)	(((n) + (d) - 1) / (d))
#define div_mask(width)		((1 << (width)) - 1)

static uint32_t calc_div(struct mtk_clk_div *div, unsigned long rate, unsigned long parent_rate)
{
	uint32_t d, maxd;

	if (rate == 0)
		rate = 1;

	maxd = div_mask(div->div_width) + 1;
	maxd = min(ULONG_MAX / rate, maxd);

	d = DIV_ROUND_UP(parent_rate, rate);

	d = max(1, d);
	d = min(maxd, d);

	return d;
}

static unsigned long mtk_clk_div_recalc_rate(struct mtk_clk *mck, unsigned long parent_rate)
{
	struct mtk_clk_div *div = div_from_mtk_clk(mck);
	uint32_t d;
	uint32_t addr;
	int32_t base;

	base = mtk_clk_get_base(mck);
	if (base < 0)
		return base;

	addr = base + div->div_ofs;

	d = hwccf_read(addr);
	d = (d >> div->div_shift) & div_mask(div->div_width);
	d = d + 1;

	return DIV_ROUND_UP(parent_rate, d);
}

static long mtk_clk_div_round_rate(struct mtk_clk *mck, unsigned long rate,
			unsigned long *parent_rate)
{
	struct mtk_clk_div *div = div_from_mtk_clk(mck);
	uint32_t d = calc_div(div, rate, *parent_rate);

	return DIV_ROUND_UP(*parent_rate, d);
}

static int mtk_clk_div_set_rate(struct mtk_clk *mck, unsigned long rate,
			unsigned long parent_rate)
{
	struct mtk_clk_div *div = div_from_mtk_clk(mck);
	uint32_t d = calc_div(div, rate, parent_rate) - 1;
	uint32_t mask = div_mask(div->div_width);
	uint32_t val, orig;
	uint32_t addr;
	int32_t base;

	base = mtk_clk_get_base(mck);
	if (base < 0)
		return base;

	addr = base + div->div_ofs;

	if (rate == 0 || parent_rate == 0)
		return -EINVAL;

	val = hwccf_read(addr);
	orig = val;

	val &= ~(mask << div->div_shift);
	val |= d << div->div_shift;

	if (val != orig)
		hwccf_write(addr, val);

	return 0;
}

const struct mtk_clk_ops mtk_clk_div_ops = {
	.recalc_rate	= mtk_clk_div_recalc_rate,
	.round_rate	= mtk_clk_div_round_rate,
	.set_rate	= mtk_clk_div_set_rate,
};

/* PDCK support */
static int mtk_clk_pdck_enable(struct mtk_clk *mck)
{
	struct mtk_clk_pdck *pdck = pdck_from_mtk_clk(mck);
	int pvd_id = PVD_IDX(mck->id);
	uint32_t ret = 0;
	uint8_t i;

	CCF_LOG("%s\n", __func__);

	if (mck->id < 0 || pvd_id >= NR_PROVIDER)
		return -EINVAL;

	for (i = 0; i < pdck->clk_num; i++) {
		struct clk *clk;
		struct mtk_clk *pdck_mck = NULL;

		clk = clk_get(pdck->clks[i]);
		if (!clk) {
			PRINTF_E("[%s]pdck get clk[%d] fail\n", mck->name, i);
			continue;
		}

		pdck_mck = mck_from_clk(clk);
		if (!pdck_mck) {
			PRINTF_E("[%s]pdck get mck[%d] fail\n", mck->name, i);
			continue;
		}

		ret = mtk_clk_enable_nolock(pdck_mck);
		if (ret)
			PRINTF_E("[%s]pdck enable clk[%d] fail\n", mck->name, i);
	}

	return 0;
}

static int mtk_clk_pdck_disable(struct mtk_clk *mck)
{
	struct mtk_clk_pdck *pdck = pdck_from_mtk_clk(mck);
	int pvd_id = PVD_IDX(mck->id);
	uint32_t ret = 0;
	uint8_t i;

	CCF_LOG("%s\n", __func__);

	if (mck->id < 0 || pvd_id >= NR_PROVIDER)
		return -EINVAL;

	for (i = 0; i < pdck->clk_num; i++) {
		struct clk *clk;
		struct mtk_clk *pdck_mck = NULL;

		clk = clk_get(pdck->clks[i]);
		if (!clk) {
			PRINTF_E("[%s]pdck get clk[%d] fail\n", mck->name, i);
			continue;
		}

		pdck_mck = mck_from_clk(clk);
		if (!pdck_mck) {
			PRINTF_E("[%s]pdck get mck[%d] fail\n", mck->name, i);
			continue;
		}

		ret = mtk_clk_disable_nolock(pdck_mck);
		if (ret)
			PRINTF_E("[%s]pdck disable clk[%d] fail\n", mck->name, i);
	}

	return 0;
}

const struct mtk_clk_ops mtk_clk_pdck_ops = {
	.enable		= mtk_clk_pdck_enable,
	.disable	= mtk_clk_pdck_disable,
};

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

#ifndef _MTK_PD_H_
#define _MTK_PD_H_

#include <stdbool.h>

struct pd;
struct mtk_pd;
struct mtk_pd_sram_ops;
struct mtk_pd_ops;

struct other_pd_callbacks {
	int (*power_on)(void);
	int (*power_off)(void);
};

/**
 * mtk_pd_get - lookup and obtain a reference to a power domain producer.
 * @id: power domain index
 * @usr_id: string to indicate user
 *
 * Returns a struct mtk_pd corresponding to the power domain producer, or
 * valid error number if failed. User can call mtk_pd_put to release the
 * memory to avoid leak.
 */
struct pd *mtk_pd_get(int id);

/**
 * is_err_or_null_pd - check the mtk_pd is available or not
 * @pd: the user power domain handle
 *
 * Returns true if the pd is invalid or NULL pointer.
 */
bool is_err_or_null_pd(struct pd *pd);

/**
 * mtk_pd_register_sram_ops - register sram callback to customize
 * sram operation
 * @pd: the user power domain handle
 * @ops: operands, customize power on/off here.
 * Must not be called from isr.
 */
void mtk_pd_register_sram_ops(struct mtk_pd *pd,
		struct mtk_pd_sram_ops *sram_ops);

/**
 * mtk_pd_register_pwr_ops - register operands callback to customize power handle flow.
 * @pd: the user power domain handle
 * @ops: operands, customize power on/off and is_on here.
 * Must not be called from isr.
 */
void mtk_pd_register_pwr_ops(struct mtk_pd *pd,
		struct mtk_pd_ops *ops);

/**
 * mtk_pd_enable - power on a power domain.
 * @mtk_pd: the user power domain handle
 * Must not be called from isr.
 */
int mtk_pd_enable(struct pd *pd);

/**
 * mtk_pd_disable - power off a power domain.
 * @mtk_pd: the user power domain handle
 * Must not be called from isr.
 */
int mtk_pd_disable(struct pd *pd);

/**
 * mtk_pd_is_enabled - check power domain status.
 * @mtk_pd: the user power domain handle
 * Return true for power on, false for power off.
 */
int mtk_pd_is_enabled(struct pd *pd);

/**
 * mtk_pd_disable_unused - check power domain disable state
 * then disable unused power.
 */
void mtk_pd_disable_unused(void);

#endif /* _MTK_PD_H_ */

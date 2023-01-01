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

#ifndef _CCF_COMMON_H_
#define _CCF_COMMON_H_

#include <stdint.h>
#include <sys/types.h>

#define ARRAY_SIZE(x)	   (sizeof(x) / sizeof(x[0]))
#define BIT(nr)			 (1 << (nr))
#define GENMASK(h, l)	   (((1U << ((h) - (l) + 1)) - 1) << (l))

#define ENODATA		1	/* no clock data */
#define EINVPVD		2	/* no provider id */
#define EINVCKD		3	/* no clk id */
#define EPVDOBD		4	/* provider id out of bound */
#define EINVSTEP	5	/* invalid step */
#define EINVPD		6	/* invalid PD index */
#define EPDNORDY	7	/* PDs is not register */
#define EPDOBD		8	/* PD id out of bound */
#define ENOMEM		12	/* Out of memory */
#define ENODEV		19	/* No such device */
#define EINVAL		22	/* Invalid argument */
#define EDEFER		23	/* Provider not ready */
#define EEXCEDE		24	/* Excede max mux id */
#define ETIMEDOUT	25	/* Connection timed out */
#define MAX_ERRNO	30	/* Max error number for clk */

/* Regular Number Definition */
#define INV_OFS	-1
#define INV_BIT	-1
#define INV_ID	-1

#ifndef container_of
/**
 * container_of - cast a member of a structure out to the containing structure
 * @ptr:	the pointer to the member.
 * @type:	the type of the container struct this is embedded in.
 * @member:	the name of the member within the struct.
 *
 */
#define container_of(ptr, type, member) ({			\
	const typeof(((type *)0)->member) * __mptr = (ptr);	\
	(type *)((char *)__mptr - offsetof(type, member)); })
#endif

static inline int ffs(int x)
{
	int r = 1;

	if (!x)
		return 0;
	if (!(x & 0xffff)) {
		x >>= 16;
		r += 16;
	}
	if (!(x & 0xff)) {
		x >>= 8;
		r += 8;
	}
	if (!(x & 0xf)) {
		x >>= 4;
		r += 4;
	}
	if (!(x & 3)) {
		x >>= 2;
		r += 2;
	}
	if (!(x & 1)) {
		x >>= 1;
		r += 1;
	}
	return r;
}

#define min(a, b)			((a) < (b)) ? (a) : (b)
#define max(a, b)			((a) < (b)) ? (b) : (a)
#define MHZ				(1000 * 1000)

#endif /* _CCF_COMMON_H_ */

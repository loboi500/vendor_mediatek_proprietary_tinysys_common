#ifndef AUDIO_KAL_CACHE_H
#define AUDIO_KAL_CACHE_H

/* os layer porting layer*/
#include <FreeRTOS.h>
#include <semphr.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#if defined(CFG_SCP_AUDIO_FW_SUPPORT)
#include <encoding.h>
#endif

/* cache hal */
#if !defined(CFG_CACHE_SUPPORT)

#define CACHE_FLUSH(addr, size)
#define CACHE_INVALIDATE(addr, size)

#elif defined(CFG_MTK_AUDIODSP_SUPPORT)

#define CACHE_FLUSH(addr, size) \
	do {  \
		if ((((uint32_t)addr) & (ADSP_CACHE_ALIGN_BYTES - 1)) != 0) { \
			pr_notice("%p fail", addr); \
			AUD_WARNING("cache not align!!"); \
		} \
		hal_cache_region_operation(HAL_CACHE_TYPE_DATA, \
					   HAL_CACHE_OPERATION_FLUSH, \
					   (void *)(addr), \
					   (size)); \
	} while (0)

#define CACHE_INVALIDATE(addr, size) \
	do {  \
		if ((((uint32_t)addr) & (ADSP_CACHE_ALIGN_BYTES - 1)) != 0) { \
			pr_notice("%p fail", addr); \
			AUD_WARNING("cache not align!!"); \
		} \
		hal_cache_region_operation(HAL_CACHE_TYPE_DATA, \
					   HAL_CACHE_OPERATION_INVALIDATE, \
					   (void *)(addr), \
					   (size)); \
	} while (0)

#elif defined(CFG_SCP_AUDIO_FW_SUPPORT)

#define HAL_DO_BYTE_ALIGN(value, mask) (((value) + mask) & (~mask))
#define CACHE_FLUSH(addr, size) \
	do {  \
		uint32_t size_align = HAL_DO_BYTE_ALIGN(size, CACHE_LINE_MASK); \
		pr_notice("addr[0x%x] CACHE_FLUSH size_align[%u] size[%u]", (uint32_t)(addr), size_align, size); \
		mrv_dcache_flush_multi_addr((uint32_t)(addr), (size_align)); \
	} while (0)

#define CACHE_INVALIDATE(addr, size) \
	do {  \
		uint32_t size_align = HAL_DO_BYTE_ALIGN(size, CACHE_LINE_MASK); \
		pr_notice("addr[0x%x] CACHE_INVALIDATE size_align[%u] size[%u]", (uint32_t)(addr), size_align, size); \
		mrv_dcache_invalid_multi_addr((uint32_t)(addr), (size_align)); \
	} while (0)
#endif

#endif /* end of AUDIO_KAL_CACHE_H */


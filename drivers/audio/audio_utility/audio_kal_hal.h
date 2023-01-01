#ifndef AUDIO_KAL_HAL_H
#define AUDIO_KAL_HAL_H
/* os layer porting layer*/
#include <FreeRTOS.h>
#include <semphr.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <xgpt.h>
#include <audio_kal_cache.h>
#if defined(CFG_SCP_AUDIO_FW_SUPPORT)
#include <encoding.h>
#include <mt_alloc.h>
#endif
#if defined(CFG_SCP_AUDIO_FW_SUPPORT)
// define CFG_COMMON_MALLOC_ALIGN_SUPPORT means this tinysys has define
// specific malloc for alignment
#if defined(CFG_COMMON_MMALLOC_ALIGN_SUPPORT)
#define kal_pvPortMalloc(sz) aligned_malloc(sz, CACHE_LINE_SIZE)
#define kal_vPortFree(x) aligned_free(x)
#else
#define kal_pvPortMalloc(sz) pvPortMalloc(sz)
#define kal_vPortFree(x) vPortFree(x)
#endif
#define kal_xTaskCreate(x...) xTaskCreate(x)
#define kal_taskENTER_CRITICAL() taskENTER_CRITICAL()
#define kal_taskEXIT_CRITICAL() taskEXIT_CRITICAL()
#define kal_vTaskSuspendAll() vTaskSuspendAll()
#define kal_xTaskResumeAll() xTaskResumeAll()
#endif
// dram malloc API
#if defined(CFG_SCP_AUDIO_FW_SUPPORT)&&defined(CFG_DRAM_HEAP_SUPPORT)
#define kal_pvPortDramMalloc(sz) aligned_dram_malloc(sz, CACHE_LINE_SIZE)
#define kal_vPortDramFree(x) aligned_dram_free(x)
#else
#define kal_pvPortDramMalloc(sz) kal_pvPortMalloc(sz)
#define kal_vPortDramFree(x) kal_vPortFree(x)
#endif
// audio hal macro function,
// 1. scp using dram malloc
// 2. HIFI series using kal_pvPortMalloc, usualliy dram.
#if defined(CFG_SCP_AUDIO_FW_SUPPORT)
#define aud_kal_pvPortMalloc(sz) kal_pvPortDramMalloc(sz)
#define aud_kal_vPortFree(sz) kal_vPortDramFree(sz)
#else
#define aud_kal_pvPortMalloc(sz) kal_pvPortMalloc(sz)
#define aud_kal_vPortFree(sz) kal_vPortFree(sz)
#endif
/* get time stamp in ns */
uint64_t get_timestamp_ns(void);
#endif /* end of AUDIO_KAL_HAL_H */

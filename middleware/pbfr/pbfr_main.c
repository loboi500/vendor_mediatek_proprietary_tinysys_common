/* Copyright Statement:
 *
 * @2015 MediaTek Inc. All rights reserved.
 *
 * This software/firmware and related documentation ("MediaTek Software") are
 * protected under relevant copyright laws. The information contained herein
 * is confidential and proprietary to MediaTek Inc. and/or its licensors.
 * Without the prior written permission of MediaTek Inc. and/or its licensors,
 * any reproduction, modification, use or disclosure of MediaTek Software,
 * and information contained herein, in whole or in part, shall be strictly prohibited.
 *
 * BY OPENING THIS FILE, RECEIVER HEREBY UNEQUIVOCALLY ACKNOWLEDGES AND AGREES
 * THAT THE SOFTWARE/FIRMWARE AND ITS DOCUMENTATIONS ("MEDIATEK SOFTWARE")
 * RECEIVED FROM MEDIATEK AND/OR ITS REPRESENTATIVES ARE PROVIDED TO RECEIVER ON
 * AN "AS-IS" BASIS ONLY. MEDIATEK EXPRESSLY DISCLAIMS ANY AND ALL WARRANTIES,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE OR NONINFRINGEMENT.
 * NEITHER DOES MEDIATEK PROVIDE ANY WARRANTY WHATSOEVER WITH RESPECT TO THE
 * SOFTWARE OF ANY THIRD PARTY WHICH MAY BE USED BY, INCORPORATED IN, OR
 * SUPPLIED WITH THE MEDIATEK SOFTWARE, AND RECEIVER AGREES TO LOOK ONLY TO SUCH
 * THIRD PARTY FOR ANY WARRANTY CLAIM RELATING THERETO. RECEIVER EXPRESSLY ACKNOWLEDGES
 * THAT IT IS RECEIVER'S SOLE RESPONSIBILITY TO OBTAIN FROM ANY THIRD PARTY ALL PROPER LICENSES
 * CONTAINED IN MEDIATEK SOFTWARE. MEDIATEK SHALL ALSO NOT BE RESPONSIBLE FOR ANY MEDIATEK
 * SOFTWARE RELEASES MADE TO RECEIVER'S SPECIFICATION OR TO CONFORM TO A PARTICULAR
 * STANDARD OR OPEN FORUM. RECEIVER'S SOLE AND EXCLUSIVE REMEDY AND MEDIATEK'S ENTIRE AND
 * CUMULATIVE LIABILITY WITH RESPECT TO THE MEDIATEK SOFTWARE RELEASED HEREUNDER WILL BE,
 * AT MEDIATEK'S OPTION, TO REVISE OR REPLACE THE MEDIATEK SOFTWARE AT ISSUE,
 * OR REFUND ANY SOFTWARE LICENSE FEES OR SERVICE CHARGE PAID BY RECEIVER TO
 * MEDIATEK FOR SUCH MEDIATEK SOFTWARE.
 */

#include "FreeRTOS.h"
#include "portmacro.h"
#include "task.h"
#include "semphr.h"
#include "stdio.h"

#include "mt_pbfr.h"

#ifdef PBFR_SUPPORT_POLLING
#include "timers.h"
#ifdef PBFR_SUPPORT_POLLING_MS
#define POLLING_TIME PBFR_SUPPORT_POLLING_MS
#else
#define POLLING_TIME 100	/* polling every 100ms */
#endif
TimerHandle_t PBFRTimer;
#endif

#ifdef __clang__
#pragma clang diagnostic ignored "-Wcast-align"
#endif

#define PBFR_ERR_TASK_CREATE_OVERFLOW      0x00000001
#define PBFR_ERR_TASK_CREATE_NO            0x00000002
#define PBFR_ERR_TASK_SWITCH_OVERFLOW      0x00000004
#define PBFR_ERR_TASK_SWITCH_NO            0x00000008
#define PBFR_ERR_MASK                      0x0000FFFF
#define PBFR_STS_ON                        0x00010000
static unsigned int pbfr_status;
#ifdef PBFR_SUPPORT_PERF_BUDGET
static unsigned long long start_ns, stop_ns;
#endif /* PBFR_SUPPORT_PERF_BUDGET */

#ifndef portNUM_CPUS
#define portNUM_CPUS 1
#endif

unsigned long long last_switch_ns[portNUM_CPUS];
#ifdef PBFR_SUPPORT_CACHE_COUNT
unsigned long long last_cache_access_cnt_i;
unsigned long long last_cache_miss_cnt_i;
unsigned long long last_cache_access_cnt_d;
unsigned long long last_cache_miss_cnt_d;
#define cache_access_count vPortGetICacheAccessCount
#define cache_miss_count vPortGetICacheMissCount
extern void vPortEnableCacheCount(void);
extern void vPortDisableCacheCount(void);
extern unsigned long long vPortGetICacheAccessCount(void);
extern unsigned long long vPortGetICacheMissCount(void);
extern unsigned long long vPortGetDCacheAccessCount(void);
extern unsigned long long vPortGetDCacheMissCount(void);
extern void vPortHaltCacheCount(void);
extern void vPortResumeCacheCount(void);
#endif

#ifdef PBFR_SUPPORT_IOSTALL
extern void vPortEnableIOStallRate(void);
extern void vPortDisableIOStallRate(void);
extern int vPortGetIOStallRate(void);
#endif

struct task_loadinfo_t {
	unsigned long long runtime_ns;
#ifdef PBFR_SUPPORT_POLLING
	unsigned long long polling_ns;
	unsigned long long maxload_ns;
	int maxload;
#endif
#ifdef PBFR_SUPPORT_WORKSLOT_MONITOR
	unsigned int cnt1;
	unsigned int cnt2;
#endif
#ifdef PBFR_SUPPORT_CACHE_COUNT
	unsigned long long cache_access_cnt_i;
	unsigned long long cache_miss_cnt_i;
	unsigned long long cache_access_cnt_d;
	unsigned long long cache_miss_cnt_d;
#endif
};

struct task_info_t {
	unsigned int taskno: 8,
		     priority: 8,
		     watermark: 16;
	char name[configMAX_TASK_NAME_LEN];
};

struct task_info_t task_info[FREERTOS_MAX_TASKS];
struct task_loadinfo_t task_load[FREERTOS_MAX_TASKS];
static unsigned char freertos_task_no;
static unsigned char idle_task_idx[portNUM_CPUS];

#ifdef PBFR_SUPPORT_POLLING
static void pbfr_polling_cb(TimerHandle_t pxTimer)
{
	(void) pxTimer;
	int i, loading;
	unsigned long long time_ns, period_ns;

	time_ns = pbfr_get_timestamp(0);
	period_ns = POLLING_TIME * 1000000;
	vTaskSuspendAll();
	for(i = 0; i < freertos_task_no; i++) {
		loading = (int)((task_load[i].polling_ns * 10000) / period_ns);
		if (loading > 10000) {
			loading = 10000;
		}
		if (loading > task_load[i].maxload) {
			task_load[i].maxload = loading;
			task_load[i].maxload_ns = time_ns;
		}
		task_load[i].polling_ns = 0;
	}
	xTaskResumeAll();
}
#endif

int pbfr_init(void)
{
#ifdef PBFR_SUPPORT_FLIGHT_REC
	extern int pbfr_rec_init_buffer(void);
	pbfr_rec_init_buffer();
#endif
#ifdef PBFR_SUPPORT_POLLING
	PBFRTimer = xTimerCreate("PBFR", POLLING_TIME / portTICK_PERIOD_MS,
			pdTRUE, (void *) 0, pbfr_polling_cb);
#endif
	return 0;
}

#ifdef PBFR_SUPPORT_PERF_BUDGET
int pbfr_start_loadinfo(int mode)
{
	int i;
	(void) mode;

	/* only hart0 can start loading */
	if(uxPortGetHartId() != 0)
		return 0;

#ifdef PBFR_SUPPORT_IOSTALL
	vPortEnableIOStallRate();
#endif
	for (i = 0; i < freertos_task_no; i++) {
		memset(task_load, 0, sizeof(task_load));
	}

	stop_ns = 0;
	start_ns = pbfr_get_timestamp(is_in_isr()
			|| (xGetCriticalNesting() > 0));
	for(i = 0; i < portNUM_CPUS; i++)
		last_switch_ns[i] = start_ns;

#ifdef PBFR_SUPPORT_CACHE_COUNT
	vPortEnableCacheCount();
	vPortHaltCacheCount();
	last_cache_access_cnt_i = vPortGetICacheAccessCount();
	last_cache_miss_cnt_i = vPortGetICacheMissCount();
	last_cache_access_cnt_d = vPortGetDCacheAccessCount();
	last_cache_miss_cnt_d = vPortGetDCacheMissCount();
	vPortResumeCacheCount();
#endif

#ifdef PBFR_SUPPORT_POLLING
	xTimerStart(PBFRTimer, portMAX_DELAY);
#endif
	pbfr_status = PBFR_STS_ON;
	return 0;
}

int pbfr_stop_loadinfo(void)
{
	/* only hart0 can start loading */
	if(uxPortGetHartId() != 0)
		return 0;
#ifdef PBFR_SUPPORT_IOSTALL
	vPortDisableIOStallRate();
#endif
	pbfr_status &= ~PBFR_STS_ON;
#ifdef PBFR_SUPPORT_POLLING
	xTimerStop(PBFRTimer, portMAX_DELAY);
#endif
	stop_ns = pbfr_get_timestamp(is_in_isr()
			|| (xGetCriticalNesting() > 0));

	return 0;
}
#endif				/* PBFR_SUPPORT_PERF_BUDGET */

#ifdef PBFR_SUPPORT_TASK_CHECKER
struct task_checking_t {
#ifdef PBFR_SUPPORT_POLLING
	int peak_loading;
#endif
	int average_loading;
	int cpu_average_loading;
	unsigned int taskno;
};
struct task_checking_t checking, golden;

#ifdef CFG_PBFR_FLIGHT_REC_DEBUG
extern void pbfr_rec_show_records(int ISR);
#endif				/* CFG_PBFR_FLIGHT_REC_DEBUG */

static void pbfr_task_checking()
{
	int fail = 0;

#ifdef PBFR_SUPPORT_POLLING
	if (checking.peak_loading > golden.peak_loading) {
		fail = 1;

		printf("-- max loading fail: > %02d.%02d%%\n\r",
				golden.peak_loading / 100, golden.peak_loading % 100);
	}
#endif
	if (checking.average_loading > golden.average_loading) {
		fail = 1;

		printf("-- task average loading fail: > %02d.%02d%%\n\r",
				golden.average_loading / 100,
				golden.average_loading % 100);
	}

	if (checking.cpu_average_loading > golden.cpu_average_loading) {
		fail = 1;

		printf("-- cpu loading fail: > %02d.%02d%%\n\r",
				golden.cpu_average_loading / 100,
				golden.cpu_average_loading % 100);
	}

	if (fail) {
		printf("task checker: T%02d FAIL!!\n\r", golden.taskno);
		configASSERT(0);
	} else
		printf("task checker: T%02d PASS!!\n\r", golden.taskno);

}
#endif				/* PBFR_SUPPORT_TASK_CHECKER */

#ifdef PBFR_SUPPORT_PERF_BUDGET
int pbfr_report_loadinfo(int mode)
{
	(void) mode;
	int i, loading, cpu_load;
	unsigned long long time_ns;
#ifdef PBFR_SUPPORT_IOSTALL
	int IO_loading;
#endif
#ifdef PBFR_SUPPORT_CACHE_COUNT
	int cache_miss_rate_i;
	int cache_miss_rate_d;
#endif
#ifdef _misaligned_access
	/* miss aligned load/store should be counted at platform */
	extern unsigned int misalignedLoad;
	extern unsigned int misalignedStore;
#endif /* _misaligned_access */

	if (pbfr_status & PBFR_ERR_MASK) {
		printf("PBFR Error = %X\n", pbfr_status);
		return -1;
	}

	if (stop_ns == 0) {
		/* still monitor */
		time_ns = pbfr_get_timestamp(0);
	} else {
		time_ns = stop_ns;
	}
	time_ns = time_ns - start_ns;

	printf("Task Loading => total_us: %d\n", (int) (time_ns / 1000));
	vTaskSuspendAll();

#ifdef PBFR_SUPPORT_IOSTALL
	IO_loading = vPortGetIOStallRate();
#endif

	for (i = 0; i < freertos_task_no; i++) {
		loading = (int) ((task_load[i].runtime_ns * 10000) / time_ns);
#ifdef PBFR_SUPPORT_POLLING
		printf("[%s]T%02d %d: %02d.%02d%% max: %02d.%02d%% (ts=%d)\n",
				task_load[i].name, task_load[i].taskno,
				task_load[i].priority, loading / 100, loading % 100,
				task_load[i].maxload / 100, task_load[i].maxload % 100,
				(int) (task_load[i].maxload_ns / 1000));
#else
#ifdef PBFR_SUPPORT_CACHE_COUNT
		if (task_load[i].cache_access_cnt_i) {
			cache_miss_rate_i =
				(int) (((task_load[i].cache_miss_cnt_i)
							* 10000) / task_load[i].cache_access_cnt_i);
		} else {
			cache_miss_rate_i = 0;
		}
		if (task_load[i].cache_access_cnt_d) {
			cache_miss_rate_d =
				(int) (((task_load[i].cache_miss_cnt_d)
							* 10000) / task_load[i].cache_access_cnt_d);
		} else {
			cache_miss_rate_d = 0;
		}
		printf
			("[%s]T%02d %d: %02d.%02d%% %02d.%02d%% (I$access=%lld, I$miss=%lld), %02d.%02d%% (D$access=%lld, D$miss=%lld)\n",
			 task_info[i].name, task_info[i].taskno,
			 task_info[i].priority, loading / 100, loading % 100,
			 cache_miss_rate_i / 100, cache_miss_rate_i % 100,
			 task_load[i].cache_access_cnt_i,
			 task_load[i].cache_miss_cnt_i, cache_miss_rate_d / 100,
			 cache_miss_rate_d % 100, task_load[i].cache_access_cnt_d,
			 task_load[i].cache_miss_cnt_d);

#else
		printf("[%s]T%02d %d: %02d.%02d%% (%d)\n",
				task_info[i].name, task_info[i].taskno,
				task_info[i].priority, loading / 100, loading % 100,
				(int) (task_load[i].runtime_ns / 1000));
#endif
#endif
#ifdef PBFR_SUPPORT_WORKSLOT_MONITOR
		if (task_load[i].cnt1) {
			printf("\tWarning: W1Slot=%d W2Slot=%d\n",
					task_load[i].cnt1, task_load[i].cnt2);
		}
#endif
		if (i == idle_task_idx[uxPortGetHartId()]) {
			cpu_load = 10000 - loading;
		}
#ifdef PBFR_SUPPORT_TASK_CHECKER
		if (task_info[i].taskno == golden.taskno) {
#ifdef PBFR_SUPPORT_POLLING
			checking.peak_loading = task_load[i].maxload;
#endif
			checking.average_loading = loading;
		}
#endif
	}
#ifdef _misaligned_access
	printf("misaligned load counter : %d\n", misalignedLoad);
	printf("misaligned store counter: %d\n", misalignedStore);
#endif /* _misaligned_access */
	printf("CPU Loading: %02d.%02d%%\n", cpu_load / 100, cpu_load % 100);
#ifdef PBFR_SUPPORT_IOSTALL
	printf("IO stall: %02d.%2d%%\n", IO_loading / 100, IO_loading % 100);
#endif

#ifdef PBFR_SUPPORT_TASK_CHECKER
	checking.cpu_average_loading = cpu_load;
	pbfr_task_checking();
#endif
	xTaskResumeAll();

	return 0;
}
#endif				/* PBFR_SUPPORT_PERF_BUDGET */

#ifdef PBFR_SUPPORT_WORKSLOT_MONITOR
#ifndef PBFR_SUPPORT_WORKSLOT_COUNTER1_TICKS
#define PBFR_SUPPORT_WORKSLOT_COUNTER1_TICKS 4
#endif
#ifndef PBFR_SUPPORT_WORKSLOT_COUNTER2_TICKS
#define PBFR_SUPPORT_WORKSLOT_COUNTER2_TICKS 5
#endif
#define portTICK_PERIOD_NS          ( 1000000000 / configTICK_RATE_HZ )
static void pbfr_check_workslot(int idx, int diff_ns)
{
	if (idx == idle_task_idx) {
		return;
	}

	if (diff_ns >
			(int) (portTICK_PERIOD_NS * PBFR_SUPPORT_WORKSLOT_COUNTER1_TICKS)) {
		task_load[idx].cnt1++;
	}
	if (diff_ns >
			(int) (portTICK_PERIOD_NS * PBFR_SUPPORT_WORKSLOT_COUNTER2_TICKS)) {
		task_load[idx].cnt2++;
	}

	return;
}
#endif

#define IDLE_NAME   0x454C4449

int pbfr_task_create(int taskno, int priority, char *name)
{

	if (freertos_task_no >= FREERTOS_MAX_TASKS) {
		pbfr_status |= PBFR_ERR_TASK_CREATE_OVERFLOW;
		return -1;
	}

	if ((taskno - 1) != freertos_task_no) {
		pbfr_status |= PBFR_ERR_TASK_CREATE_NO;
		return -1;
	}

	task_info[freertos_task_no].taskno = taskno;
	task_info[freertos_task_no].priority = priority;
	task_info[freertos_task_no].watermark = 0;
	memcpy(task_info[freertos_task_no].name, name, configMAX_TASK_NAME_LEN);
	if ((*(unsigned int *) name) == IDLE_NAME) {
		idle_task_idx[uxPortGetHartId()] = freertos_task_no;
	}
	freertos_task_no++;

	return 0;
}

int pbfr_task_switch(int taskno);
int pbfr_task_switch(int taskno)
{
#ifdef PBFR_SUPPORT_PERF_BUDGET
	int idx;
	unsigned long long now_ns, diff_ns;
#ifdef PBFR_SUPPORT_CACHE_COUNT
	unsigned long long now_cache_access_cnt_i, now_cache_miss_cnt_i;
	unsigned long long now_cache_access_cnt_d, now_cache_miss_cnt_d;
#endif

	if ((pbfr_status & PBFR_STS_ON) == 0) {
		return 0;
	}

	idx = taskno - 1;

	if (idx >= FREERTOS_MAX_TASKS) {
		pbfr_status |= PBFR_ERR_TASK_SWITCH_OVERFLOW;
		return -1;
	}

	if ((int) (task_info[idx].taskno) != taskno) {
		pbfr_status |= PBFR_ERR_TASK_SWITCH_NO;
		return -1;
	}

	now_ns = pbfr_get_timestamp(1);

	if (now_ns <= last_switch_ns[uxPortGetHartId()]) {
		/* timer sync with AP, ignore this */
		;
	} else {
		diff_ns = now_ns - last_switch_ns[uxPortGetHartId()];
		task_load[idx].runtime_ns += diff_ns;
#ifdef PBFR_SUPPORT_POLLING
		task_load[idx].polling_ns += diff_ns;
#endif
#ifdef PBFR_SUPPORT_WORKSLOT_MONITOR
		pbfr_check_workslot(idx, (int) diff_ns);
#endif
	}
	last_switch_ns[uxPortGetHartId()] = now_ns;

#ifdef PBFR_SUPPORT_CACHE_COUNT
	vPortHaltCacheCount();
	now_cache_access_cnt_i = vPortGetICacheAccessCount();
	now_cache_miss_cnt_i = vPortGetICacheMissCount();
	now_cache_access_cnt_d = vPortGetDCacheAccessCount();
	now_cache_miss_cnt_d = vPortGetDCacheMissCount();
	vPortResumeCacheCount();

	if (now_cache_access_cnt_i <= last_cache_access_cnt_i) {
		/* count overflow or something wrong */
		;
	} else {
		task_load[idx].cache_access_cnt_i +=
			(now_cache_access_cnt_i - last_cache_access_cnt_i);
		task_load[idx].cache_miss_cnt_i +=
			(now_cache_miss_cnt_i - last_cache_miss_cnt_i);
	}
	last_cache_access_cnt_i = now_cache_access_cnt_i;
	last_cache_miss_cnt_i = now_cache_miss_cnt_i;

	if (now_cache_access_cnt_d <= last_cache_access_cnt_d) {
		/* count overflow or something wrong */
		;
	} else {
		task_load[idx].cache_access_cnt_d +=
			(now_cache_access_cnt_d - last_cache_access_cnt_d);
		task_load[idx].cache_miss_cnt_d +=
			(now_cache_miss_cnt_d - last_cache_miss_cnt_d);
	}
	last_cache_access_cnt_d = now_cache_access_cnt_d;
	last_cache_miss_cnt_d = now_cache_miss_cnt_d;
#endif

#else /* not PBFR_SUPPORT_PERF_BUDGET */
	(void) taskno;
	last_switch_ns[cpuid] = pbfr_get_timestamp(1);
#endif /* PBFR_SUPPORT_PERF_BUDGET */
	return 0;
}

#ifdef PBFR_SUPPORT_TASK_CHECKER
#ifdef PBFR_SUPPORT_POLLING
void pbfr_task_checker_reg(float task_peak, float task_aver, char * task)
{
	int i;

	golden.peak_loading = (int)(task_peak*100);
	golden.average_loading = (int)(task_aver*100);
	golden.cpu_average_loading = (int)(configMCU_LOADING_CRITERIA*100);
	golden.taskno = FREERTOS_MAX_TASKS;

	for(i=0; i<freertos_task_no; i++) {
		if(strcmp(task, task_load[i].name) == 0)
			golden.taskno = task_load[i].taskno;
	}
}
#else
void pbfr_task_checker_reg(float task_aver, char * task)
{
	int i;

	golden.average_loading = (int)(task_aver*100);
	golden.cpu_average_loading = (int)(configMCU_LOADING_CRITERIA*100);
	golden.taskno = FREERTOS_MAX_TASKS;

	for(i=0; i<freertos_task_no; i++) {
		if(strcmp(task, task_load[i].name) == 0)
			golden.taskno = task_load[i].taskno;
	}
}
#endif
#endif

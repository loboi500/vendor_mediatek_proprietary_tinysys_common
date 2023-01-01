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
#ifndef _PBFR_HOOK_H
#define _PBFR_HOOK_H

#include "mt_pbfr.h"

#if (defined(PBFR_SUPPORT_PERF_BUDGET) || defined(PBFR_SUPPORT_FLIGHT_REC))

#ifndef FREERTOS_MAX_TASKS
#define FREERTOS_MAX_TASKS 32
#endif

extern int pbfr_init(void);
extern int pbfr_start_loadinfo(int mode);
extern int pbfr_stop_loadinfo(void);
extern int pbfr_report_loadinfo(int mode);
#ifdef PBFR_SUPPORT_TASK_CHECKER
#ifdef PBFR_SUPPORT_POLLING
extern void pbfr_task_checker_reg(float task_peak, float task_aver, char *task);
#else
extern void pbfr_task_checker_reg(float task_aver, char *task);
#endif
#endif

#ifdef MET_TRACE_ENABLE
#error "PBFR cannot work woth ondiemet"
#endif				/* MET_TRACE_ENABLE */

#ifdef PBFR_SUPPORT_FLIGHT_REC
#include "pbfr_rec.h"
#endif				/* PBFR_SUPPORT_FLIGHT_REC */

extern int pbfr_task_create(int taskno, int priority, char *name);
#define traceTASK_CREATE( pxNewTCB )                                    \
{                                                                       \
    pbfr_task_create(pxNewTCB->uxTCBNumber, pxNewTCB->uxPriority,       \
                     pxNewTCB->pcTaskName);                             \
}

#ifdef PBFR_SUPPORT_REC_TASK
extern struct ev_task_t event_task;
extern unsigned char freertos_current_task_no;
extern int pbfr_task_switch(int taskno);
#if ((INCLUDE_vTaskDelete == 1) && (INCLUDE_vTaskSuspend == 1))
#define traceTASK_SWITCHED_OUT()                                        \
{                                                                       \
    List_t *pxStateList;                                                \
                                                                        \
    pxStateList = (List_t *)listLIST_ITEM_CONTAINER(&(pxCurrentTCB->xStateListItem)); \
                                                                        \
    if ((pxStateList == pxDelayedTaskList) || (pxStateList == pxOverflowDelayedTaskList)) \
    {                                                                   \
        event_task.type = EV_TASK_SWTCH_2;                              \
    }                                                                   \
    else if (pxStateList == &xSuspendedTaskList)                        \
    {                                                                   \
        if (listLIST_ITEM_CONTAINER(&(pxCurrentTCB->xEventListItem)) == NULL) \
        {                                                               \
            event_task.type = EV_TASK_SWTCH_3;                          \
        }                                                               \
        else                                                            \
        {                                                               \
            event_task.type = EV_TASK_SWTCH_2;                          \
        }                                                               \
    }                                                                   \
    else if (pxStateList == &xTasksWaitingTermination)                  \
    {                                                                   \
        event_task.type = EV_TASK_SWTCH_4;                              \
    }                                                                   \
    else                                                                \
    {                                                                   \
        event_task.type = EV_TASK_SWTCH_1;                              \
    }                                                                   \
    pbfr_task_switch(pxCurrentTCB->uxTCBNumber);                        \
}
#endif				/* ((INCLUDE_vTaskDelete == 1) && (INCLUDE_vTaskSuspend == 1)) */

#if ((INCLUDE_vTaskDelete == 1) && (INCLUDE_vTaskSuspend == 0))
#define traceTASK_SWITCHED_OUT()                                        \
{                                                                       \
    List_t *pxStateList;                                                \
                                                                        \
    pxStateList = (List_t *) listLIST_ITEM_CONTAINER(&(pxCurrentTCB->xStateListItem)); \
                                                                        \
    if ((pxStateList == pxDelayedTaskList) || (pxStateList == pxOverflowDelayedTaskList)) \
    {                                                                   \
        event_task.type = EV_TASK_SWTCH_2;                              \
    }                                                                   \
    else if (pxStateList == &xTasksWaitingTermination)                  \
    {                                                                   \
        event_task.type = EV_TASK_SWTCH_4;                              \
    }                                                                   \
    else                                                                \
    {                                                                   \
        event_task.type = EV_TASK_SWTCH_1;                              \
    }                                                                   \
    pbfr_task_switch(pxCurrentTCB->uxTCBNumber);                        \
}
#endif				/* ((INCLUDE_vTaskDelete == 1) && (INCLUDE_vTaskSuspend == 0)) */

#if ((INCLUDE_vTaskDelete == 0) && (INCLUDE_vTaskSuspend == 1))
#define traceTASK_SWITCHED_OUT()                                        \
{                                                                       \
    List_t *pxStateList;                                                \
                                                                        \
    pxStateList = (List_t *)listLIST_ITEM_CONTAINER(&(pxCurrentTCB->xStateListItem)); \
                                                                        \
    if ((pxStateList == pxDelayedTaskList) || (pxStateList == pxOverflowDelayedTaskList)) \
    {                                                                   \
        event_task.type = EV_TASK_SWTCH_2;                              \
    }                                                                   \
    else if (pxStateList == &xSuspendedTaskList)                        \
    {                                                                   \
        if (listLIST_ITEM_CONTAINER(&(pxCurrentTCB->xEventListItem)) == NULL) \
        {                                                               \
            event_task.type = EV_TASK_SWTCH_3;                          \
        }                                                               \
        else                                                            \
        {                                                               \
            event_task.type = EV_TASK_SWTCH_2;                          \
        }                                                               \
    }                                                                   \
    else                                                                \
    {                                                                   \
        event_task.type = EV_TASK_SWTCH_1;                              \
    }                                                                   \
    pbfr_task_switch(pxCurrentTCB->uxTCBNumber);                        \
}
#endif				/* ((INCLUDE_vTaskDelete == 0) && (INCLUDE_vTaskSuspend == 1)) */

#if ((INCLUDE_vTaskDelete == 0) && (INCLUDE_vTaskSuspend == 0))
#define traceTASK_SWITCHED_OUT()                                        \
{                                                                       \
    List_t *pxStateList;                                                \
                                                                        \
    pxStateList = (List_t *)listLIST_ITEM_CONTAINER(&(pxCurrentTCB->xStateListItem)); \
                                                                        \
    if ((pxStateList == pxDelayedTaskList) || (pxStateList == pxOverflowDelayedTaskList)) \
    {                                                                   \
        event_task.type = EV_TASK_SWTCH_2;                              \
    }                                                                   \
    else                                                                \
    {                                                                   \
        event_task.type = EV_TASK_SWTCH_1;                              \
    }                                                                   \
    pbfr_task_switch(pxCurrentTCB->uxTCBNumber);                        \
}
#endif				/* ((INCLUDE_vTaskDelete == 0) && (INCLUDE_vTaskSuspend == 0)) */

#ifdef MRV_PROFILING
#define traceTASK_SWITCHED_IN()                                         \
{                                                                       \
    event_task.taskno = pxCurrentTCB->uxTCBNumber;                      \
    pbfr_rec_add_event((struct ev_rec_t*)(&event_task));                \
    freertos_current_task_no = pxCurrentTCB->uxTCBNumber;               \
    MrvProfilingTaskSwtich();                                           \
}
#else
#define traceTASK_SWITCHED_IN()                                         \
{                                                                       \
    event_task.taskno = pxCurrentTCB->uxTCBNumber;                      \
    pbfr_rec_add_event((struct ev_rec_t*)(&event_task));                \
    freertos_current_task_no = pxCurrentTCB->uxTCBNumber;               \
}
#endif				/* MRV_PROFILING */

#define traceMOVED_TASK_TO_READY_STATE( pTCB )                          \
{                                                                       \
    event_task.type = EV_TASK_WAKEUP;                                   \
    event_task.taskno = pTCB->uxTCBNumber;                              \
    pbfr_rec_add_event((struct ev_rec_t*)(&event_task));                \
}
#else				/* only PBFR_SUPPORT_PERF_BUDGET, no PBFR_SUPPORT_REC_TASK */
extern int pbfr_task_switch(int taskno);
#define traceTASK_SWITCHED_OUT()                                        \
{                                                                       \
    pbfr_task_switch(pxCurrentTCB->uxTCBNumber);                        \
}
#endif				/* PBFR_SUPPORT_REC_TASK */

#ifdef PBFR_SUPPORT_REC_QUEUE
#define traceQUEUE_CREATE( pxNewQueue )                                 \
{                                                                       \
    taskENTER_CRITICAL();                                               \
    pxNewQueue->uxQueueNumber =                                         \
        pbfr_rec_queue_create((void*)pxNewQueue, pxNewQueue->ucQueueType, \
        pxNewQueue->uxLength, 0);                                       \
    taskEXIT_CRITICAL();                                                \
}

#define traceCREATE_COUNTING_SEMAPHORE( )                               \
{                                                                       \
    pbfr_rec_queue_change(((Queue_t *)xHandle)->uxQueueNumber,          \
                          uxInitialCount);                              \
}

#define traceCREATE_MUTEX( pxNewQueue )                                 \
{                                                                       \
    taskENTER_CRITICAL();                                               \
    pxNewQueue->uxQueueNumber =                                         \
        pbfr_rec_queue_create((void*)pxNewQueue, pxNewQueue->ucQueueType, \
        pxNewQueue->uxLength, 1);                                       \
    taskEXIT_CRITICAL();                                                \
}

#define traceQUEUE_RECEIVE( pxQueue )                                   \
{                                                                       \
    pbfr_rec_queue_event(EV_QUEUE_RECV,                                 \
        pxQueue->uxQueueNumber, pxQueue->uxMessagesWaiting);            \
}

#define traceQUEUE_RECEIVE_FAILED( pxQueue )                            \
{                                                                       \
    pbfr_rec_queue_event(EV_QUEUE_RECV_F,                               \
        pxQueue->uxQueueNumber, pxQueue->uxMessagesWaiting);            \
}

#define traceBLOCKING_ON_QUEUE_RECEIVE( pxQueue )                       \
{                                                                       \
    pbfr_rec_queue_event(EV_QUEUE_RECV_B,                               \
        pxQueue->uxQueueNumber, pxQueue->uxMessagesWaiting);            \
}

#define traceQUEUE_SEND( pxQueue )                                      \
{                                                                       \
    pbfr_rec_queue_event(EV_QUEUE_SEND,                                 \
        pxQueue->uxQueueNumber, pxQueue->uxMessagesWaiting);            \
}

#define traceQUEUE_SEND_FAILED( pxQueue )                               \
{                                                                       \
    pbfr_rec_queue_event(EV_QUEUE_SEND_F,                               \
        pxQueue->uxQueueNumber, pxQueue->uxMessagesWaiting);            \
}

#define traceBLOCKING_ON_QUEUE_SEND( pxQueue )                          \
{                                                                       \
    pbfr_rec_queue_event(EV_QUEUE_SEND_B,                               \
        pxQueue->uxQueueNumber, pxQueue->uxMessagesWaiting);            \
}

#define traceQUEUE_SEND_FROM_ISR( pxQueue )                             \
{                                                                       \
    pbfr_rec_queue_event(EV_QUEUE_SEND_ISR,                             \
        pxQueue->uxQueueNumber, pxQueue->uxMessagesWaiting);            \
}

#define traceQUEUE_SEND_FROM_ISR_FAILED( pxQueue )                      \
{                                                                       \
    pbfr_rec_queue_event(EV_QUEUE_SEND_ISR_F,                           \
        pxQueue->uxQueueNumber, pxQueue->uxMessagesWaiting);            \
}
#endif				/* PBFR_SUPPORT_REC_QUEUE */

#ifdef PBFR_SUPPORT_REC_SWTIMER
extern struct ev_swt_t event_swt;
extern int pbfr_rec_swt_create(unsigned int pxNewTimer);
#define traceTIMER_CREATE( pxNewTimer )                                        \
{                                                                              \
    taskENTER_CRITICAL();                                                      \
    pxNewTimer->uxTimerNumber = pbfr_rec_swt_create((unsigned int)pxNewTimer); \
    taskEXIT_CRITICAL();                                                       \
}

#define traceTIMER_CALLBACK_START(pxTimer)                              \
{                                                                       \
    event_swt.type = EV_SWTIMER_CB_START;                               \
    event_swt.timerno = pxTimer->uxTimerNumber;                         \
    taskENTER_CRITICAL();                                               \
    pbfr_rec_add_event((struct ev_rec_t *)&event_swt);                  \
    taskEXIT_CRITICAL();                                                \
}

#define traceTIMER_CALLBACK_END(pxTimer)                                \
{                                                                       \
    event_swt.type = EV_SWTIMER_CB_END;                                 \
    event_swt.timerno = pxTimer->uxTimerNumber;                         \
    taskENTER_CRITICAL();                                               \
    pbfr_rec_add_event((struct ev_rec_t *)&event_swt);                  \
    taskEXIT_CRITICAL();                                                \
}

#define traceTIMER_COMMAND_SEND( xTimer, xMessageID, xMessageValueValue, xReturn )
#define traceTIMER_COMMAND_RECEIVED( pxTimer, xMessageID, xMessageValue )
#endif				/* PBFR_SUPPORT_REC_SWTIMER */

#ifdef PBFR_SUPPORT_REC_INT
extern struct ev_int_t event_int;
extern unsigned long long pbfr_rtos_ts;
extern unsigned long long pbfr_rtos_ts_diff;
extern char pbfr_rtos_ts_nesting;
// use PGT6 timer
#ifndef configTRACE_INT_MASK_TIME_BOUND_NS
#define configTRACE_INT_MASK_TIME_BOUND_NS (100*1000)	// 100 us
#endif

#ifndef traceINT_OFF
#undef traceINT_OFF
#define traceINT_OFF()                                                  \
{                                                                       \
    pbfr_rec_int_off();                                                 \
}
#endif

#ifndef traceINT_ON
#undef traceINT_ON
#define traceINT_ON()                                                   \
{                                                                       \
    pbfr_rec_int_on(EV_INT_ON);                                         \
}
#endif

#ifndef traceINT_OFF_FROM_ISR
#undef traceINT_OFF_FROM_ISR
#define traceINT_OFF_FROM_ISR()                                         \
{                                                                       \
    pbfr_rtos_ts_nesting++;                                             \
    if (pbfr_rtos_ts_nesting == 1) {                                    \
        pbfr_rec_int_off();                                             \
   }                                                                    \
}
#endif

#ifndef traceINT_ON_FROM_ISR
#undef traceINT_ON_FROM_ISR
#define traceINT_ON_FROM_ISR()                                          \
{                                                                       \
    pbfr_rtos_ts_nesting--;                                             \
    if (pbfr_rtos_ts_nesting == 0) {                                    \
        pbfr_rec_int_on(EV_INT_ON_FROM_ISR);                            \
    }                                                                   \
}
#endif
#endif				/* PBFR_SUPPORT_REC_INT */

#ifdef PBFR_SUPPORT_REC_ISR
extern struct ev_isr_t event_isr;
#ifndef traceISR_ENTER
#undef traceISR_ENTER
#define traceISR_ENTER()                                                \
{                                                                       \
    event_isr.type = EV_ISR_ENTER;                                      \
    event_isr.vec = read_taken_INT();                                   \
    pbfr_rec_add_event((struct ev_rec_t *)&event_isr);                  \
}
#endif

#ifndef traceISR_EXIT
#undef traceISR_EXIT
#define traceISR_EXIT()                                                 \
{                                                                       \
    event_isr.type = EV_ISR_EXIT;                                       \
    pbfr_rec_add_event((struct ev_rec_t *)&event_isr);                  \
}
#endif
#endif				/* PBFR_SUPPORT_REC_ISR */

#ifdef PBFR_SUPPORT_REC_OSTICK
extern struct ev_rec_t event_os_tick;
#ifndef configTRACE_TICK_PERIOD
#define configTRACE_TICK_PERIOD 10
#endif
#define traceTASK_INCREMENT_TICK(xTickCount)                            \
{                                                                       \
    static TickType_t xTickCountLast = 0;                               \
    event_os_tick.type = EV_SYS_OS_TICK;                                \
    if (xTickCount - xTickCountLast >= configTRACE_TICK_PERIOD) {       \
        pbfr_rec_add_event(&event_os_tick);                             \
        xTickCountLast = xTickCount;                                    \
    }                                                                   \
}
#endif

#else				/* neither PBFR_SUPPORT_PERF_BUDGET nor PBFR_SUPPORT_FLIGHT_REC */
#define pbfr_init()
#define pbfr_start_loadinfo(mode)
#define pbfr_stop_loadinfo()
#define pbfr_report_loadinfo(mode)
#endif				/* PBFR_SUPPORT_PERF_BUDGET or PBFR_SUPPORT_FLIGHT_REC */

#endif				//_PBFR_HOOK_H

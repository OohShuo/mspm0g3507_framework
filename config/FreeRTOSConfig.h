/*
 * FreeRTOS Kernel <DEVELOPMENT BRANCH>
 * Copyright (C) 2021 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * https://www.FreeRTOS.org
 * https://github.com/FreeRTOS
 *
 */

#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

/* 在 FreeRTOSConfig.h 的开头 */
#ifdef __cplusplus
extern "C" {
#endif

#if FRAMEWORK_USE_LVGL
    // NOLINTBEGIN(readability-identifier-naming)
    void lv_freertos_task_switch_in(const char* name);
    void lv_freertos_task_switch_out(void);
    // NOLINTEND(readability-identifier-naming)
#endif
#ifdef __cplusplus
}
#endif

#define configENABLE_MPU                             0U

/******************************************************************************/
/* Hardware description related definitions. **********************************/
/******************************************************************************/

#define configCPU_CLOCK_HZ                           ((unsigned long)80000000)

/******************************************************************************/
/* Scheduling behaviour related definitions. **********************************/
/******************************************************************************/

#define configTICK_RATE_HZ                           (1000U)
#define configUSE_PREEMPTION                         1
#define configUSE_TIME_SLICING                       1
#define configUSE_PORT_OPTIMISED_TASK_SELECTION      0
#define configUSE_TICKLESS_IDLE                      0
#define configMAX_PRIORITIES                         5U
#define configMINIMAL_STACK_SIZE                     128U
#define configMAX_TASK_NAME_LEN                      4U
#define configTICK_TYPE_WIDTH_IN_BITS                TICK_TYPE_WIDTH_32_BITS
#define configIDLE_SHOULD_YIELD                      1
#define configTASK_NOTIFICATION_ARRAY_ENTRIES        1U
#define configQUEUE_REGISTRY_SIZE                    0U
#define configENABLE_BACKWARD_COMPATIBILITY          1
#define configNUM_THREAD_LOCAL_STORAGE_POINTERS      0
#define configSTACK_DEPTH_TYPE                       size_t
#define configMESSAGE_BUFFER_LENGTH_TYPE             size_t
#define configUSE_NEWLIB_REENTRANT                   0

/******************************************************************************/
/* Software timer related definitions. ****************************************/
/******************************************************************************/

#define configUSE_TIMERS                             0
#define configTIMER_TASK_PRIORITY                    (configMAX_PRIORITIES - 1U)
#define configTIMER_TASK_STACK_DEPTH                 configMINIMAL_STACK_SIZE
#define configTIMER_QUEUE_LENGTH                     10U

/******************************************************************************/
/* Memory allocation related definitions. *************************************/
/******************************************************************************/

#define configSUPPORT_STATIC_ALLOCATION              0
#define configSUPPORT_DYNAMIC_ALLOCATION             1
#define configTOTAL_HEAP_SIZE                        ((size_t)(12 * 1024))
#define configAPPLICATION_ALLOCATED_HEAP             0
#define configSTACK_ALLOCATION_FROM_SEPARATE_HEAP    0
#define configUSE_MINI_LIST_ITEM                     0

/******************************************************************************/
/* Interrupt nesting behaviour configuration. *********************************/
/******************************************************************************/

#define configLIBRARY_LOWEST_INTERRUPT_PRIORITY      3
#define configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY 1
#define configKERNEL_INTERRUPT_PRIORITY              (configLIBRARY_LOWEST_INTERRUPT_PRIORITY << (8 - 2))
#define configMAX_SYSCALL_INTERRUPT_PRIORITY         (configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY << (8 - 2))
#define configMAX_API_CALL_INTERRUPT_PRIORITY        0U

/******************************************************************************/
/* Hook and callback function related definitions. ****************************/
/******************************************************************************/

#define configUSE_IDLE_HOOK                          0
#define configUSE_TICK_HOOK                          0
#define configUSE_MALLOC_FAILED_HOOK                 1
#define configUSE_DAEMON_TASK_STARTUP_HOOK           0
#define configCHECK_FOR_STACK_OVERFLOW               2

/******************************************************************************/
/* Run time and task stats gathering related definitions. *********************/
/******************************************************************************/

#define configGENERATE_RUN_TIME_STATS                0
#define configUSE_TRACE_FACILITY                     0
#define configUSE_STATS_FORMATTING_FUNCTIONS         0
#define configKERNEL_PROVIDED_STATIC_MEMORY          0

/******************************************************************************/
/* Definitions that include or exclude functionality. *************************/
/******************************************************************************/

#define configUSE_TASK_NOTIFICATIONS                 1
#define configUSE_MUTEXES                            1
#define configUSE_RECURSIVE_MUTEXES                  0
#define configUSE_COUNTING_SEMAPHORES                0
#define configUSE_QUEUE_SETS                         0
#define configUSE_APPLICATION_TASK_TAG               0
#define INCLUDE_vTaskPrioritySet                     0
#define INCLUDE_uxTaskPriorityGet                    0
#define INCLUDE_vTaskDelete                          0
#define INCLUDE_vTaskSuspend                         1
#define INCLUDE_vTaskDelayUntil                      1
#define INCLUDE_vTaskDelay                           1
#define INCLUDE_xTaskGetSchedulerState               0
#define INCLUDE_xTaskGetCurrentTaskHandle            0
#define INCLUDE_uxTaskGetStackHighWaterMark          0
#define INCLUDE_xTaskGetIdleTaskHandle               0
#define INCLUDE_eTaskGetState                        0
#define INCLUDE_xTimerPendFunctionCall               0
#define INCLUDE_xTaskAbortDelay                      0
#define INCLUDE_xTaskGetHandle                       0
#define INCLUDE_xTaskResumeFromISR                   0

/******************************************************************************/
/* Set for LVGL support. ******************************************************/
/******************************************************************************/

#if FRAMEWORK_USE_LVGL
    #define traceTASK_SWITCHED_IN()                  lv_freertos_task_switch_in(pxCurrentTCB->pcTaskName)
    #define traceTASK_SWITCHED_OUT()                 lv_freertos_task_switch_out()
#else
    #define traceTASK_SWITCHED_IN()
    #define traceTASK_SWITCHED_OUT()
#endif

/******************************************************************************/
/* Set assertion macro. *******************************************************/
/******************************************************************************/

#define configASSERT(x)           \
    if ((x) == 0) {               \
        taskDISABLE_INTERRUPTS(); \
        for (;;);                 \
    }

#endif /* FREERTOS_CONFIG_H */

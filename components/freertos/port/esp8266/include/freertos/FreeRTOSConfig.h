/*
 * FreeRTOS Kernel V10.0.1
 * Copyright (C) 2017 Amazon.com, Inc. or its affiliates.  All Rights Reserved.
 * Additions Copyright 2018 Espressif Systems (Shanghai) PTE LTD
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
 * http://www.FreeRTOS.org
 * http://aws.amazon.com/freertos
 *
 * 1 tab == 4 spaces!
 */

#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

#include "sdkconfig.h"

#ifndef __ASSEMBLER__
#include <stdlib.h>
#include "rom/ets_sys.h"
#endif

/*-----------------------------------------------------------
 * Application specific definitions.
 *
 * These definitions should be adjusted for your particular hardware and
 * application requirements.
 *
 * THESE PARAMETERS ARE DESCRIBED WITHIN THE 'CONFIGURATION' SECTION OF THE
 * FreeRTOS API DOCUMENTATION AVAILABLE ON THE FreeRTOS.org WEB SITE. 
 *
 * See http://www.freertos.org/a00110.html.
 *----------------------------------------------------------*/

#define portNUM_PROCESSORS          1
#define configUSE_PREEMPTION		1

#define configUSE_IDLE_HOOK			1
#define configUSE_TICK_HOOK			1

#define configUSE_TICKLESS_IDLE 	1
#define configCPU_CLOCK_HZ			( ( unsigned long ) 80000000 )	
#define configTICK_RATE_HZ			( ( portTickType ) CONFIG_FREERTOS_HZ )
#define configMAX_PRIORITIES		15
/* ESPurna (FreeRTOS-Kernel V11): the idle task's stack is configMINIMAL_STACK_SIZE (the SDK's 10.0.1 kernel took
configIDLE_TASK_STACK_SIZE there), nothing else uses it; bytes, StackType_t is uint8_t */
#define configMINIMAL_STACK_SIZE	( ( unsigned short ) CONFIG_FREERTOS_IDLE_TASK_STACKSIZE )
/* ESPurna (FreeRTOS-Kernel V11): the stack depth type defaults to StackType_t, uint8_t on this port (depths in
bytes): every depth was cut to its low byte (3584, 8192 -> 0) and every task ran on a stack of nothing */
#define configSTACK_DEPTH_TYPE uint32_t
//#define configTOTAL_HEAP_SIZE		( ( size_t ) ( 17 * 1024 ) )
#define configMAX_TASK_NAME_LEN		( 16 )

#define configUSE_16_BIT_TICKS		0
#define configIDLE_SHOULD_YIELD		1

#define INCLUDE_xTaskGetIdleTaskHandle 1
#define INCLUDE_xTimerGetTimerDaemonTaskHandle 1

#ifndef CONFIG_FREERTOS_WATCHPOINT_END_OF_STACK
#define configCHECK_FOR_STACK_OVERFLOW  2
#endif

#define configUSE_MUTEXES  1
#define configUSE_RECURSIVE_MUTEXES  1
#define configUSE_COUNTING_SEMAPHORES   1
#define configUSE_TIMERS    1

#if configUSE_TIMERS
#define configTIMER_TASK_PRIORITY ( tskIDLE_PRIORITY + 2 )
#define configTIMER_QUEUE_LENGTH (10)
#define configTIMER_TASK_STACK_DEPTH  ( ( unsigned short ) CONFIG_FREERTOS_TIMER_STACKSIZE )
#define INCLUDE_xTimerPendFunctionCall 1
#endif

/* Co-routine definitions. */
#define configUSE_CO_ROUTINES 		0
#define configMAX_CO_ROUTINE_PRIORITIES ( 2 )

/* Set the following definitions to 1 to include the API function, or zero
to exclude the API function. */

#define INCLUDE_vTaskPrioritySet		1
#define INCLUDE_uxTaskPriorityGet		1
#define INCLUDE_vTaskDelete				1
#define INCLUDE_vTaskCleanUpResources	0
#define INCLUDE_vTaskSuspend			1
#define INCLUDE_vTaskDelayUntil			1
#define INCLUDE_vTaskDelay				1

/*set the #define for debug info*/
#define INCLUDE_xTaskGetCurrentTaskHandle 1
#define INCLUDE_uxTaskGetStackHighWaterMark 1

#define INCLUDE_xSemaphoreGetMutexHolder    1

/* This is the raw value as per the Cortex-M3 NVIC.  Values can be 255
(lowest) to 0 (1?) (highest). */
#define configKERNEL_INTERRUPT_PRIORITY 		255
/* !!!! configMAX_SYSCALL_INTERRUPT_PRIORITY must not be set to zero !!!!
See http://www.FreeRTOS.org/RTOS-Cortex-M3-M4.html. */
#define configMAX_SYSCALL_INTERRUPT_PRIORITY 	191 /* equivalent to 0xb0, or priority 11. */


/* This is the value being used as per the ST library which permits 16
priority values, 0 to 15.  This must correspond to the
configKERNEL_INTERRUPT_PRIORITY setting.  Here 15 corresponds to the lowest
NVIC value of 255. */
#define configLIBRARY_KERNEL_INTERRUPT_PRIORITY	15

/* espurna: static objects too (newlib 4.3 of GCC 14.2: its static locks are static FreeRTOS mutexes,
 * components/newlib/src/locks.c); the idle and timer tasks then take the kernel's own static memory */
#define configSUPPORT_STATIC_ALLOCATION     1
#define configKERNEL_PROVIDED_STATIC_MEMORY 1

// add it to menuconfig later
#ifdef CONFIG_FREERTOS_ENABLE_REENT
#define configUSE_NEWLIB_REENTRANT  1
#endif

/**
 * 0: LwIP
 * 1: pthread (optional)
 * 2: errno
 */
/* ESPurna (FreeRTOS-Kernel V11): 3 pointers of the SDK (errno, pthread keys) and, after them, their delete callbacks
(portTLS_POINTERS, freertos_tasks_c_additions.h: the kernel has no delete callbacks of its own) */
#define portTLS_POINTERS 3
#define configNUM_THREAD_LOCAL_STORAGE_POINTERS ( 2 * portTLS_POINTERS )
#define configTHREAD_LOCAL_STORAGE_DELETE_CALLBACKS 1
/* the port's additions to the kernel's tasks.c: TLS delete callbacks, the expected idle time of the sleep code */
#define configINCLUDE_FREERTOS_TASK_C_ADDITIONS_H 1

/* add this to dump task stack information */
#define configRECORD_STACK_HIGH_ADDRESS 1

#ifdef CONFIG_TASK_SWITCH_FASTER
#define TASK_SW_ATTR IRAM_ATTR
#else
#define TASK_SW_ATTR
#endif

#if CONFIG_USE_QUEUE_SETS
#define configUSE_QUEUE_SETS 1
#endif

#ifdef CONFIG_FREERTOS_USE_TRACE_FACILITY
#define configUSE_TRACE_FACILITY        1       /* Used by uxTaskGetSystemState(), and other trace facility functions */
#endif

#ifdef CONFIG_FREERTOS_USE_STATS_FORMATTING_FUNCTIONS
#define configUSE_STATS_FORMATTING_FUNCTIONS    1   /* Used by vTaskList() */
#endif

#ifdef CONFIG_FREERTOS_GENERATE_RUN_TIME_STATS
#define configGENERATE_RUN_TIME_STATS           1   /* Used by vTaskGetRunTimeStats() */
#define configSUPPORT_DYNAMIC_ALLOCATION        1

//ccount or esp_timer are initialized elsewhere
#define portCONFIGURE_TIMER_FOR_RUN_TIME_STATS()

#ifdef CONFIG_FREERTOS_RUN_TIME_STATS_USING_CPU_CLK
#ifndef __ASSEMBLER__
extern uint64_t g_esp_os_cpu_clk;
#define portGET_RUN_TIME_COUNTER_VALUE()  g_esp_os_cpu_clk
#endif
/* Fine resolution time */
#elif defined(CONFIG_FREERTOS_RUN_TIME_STATS_USING_ESP_TIMER)
/* Coarse resolution time (us) */
#ifndef __ASSEMBLER__
uint32_t esp_get_time(void);
#define portALT_GET_RUN_TIME_COUNTER_VALUE(x)    x = (uint32_t)esp_get_time()
#endif /* __ASSEMBLER__ */
#endif /* CONFIG_FREERTOS_RUN_TIME_STATS_USING_CPU_CLK */

#endif /* CONFIG_FREERTOS_GENERATE_RUN_TIME_STATS */

#define traceINCREASE_TICK_COUNT(_ticks)    esp_increase_tick_cnt(_ticks)

/* DIAG 11-18 (ESPurna, temporary): a task that blocks while the scheduler is suspended (or in an interrupt) breaks the
   lists of FreeRTOS-Kernel V11 - who does it (port.c vPortTraceBlocking) */
#ifndef __ASSEMBLER__
void vPortTraceBlocking( const char * what, void * object, void * caller );
#endif
#define traceBLOCKING_ON_QUEUE_RECEIVE( pxQueue ) vPortTraceBlocking( "queue receive", ( pxQueue ), __builtin_return_address( 0 ) )
#define traceBLOCKING_ON_QUEUE_SEND( pxQueue )    vPortTraceBlocking( "queue send", ( pxQueue ), __builtin_return_address( 0 ) )
#define traceBLOCKING_ON_QUEUE_PEEK( pxQueue )    vPortTraceBlocking( "queue peek", ( pxQueue ), __builtin_return_address( 0 ) )
#define traceTASK_DELAY()                         vPortTraceBlocking( "task delay", NULL, __builtin_return_address( 0 ) )

/* DIAG 11-18 (ESPurna): the kernel's own checks (blocking while suspended, from an ISR) - the first
   one that fails prints its line and the caller and stops (port.c vPortAssert). No list integrity bytes: they move
   pxStack in the TCB off the offset 48 of os_cpu_a.S (the end-of-stack watchpoint, false "Stack canary" 11-18) */
#ifndef __ASSEMBLER__
void vPortAssert( const char * file, int line, void * caller );
#endif
#define configASSERT( x ) do { if( !( x ) ) { vPortAssert( __FILE__, __LINE__, __builtin_return_address( 0 ) ); } } while( 0 )

/* DIAG 11-19 (ESPurna, temporary, regress-todo.md items 3/17): the Wi-Fi blob's crash is a task on the pending ready
   list whose event list item belongs to no list (xTaskResumeAll LoadProhibited) - an event item put into a second list
   while still in the first. The invariants of the event items, checked where the kernel moves them
   (freertos_tasks_c_additions.h): the first broken one prints the task, the lists, the context and its caller, then
   aborts (the panic backtrace is the path that broke it) */
#ifndef __ASSEMBLER__
void vPortCheckPlaceOnEventList( const char * what, void * pxEventList, void * caller );
void vPortCheckRemoveFromEventList( void * pxEventList, void * caller );
void vPortCheckPendingReady( void * caller );
#endif
#define traceENTER_vTaskPlaceOnEventList( pxEventList, xTicksToWait ) \
    vPortCheckPlaceOnEventList( "place", ( pxEventList ), __builtin_return_address( 0 ) )
#define traceENTER_vTaskPlaceOnUnorderedEventList( pxEventList, xItemValue, xTicksToWait ) \
    vPortCheckPlaceOnEventList( "place unordered", ( pxEventList ), __builtin_return_address( 0 ) )
#define traceENTER_vTaskPlaceOnEventListRestricted( pxEventList, xTicksToWait, xWaitIndefinitely ) \
    vPortCheckPlaceOnEventList( "place restricted", ( pxEventList ), __builtin_return_address( 0 ) )
#define traceENTER_xTaskRemoveFromEventList( pxEventList ) \
    vPortCheckRemoveFromEventList( ( void * ) ( pxEventList ), __builtin_return_address( 0 ) )
#define traceENTER_xTaskResumeAll()    vPortCheckPendingReady( __builtin_return_address( 0 ) )

#ifndef configIDLE_TASK_STACK_SIZE
#define configIDLE_TASK_STACK_SIZE CONFIG_FREERTOS_IDLE_TASK_STACKSIZE
#endif /* configIDLE_TASK_STACK_SIZE */

#endif /* FREERTOS_CONFIG_H */


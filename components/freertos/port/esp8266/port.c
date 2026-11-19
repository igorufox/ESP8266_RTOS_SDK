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

/* Scheduler includes. */

#include <stdint.h>
#include <string.h>

#include <xtensa/config/core.h>
#include <xtensa/tie/xt_interrupt.h>
#include <xtensa/tie/xt_timer.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/xtensa_rtos.h"

#include "esp_attr.h"
#include "esp_heap_caps.h"
#include "esp_libc.h"
#include "esp_task_wdt.h"
#include "esp_sleep.h"

#include "esp8266/eagle_soc.h"
#include "rom/ets_sys.h"
#include "esp8266/rom_functions.h"
#include "driver/soc.h"

#define SET_STKREG(r,v)     sp[(r) >> 2] = (uint32_t)(v)
#define PORT_ASSERT(x)      do { if (!(x)) {ets_printf("%s %u\n", "rtos_port", __LINE__); while(1){}; }} while (0)

extern uint8_t NMIIrqIsOn;

uint32_t cpu_sr;

uint32_t _xt_tick_divisor;

/* Each task maintains its own interrupt status in the critical nesting
variable. */
static uint32_t uxCriticalNesting = 0;

uint32_t g_esp_boot_ccount;
uint64_t g_esp_os_ticks;
uint64_t g_esp_os_us;
uint64_t g_esp_os_cpu_clk;

static uint32_t s_switch_ctx_flag;

#define ISR_STACK_PATTERN   0xa5a5a5a5

void vPortEnterCritical(void);
void vPortExitCritical(void);

void IRAM_ATTR portYIELD_FROM_ISR(void)
{
    s_switch_ctx_flag = 1;
}

uint8_t *__cpu_init_stk(uint8_t *stack_top, void (*_entry)(void *), void *param, void (*_exit)(void))
{

    uint32_t *sp, *tp, *stk = (uint32_t *)stack_top;

    /* Create interrupt stack frame aligned to 16 byte boundary */
    sp = (uint32_t *)(((uint32_t)(stk + 1) - XT_CP_SIZE - XT_STK_FRMSZ) & ~0xf);

    /* Clear the entire frame (do not use memset() because we don't depend on C library) */
    for (tp = sp; tp <= stk; ++tp) {
        *tp = 0;
    }

    /* Explicitly initialize certain saved registers */
    SET_STKREG(XT_STK_PC,   _entry);                        /* task entrypoint                  */
    SET_STKREG(XT_STK_A0,   _exit);                         /* to terminate GDB backtrace       */
    SET_STKREG(XT_STK_A1,   (uint32_t)sp + XT_STK_FRMSZ);   /* physical top of stack frame      */
    SET_STKREG(XT_STK_A2,   param);                         /* parameters      */
    SET_STKREG(XT_STK_EXIT, _xt_user_exit);                 /* user exception exit dispatcher   */

    /* Set initial PS to int level 0, EXCM disabled ('rfe' will enable), user mode. */
    SET_STKREG(XT_STK_PS,      PS_UM | PS_EXCM);

    return (uint8_t *)sp;
}

#ifndef DISABLE_FREERTOS
/*
 * See header file for description.
 */
StackType_t *pxPortInitialiseStack(StackType_t *pxTopOfStack, pdTASK_CODE pxCode, void *pvParameters)
{
    return (StackType_t *)__cpu_init_stk((uint8_t *)pxTopOfStack, pxCode, pvParameters, NULL);
}
#endif

void IRAM_ATTR PendSV(int req)
{
    if (req == 1) {
        vPortEnterCritical();
        s_switch_ctx_flag = 1;
        xthal_set_intset(1 << ETS_SOFT_INUM);
        vPortExitCritical();
    } else if (req == 2) {
        xthal_set_intset(1 << ETS_SOFT_INUM);
    }
}

void IRAM_ATTR SoftIsrHdl(void* arg)
{
    extern int MacIsrSigPostDefHdl(void);

    if (MacIsrSigPostDefHdl()) {
        portYIELD_FROM_ISR();
    }
}

void IRAM_ATTR esp_increase_tick_cnt(const TickType_t ticks)
{
    g_esp_os_ticks += ticks;
}

void IRAM_ATTR xPortSysTickHandle(void *p)
{
    uint32_t us;
    uint32_t ticks;
    uint32_t ccount;

    /**
     * System or application may close interrupt too long, such as the operation of read/write/erase flash.
     * And then the "ccount" value may be overflow.
     *
     * So add code here to calibrate system time.
     */
    ccount = soc_get_ccount();
    us = ccount / g_esp_ticks_per_us;
  
    g_esp_os_us += us;
    g_esp_os_cpu_clk += ccount;

    soc_set_ccount(0);
    soc_set_ccompare(_xt_tick_divisor);

    ticks = us / 1000 / portTICK_PERIOD_MS;

    /* ESPurna (FreeRTOS-Kernel V11): the ticks lost while the interrupts were closed (flash, the PHY calibration of the
     * ROM) one by one through the kernel's tick. vTaskStepTick() is the tickless sleep's, for a suspended scheduler
     * only: V11 asserts it never jumps past the next unblock time and counts that tick as pended (the SDK's 10.0.1
     * kernel only added to xTickCount). */
    BaseType_t switch_required = pdFALSE;

    if (ticks == 0) {
        ticks = 1;
    }

    while (ticks--) {
        g_esp_os_ticks++;

        if (xTaskIncrementTick() != pdFALSE) {
            switch_required = pdTRUE;
        }
    }

    if (switch_required != pdFALSE) {
        portYIELD_FROM_ISR();
    }
}

/**
 * @brief Return current CPU clock frequency
 */
int esp_clk_cpu_freq(void)
{
    return _xt_tick_divisor * XT_TICK_PER_SEC;
}

/*
 * See header file for description.
 */
portBASE_TYPE xPortStartScheduler(void)
{
    /*
     * TAG 1.2.3 FreeRTOS call "portDISABLE_INTERRUPTS" at file tasks.c line 1973, this is not at old one.
     * This makes it to be a wrong value.
     * 
     * So we should initialize global value "cpu_sr" with a right value.
     * 
     * Todo: Remove this one when refactor startup function.
     */
    cpu_sr = 0x20;

    /* DIAG 11-19 (ESPurna, temporary): the interrupt stack (CONFIG_FREERTOS_ISR_STACKSIZE, xtensa_vectors.S, every
     * level 1 handler and the task switch run on it) filled with a pattern, the idle hook reports how much of it the
     * deepest interrupt left (below it: _chip_nmi_cnt, the NMI's save area pick) */
    {
        extern uint32_t _chip_interrupt_stk[];

        for (int i = 0; i < (CONFIG_FREERTOS_ISR_STACKSIZE / 4); i++) {
            _chip_interrupt_stk[i] = ISR_STACK_PATTERN;
        }
    }

    /*******software isr*********/
    _xt_isr_attach(ETS_SOFT_INUM, SoftIsrHdl, NULL);
    _xt_isr_unmask(1 << ETS_SOFT_INUM);

    _xt_isr_attach(ETS_MAX_INUM, xPortSysTickHandle, NULL);

    /* Initialize system tick timer interrupt and schedule the first tick. */
    _xt_tick_divisor = xtbsp_clock_freq_hz() / XT_TICK_PER_SEC;

    g_esp_boot_ccount = soc_get_ccount();
    soc_set_ccount(0);
    _xt_tick_timer_init();

    vTaskSwitchContext();

    /* Restore the context of the first task that is going to run. */
    _xt_enter_first_task();

    /* Should not get here as the tasks are now running! */
    return pdTRUE;
}

void vPortInitContextFromOldStack(StackType_t *newStackTop, StackType_t *oldStackTop, UBaseType_t stackSize)
{
    uintptr_t *sp;

    memcpy(newStackTop, oldStackTop, stackSize);
    sp = (uintptr_t *)newStackTop;
    sp[XT_STK_A1 / sizeof(uintptr_t)] = (uintptr_t)sp + XT_STK_FRMSZ;
}

void vPortEndScheduler(void)
{
    /* It is unlikely that the CM3 port will require this function as there
    is nothing to return to.  */
}
/*-----------------------------------------------------------*/

/*-----------------------------------------------------------*/

static char ClosedLv1Isr = 0;

void IRAM_ATTR vPortEnterCritical(void)
{
    if (NMIIrqIsOn == 0) {
        if (ClosedLv1Isr != 1) {
            portDISABLE_INTERRUPTS();
            ClosedLv1Isr = 1;
        }
        uxCriticalNesting++;
    }
}
/*-----------------------------------------------------------*/

void IRAM_ATTR vPortExitCritical(void)
{
    if (NMIIrqIsOn == 0) {
        if (uxCriticalNesting > 0) {
            uxCriticalNesting--;

            if (uxCriticalNesting == 0) {
                if (ClosedLv1Isr == 1) {
                    ClosedLv1Isr = 0;
                    portENABLE_INTERRUPTS();
                }
            }
        } else {
            ets_printf(DRAM_STR("E:C:%u\n"), uxCriticalNesting);
            PORT_ASSERT((uxCriticalNesting > 0));
        }
    }
}

void show_critical_info(void)
{
    ets_printf("ShowCritical:%u\n", uxCriticalNesting);
    ets_printf("s_switch_ctx_flag:%u\n", s_switch_ctx_flag);
}

#ifdef ESP_DPORT_CLOSE_NMI
static int s_nmi_is_closed;

void esp_dport_close_nmi(void)
{
    vPortEnterCritical();
    REG_WRITE(PERIPHS_DPORT_BASEADDR, REG_READ(PERIPHS_DPORT_BASEADDR) & ~0x1);
    s_nmi_is_closed = 1;
    vPortExitCritical();
}

#define ESP_NMI_IS_CLOSED()     s_nmi_is_closed
#else
#define ESP_NMI_IS_CLOSED()     0
#endif

void IRAM_ATTR vPortETSIntrLock(void)
{
    if (NMIIrqIsOn == 0) {
        uint32_t regval = REG_READ(NMI_INT_ENABLE_REG);

        vPortEnterCritical();

        REG_WRITE(NMI_INT_ENABLE_REG, 0);

        if (!ESP_NMI_IS_CLOSED()) {
            do {
                REG_WRITE(INT_ENA_WDEV, WDEV_TSF0_REACH_INT);
            } while(REG_READ(INT_ENA_WDEV) != WDEV_TSF0_REACH_INT);
        }

        REG_WRITE(NMI_INT_ENABLE_REG, regval);
    }
}

void IRAM_ATTR vPortETSIntrUnlock(void)
{
    if (NMIIrqIsOn == 0) {
        uint32_t regval = REG_READ(NMI_INT_ENABLE_REG);

        REG_WRITE(NMI_INT_ENABLE_REG, 0);

        if (!ESP_NMI_IS_CLOSED()) {
            extern uint32_t WDEV_INTEREST_EVENT;

            REG_WRITE(INT_ENA_WDEV, WDEV_INTEREST_EVENT);
        }

        REG_WRITE(NMI_INT_ENABLE_REG, regval);

        vPortExitCritical();
    }
}

/*
 * @brief check if CPU core interrupt is disable
 */
bool interrupt_is_disable(void)
{
    uint32_t tmp;

    __asm__ __volatile__ (
        "rsr %0, PS\n"
        : "=a"(tmp) : : "memory");

    return tmp & 0xFUL ? true : false;
}

static _xt_isr_entry s_isr[16];
static uint8_t s_xt_isr_status = 0;

void _xt_isr_attach(uint8_t i, _xt_isr func, void* arg)
{
    s_isr[i].handler = func;
    s_isr[i].arg = arg;
}

void IRAM_ATTR _xt_isr_handler(void)
{
    do {
        uint32_t mask = soc_get_int_mask();

        for (int i = 0; i < ETS_INT_MAX && mask; i++) {
            int bit = 1 << i;

            if (!(bit & mask) || !s_isr[i].handler)
                continue;

            soc_clear_int_mask(bit);

            s_xt_isr_status = 1;
            s_isr[i].handler(s_isr[i].arg);
            s_xt_isr_status = 0;

            mask &= ~bit;
        }
    } while (soc_get_int_mask());

    if (s_switch_ctx_flag) {
        vTaskSwitchContext();
        s_switch_ctx_flag = 0;
    }
}

int xPortInIsrContext(void)
{
    return s_xt_isr_status != 0;
}

/* DIAG 11-19 (ESPurna, temporary): the nesting of the port's critical section, for the list checks of
 * freertos_tasks_c_additions.h (a task that blocks inside it cannot yield) */
UBaseType_t uxPortCriticalNesting(void)
{
    return uxCriticalNesting;
}

/* DIAG 11-19 (ESPurna, temporary): the untouched bottom of the interrupt stack, checked from the idle hook (task
 * context: an interrupt that prints would itself eat the stack it reports on). The first 32 words only, cheap enough
 * for every idle pass; once less than 96 bytes stayed untouched it prints the exact figure once */
static void isr_stack_check(void)
{
    extern uint32_t _chip_interrupt_stk[];
    static int s_reported;

    if (s_reported) {
        return;
    }

    int words = 0;
    while ((words < 32) && (_chip_interrupt_stk[words] == ISR_STACK_PATTERN)) {
        words++;
    }

    if (words < 24) {
        s_reported = 1;
        ets_printf("DIAG isr stack: %d of %d bytes never used\n", words * 4, CONFIG_FREERTOS_ISR_STACKSIZE);
    }
}

/* DIAG 11-18 (ESPurna): configASSERT of the kernel */
extern UBaseType_t uxPortSchedulerSuspended(void);
void IRAM_ATTR vPortAssert(const char *file, int line, void *caller)
{
    portDISABLE_INTERRUPTS();
    ets_printf("DIAG assert %s:%d from %p task %s susp %u isr %d nmi %d\n", file, line, caller,
               xTaskGetCurrentTaskHandle() ? pcTaskGetName(NULL) : "-", (unsigned) uxPortSchedulerSuspended(),
               s_xt_isr_status != 0, NMIIrqIsOn);
    abort();
}

/* DIAG 11-18 (ESPurna, temporary): FreeRTOSConfig.h trace macros */
static uint32_t s_trace_blocking;

void vPortTraceBlocking(const char *what, void *object, void *caller)
{
    /* the kernel suspends the scheduler itself right before these trace points: a second level is somebody else's */
    const int suspended = uxPortSchedulerSuspended() >= 2;
    const int isr = s_xt_isr_status != 0;
    if ((suspended || isr || NMIIrqIsOn) && (s_trace_blocking < 40)) {
        ++s_trace_blocking;
        ets_printf("DIAG blocking %s %p from %p task %s susp %d isr %d nmi %d\n", what, object, caller,
                   pcTaskGetName(NULL), suspended, isr, NMIIrqIsOn);
    }
}

void __attribute__((weak, noreturn)) vApplicationStackOverflowHook(xTaskHandle xTask, const char *pcTaskName)
{
    ets_printf("***ERROR*** A stack overflow in task %s has been detected.\r\n", pcTaskName);
    abort();
}

signed portBASE_TYPE xTaskGenericCreate(TaskFunction_t pxTaskCode,
                                        const signed char * const pcName,
                                        unsigned short usStackDepth,
                                        void *pvParameters,
                                        unsigned portBASE_TYPE uxPriority,
                                        TaskHandle_t *pxCreatedTask,
                                        StackType_t *puxStackBuffer,
                                        const MemoryRegion_t * const xRegions)
{
    (void)puxStackBuffer;
    (void)xRegions;
    return xTaskCreate(pxTaskCode, (const char * const)pcName, usStackDepth,
                       pvParameters, uxPriority, pxCreatedTask);
}

BaseType_t xQueueGenericReceive(QueueHandle_t xQueue, void * const pvBuffer,
                                TickType_t xTicksToWait, const BaseType_t xJustPeeking)
{
    configASSERT(xJustPeeking == 0);
    return xQueueReceive(xQueue, pvBuffer, xTicksToWait);
}

void esp_internal_idle_hook(void)
{
    isr_stack_check();

    esp_task_wdt_reset();

    esp_sleep_start();
}

#ifndef DISABLE_FREERTOS
/* ESPurna (FreeRTOS-Kernel V11): the SDK's 10.0.1 kernel called the SDK's idle and tick work from its own patched
 * tasks.c; the kernel package is unchanged, so the hooks of the kernel do it */
#if configUSE_IDLE_HOOK == 1
void vApplicationIdleHook(void)
{
#ifdef CONFIG_FREERTOS_EXTENED_HOOKS
    esp_vApplicationIdleHook();
#endif
    esp_internal_idle_hook();
}
#endif

#if configUSE_TICK_HOOK == 1
void vApplicationTickHook(void)
{
#ifdef CONFIG_FREERTOS_EXTENED_HOOKS
    esp_vApplicationTickHook();
#endif
}
#endif

/* the delete callbacks of a deleted task's thread local storage pointers (portCLEAN_UP_TCB) */
void vPortCleanUpTLS(void **ppvPointers)
{
    for (int x = 0; x < portTLS_POINTERS; x++) {
        TlsDeleteCallbackFunction_t callback = (TlsDeleteCallbackFunction_t)ppvPointers[portTLS_POINTERS + x];
        if (callback) {
            callback(x, ppvPointers[x]);
        }
    }
}

/* the kernel's allocations on the SDK's heap, as the SDK's 10.0.1 portable.h mapped them */
void *pvPortMalloc(size_t xSize)
{
    return heap_caps_malloc(xSize, MALLOC_CAP_8BIT | MALLOC_CAP_32BIT | MALLOC_CAP_DMA);
}

void vPortFree(void *pv)
{
    heap_caps_free(pv);
}

void *pvPortCalloc(size_t xNum, size_t xSize)
{
    return heap_caps_calloc(xNum, xSize, MALLOC_CAP_32BIT);
}

void *pvPortZalloc(size_t xSize)
{
    return heap_caps_zalloc(xSize, MALLOC_CAP_32BIT);
}

void *pvPortRealloc(void *pv, size_t xSize)
{
    return heap_caps_realloc(pv, xSize, MALLOC_CAP_32BIT);
}

size_t xPortGetFreeHeapSize(void)
{
    return heap_caps_get_free_size(MALLOC_CAP_32BIT);
}

size_t xPortGetMinimumEverFreeHeapSize(void)
{
    return heap_caps_get_minimum_free_size(MALLOC_CAP_32BIT);
}
#endif

uint32_t xPortGetTickRateHz(void)
{
    return (uint32_t)configTICK_RATE_HZ;
}

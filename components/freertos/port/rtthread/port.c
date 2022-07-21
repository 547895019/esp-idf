/*
    FreeRTOS V8.2.3 - Copyright (C) 2015 Real Time Engineers Ltd.
    All rights reserved

    VISIT http://www.FreeRTOS.org TO ENSURE YOU ARE USING THE LATEST VERSION.

    This file is part of the FreeRTOS distribution and was contributed
    to the project by Technolution B.V. (www.technolution.nl,
    freertos-riscv@technolution.eu) under the terms of the FreeRTOS
    contributors license.

    FreeRTOS is free software; you can redistribute it and/or modify it under
    the terms of the GNU General Public License (version 2) as published by the
    Free Software Foundation >>>> AND MODIFIED BY <<<< the FreeRTOS exception.

    ***************************************************************************
    >>!   NOTE: The modification to the GPL is included to allow you to     !<<
    >>!   distribute a combined work that includes FreeRTOS without being   !<<
    >>!   obliged to provide the source code for proprietary components     !<<
    >>!   outside of the FreeRTOS kernel.                                   !<<
    ***************************************************************************

    FreeRTOS is distributed in the hope that it will be useful, but WITHOUT ANY
    WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
    FOR A PARTICULAR PURPOSE.  Full license text is available on the following
    link: http://www.freertos.org/a00114.html

    ***************************************************************************
     *                                                                       *
     *    FreeRTOS provides completely free yet professionally developed,    *
     *    robust, strictly quality controlled, supported, and cross          *
     *    platform software that is more than just the market leader, it     *
     *    is the industry's de facto standard.                               *
     *                                                                       *
     *    Help yourself get started quickly while simultaneously helping     *
     *    to support the FreeRTOS project by purchasing a FreeRTOS           *
     *    tutorial book, reference manual, or both:                          *
     *    http://www.FreeRTOS.org/Documentation                              *
     *                                                                       *
    ***************************************************************************

    http://www.FreeRTOS.org/FAQHelp.html - Having a problem?  Start by reading
    the FAQ page "My application does not run, what could be wrong?".  Have you
    defined configASSERT()?

    http://www.FreeRTOS.org/support - In return for receiving this top quality
    embedded software for free we request you assist our global community by
    participating in the support forum.

    http://www.FreeRTOS.org/training - Investing in training allows your team to
    be as productive as possible as early as possible.  Now you can receive
    FreeRTOS training directly from Richard Barry, CEO of Real Time Engineers
    Ltd, and the world's leading authority on the world's leading RTOS.

    http://www.FreeRTOS.org/plus - A selection of FreeRTOS ecosystem products,
    including FreeRTOS+Trace - an indispensable productivity tool, a DOS
    compatible FAT file system, and our tiny thread aware UDP/IP stack.

    http://www.FreeRTOS.org/labs - Where new FreeRTOS products go to incubate.
    Come and try FreeRTOS+TCP, our new open source TCP/IP stack for FreeRTOS.

    http://www.OpenRTOS.com - Real Time Engineers ltd. license FreeRTOS to High
    Integrity Systems ltd. to sell under the OpenRTOS brand.  Low cost OpenRTOS
    licenses offer ticketed support, indemnification and commercial middleware.

    http://www.SafeRTOS.com - High Integrity Systems also provide a safety
    engineered and independently SIL3 certified version for use in safety and
    mission critical applications that require provable dependability.

    1 tab == 4 spaces!
*/

/*-----------------------------------------------------------------------
 * Implementation of functions defined in portable.h for the RISC-V port.
 *----------------------------------------------------------------------*/

#include "sdkconfig.h"
#include <string.h>
#include "soc/soc_caps.h"
#include "soc/periph_defs.h"
#include "soc/system_reg.h"
#include "hal/systimer_hal.h"
#include "hal/systimer_ll.h"
#include "riscv/rvruntime-frames.h"
#include "riscv/riscv_interrupts.h"
#include "riscv/interrupt.h"
#include "esp_private/crosscore_int.h"
#include "esp_private/pm_trace.h"
#include "esp_attr.h"
#include "esp_system.h"
#include "esp_intr_alloc.h"
#include "esp_debug_helpers.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "FreeRTOS.h"       /* This pulls in portmacro.h */
#include "task.h"
#include "timers.h"
#include "portmacro.h"
#include "port_systick.h"

#include "rtthread.h"
#include "rthw.h"
/* ---------------------------------------------------- Variables ------------------------------------------------------
 *
 * ------------------------------------------------------------------------------------------------------------------ */

static const char *TAG = "cpu_start"; // [refactor-todo]: might be appropriate to change in the future, but

/**
 * @brief A variable is used to keep track of the critical section nesting.
 * @note This variable has to be stored as part of the task context and must be initialized to a non zero value
 *       to ensure interrupts don't inadvertently become unmasked before the scheduler starts.
 *       As it is stored as part of the task context it will automatically be set to 0 when the first task is started.
 */

/* ------------------------------------------------ FreeRTOS Portable --------------------------------------------------
 * - Provides implementation for functions required by FreeRTOS
 * - Declared in portable.h
 * ------------------------------------------------------------------------------------------------------------------ */

// ----------------- Scheduler Start/End and Hook -------------------

BaseType_t xPortStartScheduler(void)
{
    vPortSetupTimer();
    xTimerCreateTimerTask();
    return pdFALSE;
}

void vPortEndScheduler(void)
{
    /* very unlikely this function will be called, so just trap here */
    abort();
}

// ------------------------ Stack --------------------------
StackType_t *pxPortInitialiseStack(StackType_t *pxTopOfStack, TaskFunction_t pxCode, void *pvParameters)
{
    //TODO: IDF-2393
    return RT_NULL;
}

/* ---------------------------------------------- Port Implementations -------------------------------------------------
 *
 * ------------------------------------------------------------------------------------------------------------------ */

// ---------------------- rtthread ------------------------
#ifdef RT_USING_HEAP
#ifdef RT_USING_USERHEAP
void rt_system_heap_init(void *begin_addr, void *end_addr){
    heap_caps_print_heap_info(MALLOC_CAP_DEFAULT);
}

void *rt_malloc(rt_size_t size)
{
    return malloc(size);
}
void rt_free(void *ptr)
{
    free(ptr);
}
void *rt_realloc(void *ptr, rt_size_t nbytes)
{
    return realloc(ptr,nbytes);
}
void *rt_calloc(rt_size_t count, rt_size_t size){
    return calloc(count,size);
}
void rt_memory_info(rt_size_t *total,
                    rt_size_t *used,
                    rt_size_t *max_used){
         *total = heap_caps_get_total_size( MALLOC_CAP_DEFAULT );     
       *max_used =  *total - heap_caps_get_minimum_free_size( MALLOC_CAP_DEFAULT );
        *used =   *total - heap_caps_get_free_size( MALLOC_CAP_DEFAULT );       
}
#endif                 
#endif

// --------------------- Interrupts ------------------------
BaseType_t xPortInIsrContext(void)
{
    return rt_interrupt_get_nest();
}

BaseType_t IRAM_ATTR xPortInterruptedFromISRContext(void)
{
    /* For single core, this can be the same as xPortInIsrContext() because reading it is atomic */
    return rt_interrupt_get_nest();
}

// ------------------ Critical Sections --------------------

void vPortEnterCritical(void)
{
    rt_enter_critical();
}

void vPortExitCritical(void)
{
    rt_exit_critical();
}

// ---------------------- Yielding -------------------------

int vPortSetInterruptMask(void)
{
    return rt_hw_interrupt_disable();
}

void vPortClearInterruptMask(int mask)
{
    rt_hw_interrupt_enable(mask);
}

void vPortYield(void)
{
    rt_thread_yield();
}

void vPortYieldFromISR( void )
{
    rt_thread_yield();
}

void vPortYieldOtherCore(BaseType_t coreid)
{
    ESP_LOGI(TAG, "vPortYieldOtherCore no support!");
}

// ------------------- Hook Functions ----------------------

void __attribute__((weak)) vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
#define ERR_STR1 "***ERROR*** A stack overflow in task "
#define ERR_STR2 " has been detected."
    const char *str[] = {ERR_STR1, pcTaskName, ERR_STR2};

    char buf[sizeof(ERR_STR1) + CONFIG_FREERTOS_MAX_TASK_NAME_LEN + sizeof(ERR_STR2) + 1 /* null char */] = {0};

    char *dest = buf;
    for (int i = 0; i < sizeof(str) / sizeof(str[0]); i++) {
        dest = strcat(dest, str[i]);
    }
    esp_system_abort(buf);
}

// ----------------------- System --------------------------

uint32_t xPortGetTickRateHz(void)
{
    return (uint32_t)configTICK_RATE_HZ;
}

void vPortSetStackWatchpoint(void *pxStackStart)
{
    ESP_LOGI(TAG, "vPortSetStackWatchpoint no support!");
}

/* ---------------------------------------------- Misc Implementations -------------------------------------------------
 *
 * ------------------------------------------------------------------------------------------------------------------ */

// --------------------- App Start-up ----------------------

/* [refactor-todo]: See if we can include this through a header */
extern void esp_startup_start_app_common(void);

void esp_startup_start_app(void)
{
    esp_startup_start_app_common();
    ESP_LOGI(TAG, "Starting scheduler.");
    vTaskStartScheduler();
}

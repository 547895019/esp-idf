/*
 * FreeRTOS Kernel V10.4.3
 * Copyright (C) 2020 Amazon.com, Inc. or its affiliates.  All Rights Reserved.
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

/* Standard includes. */
#include "freertos_internal.h"
#define ADAPT_DEBUG_TIMER 1

#include <stdlib.h>

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "timers.h"

#if ( INCLUDE_xTimerPendFunctionCall == 1 ) && ( configUSE_TIMERS == 0 )
#error configUSE_TIMERS must be set to 1 to make the xTimerPendFunctionCall() function available.
#endif

/* This entire source file will be skipped if the application is not configured
 * to include software timer functionality.  This #if is closed at the very bottom
 * of this file.  If you want to include software timer functionality then ensure
 * configUSE_TIMERS is set to 1 in FreeRTOSConfig.h. */
#if ( configUSE_TIMERS == 1 )
/* The old xTIMER name is maintained above then typedefed to the new Timer_t
 * name below to enable the use of older kernel aware debuggers. */
typedef StaticTimer_t Timer_t;

/*-----------------------------------------------------------*/

BaseType_t xTimerCreateTimerTask( void )
{
    BaseType_t xReturn = pdPASS;
    /* timer thread initialization */
    rt_system_timer_thread_init();
    return xReturn;
}
/*-----------------------------------------------------------*/

#if ( configSUPPORT_DYNAMIC_ALLOCATION == 1 )

TimerHandle_t xTimerCreate( const char * const pcTimerName, /*lint !e971 Unqualified char types are allowed for strings and single characters only. */
                            const TickType_t xTimerPeriodInTicks,
                            const UBaseType_t uxAutoReload,
                            void * const pvTimerID,
                            TimerCallbackFunction_t pxCallbackFunction )
{
    Timer_t *pxTimer = RT_NULL;

    FREERTOS_ADAPT_LOG(ADAPT_DEBUG_TIMER, ("xTimerCreate %s id %d\r\n", pcTimerName,pvTimerID));
    pxTimer = (Timer_t*)rt_malloc(sizeof(Timer_t));
    RT_ASSERT( pxTimer );
    pxTimer->pvTimerID = pvTimerID;
    pxTimer->ucStaticallyAllocated = pdFALSE;
    rt_timer_init((rt_timer_t)pxTimer,
                  pcTimerName,
                  pxCallbackFunction,
                  RT_NULL,
                  xTimerPeriodInTicks,
                  (uxAutoReload == 1 ? RT_TIMER_FLAG_PERIODIC : RT_TIMER_FLAG_ONE_SHOT));
                  

    return pxTimer;
}

#endif /* configSUPPORT_DYNAMIC_ALLOCATION */
/*-----------------------------------------------------------*/

#if ( configSUPPORT_STATIC_ALLOCATION == 1 )

TimerHandle_t xTimerCreateStatic( const char * const pcTimerName, /*lint !e971 Unqualified char types are allowed for strings and single characters only. */
                                  const TickType_t xTimerPeriodInTicks,
                                  const UBaseType_t uxAutoReload,
                                  void * const pvTimerID,
                                  TimerCallbackFunction_t pxCallbackFunction,
                                  StaticTimer_t * pxTimerBuffer )
{
    Timer_t *pxTimer = pxTimerBuffer;
    RT_ASSERT( pxTimer );
    pxTimer->pvTimerID = pvTimerID;
    pxTimer->ucStaticallyAllocated = pdTRUE;
    FREERTOS_ADAPT_LOG(ADAPT_DEBUG_TIMER, ("xTimerCreateStatic %s id %d\r\n", pcTimerName,pvTimerID));
    rt_timer_init((rt_timer_t)pxTimer,
                  pcTimerName,
                  pxCallbackFunction,
                  RT_NULL,
                  xTimerPeriodInTicks,
                  (uxAutoReload == 1 ? RT_TIMER_FLAG_PERIODIC : RT_TIMER_FLAG_ONE_SHOT));
    return pxTimer;
}

#endif /* configSUPPORT_STATIC_ALLOCATION */
/*-----------------------------------------------------------*/

/*-----------------------------------------------------------*/

BaseType_t xTimerGenericCommand( TimerHandle_t xTimer,
                                 const BaseType_t xCommandID,
                                 const TickType_t xOptionalValue,
                                 BaseType_t * const pxHigherPriorityTaskWoken,
                                 const TickType_t xTicksToWait )
{
    rt_err_t        ret;
    Timer_t *pxTimer = xTimer;
    RT_ASSERT( pxTimer );
    switch (xCommandID) {
    case tmrCOMMAND_START_DONT_TRACE:
    case tmrCOMMAND_RESET:
    case tmrCOMMAND_START:
    case tmrCOMMAND_RESET_FROM_ISR:
    case tmrCOMMAND_START_FROM_ISR:
        ret = rt_timer_start((rt_timer_t)pxTimer);
        break;
    case tmrCOMMAND_STOP:
    case tmrCOMMAND_STOP_FROM_ISR:
        ret = rt_timer_stop((rt_timer_t)pxTimer);
        break;
    case tmrCOMMAND_DELETE:

        ret = rt_timer_detach((rt_timer_t)pxTimer);
#if ( ( configSUPPORT_DYNAMIC_ALLOCATION == 1 ) && ( configSUPPORT_STATIC_ALLOCATION == 1 ) )
        if(pxTimer->ucStaticallyAllocated == pdFALSE) {
            rt_free(pxTimer);
        }
#endif
        break;
    case tmrCOMMAND_CHANGE_PERIOD:
    case tmrCOMMAND_CHANGE_PERIOD_FROM_ISR:
        ret = rt_timer_control((rt_timer_t)pxTimer, RT_TIMER_CTRL_SET_TIME,&xOptionalValue);
        break;
    default:
        FREERTOS_ADAPT_LOG(ADAPT_DEBUG_TIMER, ("timer cmd(%d) not realize yet\r\n", xCommandID));
        ret = RT_ENOSYS;
        break;
    }

    if (RT_NULL != pxHigherPriorityTaskWoken) {
        *pxHigherPriorityTaskWoken = pdFALSE;
    }

    return (ret == RT_EOK) ? pdPASS : pdFAIL;
}
/*-----------------------------------------------------------*/

TaskHandle_t xTimerGetTimerDaemonTaskHandle( void )
{
    /* If xTimerGetTimerDaemonTaskHandle() is called before the scheduler has been
     * started, then xTimerTaskHandle will be NULL. */
    return xTaskGetHandle("timer");
}
/*-----------------------------------------------------------*/

TickType_t xTimerGetPeriod( TimerHandle_t xTimer )
{
    rt_timer_t pxTimer = (rt_timer_t)xTimer;
    RT_ASSERT( pxTimer );
    return (TickType_t) pxTimer->init_tick;
}
/*-----------------------------------------------------------*/

void vTimerSetReloadMode( TimerHandle_t xTimer,
                          const UBaseType_t uxAutoReload )
{
    rt_timer_t pxTimer = (rt_timer_t)xTimer;
    RT_ASSERT( pxTimer );
    rt_base_t level;
    level = rt_hw_interrupt_disable();
    {
        if(uxAutoReload == 0)
            pxTimer->parent.flag &= ~RT_TIMER_FLAG_PERIODIC;
        else
            pxTimer->parent.flag |= RT_TIMER_FLAG_PERIODIC;
    }
    rt_hw_interrupt_enable(level);
}
/*-----------------------------------------------------------*/

UBaseType_t uxTimerGetReloadMode( TimerHandle_t xTimer )
{
    UBaseType_t xReturn;
    rt_timer_t pxTimer = (rt_timer_t)xTimer;
    RT_ASSERT( pxTimer );
    rt_base_t level;
    level = rt_hw_interrupt_disable();
    {
        if(pxTimer->parent.flag & RT_TIMER_FLAG_PERIODIC) {
            xReturn = pdTRUE;
        } else {
            xReturn = pdFALSE;
        }
    }
    rt_hw_interrupt_enable(level);
    return xReturn;
}
/*-----------------------------------------------------------*/

TickType_t xTimerGetExpiryTime( TimerHandle_t xTimer )
{
    rt_timer_t pxTimer = (rt_timer_t)xTimer;
    RT_ASSERT( pxTimer );
    TickType_t xReturn;
    xReturn = (TickType_t)(rt_tick_get() + pxTimer->timeout_tick);
    return xReturn;

}
/*-----------------------------------------------------------*/

const char * pcTimerGetName( TimerHandle_t xTimer ) /*lint !e971 Unqualified char types are allowed for strings and single characters only. */
{
    rt_timer_t pxTimer = (rt_timer_t)xTimer;
    RT_ASSERT( pxTimer );
    return pxTimer->parent.name;
}
/*-----------------------------------------------------------*/

BaseType_t xTimerIsTimerActive( TimerHandle_t xTimer )
{
    BaseType_t xReturn;
    rt_timer_t pxTimer = (rt_timer_t)xTimer;
    RT_ASSERT( pxTimer );
    rt_base_t level;
    level = rt_hw_interrupt_disable();
    {
        if(pxTimer->parent.flag & RT_TIMER_FLAG_ACTIVATED) {
            xReturn = pdTRUE;
        } else {
            xReturn = pdFALSE;
        }
    }
    rt_hw_interrupt_enable(level);

    return xReturn;
} /*lint !e818 Can't be pointer to const due to the typedef. */
/*-----------------------------------------------------------*/

void * pvTimerGetTimerID( const TimerHandle_t xTimer )
{
    Timer_t * const pxTimer = xTimer;
    void * pvReturn;
    RT_ASSERT( xTimer );
    rt_base_t level;
    level = rt_hw_interrupt_disable();
    {
        pvReturn = pxTimer->pvTimerID;
    }
    rt_hw_interrupt_enable(level);
    return pvReturn;
}
/*-----------------------------------------------------------*/

void vTimerSetTimerID( TimerHandle_t xTimer,
                       void * pvNewID )
{
    Timer_t * const pxTimer = xTimer;

    RT_ASSERT( xTimer );

    rt_base_t level;
    level = rt_hw_interrupt_disable();
    {
        pxTimer->pvTimerID = pvNewID;
    }
    rt_hw_interrupt_enable(level);
}
/*-----------------------------------------------------------*/

#if ( INCLUDE_xTimerPendFunctionCall == 1 )

BaseType_t xTimerPendFunctionCallFromISR( PendedFunction_t xFunctionToPend,
        void * pvParameter1,
        uint32_t ulParameter2,
        BaseType_t * pxHigherPriorityTaskWoken )
{
    return pdFAIL;
}

#endif /* INCLUDE_xTimerPendFunctionCall */
/*-----------------------------------------------------------*/

#if ( INCLUDE_xTimerPendFunctionCall == 1 )

BaseType_t xTimerPendFunctionCall( PendedFunction_t xFunctionToPend,
                                   void * pvParameter1,
                                   uint32_t ulParameter2,
                                   TickType_t xTicksToWait )
{
    return pdFAIL;
}

#endif /* INCLUDE_xTimerPendFunctionCall */
/*-----------------------------------------------------------*/

#if ( configUSE_TRACE_FACILITY == 1 )

UBaseType_t uxTimerGetTimerNumber( TimerHandle_t xTimer )
{
    return ( ( Timer_t * ) xTimer )->uxTimerNumber;
}

#endif /* configUSE_TRACE_FACILITY */
/*-----------------------------------------------------------*/

#if ( configUSE_TRACE_FACILITY == 1 )

void vTimerSetTimerNumber( TimerHandle_t xTimer,
                           UBaseType_t uxTimerNumber )
{
    ( ( Timer_t * ) xTimer )->uxTimerNumber = uxTimerNumber;
}

#endif /* configUSE_TRACE_FACILITY */
/*-----------------------------------------------------------*/

/* This entire source file will be skipped if the application is not configured
 * to include software timer functionality.  If you want to include software timer
 * functionality then ensure configUSE_TIMERS is set to 1 in FreeRTOSConfig.h. */
#endif /* configUSE_TIMERS == 1 */

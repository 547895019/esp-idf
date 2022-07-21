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
#define ADAPT_DEBUG_TASK 0

#include <stdlib.h>
#include <string.h>


/* FreeRTOS includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"
#include "stack_macros.h"

#ifndef configIDLE_RTTHREAD_IDLE_TASK_NAME
#define configIDLE_RTTHREAD_IDLE_TASK_NAME    "tidle"
#endif

BaseType_t uxSchedulerRunning = taskSCHEDULER_NOT_STARTED;

typedef StaticTask_t Task_t;

/* Set configUSE_STATS_FORMATTING_FUNCTIONS to 2 to include the stats formatting
 * functions but without including stdio.h here. */
#if ( configUSE_STATS_FORMATTING_FUNCTIONS == 1 )

/* At the bottom of this file are two optional functions that can be used
 * to generate human readable text from the raw data generated by the
 * uxTaskGetSystemState() function.  Note the formatting functions are provided
 * for convenience only, and are NOT considered part of the kernel. */
#include <stdio.h>
#endif /* configUSE_STATS_FORMATTING_FUNCTIONS == 1 ) */

static void xPortApplicationIdleHook(void)
{
#if ( configUSE_IDLE_HOOK == 1 )
    {
        extern void vApplicationIdleHook( void );
        /* Call the user defined function from within the idle task.  This
         * allows the application designer to add background functionality
         * without the overhead of a separate task.
         * NOTE: vApplicationIdleHook() MUST NOT, UNDER ANY CIRCUMSTANCES,
         * CALL A FUNCTION THAT MIGHT BLOCK. */
        vApplicationIdleHook();
    }
#endif /* configUSE_IDLE_HOOK */
#if ( CONFIG_FREERTOS_LEGACY_HOOKS == 1 )
    {
        /* Call the esp-idf hook system */
        esp_vApplicationIdleHook();
    }
#endif /* CONFIG_FREERTOS_LEGACY_HOOKS */
}
static void xPortApplicationSchedulerHook(rt_thread_t from, rt_thread_t to)
{
    if(uxSchedulerRunning == taskSCHEDULER_NOT_STARTED)
        uxSchedulerRunning = taskSCHEDULER_RUNNING;
}

#if ( configSUPPORT_STATIC_ALLOCATION == 1 )

TaskHandle_t xTaskCreateStaticPinnedToCore( TaskFunction_t pvTaskCode,
        const char * const pcName,
        const uint32_t ulStackDepth,
        void * const pvParameters,
        UBaseType_t uxPriority,
        StackType_t * const pxStackBuffer,
        StaticTask_t * const pxTaskBuffer,
        const BaseType_t xCoreID )
{
    rt_err_t ret;

    FREERTOS_ADAPT_LOG(ADAPT_DEBUG_TASK, ("xTaskCreateStaticPinnedToCore %s coreid %x\r\n", pcName,xCoreID));
    pxTaskBuffer->ucStaticallyAllocated = pdTRUE;
#if ( configUSE_TASK_NOTIFICATIONS == 1 )
    for(rt_base_t i= 0; i<configTASK_NOTIFICATION_ARRAY_ENTRIES; i++)
        pxTaskBuffer->ulNotified[i] = RT_NULL;
#endif
    ret = rt_thread_init((rt_thread_t)pxTaskBuffer,
                         pcName,
                         pvTaskCode,
                         pvParameters,
                         pxStackBuffer,
                         ulStackDepth,
                         uxPriority,
                         20);
    if (RT_EOK != ret) {
        return RT_NULL;
    }

    ret = rt_thread_startup((rt_thread_t)pxTaskBuffer);
    if (RT_EOK != ret) {
        return RT_NULL;
    }
#ifdef RT_USING_SMP
    if(xCoreID != tskNO_AFFINITY) {
        rt_size_t cpu = xCoreID;
        rt_thread_control((rt_thread_t)pxTaskBuffer,RT_THREAD_CTRL_BIND_CPU,(void *)cpu);
    }
#endif
    return (TaskHandle_t)pxTaskBuffer;
}

#endif /* SUPPORT_STATIC_ALLOCATION */
/*-----------------------------------------------------------*/

#if ( ( portUSING_MPU_WRAPPERS == 1 ) && ( configSUPPORT_STATIC_ALLOCATION == 1 ) )

BaseType_t xTaskCreateRestrictedStatic( const TaskParameters_t * const pxTaskDefinition,
                                        TaskHandle_t * pxCreatedTask )
{
    FREERTOS_ADAPT_LOG(ADAPT_DEBUG_TASK, ("xTaskCreateRestrictedStatic not support\n"));
    return pdFAIL;
}

#endif /* ( portUSING_MPU_WRAPPERS == 1 ) && ( configSUPPORT_STATIC_ALLOCATION == 1 ) */
/*-----------------------------------------------------------*/

#if ( ( portUSING_MPU_WRAPPERS == 1 ) && ( configSUPPORT_DYNAMIC_ALLOCATION == 1 ) )

BaseType_t xTaskCreateRestricted( const TaskParameters_t * const pxTaskDefinition,
                                  TaskHandle_t * pxCreatedTask )
{
    FREERTOS_ADAPT_LOG(ADAPT_DEBUG_TASK, ("xTaskCreateRestricted not support\n"));
    return pdFAIL;
}

#endif /* portUSING_MPU_WRAPPERS */
/*-----------------------------------------------------------*/

#if ( configSUPPORT_DYNAMIC_ALLOCATION == 1 )

BaseType_t xTaskCreatePinnedToCore( TaskFunction_t pvTaskCode,
                                    const char * const pcName, /*lint !e971 Unqualified char types are allowed for strings and single characters only. */
                                    const uint32_t usStackDepth,
                                    void * const pvParameters,
                                    UBaseType_t uxPriority,
                                    TaskHandle_t * const pvCreatedTask,
                                    const BaseType_t xCoreID)
{
    rt_err_t     ret;
    void        *stack_begin;

    RT_ASSERT(((uxPriority & (~portPRIVILEGE_BIT)) < configMAX_PRIORITIES));
    Task_t *task_handle_t= (Task_t*)rt_malloc(sizeof(Task_t));
    RT_ASSERT( task_handle_t );
    stack_begin = (void *)rt_malloc(usStackDepth);
    RT_ASSERT( stack_begin );
    FREERTOS_ADAPT_LOG(ADAPT_DEBUG_TASK, ("xTaskCreatePinnedToCore %s coreid %x\r\n", pcName,xCoreID));

    task_handle_t->ucStaticallyAllocated = pdFALSE;
#if ( configUSE_TASK_NOTIFICATIONS == 1 )
    for(rt_base_t i= 0; i<configTASK_NOTIFICATION_ARRAY_ENTRIES; i++)
        task_handle_t->ulNotified[i] = RT_NULL;
#endif
    ret = rt_thread_init((rt_thread_t)task_handle_t,
                         pcName,
                         pvTaskCode,
                         pvParameters,
                         stack_begin,
                         usStackDepth,
                         uxPriority,
                         20);
    if (RT_EOK != ret) {
        rt_free(task_handle_t);
        return pdFAIL;
    }

    ret = rt_thread_startup((rt_thread_t)task_handle_t);
    if (RT_EOK != ret) {
        rt_free(stack_begin);
        rt_free(task_handle_t);
        return pdFAIL;
    }

#ifdef RT_USING_SMP
    if(xCoreID != tskNO_AFFINITY) {
        rt_size_t cpu = xCoreID;
        rt_thread_control((rt_thread_t)task_handle_t,RT_THREAD_CTRL_BIND_CPU,(void *)cpu);
    }
#endif

    if (RT_NULL != pvCreatedTask) {
        *pvCreatedTask = task_handle_t;
    }
    return pdPASS;
}

#endif /* configSUPPORT_DYNAMIC_ALLOCATION */

#if ( INCLUDE_vTaskDelete == 1 )

void vTaskDelete( TaskHandle_t xTaskToDelete )
{
    Task_t * xTask = RT_NULL;
    if(xTaskToDelete) {
        xTask = (Task_t *)xTaskToDelete;
    } else {
        xTask = (Task_t *)rt_thread_self();
    }

    if(xTask) {
        FREERTOS_ADAPT_LOG(ADAPT_DEBUG_TASK, ("vTaskDelete %s\n",((rt_thread_t)xTask)->name));
        rt_thread_detach((rt_thread_t)xTask);
#if ( ( configSUPPORT_STATIC_ALLOCATION == 1 ) && ( configSUPPORT_DYNAMIC_ALLOCATION == 1 ) )
        if(xTask->ucStaticallyAllocated == pdFALSE) {
            rt_free(((rt_thread_t)xTask)->stack_addr);
            rt_free(xTask);
        }
#endif
#if ( configUSE_TASK_NOTIFICATIONS == 1 )
        for(rt_base_t i= 0; i<configTASK_NOTIFICATION_ARRAY_ENTRIES; i++) {
            if(xTask->ulNotified[i]) {
                rt_uint8_t type = ((rt_object_t)(xTask->ulNotified[i]))->type & ~RT_Object_Class_Static;

                if(type == RT_Object_Class_MessageQueue) {

                    rt_event_delete((rt_event_t)(xTask->ulNotified[i]));
                }
                xTask->ulNotified[i] = RT_NULL;
            }
        }
#endif
    }
}

#endif /* INCLUDE_vTaskDelete */
/*-----------------------------------------------------------*/

#if ( INCLUDE_xTaskDelayUntil == 1 )
#ifdef ESP_PLATFORM
// backward binary compatibility - remove later
#undef vTaskDelayUntil
void vTaskDelayUntil( TickType_t * const pxPreviousWakeTime,
                      const TickType_t xTimeIncrement )
{
    xTaskDelayUntil(pxPreviousWakeTime, xTimeIncrement);
}
#endif // ESP_PLATFORM

BaseType_t xTaskDelayUntil( TickType_t * const pxPreviousWakeTime,
                            const TickType_t xTimeIncrement )
{
    return rt_thread_delay_until(pxPreviousWakeTime, xTimeIncrement);
}

#endif /* INCLUDE_xTaskDelayUntil */
/*-----------------------------------------------------------*/
#if ( INCLUDE_vTaskDelay == 1 )

void vTaskDelay( const TickType_t xTicksToDelay )
{
    rt_thread_delay(xTicksToDelay);
}

#endif /* INCLUDE_vTaskDelay */
/*-----------------------------------------------------------*/

#if ( ( INCLUDE_eTaskGetState == 1 ) || ( configUSE_TRACE_FACILITY == 1 ) || ( INCLUDE_xTaskAbortDelay == 1 ) )

eTaskState eTaskGetState( TaskHandle_t xTask )
{
    eTaskState eReturn = eInvalid;
    rt_uint8_t stat;
    rt_thread_t thread = xTask;
    stat = (thread->stat & RT_THREAD_STAT_MASK);
    if (stat == RT_THREAD_READY)        eReturn = eReady;
    else if (stat == RT_THREAD_SUSPEND) eReturn = eSuspended;
    else if (stat == RT_THREAD_CLOSE)   eReturn = eDeleted;
    else if (stat == RT_THREAD_RUNNING) eReturn = eRunning;

    return eReturn;
} /*lint !e818 xTask cannot be a pointer to const because it is a typedef. */

#endif /* INCLUDE_eTaskGetState */
/*-----------------------------------------------------------*/

#if ( INCLUDE_uxTaskPriorityGet == 1 )

UBaseType_t uxTaskPriorityGet( const TaskHandle_t xTask )
{
    return ((rt_thread_t)xTask)->current_priority;
}

#endif /* INCLUDE_uxTaskPriorityGet */
/*-----------------------------------------------------------*/

#if ( INCLUDE_uxTaskPriorityGet == 1 )

UBaseType_t uxTaskPriorityGetFromISR( const TaskHandle_t xTask )
{
    return uxTaskPriorityGet(xTask);
}

#endif /* INCLUDE_uxTaskPriorityGet */
/*-----------------------------------------------------------*/

#if ( INCLUDE_vTaskPrioritySet == 1 )

void vTaskPrioritySet( TaskHandle_t xTask,
                       UBaseType_t uxNewPriority )
{
    rt_uint8_t arg = uxNewPriority;
    rt_thread_control((rt_thread_t)xTask,RT_THREAD_CTRL_CHANGE_PRIORITY,&arg);
}

#endif /* INCLUDE_vTaskPrioritySet */
/*-----------------------------------------------------------*/

#if ( INCLUDE_vTaskSuspend == 1 )

void vTaskSuspend( TaskHandle_t xTaskToSuspend )
{
    rt_thread_suspend((rt_thread_t)xTaskToSuspend);
}

#endif /* INCLUDE_vTaskSuspend */

#if ( INCLUDE_vTaskSuspend == 1 )

void vTaskResume( TaskHandle_t xTaskToResume )
{
    rt_thread_resume((rt_thread_t)xTaskToResume);
}

#endif /* INCLUDE_vTaskSuspend */

/*-----------------------------------------------------------*/

#if ( ( INCLUDE_xTaskResumeFromISR == 1 ) && ( INCLUDE_vTaskSuspend == 1 ) )

BaseType_t xTaskResumeFromISR( TaskHandle_t xTaskToResume )
{
    BaseType_t xYieldRequired = pdFALSE;
    if(rt_thread_resume((rt_thread_t)xTaskToResume) == RT_EOK)
        xYieldRequired = pdTRUE;
    return xYieldRequired;
}

#endif /* ( ( INCLUDE_xTaskResumeFromISR == 1 ) && ( INCLUDE_vTaskSuspend == 1 ) ) */
/*-----------------------------------------------------------*/

void vTaskStartScheduler( void )
{
    /* Setup the hardware to generate the tick. */
    xPortStartScheduler();
#if ( configUSE_IDLE_HOOK == 1 ) ||  ( CONFIG_FREERTOS_LEGACY_HOOKS == 1 )
    rt_thread_idle_sethook(xPortApplicationIdleHook);
#endif
    rt_scheduler_sethook(xPortApplicationSchedulerHook);
    rt_system_scheduler_start();
    /*Should not get here*/
}
/*-----------------------------------------------------------*/

void vTaskEndScheduler( void )
{
    vPortEndScheduler();
}
/*----------------------------------------------------------*/

#if ( configUSE_NEWLIB_REENTRANT == 1 )
//Return global reent struct if FreeRTOS isn't running,
struct _reent* __getreent(void)
{
    //No lock needed because if this changes, we won't be running anymore.
    return _GLOBAL_REENT;
}
#endif


void vTaskSuspendAll( void )
{
    /* A critical section is not required as the variable is of type
     * BaseType_t.  Please read Richard Barry's reply in the following link to a
     * post in the FreeRTOS support forum before reporting this as a bug! -
     * https://goo.gl/wu4acr */
    rt_enter_critical();
}
/*----------------------------------------------------------*/

BaseType_t xTaskResumeAll( void )
{
    rt_exit_critical();
    return pdFALSE;
}
/*-----------------------------------------------------------*/

TickType_t xTaskGetTickCount( void )
{
    TickType_t xTicks;
    xTicks = rt_tick_get();
    return xTicks;
}
/*-----------------------------------------------------------*/

TickType_t xTaskGetTickCountFromISR( void )
{
    return xTaskGetTickCount();
}
/*-----------------------------------------------------------*/

UBaseType_t uxTaskGetNumberOfTasks( void )
{
    /* A critical section is not required because the variables are of type
     * BaseType_t. */
    return rt_object_get_length(RT_Object_Class_Thread);
}
/*-----------------------------------------------------------*/

char * pcTaskGetName( TaskHandle_t xTaskToQuery ) /*lint !e971 Unqualified char types are allowed for strings and single characters only. */
{
    return ((rt_thread_t)xTaskToQuery)->name;
}

/*-----------------------------------------------------------*/

#if ( INCLUDE_xTaskGetHandle == 1 )
TaskHandle_t xTaskGetHandle( const char * pcNameToQuery ) /*lint !e971 Unqualified char types are allowed for strings and single characters only. */
{
    return (TaskHandle_t)rt_thread_find(pcNameToQuery);
}
#endif /* INCLUDE_xTaskGetHandle */
/*-----------------------------------------------------------*/

#if ( configUSE_TRACE_FACILITY == 1 )
UBaseType_t uxTaskGetSystemState( TaskStatus_t * const pxTaskStatusArray,
                                  const UBaseType_t uxArraySize,
                                  uint32_t * const pulTotalRunTime )
{
    FREERTOS_ADAPT_LOG(ADAPT_DEBUG_TASK, ("uxTaskGetSystemState not support\n"));
    return 0;
}

#endif /* configUSE_TRACE_FACILITY */
/*----------------------------------------------------------*/
#if ( INCLUDE_xTaskGetIdleTaskHandle == 1 )

TaskHandle_t xTaskGetIdleTaskHandle( void )
{
    /* If xTaskGetIdleTaskHandle() is called before the scheduler has been
     * started, then xIdleTaskHandle will be NULL. */
    return xTaskGetIdleTaskHandleForCPU(0);
}

TaskHandle_t xTaskGetIdleTaskHandleForCPU( UBaseType_t cpuid )
{
    char tidle_name[RT_NAME_MAX];
    rt_sprintf(tidle_name, configIDLE_RTTHREAD_IDLE_TASK_NAME"%d", cpuid);
    return xTaskGetHandle(tidle_name);
}
#endif /* INCLUDE_xTaskGetIdleTaskHandle */
/*----------------------------------------------------------*/

/* This conditional compilation should use inequality to 0, not equality to 1.
 * This is to ensure vTaskStepTick() is available when user defined low power mode
 * implementations require configUSE_TICKLESS_IDLE to be set to a value other than
 * 1. */
#if ( configUSE_TICKLESS_IDLE != 0 )
void vTaskStepTick( const TickType_t xTicksToJump )
{
    /* Correct the tick count value after a period during which the tick
     * was suppressed.  Note this does *not* call the tick hook function for
     * each stepped tick. */
    rt_tick_set(rt_tick_get() + (rt_tick_t)xTicksToJump);
}

#endif /* configUSE_TICKLESS_IDLE */
/*----------------------------------------------------------*/

BaseType_t xTaskCatchUpTicks( TickType_t xTicksToCatchUp )
{
    BaseType_t xYieldRequired = pdFALSE;
    FREERTOS_ADAPT_LOG(ADAPT_DEBUG_TASK, ("xTaskCatchUpTicks not support\n"));
    return xYieldRequired;

}
/*----------------------------------------------------------*/

#if ( INCLUDE_xTaskAbortDelay == 1 )
BaseType_t xTaskAbortDelay( TaskHandle_t xTask )
{
    if(rt_thread_resume((rt_thread_t)xTask) == RT_EOK)
        return pdTRUE;
    else
        return pdFAIL;
}
#endif /* INCLUDE_xTaskAbortDelay */
/*----------------------------------------------------------*/

BaseType_t xTaskIncrementTick( void )
{
    rt_tick_increase();
#if ( configUSE_TICK_HOOK == 1 )
    vApplicationTickHook();
#endif /* configUSE_TICK_HOOK */
#if ( CONFIG_FREERTOS_LEGACY_HOOKS == 1 )
    esp_vApplicationTickHook();
#endif /* CONFIG_FREERTOS_LEGACY_HOOKS */
    return pdFALSE;
}
/*-----------------------------------------------------------*/

#if ( configUSE_APPLICATION_TASK_TAG == 1 )
void vTaskSetApplicationTaskTag( TaskHandle_t xTask, TaskHookFunction_t pxHookFunction )
{

}
#endif /* configUSE_APPLICATION_TASK_TAG */
/*-----------------------------------------------------------*/

#if ( configUSE_APPLICATION_TASK_TAG == 1 )
TaskHookFunction_t xTaskGetApplicationTaskTag( TaskHandle_t xTask )
{
    return RT_NULL;
}

#endif /* configUSE_APPLICATION_TASK_TAG */
/*-----------------------------------------------------------*/

#if ( configUSE_APPLICATION_TASK_TAG == 1 )

TaskHookFunction_t xTaskGetApplicationTaskTagFromISR( TaskHandle_t xTask )
{
    return RT_NULL;
}

#endif /* configUSE_APPLICATION_TASK_TAG */
/*-----------------------------------------------------------*/

#if ( configUSE_APPLICATION_TASK_TAG == 1 )
BaseType_t xTaskCallApplicationTaskHook( TaskHandle_t xTask,
        void * pvParameter )
{
    BaseType_t xReturn;
    xReturn = pdFAIL;
    return xReturn;
}

#endif /* configUSE_APPLICATION_TASK_TAG */
/*-----------------------------------------------------------*/
void vTaskSwitchContext( void )
{

}
/*-----------------------------------------------------------*/

void vTaskPlaceOnEventList( List_t * const pxEventList,
                            const TickType_t xTicksToWait )
{

}
/*-----------------------------------------------------------*/

void vTaskPlaceOnUnorderedEventList( List_t * pxEventList,
                                     const TickType_t xItemValue,
                                     const TickType_t xTicksToWait )
{

}
/*-----------------------------------------------------------*/

#if ( configUSE_TIMERS == 1 )
void vTaskPlaceOnEventListRestricted( List_t * const pxEventList, TickType_t xTicksToWait, const BaseType_t xWaitIndefinitely )
{

}

#endif /* configUSE_TIMERS */
/*-----------------------------------------------------------*/

BaseType_t xTaskRemoveFromEventList( const List_t * const pxEventList )
{

    return pdFALSE;

}
/*-----------------------------------------------------------*/

void vTaskRemoveFromUnorderedEventList( ListItem_t * pxEventListItem,
                                        const TickType_t xItemValue )
{

}
/*-----------------------------------------------------------*/

void vTaskSetTimeOutState( TimeOut_t * const pxTimeOut )
{

}
/*-----------------------------------------------------------*/

void vTaskInternalSetTimeOutState( TimeOut_t * const pxTimeOut )
{

}
/*-----------------------------------------------------------*/

BaseType_t xTaskCheckForTimeOut( TimeOut_t * const pxTimeOut,
                                 TickType_t * const pxTicksToWait )
{
    return pdFALSE;
}
/*-----------------------------------------------------------*/

void vTaskMissedYield( void )
{
    rt_thread_yield();
}
/*-----------------------------------------------------------*/

#if ( configUSE_TRACE_FACILITY == 1 )
UBaseType_t uxTaskGetTaskNumber( TaskHandle_t xTask )
{
    UBaseType_t uxReturn;
    Task_t const * pxTask;

    if( xTask != NULL ) {
        pxTask = xTask;
        uxReturn = pxTask->uxTaskNumber;
    } else {
        uxReturn = 0U;
    }

    return uxReturn;
}

#endif /* configUSE_TRACE_FACILITY */
/*-----------------------------------------------------------*/

#if ( configUSE_TRACE_FACILITY == 1 )

void vTaskSetTaskNumber( TaskHandle_t xTask,
                         const UBaseType_t uxHandle )
{
    Task_t * pxTask;

    if( xTask != NULL ) {
        pxTask = xTask;
        pxTask->uxTaskNumber = uxHandle;
    }
}

#endif /* configUSE_TRACE_FACILITY */


/*-----------------------------------------------------------*/

#if ( configUSE_TICKLESS_IDLE != 0 )

eSleepModeStatus eTaskConfirmSleepModeStatus( void )
{
    return eNoTasksWaitingTimeout;
}

#endif /* configUSE_TICKLESS_IDLE */
/*-----------------------------------------------------------*/

#if ( configNUM_THREAD_LOCAL_STORAGE_POINTERS != 0 )

#if ( configTHREAD_LOCAL_STORAGE_DELETE_CALLBACKS )

void vTaskSetThreadLocalStoragePointerAndDelCallback( TaskHandle_t xTaskToSet, BaseType_t xIndex, void *pvValue, TlsDeleteCallbackFunction_t xDelCallback)
{

}

void vTaskSetThreadLocalStoragePointer( TaskHandle_t xTaskToSet, BaseType_t xIndex, void *pvValue )
{
    vTaskSetThreadLocalStoragePointerAndDelCallback( xTaskToSet, xIndex, pvValue, (TlsDeleteCallbackFunction_t)NULL );
}


#else
void vTaskSetThreadLocalStoragePointer( TaskHandle_t xTaskToSet,
                                        BaseType_t xIndex,
                                        void * pvValue )
{

}
#endif /* configTHREAD_LOCAL_STORAGE_DELETE_CALLBACKS */

#endif /* configNUM_THREAD_LOCAL_STORAGE_POINTERS */
/*-----------------------------------------------------------*/

#if ( configNUM_THREAD_LOCAL_STORAGE_POINTERS != 0 )

void * pvTaskGetThreadLocalStoragePointer( TaskHandle_t xTaskToQuery,
        BaseType_t xIndex )
{
    void * pvReturn = NULL;

    return pvReturn;
}

#endif /* configNUM_THREAD_LOCAL_STORAGE_POINTERS */
/*-----------------------------------------------------------*/

#if ( portUSING_MPU_WRAPPERS == 1 )

void vTaskAllocateMPURegions( TaskHandle_t xTaskToModify,
                              const MemoryRegion_t * const xRegions )
{

}

#endif /* portUSING_MPU_WRAPPERS */


#if ( configUSE_TRACE_FACILITY == 1 )

void vTaskGetInfo( TaskHandle_t xTask,
                   TaskStatus_t * pxTaskStatus,
                   BaseType_t xGetFreeStackSpace,
                   eTaskState eState )
{

}

#endif /* configUSE_TRACE_FACILITY */
/*-----------------------------------------------------------*/

BaseType_t xTaskGetAffinity( TaskHandle_t xTask )
{
#ifdef RT_USING_SMP
    rt_thread_t thread = (rt_thread_t)xTask;
    return thread->oncpu;
#else
    return 0;
#endif
}

#if ( INCLUDE_uxTaskGetStackHighWaterMark2 == 1 )

/* uxTaskGetStackHighWaterMark() and uxTaskGetStackHighWaterMark2() are the
 * same except for their return type.  Using configSTACK_DEPTH_TYPE allows the
 * user to determine the return type.  It gets around the problem of the value
 * overflowing on 8-bit types without breaking backward compatibility for
 * applications that expect an 8-bit return type. */
configSTACK_DEPTH_TYPE uxTaskGetStackHighWaterMark2( TaskHandle_t xTask )
{

    return 0;
}

#endif /* INCLUDE_uxTaskGetStackHighWaterMark2 */
/*-----------------------------------------------------------*/

#if ( INCLUDE_uxTaskGetStackHighWaterMark == 1 )

UBaseType_t uxTaskGetStackHighWaterMark( TaskHandle_t xTask )
{
    return 0;
}

#endif /* INCLUDE_uxTaskGetStackHighWaterMark */
/*-----------------------------------------------------------*/
#if (INCLUDE_pxTaskGetStackStart == 1)

uint8_t* pxTaskGetStackStart( TaskHandle_t xTask)
{
    rt_thread_t thread = (rt_thread_t)xTask;
    return thread->stack_addr;
}

#endif /* INCLUDE_pxTaskGetStackStart */


#if ( ( INCLUDE_xTaskGetCurrentTaskHandle == 1 ) || ( configUSE_MUTEXES == 1 ) || (configNUM_CORES > 1) )

TaskHandle_t xTaskGetCurrentTaskHandle( void )
{
    TaskHandle_t xReturn = RT_NULL;
    if(uxSchedulerRunning == taskSCHEDULER_NOT_STARTED) return xReturn;
    xReturn = (TaskHandle_t)rt_thread_self();
    return xReturn;
}

TaskHandle_t xTaskGetCurrentTaskHandleForCPU( BaseType_t cpuid )
{
    TaskHandle_t xReturn=RT_NULL;
    if(uxSchedulerRunning == taskSCHEDULER_NOT_STARTED) return xReturn;
#ifdef RT_USING_SMP
    rt_base_t lock;

    lock = rt_hw_local_irq_disable();
    xReturn = (TaskHandle_t) rt_cpu_index(cpuid)->current_thread;
    rt_hw_local_irq_enable(lock);
#else
    extern rt_thread_t rt_current_thread;
    xReturn = (TaskHandle_t) rt_current_thread;
#endif /* RT_USING_SMP */
    return xReturn;
}

#endif /* ( ( INCLUDE_xTaskGetCurrentTaskHandle == 1 ) || ( configUSE_MUTEXES == 1 ) ) */
/*-----------------------------------------------------------*/

#if ( ( INCLUDE_xTaskGetSchedulerState == 1 ) || ( configUSE_TIMERS == 1 ) )
BaseType_t xTaskGetSchedulerState( void )
{
    return uxSchedulerRunning;
}

#endif /* ( ( INCLUDE_xTaskGetSchedulerState == 1 ) || ( configUSE_TIMERS == 1 ) ) */
/*-----------------------------------------------------------*/

#if ( configUSE_MUTEXES == 1 )

BaseType_t xTaskPriorityInherit( TaskHandle_t const pxMutexHolder )
{

    BaseType_t xReturn = pdFALSE;
    return xReturn;
}

#endif /* configUSE_MUTEXES */
/*-----------------------------------------------------------*/

#if ( configUSE_MUTEXES == 1 )

BaseType_t xTaskPriorityDisinherit( TaskHandle_t const pxMutexHolder )
{

    BaseType_t xReturn = pdFALSE;


    return xReturn;
}

#endif /* configUSE_MUTEXES */
/*-----------------------------------------------------------*/

#if ( configUSE_MUTEXES == 1 )

void vTaskPriorityDisinheritAfterTimeout( TaskHandle_t const pxMutexHolder,
        UBaseType_t uxHighestPriorityWaitingTask )
{

}

#endif /* configUSE_MUTEXES */
/*-----------------------------------------------------------*/

#if ( portCRITICAL_NESTING_IN_TCB == 1 )
void vTaskEnterCritical( void )
{
    rt_enter_critical();
}
#endif /* portCRITICAL_NESTING_IN_TCB */
/*-----------------------------------------------------------*/

#if ( portCRITICAL_NESTING_IN_TCB == 1 )
void vTaskExitCritical( void )
{
    rt_exit_critical();
}
#endif /* portCRITICAL_NESTING_IN_TCB */

#if ( ( configUSE_TRACE_FACILITY == 1 ) && ( configUSE_STATS_FORMATTING_FUNCTIONS > 0 ) && ( configSUPPORT_DYNAMIC_ALLOCATION == 1 ) )

void vTaskList( char * pcWriteBuffer )
{

}

#endif /* ( ( configUSE_TRACE_FACILITY == 1 ) && ( configUSE_STATS_FORMATTING_FUNCTIONS > 0 ) && ( configSUPPORT_DYNAMIC_ALLOCATION == 1 ) ) */
/*----------------------------------------------------------*/

#if ( ( configGENERATE_RUN_TIME_STATS == 1 ) && ( configUSE_STATS_FORMATTING_FUNCTIONS > 0 ) && ( configSUPPORT_DYNAMIC_ALLOCATION == 1 ) )

void vTaskGetRunTimeStats( char * pcWriteBuffer )
{

}

#endif /* ( ( configGENERATE_RUN_TIME_STATS == 1 ) && ( configUSE_STATS_FORMATTING_FUNCTIONS > 0 ) && ( configSUPPORT_STATIC_ALLOCATION == 1 ) ) */
/*-----------------------------------------------------------*/

TickType_t uxTaskResetEventItemValue( void )
{
    return 0;
}
/*-----------------------------------------------------------*/

#if ( configUSE_MUTEXES == 1 )
void *pvTaskIncrementMutexHeldCount( void )
{
    return RT_NULL;
}
#endif /* configUSE_MUTEXES */
/*-----------------------------------------------------------*/

#if ( configUSE_TASK_NOTIFICATIONS == 1 )

#ifdef ESP_PLATFORM // IDF-3851
// included here for backward binary compatibility
#undef ulTaskNotifyTake
uint32_t ulTaskNotifyTake(BaseType_t xClearCountOnExit,
                          TickType_t xTicksToWait )
{
    return ulTaskGenericNotifyTake(tskDEFAULT_INDEX_TO_NOTIFY, xClearCountOnExit, xTicksToWait);
}
#endif // ESP-PLATFORM

uint32_t ulTaskGenericNotifyTake( UBaseType_t uxIndexToWait,
                                  BaseType_t xClearCountOnExit,
                                  TickType_t xTicksToWait )
{


    return xTaskGenericNotifyWait( uxIndexToWait,
                                   0,
                                   xClearCountOnExit,
                                   RT_NULL,
                                   xTicksToWait );
}

#endif /* configUSE_TASK_NOTIFICATIONS */
/*-----------------------------------------------------------*/

#if ( configUSE_TASK_NOTIFICATIONS == 1 )

#ifdef ESP_PLATFORM // IDF-3851
// included for backward compatibility
#undef xTaskNotifyWait
BaseType_t xTaskNotifyWait( uint32_t ulBitsToClearOnEntry,
                            uint32_t ulBitsToClearOnExit,
                            uint32_t * pulNotificationValue,
                            TickType_t xTicksToWait )
{
    return xTaskGenericNotifyWait(tskDEFAULT_INDEX_TO_NOTIFY, ulBitsToClearOnEntry, ulBitsToClearOnExit, pulNotificationValue, xTicksToWait);
}
#endif // ESP-PLATFORM

BaseType_t xTaskGenericNotifyWait( UBaseType_t uxIndexToWait,
                                   uint32_t ulBitsToClearOnEntry,
                                   uint32_t ulBitsToClearOnExit,
                                   uint32_t * pulNotificationValue,
                                   TickType_t xTicksToWait )
{
    uint32_t xReturn = 0;
    Task_t *xTask = (Task_t*)rt_thread_self();

    if(xTask->ulNotified[uxIndexToWait] == RT_NULL) {
        xTask->ulNotified[uxIndexToWait] = rt_event_create(((rt_thread_t)xTask)->name,RT_IPC_FLAG_FIFO);
    }

    ((rt_event_t)(xTask->ulNotified[uxIndexToWait]))->set &= ~ulBitsToClearOnEntry;

    rt_uint8_t   option = RT_EVENT_FLAG_OR;
    if( ulBitsToClearOnExit != pdFALSE ) {
        option |= RT_EVENT_FLAG_CLEAR;
    }
    rt_event_recv((rt_event_t)(xTask->ulNotified[uxIndexToWait]), RT_UINT32_MAX, option, xTicksToWait, &xReturn);
    if( pulNotificationValue != NULL ) {
        /* Output the current notification value, which may or may not
         * have changed. */
        *pulNotificationValue = xReturn;
    }

    return xReturn;
}

#endif /* configUSE_TASK_NOTIFICATIONS */
/*-----------------------------------------------------------*/

#if ( configUSE_TASK_NOTIFICATIONS == 1 )

BaseType_t xTaskGenericNotify( TaskHandle_t xTaskToNotify,
                               UBaseType_t uxIndexToNotify,
                               uint32_t ulValue,
                               eNotifyAction eAction,
                               uint32_t * pulPreviousNotificationValue )
{
    BaseType_t xReturn = pdPASS;
    rt_uint32_t event = 0;
    Task_t *xTask = (Task_t*)xTaskToNotify;
    if(xTask->ulNotified[uxIndexToNotify] == RT_NULL) {
        xTask->ulNotified[uxIndexToNotify] = rt_event_create(((rt_thread_t)xTask)->name,RT_IPC_FLAG_FIFO);
    }
    rt_base_t level;
    /* disable interrupt */
    level = rt_hw_interrupt_disable();
    if( pulPreviousNotificationValue != NULL ) {
        *pulPreviousNotificationValue =((rt_event_t)(xTask->ulNotified[uxIndexToNotify]))->set;
    }
    switch( eAction ) {
    case eSetBits:
        event = ulValue;
        break;
    case eIncrement:
        event =  ((rt_event_t)(xTask->ulNotified[uxIndexToNotify]))->set;
        event++;
        break;

    case eSetValueWithOverwrite:
        event = ulValue;
        ((rt_event_t)(xTask->ulNotified[uxIndexToNotify]))->set = event;
        break;

    case eSetValueWithoutOverwrite:

        if (rt_list_isempty(&(((rt_event_t)(xTask->ulNotified[uxIndexToNotify]))->parent.suspend_thread))) {
            event = ulValue;
            ((rt_event_t)(xTask->ulNotified[uxIndexToNotify]))->set = event;
        } else {
            /* The value could not be written to the task. */
            xReturn = pdFAIL;
        }
        break;

    case eNoAction:
        event = 1;
        /* The task is being notified without its notify value being
         * updated. */
        break;

    default:
        /* Should not get here if all enums are handled.
         * Artificially force an assert by testing a value the
         * compiler can't assume is const. */

        break;
    }
    /* enable interrupt */
    rt_hw_interrupt_enable(level);
    if(xReturn)
        rt_event_send((rt_event_t)(xTask->ulNotified[uxIndexToNotify]),event);
    return xReturn;
}

#endif /* configUSE_TASK_NOTIFICATIONS */
/*-----------------------------------------------------------*/

#if ( configUSE_TASK_NOTIFICATIONS == 1 )
BaseType_t xTaskGenericNotifyFromISR( TaskHandle_t xTaskToNotify,
                                      UBaseType_t uxIndexToNotify,
                                      uint32_t ulValue,
                                      eNotifyAction eAction,
                                      uint32_t * pulPreviousNotificationValue,
                                      BaseType_t * pxHigherPriorityTaskWoken )
{

    /* The notified task has a priority above the currently
     * executing task so a yield is required. */
    if( pxHigherPriorityTaskWoken != NULL ) {
        *pxHigherPriorityTaskWoken = pdFALSE;
    }

    return xTaskGenericNotify(  xTaskToNotify,
                                uxIndexToNotify,
                                ulValue,
                                eAction,
                                pulPreviousNotificationValue );
}

#endif /* configUSE_TASK_NOTIFICATIONS */
/*-----------------------------------------------------------*/

#if ( configUSE_TASK_NOTIFICATIONS == 1 )

void vTaskGenericNotifyGiveFromISR( TaskHandle_t xTaskToNotify,
                                    UBaseType_t uxIndexToNotify,
                                    BaseType_t * pxHigherPriorityTaskWoken )
{

    /* The notified task has a priority above the currently
     * executing task so a yield is required. */
    if( pxHigherPriorityTaskWoken != NULL ) {
        *pxHigherPriorityTaskWoken = pdFALSE;
    }

    xTaskGenericNotify( xTaskToNotify,
                        uxIndexToNotify,
                        0,
                        eIncrement,
                        RT_NULL);
}

#endif /* configUSE_TASK_NOTIFICATIONS */
/*-----------------------------------------------------------*/

#if ( configUSE_TASK_NOTIFICATIONS == 1 )

BaseType_t xTaskGenericNotifyStateClear( TaskHandle_t xTask,
        UBaseType_t uxIndexToClear )
{


    return pdPASS;
}

#endif /* configUSE_TASK_NOTIFICATIONS */
/*-----------------------------------------------------------*/

#if ( configUSE_TASK_NOTIFICATIONS == 1 )

uint32_t ulTaskGenericNotifyValueClear( TaskHandle_t xTask,
                                        UBaseType_t uxIndexToClear,
                                        uint32_t ulBitsToClear )
{
    uint32_t ulReturn = 0 ;
    if(((Task_t *)xTask)->ulNotified[uxIndexToClear]) {
        ulReturn = ((rt_event_t)(((Task_t *)xTask)->ulNotified[uxIndexToClear]))->set;
        ((rt_event_t)(((Task_t *)xTask)->ulNotified[uxIndexToClear]))->set &= ~ulBitsToClear;
    }
    return ulReturn;
}

#endif /* configUSE_TASK_NOTIFICATIONS */
/*-----------------------------------------------------------*/

#if ( ( configGENERATE_RUN_TIME_STATS == 1 ) && ( INCLUDE_xTaskGetIdleTaskHandle == 1 ) )

uint32_t ulTaskGetIdleRunTimeCounter( void )
{
#ifdef RT_USING_CPU_USAGE
#ifdef RT_USING_SMP
    rt_thread_t thread = (rt_thread_t)xTaskGetIdleTaskHandleForCPU(rt_cpu_self()->current_thread->bind_cpu);
#else
    rt_thread_t thread = (rt_thread_t)xTaskGetIdleTaskHandleForCPU(0);
#endif
    return (uint32_t)thread->duration_tick;
#else
    return 0;
#endif /* RT_USING_CPU_USAGE */
}

#endif


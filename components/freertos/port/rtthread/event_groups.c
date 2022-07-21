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
#define ADAPT_DEBUG_EVENT 1

#include <stdlib.h>

/* FreeRTOS includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"
#include "event_groups.h"


typedef StaticEventGroup_t EventGroup_t;
static uint32_t e = 0;
/*-----------------------------------------------------------*/

#if ( configSUPPORT_STATIC_ALLOCATION == 1 )

    EventGroupHandle_t xEventGroupCreateStatic( StaticEventGroup_t * pxEventGroupBuffer )
    {
        char name[RT_NAME_MAX];
        EventGroup_t *pxEventBits = pxEventGroupBuffer;
        RT_ASSERT( pxEventBits );
        
        pxEventBits->ucStaticallyAllocated = pdTRUE;
        FREERTOS_ADAPT_LOG(ADAPT_DEBUG_EVENT, ("xEventGroupCreateStatic\r\n"));
        rt_snprintf(name,sizeof(name),"event_%d",e++);
        if(rt_event_init((rt_event_t)pxEventBits,
                      name,
                      RT_IPC_FLAG_FIFO)!=RT_EOK){
                          return RT_NULL;
                      }
        return pxEventBits;
    }

#endif /* configSUPPORT_STATIC_ALLOCATION */
/*-----------------------------------------------------------*/

#if ( configSUPPORT_DYNAMIC_ALLOCATION == 1 )

    EventGroupHandle_t xEventGroupCreate( void )
    {
        char name[RT_NAME_MAX];
        EventGroup_t *pxEventBits= (EventGroup_t*)rt_malloc(sizeof(EventGroup_t));
        RT_ASSERT( pxEventBits );
        
        pxEventBits->ucStaticallyAllocated = pdFALSE;
        FREERTOS_ADAPT_LOG(ADAPT_DEBUG_EVENT, ("xEventGroupCreate\r\n"));
        rt_snprintf(name,sizeof(name),"event_%d",e++);
        if(rt_event_init((rt_event_t)pxEventBits,
                      name,
                      RT_IPC_FLAG_FIFO)!=RT_EOK){
                          rt_free(pxEventBits);
                          return RT_NULL;
                      }
        return pxEventBits;
    }

#endif /* configSUPPORT_DYNAMIC_ALLOCATION */
/*-----------------------------------------------------------*/

EventBits_t xEventGroupSync( EventGroupHandle_t xEventGroup,
                             const EventBits_t uxBitsToSet,
                             const EventBits_t uxBitsToWaitFor,
                             TickType_t xTicksToWait )
{
    rt_uint32_t uxReturn = 0;
    rt_uint8_t uxOriginalBitValue = RT_EVENT_FLAG_CLEAR | RT_EVENT_FLAG_AND;
    rt_event_t event = (rt_event_t) xEventGroup;
    RT_ASSERT( event );

    RT_ASSERT(xEventGroup);
    if(uxBitsToSet > 0)
   rt_event_send(event, uxBitsToSet);
(void)rt_event_recv((rt_event_t)event, uxBitsToWaitFor, uxOriginalBitValue, xTicksToWait, &uxReturn);
    return uxReturn;
}
/*-----------------------------------------------------------*/

EventBits_t xEventGroupWaitBits( EventGroupHandle_t xEventGroup,
                                 const EventBits_t uxBitsToWaitFor,
                                 const BaseType_t xClearOnExit,
                                 const BaseType_t xWaitForAllBits,
                                 TickType_t xTicksToWait )
{
    rt_uint8_t uxOriginalBitValue = 0;
    rt_uint32_t uxReturn = 0;
    EventGroup_t *pxEventBits = xEventGroup;
    RT_ASSERT( pxEventBits );
    if (xClearOnExit)
    {
        uxOriginalBitValue |= RT_EVENT_FLAG_CLEAR;
    }
    if (xWaitForAllBits)
    {
        uxOriginalBitValue |= RT_EVENT_FLAG_AND;
    }
    else
    {
        uxOriginalBitValue |= RT_EVENT_FLAG_OR;
    }
    (void)rt_event_recv((rt_event_t)pxEventBits, uxBitsToWaitFor, uxOriginalBitValue, xTicksToWait, &uxReturn);

    return uxReturn;
}
/*-----------------------------------------------------------*/

EventBits_t xEventGroupClearBits( EventGroupHandle_t xEventGroup,
                                  const EventBits_t uxBitsToClear )
{
    rt_event_t event = (rt_event_t) xEventGroup;
    RT_ASSERT( event );
    event->set &= ~uxBitsToClear;
    return (EventBits_t) event->set;
}
/*-----------------------------------------------------------*/

#if ( ( configUSE_TRACE_FACILITY == 1 ) && ( INCLUDE_xTimerPendFunctionCall == 1 ) && ( configUSE_TIMERS == 1 ) )

    BaseType_t xEventGroupClearBitsFromISR( EventGroupHandle_t xEventGroup,
                                            const EventBits_t uxBitsToClear )
    {
        xEventGroupClearBits(xEventGroup,uxBitsToClear);
        return pdPASS;
    }

#endif /* if ( ( configUSE_TRACE_FACILITY == 1 ) && ( INCLUDE_xTimerPendFunctionCall == 1 ) && ( configUSE_TIMERS == 1 ) ) */
/*-----------------------------------------------------------*/

EventBits_t xEventGroupGetBitsFromISR( EventGroupHandle_t xEventGroup )
{
     rt_event_t event = (rt_event_t) xEventGroup;
    RT_ASSERT( event );
    rt_uint32_t uxReturn;
    rt_base_t level;
    level = rt_hw_interrupt_disable();
    {
        uxReturn = event->set;
    }
    rt_hw_interrupt_enable(level);

    return (EventBits_t)uxReturn;
} /*lint !e818 EventGroupHandle_t is a typedef used in other functions to so can't be pointer to const. */
/*-----------------------------------------------------------*/

EventBits_t xEventGroupSetBits( EventGroupHandle_t xEventGroup,
                                const EventBits_t uxBitsToSet )
{
    rt_event_t event = (rt_event_t) xEventGroup;
    RT_ASSERT( event );
    rt_uint32_t uxReturn;
    rt_event_send(event, uxBitsToSet);
    uxReturn = event->set;
    return (EventBits_t)uxReturn;
}
/*-----------------------------------------------------------*/

void vEventGroupDelete( EventGroupHandle_t xEventGroup )
{
     EventGroup_t *event = (EventGroup_t*) xEventGroup;
    RT_ASSERT( event );
     rt_event_detach((rt_event_t)event);
     #if ( ( configSUPPORT_DYNAMIC_ALLOCATION == 1 ) && ( configSUPPORT_STATIC_ALLOCATION == 1 ) )
    if (event->ucStaticallyAllocated == pdFALSE)
        rt_free(event);
     #endif
       
}
/*-----------------------------------------------------------*/

/* For internal use only - execute a 'set bits' command that was pended from
 * an interrupt. */
void vEventGroupSetBitsCallback( void * pvEventGroup,
                                 const uint32_t ulBitsToSet )
{
    ( void ) xEventGroupSetBits( pvEventGroup, ( EventBits_t ) ulBitsToSet ); /*lint !e9079 Can't avoid cast to void* as a generic timer callback prototype. Callback casts back to original type so safe. */
}
/*-----------------------------------------------------------*/

/* For internal use only - execute a 'clear bits' command that was pended from
 * an interrupt. */
void vEventGroupClearBitsCallback( void * pvEventGroup,
                                   const uint32_t ulBitsToClear )
{
    ( void ) xEventGroupClearBits( pvEventGroup, ( EventBits_t ) ulBitsToClear ); /*lint !e9079 Can't avoid cast to void* as a generic timer callback prototype. Callback casts back to original type so safe. */
}
/*-----------------------------------------------------------*/

#if ( ( configUSE_TRACE_FACILITY == 1 ) && ( INCLUDE_xTimerPendFunctionCall == 1 ) && ( configUSE_TIMERS == 1 ) )

    BaseType_t xEventGroupSetBitsFromISR( EventGroupHandle_t xEventGroup,
                                          const EventBits_t uxBitsToSet,
                                          BaseType_t * pxHigherPriorityTaskWoken )
    {
        BaseType_t xReturn;
rt_event_t event = (rt_event_t) xEventGroup;
    RT_ASSERT( event );
        if (pxHigherPriorityTaskWoken)
        {
            *pxHigherPriorityTaskWoken = pdFALSE;
        }
         if (OS_EOK == (rt_event_send(event, uxBitsToSet)))
        {
            xReturn = pdPASS;
        }
        else
        {
            xReturn = pdFAIL;
        }
  
        return xReturn;
    }

#endif /* if ( ( configUSE_TRACE_FACILITY == 1 ) && ( INCLUDE_xTimerPendFunctionCall == 1 ) && ( configUSE_TIMERS == 1 ) ) */
/*-----------------------------------------------------------*/

#if ( configUSE_TRACE_FACILITY == 1 )

    UBaseType_t uxEventGroupGetNumber( void * xEventGroup )
    {
        UBaseType_t xReturn;
        EventGroup_t const * pxEventBits = ( EventGroup_t * ) xEventGroup; /*lint !e9087 !e9079 EventGroupHandle_t is a pointer to an EventGroup_t, but EventGroupHandle_t is kept opaque outside of this file for data hiding purposes. */

        if( xEventGroup == NULL )
        {
            xReturn = 0;
        }
        else
        {
            xReturn = pxEventBits->uxEventGroupNumber;
        }

        return xReturn;
    }

#endif /* configUSE_TRACE_FACILITY */
/*-----------------------------------------------------------*/

#if ( configUSE_TRACE_FACILITY == 1 )

    void vEventGroupSetNumber( void * xEventGroup,
                               UBaseType_t uxEventGroupNumber )
    {
        ( ( EventGroup_t * ) xEventGroup )->uxEventGroupNumber = uxEventGroupNumber; /*lint !e9087 !e9079 EventGroupHandle_t is a pointer to an EventGroup_t, but EventGroupHandle_t is kept opaque outside of this file for data hiding purposes. */
    }

#endif /* configUSE_TRACE_FACILITY */
/*-----------------------------------------------------------*/

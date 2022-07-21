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
#define ADAPT_DEBUG_QUEUE 0

#include <stdlib.h>
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

#if ( configUSE_CO_ROUTINES == 1 )
#include "croutine.h"
#endif

/* When the Queue_t structure is used to represent a base queue its pcHead and
 * pcTail members are used as pointers into the queue storage area.  When the
 * Queue_t structure is used to represent a mutex pcHead and pcTail pointers are
 * not necessary, and the pcHead pointer is set to NULL to indicate that the
 * structure instead holds a pointer to the mutex holder (if any).  Map alternative
 * names to the pcHead and structure member to ensure the readability of the code
 * is maintained.  The QueuePointers_t and SemaphoreData_t types are used to form
 * a union as their usage is mutually exclusive dependent on what the queue is
 * being used for. */


static uint32_t m = 0;			 /* used for index for mutex name	*/
static uint32_t s = 0;			 /* used for index for sem name 	*/
static uint32_t q = 0;			 /* used for index for queue name	*/

typedef StaticQueue_t Queue_t;
/*-----------------------------------------------------------*/

BaseType_t xQueueGenericReset( QueueHandle_t xQueue,
                               BaseType_t xNewQueue )
{
    rt_err_t ret;
    rt_uint8_t type = ((rt_object_t)xQueue)->type & ~RT_Object_Class_Static;
    if(type == RT_Object_Class_Mutex) {
        FREERTOS_ADAPT_LOG(ADAPT_DEBUG_QUEUE, ("xQueueGenericReset not support\n"));
        ret = -RT_ERROR;
    } else if(type == RT_Object_Class_Semaphore) {
        rt_ubase_t value = 0;
        ret = rt_sem_control((rt_sem_t)xQueue,RT_IPC_CMD_RESET, &value);
    } else {
        ret = rt_mq_control((rt_mq_t)xQueue,RT_IPC_CMD_RESET,RT_NULL);
    }
    return ret == RT_EOK ? pdPASS : pdFAIL;
}
/*-----------------------------------------------------------*/

#if ( configSUPPORT_STATIC_ALLOCATION == 1 )

QueueHandle_t xQueueGenericCreateStatic( const UBaseType_t uxQueueLength,
        const UBaseType_t uxItemSize,
        uint8_t * pucQueueStorage,
        StaticQueue_t * pxStaticQueue,
        const uint8_t ucQueueType )
{
    char name[RT_NAME_MAX];
    Queue_t *xQueue= pxStaticQueue;
    RT_ASSERT( xQueue );
    xQueue->ucQueueType = ucQueueType;
    FREERTOS_ADAPT_LOG(ADAPT_DEBUG_QUEUE, ("xQueueGenericCreateStatic %d\n",ucQueueType));
    xQueue->ucStaticallyAllocated = pdTRUE;
    if (queueQUEUE_TYPE_RECURSIVE_MUTEX == ucQueueType || queueQUEUE_TYPE_MUTEX == ucQueueType) {
        rt_snprintf(name,sizeof(name),"mutex_%d",m++);
        if(rt_mutex_init((rt_mutex_t)xQueue,
                         name,
                         RT_IPC_FLAG_FIFO)!=RT_EOK) {
            return RT_NULL;
        }
    } else if (queueQUEUE_TYPE_BINARY_SEMAPHORE == ucQueueType || queueQUEUE_TYPE_COUNTING_SEMAPHORE == ucQueueType) {
        rt_snprintf(name,sizeof(name),"sem_%d",s++);
        if(rt_sem_init((rt_sem_t)xQueue,
                       name,
                       0,
                       RT_IPC_FLAG_FIFO)!=RT_EOK) {
            return RT_NULL;
        }
    } else {
        rt_snprintf(name,sizeof(name),"mq_%d",q++);
        void *msg_pool = RT_NULL;
        rt_size_t msg_pool_size;
        msg_pool_size = uxItemSize*uxQueueLength;
        msg_pool = rt_malloc(msg_pool_size);
        RT_ASSERT(msg_pool);
        if(rt_mq_init((rt_mq_t)xQueue,
                      name,
                      msg_pool,
                      uxItemSize,
                      msg_pool_size,
                      RT_IPC_FLAG_FIFO)!=RT_EOK) {
            rt_free(msg_pool);
            return RT_NULL;
        }

    }
    return xQueue;
}

#endif /* configSUPPORT_STATIC_ALLOCATION */
/*-----------------------------------------------------------*/

#if ( configSUPPORT_DYNAMIC_ALLOCATION == 1 )

QueueHandle_t xQueueGenericCreate( const UBaseType_t uxQueueLength,
                                   const UBaseType_t uxItemSize,
                                   const uint8_t ucQueueType )
{
    char name[RT_NAME_MAX];
    Queue_t *xQueue= (Queue_t*)rt_malloc(sizeof(Queue_t));

    RT_ASSERT( xQueue );
    xQueue->ucQueueType = ucQueueType;
    FREERTOS_ADAPT_LOG(ADAPT_DEBUG_QUEUE, ("xQueueGenericCreate %d\n",ucQueueType));
    xQueue->ucStaticallyAllocated = pdFALSE;
    if (queueQUEUE_TYPE_RECURSIVE_MUTEX == ucQueueType || queueQUEUE_TYPE_MUTEX == ucQueueType) {
        rt_snprintf(name,sizeof(name),"mutex_%d",m++);
        if(rt_mutex_init((rt_mutex_t)xQueue,
                         name,
                         RT_IPC_FLAG_FIFO)!=RT_EOK) {
            rt_free(xQueue);
            return RT_NULL;
        }
    } else if (queueQUEUE_TYPE_BINARY_SEMAPHORE == ucQueueType || queueQUEUE_TYPE_COUNTING_SEMAPHORE == ucQueueType) {
        rt_snprintf(name,sizeof(name),"sem_%d",s++);
        if(rt_sem_init((rt_sem_t)xQueue,
                       name,
                       0,
                       RT_IPC_FLAG_FIFO)!=RT_EOK) {
            rt_free(xQueue);
            return RT_NULL;
        }
    } else {
        rt_snprintf(name,sizeof(name),"mq_%d",q++);
        void *msg_pool = RT_NULL;
        rt_size_t msg_pool_size;
        msg_pool_size = uxItemSize*uxQueueLength;
        msg_pool = rt_malloc(msg_pool_size);
        RT_ASSERT(msg_pool);
        if(rt_mq_init((rt_mq_t)xQueue,
                      name,
                      msg_pool,
                      uxItemSize,
                      msg_pool_size,
                      RT_IPC_FLAG_FIFO)!=RT_EOK) {
            rt_free(xQueue);
            rt_free(msg_pool);
            return RT_NULL;
        }

    }
    return xQueue;
}

#endif /* configSUPPORT_STATIC_ALLOCATION */
/*-----------------------------------------------------------*/


#if ( ( configUSE_MUTEXES == 1 ) && ( configSUPPORT_DYNAMIC_ALLOCATION == 1 ) )

QueueHandle_t xQueueCreateMutex( const uint8_t ucQueueType )
{
    QueueHandle_t xQueue;

    xQueue = xQueueGenericCreate( 0, 0, ucQueueType );

    return xQueue;
}

#endif /* configUSE_MUTEXES */
/*-----------------------------------------------------------*/

#if ( ( configUSE_MUTEXES == 1 ) && ( configSUPPORT_STATIC_ALLOCATION == 1 ) )

QueueHandle_t xQueueCreateMutexStatic( const uint8_t ucQueueType,
                                       StaticQueue_t * pxStaticQueue )
{
    QueueHandle_t xQueue;
    const UBaseType_t uxMutexLength = ( UBaseType_t ) 1, uxMutexSize = ( UBaseType_t ) 0;

    /* Prevent compiler warnings about unused parameters if
     * configUSE_TRACE_FACILITY does not equal 1. */
    ( void ) ucQueueType;

    xQueue = xQueueGenericCreateStatic( uxMutexLength, uxMutexSize, NULL, pxStaticQueue, ucQueueType );


    return xQueue;
}

#endif /* configUSE_MUTEXES */
/*-----------------------------------------------------------*/

#if ( ( configUSE_MUTEXES == 1 ) && ( INCLUDE_xSemaphoreGetMutexHolder == 1 ) )

TaskHandle_t xQueueGetMutexHolder( QueueHandle_t xSemaphore )
{
    TaskHandle_t pxReturn;
    Queue_t * const xQueue = ( Queue_t * ) xSemaphore;
    RT_ASSERT(xQueue);
    rt_base_t level;
    level = rt_hw_interrupt_disable();
    {
        rt_uint8_t type = ((rt_object_t)xQueue)->type & ~RT_Object_Class_Static;
        if( type == RT_Object_Class_Mutex ) {
            pxReturn = xQueue->u.xMutex.owner;
        } else {
            pxReturn = NULL;
        }
    }
    rt_hw_interrupt_enable(level);

    return pxReturn;
} /*lint !e818 xSemaphore cannot be a pointer to const because it is a typedef. */

#endif /* if ( ( configUSE_MUTEXES == 1 ) && ( INCLUDE_xSemaphoreGetMutexHolder == 1 ) ) */
/*-----------------------------------------------------------*/

#if ( ( configUSE_MUTEXES == 1 ) && ( INCLUDE_xSemaphoreGetMutexHolder == 1 ) )

TaskHandle_t xQueueGetMutexHolderFromISR( QueueHandle_t xSemaphore )
{
    return xQueueGetMutexHolder(xSemaphore);
} /*lint !e818 xSemaphore cannot be a pointer to const because it is a typedef. */

#endif /* if ( ( configUSE_MUTEXES == 1 ) && ( INCLUDE_xSemaphoreGetMutexHolder == 1 ) ) */
/*-----------------------------------------------------------*/

#if ( configUSE_RECURSIVE_MUTEXES == 1 )

BaseType_t xQueueGiveMutexRecursive( QueueHandle_t xMutex )
{
    return xQueueGenericSend(xMutex, RT_NULL, 0, 0);
}

#endif /* configUSE_RECURSIVE_MUTEXES */
/*-----------------------------------------------------------*/

#if ( configUSE_RECURSIVE_MUTEXES == 1 )

BaseType_t xQueueTakeMutexRecursive( QueueHandle_t xMutex,
                                     TickType_t xTicksToWait )
{
    return xQueueReceive(xMutex, 0, xTicksToWait);
}

#endif /* configUSE_RECURSIVE_MUTEXES */
/*-----------------------------------------------------------*/

#if ( ( configUSE_COUNTING_SEMAPHORES == 1 ) && ( configSUPPORT_STATIC_ALLOCATION == 1 ) )

QueueHandle_t xQueueCreateCountingSemaphoreStatic( const UBaseType_t uxMaxCount,
        const UBaseType_t uxInitialCount,
        StaticQueue_t * pxStaticQueue )
{
    char name[RT_NAME_MAX];
    Queue_t *xQueue= pxStaticQueue;
    RT_ASSERT( xQueue );

    RT_ASSERT( uxMaxCount != 0 );
    RT_ASSERT( uxInitialCount <= uxMaxCount );
    xQueue->ucQueueType = queueQUEUE_TYPE_COUNTING_SEMAPHORE;
    xQueue->ucStaticallyAllocated = pdTRUE;
    rt_snprintf(name,sizeof(name),"sem_%d",s++);
    if(rt_sem_init((rt_sem_t)xQueue,
                   name,
                   uxInitialCount,
                   RT_IPC_FLAG_FIFO)!=RT_EOK) {
        return RT_NULL;
    }
    ((rt_sem_t)xQueue)->max_value = uxMaxCount;
    return xQueue;
}

#endif /* ( ( configUSE_COUNTING_SEMAPHORES == 1 ) && ( configSUPPORT_DYNAMIC_ALLOCATION == 1 ) ) */
/*-----------------------------------------------------------*/

#if ( ( configUSE_COUNTING_SEMAPHORES == 1 ) && ( configSUPPORT_DYNAMIC_ALLOCATION == 1 ) )

QueueHandle_t xQueueCreateCountingSemaphore( const UBaseType_t uxMaxCount,
        const UBaseType_t uxInitialCount )
{
    char name[RT_NAME_MAX];
    Queue_t *xQueue= (Queue_t*)rt_malloc(sizeof(Queue_t));
    RT_ASSERT( xQueue );
    xQueue->ucQueueType = queueQUEUE_TYPE_COUNTING_SEMAPHORE;
    rt_snprintf(name,sizeof(name),"sem_%d",s++);
    xQueue->ucStaticallyAllocated = pdFALSE;
    if(rt_sem_init((rt_sem_t)xQueue,
                   name,
                   uxInitialCount,
                   RT_IPC_FLAG_FIFO)!=RT_EOK) {
        rt_free(xQueue);
        return RT_NULL;
    }
    ((rt_sem_t)xQueue)->max_value = uxMaxCount;
    return xQueue;
}

#endif /* ( ( configUSE_COUNTING_SEMAPHORES == 1 ) && ( configSUPPORT_DYNAMIC_ALLOCATION == 1 ) ) */
/*-----------------------------------------------------------*/

BaseType_t xQueueGenericSend( QueueHandle_t xQueue,
                              const void * const pvItemToQueue,
                              TickType_t xTicksToWait,
                              const BaseType_t xCopyPosition )
{
    rt_err_t ret;

    rt_uint8_t type = ((rt_object_t)xQueue)->type & ~RT_Object_Class_Static;
    if(type == RT_Object_Class_Mutex) {
        ret = rt_mutex_release((rt_mutex_t)xQueue);
    } else if(type == RT_Object_Class_Semaphore) {
        ret = rt_sem_release((rt_sem_t)xQueue);
    } else {
        if (queueSEND_TO_BACK == xCopyPosition) {
            ret = rt_mq_send((rt_mq_t)xQueue,pvItemToQueue, ((rt_mq_t)xQueue)->msg_size);
        } else if(queueSEND_TO_FRONT == xCopyPosition) {
            ret = rt_mq_send_wait((rt_mq_t)xQueue,
                                  pvItemToQueue,
                                  ((rt_mq_t)xQueue)->msg_size,
                                  xTicksToWait);
        } else {
            ret = RT_ENOSYS;
        }
    }
    if (RT_EOK == ret) {
        return pdPASS;
    } else {
        return errQUEUE_FULL;
    }
}
/*-----------------------------------------------------------*/

BaseType_t xQueueGenericSendFromISR( QueueHandle_t xQueue,
                                     const void * const pvItemToQueue,
                                     BaseType_t * const pxHigherPriorityTaskWoken,
                                     const BaseType_t xCopyPosition )
{

    if (pxHigherPriorityTaskWoken) {
        *pxHigherPriorityTaskWoken = pdFALSE;
    }

    return xQueueGenericSend(xQueue, pvItemToQueue, 0, xCopyPosition);
}
/*-----------------------------------------------------------*/

BaseType_t xQueueGiveFromISR( QueueHandle_t xQueue,
                              BaseType_t * const pxHigherPriorityTaskWoken )
{

    if (pxHigherPriorityTaskWoken) {
        *pxHigherPriorityTaskWoken = pdFALSE;
    }

    return xQueueGenericSend(xQueue, RT_NULL, 0, 0);
}
/*-----------------------------------------------------------*/

BaseType_t xQueueReceive( QueueHandle_t xQueue,
                          void * const pvBuffer,
                          TickType_t xTicksToWait )
{
    rt_err_t ret;

    rt_uint8_t type = ((rt_object_t)xQueue)->type & ~RT_Object_Class_Static;
    if(type == RT_Object_Class_Mutex) {
        ret = rt_mutex_take((rt_mutex_t)xQueue, xTicksToWait);
    } else if(type == RT_Object_Class_Semaphore) {
        ret = rt_sem_take((rt_sem_t)xQueue, xTicksToWait);
    } else {
        ret = rt_mq_recv((rt_mq_t)xQueue,
                         pvBuffer,
                         ((rt_mq_t)xQueue)->msg_size,
                         xTicksToWait);
    }
    if (RT_EOK == ret) {
        return pdPASS;
    } else {
        return errQUEUE_EMPTY;
    }

}
/*-----------------------------------------------------------*/

BaseType_t xQueueSemaphoreTake( QueueHandle_t xQueue,
                                TickType_t xTicksToWait )
{
    return xQueueReceive(xQueue,RT_NULL,xTicksToWait );
}
/*-----------------------------------------------------------*/

BaseType_t xQueuePeek( QueueHandle_t xQueue,
                       void * const pvBuffer,
                       TickType_t xTicksToWait )
{
    FREERTOS_ADAPT_LOG(ADAPT_DEBUG_QUEUE, ("xQueuePeek not support\n"));
    return 0;
}
/*-----------------------------------------------------------*/

BaseType_t xQueueReceiveFromISR( QueueHandle_t xQueue,
                                 void * const pvBuffer,
                                 BaseType_t * const pxHigherPriorityTaskWoken )
{

    if (pxHigherPriorityTaskWoken) {
        *pxHigherPriorityTaskWoken = pdFALSE;
    }

    return xQueueReceive(xQueue, pvBuffer, 0);
}
/*-----------------------------------------------------------*/

BaseType_t xQueuePeekFromISR( QueueHandle_t xQueue,
                              void * const pvBuffer )
{
    return xQueuePeek(xQueue, pvBuffer, 0);
}
/*-----------------------------------------------------------*/

UBaseType_t uxQueueMessagesWaiting( const QueueHandle_t xQueue )
{
    rt_uint32_t pxReturn;
    rt_uint8_t type = ((rt_object_t)xQueue)->type & ~RT_Object_Class_Static;
    if(type == RT_Object_Class_MessageQueue) {
       pxReturn = ((rt_mq_t)xQueue)->entry;
    } else if(type == RT_Object_Class_Semaphore) {
        pxReturn = ((rt_sem_t)xQueue)->value;
    } else {
        pxReturn =((rt_mutex_t)xQueue)->value;
    }
    return (UBaseType_t)pxReturn;
} /*lint !e818 Pointer cannot be declared const as xQueue is a typedef not pointer. */
/*-----------------------------------------------------------*/

UBaseType_t uxQueueSpacesAvailable( const QueueHandle_t xQueue )
{
    RT_ASSERT(OS_ADAPT_QUEUE_TYPE_MESSAGEQUEUE == (((Queue_t*)xQueue)->ucQueueType));
    return (UBaseType_t)(((rt_mq_t)xQueue)->max_msgs - ((rt_mq_t)xQueue)->entry);
} /*lint !e818 Pointer cannot be declared const as xQueue is a typedef not pointer. */
/*-----------------------------------------------------------*/

UBaseType_t uxQueueMessagesWaitingFromISR( const QueueHandle_t xQueue )
{
    return uxQueueMessagesWaiting(xQueue);
} /*lint !e818 Pointer cannot be declared const as xQueue is a typedef not pointer. */
/*-----------------------------------------------------------*/

void vQueueDelete( QueueHandle_t xQueue )
{
    FREERTOS_ADAPT_LOG(ADAPT_DEBUG_QUEUE, ("vQueueDelete\n"));
    rt_uint8_t type = ((rt_object_t)xQueue)->type & ~RT_Object_Class_Static;
    if(type == RT_Object_Class_Mutex) {
        rt_mutex_detach((rt_mutex_t)xQueue);
        #if ( ( configSUPPORT_DYNAMIC_ALLOCATION == 1 ) && ( configSUPPORT_STATIC_ALLOCATION == 1 ) )
        if(((Queue_t *)xQueue)->ucStaticallyAllocated == pdFALSE) {
            rt_free(xQueue);
        }
        #endif
    } else if(type == RT_Object_Class_Semaphore) {
        rt_sem_detach((rt_sem_t)xQueue);
        #if ( ( configSUPPORT_DYNAMIC_ALLOCATION == 1 ) && ( configSUPPORT_STATIC_ALLOCATION == 1 ) )
        if(((Queue_t *)xQueue)->ucStaticallyAllocated == pdFALSE) {
            rt_free(xQueue);
        }
        #endif
    } else {
        rt_mq_detach((rt_mq_t)xQueue);
        #if ( ( configSUPPORT_DYNAMIC_ALLOCATION == 1 ) && ( configSUPPORT_STATIC_ALLOCATION == 1 ) )
        if(((Queue_t *)xQueue)->ucStaticallyAllocated == pdFALSE) {
            rt_free(((rt_mq_t)xQueue)->msg_pool);
            rt_free(xQueue);
        }
        #endif
    }
}
/*-----------------------------------------------------------*/

#if ( configUSE_TRACE_FACILITY == 1 )

UBaseType_t uxQueueGetQueueNumber( QueueHandle_t xQueue )
{
    return ( ( Queue_t * ) xQueue )->uxQueueNumber;
}

#endif /* configUSE_TRACE_FACILITY */
/*-----------------------------------------------------------*/

#if ( configUSE_TRACE_FACILITY == 1 )

void vQueueSetQueueNumber( QueueHandle_t xQueue,
                           UBaseType_t uxQueueNumber )
{
    ( ( Queue_t * ) xQueue )->uxQueueNumber = uxQueueNumber;
}

#endif /* configUSE_TRACE_FACILITY */
/*-----------------------------------------------------------*/

#if ( configUSE_TRACE_FACILITY == 1 )

uint8_t ucQueueGetQueueType( QueueHandle_t xQueue )
{
    return ( ( Queue_t * ) xQueue )->ucQueueType;
}

#endif /* configUSE_TRACE_FACILITY */

/*-----------------------------------------------------------*/

BaseType_t xQueueIsQueueEmptyFromISR( const QueueHandle_t xQueue )
{
    RT_ASSERT(OS_ADAPT_QUEUE_TYPE_MESSAGEQUEUE == (((Queue_t*)xQueue)->ucQueueType));

    return (((rt_mq_t)xQueue)->entry == 0) ? pdTRUE : pdFALSE;

} /*lint !e818 xQueue could not be pointer to const because it is a typedef. */
/*-----------------------------------------------------------*/

BaseType_t xQueueIsQueueFullFromISR( const QueueHandle_t xQueue )
{
    RT_ASSERT(OS_ADAPT_QUEUE_TYPE_MESSAGEQUEUE == (((Queue_t*)xQueue)->ucQueueType));

    return (((rt_mq_t)xQueue)->max_msgs == ((rt_mq_t)xQueue)->entry) ? pdTRUE : pdFALSE;
} /*lint !e818 xQueue could not be pointer to const because it is a typedef. */
/*-----------------------------------------------------------*/

#if ( configUSE_CO_ROUTINES == 1 )

BaseType_t xQueueCRSend( QueueHandle_t xQueue,
                         const void * pvItemToQueue,
                         TickType_t xTicksToWait )
{


    return 0;
}

#endif /* configUSE_CO_ROUTINES */
/*-----------------------------------------------------------*/

#if ( configUSE_CO_ROUTINES == 1 )

BaseType_t xQueueCRReceive( QueueHandle_t xQueue,
                            void * pvBuffer,
                            TickType_t xTicksToWait )
{


    return 0;
}

#endif /* configUSE_CO_ROUTINES */
/*-----------------------------------------------------------*/

#if ( configUSE_CO_ROUTINES == 1 )

BaseType_t xQueueCRSendFromISR( QueueHandle_t xQueue,
                                const void * pvItemToQueue,
                                BaseType_t xCoRoutinePreviouslyWoken )
{
    return 0;
}

#endif /* configUSE_CO_ROUTINES */
/*-----------------------------------------------------------*/

#if ( configUSE_CO_ROUTINES == 1 )

BaseType_t xQueueCRReceiveFromISR( QueueHandle_t xQueue,
                                   void * pvBuffer,
                                   BaseType_t * pxCoRoutineWoken )
{
    return 0;
}

#endif /* configUSE_CO_ROUTINES */
/*-----------------------------------------------------------*/

#if ( configQUEUE_REGISTRY_SIZE > 0 )

void vQueueAddToRegistry( QueueHandle_t xQueue,
                          const char * pcQueueName ) /*lint !e971 Unqualified char types are allowed for strings and single characters only. */
{

}

#endif /* configQUEUE_REGISTRY_SIZE */
/*-----------------------------------------------------------*/

#if ( configQUEUE_REGISTRY_SIZE > 0 )

const char * pcQueueGetName( QueueHandle_t xQueue ) /*lint !e971 Unqualified char types are allowed for strings and single characters only. */
{
    return RT_NULL;
} /*lint !e818 xQueue cannot be a pointer to const because it is a typedef. */

#endif /* configQUEUE_REGISTRY_SIZE */
/*-----------------------------------------------------------*/

#if ( configQUEUE_REGISTRY_SIZE > 0 )

void vQueueUnregisterQueue( QueueHandle_t xQueue )
{

} /*lint !e818 xQueue could not be pointer to const because it is a typedef. */

#endif /* configQUEUE_REGISTRY_SIZE */
/*-----------------------------------------------------------*/

#if ( configUSE_TIMERS == 1 )

void vQueueWaitForMessageRestricted( QueueHandle_t xQueue,
                                     TickType_t xTicksToWait,
                                     const BaseType_t xWaitIndefinitely )
{

}

#endif /* configUSE_TIMERS */
/*-----------------------------------------------------------*/

#if ( ( configUSE_QUEUE_SETS == 1 ) && ( configSUPPORT_DYNAMIC_ALLOCATION == 1 ) )

QueueSetHandle_t xQueueCreateSet( const UBaseType_t uxEventQueueLength )
{
    QueueSetHandle_t pxQueue;

    pxQueue = xQueueGenericCreate( uxEventQueueLength, ( UBaseType_t ) sizeof( Queue_t * ), queueQUEUE_TYPE_SET );

    return pxQueue;
}

#endif /* configUSE_QUEUE_SETS */
/*-----------------------------------------------------------*/

#if ( configUSE_QUEUE_SETS == 1 )

BaseType_t xQueueAddToSet( QueueSetMemberHandle_t xQueueOrSemaphore,
                           QueueSetHandle_t xQueueSet )
{


    return 0;
}

#endif /* configUSE_QUEUE_SETS */
/*-----------------------------------------------------------*/

#if ( configUSE_QUEUE_SETS == 1 )

BaseType_t xQueueRemoveFromSet( QueueSetMemberHandle_t xQueueOrSemaphore,
                                QueueSetHandle_t xQueueSet )
{


    return 0;
} /*lint !e818 xQueueSet could not be declared as pointing to const as it is a typedef. */

#endif /* configUSE_QUEUE_SETS */
/*-----------------------------------------------------------*/

#if ( configUSE_QUEUE_SETS == 1 )

QueueSetMemberHandle_t xQueueSelectFromSet( QueueSetHandle_t xQueueSet,
        TickType_t const xTicksToWait )
{
    QueueSetMemberHandle_t xReturn = NULL;

    ( void ) xQueueReceive( ( QueueHandle_t ) xQueueSet, &xReturn, xTicksToWait ); /*lint !e961 Casting from one typedef to another is not redundant. */
    return xReturn;
}

#endif /* configUSE_QUEUE_SETS */
/*-----------------------------------------------------------*/

#if ( configUSE_QUEUE_SETS == 1 )

QueueSetMemberHandle_t xQueueSelectFromSetFromISR( QueueSetHandle_t xQueueSet )
{
    QueueSetMemberHandle_t xReturn = NULL;

    ( void ) xQueueReceiveFromISR( ( QueueHandle_t ) xQueueSet, &xReturn, NULL ); /*lint !e961 Casting from one typedef to another is not redundant. */
    return xReturn;
}

#endif /* configUSE_QUEUE_SETS */
/*-----------------------------------------------------------*/


/**
 ***********************************************************************************************************************
 * Copyright (c) 2020, China Mobile Communications Group Co.,Ltd.
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under the License is distributed on
 * an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the License for the
 * specific language governing permissions and limitations under the License.
 *
 * @file        freertos_internal.h
 *
 * @brief       internal head file for freertos adapter
 *
 * @revision
 * Date         Author          Notes
 * 2021-03-12   OneOS Team      First version.
 ***********************************************************************************************************************
 */
#ifndef __FREERTOS_RTTHREAD_H__
#define __FREERTOS_RTTHREAD_H__

#include <rtconfig.h>
#include "rtthread.h"
#include "rthw.h"

#ifndef RT_USING_HEAP
#error "RT_USING_HEAP should be defined in menuconfig, otherwise xxx_create will no use"
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define OSAL_FOR_FREERTOS_ENABLE_LOG

#ifdef  OSAL_FOR_FREERTOS_ENABLE_LOG
#define FREERTOS_ADAPT_LOG(type, message)                     \
do                                                      \
{                                                       \
    if (type)                                           \
        rt_kprintf message;                             \
}                                                       \
while (0)
#else
#define FREERTOS_ADAPT_LOG(type, message)
#endif

#ifdef __cplusplus
}
#endif

#endif /* __FREERTOS_ONEOS_H__ */



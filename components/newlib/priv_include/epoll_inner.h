/**
 * @file epoll_inner.h
 * epoll_inner.h API header file.
 *
 * @version   V1.0
 * @date      2020-04-26
 * @copyright Copyright (C) 2015-2020 Alibaba Group Holding Limited
 */


#ifndef ___EPOLL_INNNER_H__
#define ___EPOLL_INNNER_H__

#ifdef __cplusplus
extern "C" {
#endif
#include <stddef.h>
#include <stdint.h>
#include <sys/epoll.h>


#define EPOLL_VFS_CTL             10
#define EPOLL_VFS_WAIT            11

typedef struct {
    int                 fd;
    int                 op;
    struct epoll_event  *event;
} epoll_ctl_data_t;

typedef struct {
    int                 maxevents;
    int                 timeout;
    struct epoll_event  *events;
} epoll_wait_data_t;

#ifdef __cplusplus
}
#endif

#endif
// Copyright 2019-2020 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/poll.h>
#include <sys/epoll.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/errno.h>
#include <sys/param.h>
#include "epoll_inner.h"

int epoll_create(int size)
{
    if (size < 0) {
        errno = EINVAL;
        return -1;
    }
    return epoll_create1(0);
}
int epoll_create1(int flag){
    int ret = -1;

    if (flag < 0) {
        errno = EINVAL;
        return ret;
    }

    ret = open("/dev/epoll", flag == EPOLL_CLOEXEC ? O_CLOEXEC : 0);

    if (ret < 0) {
        if (ret < -1) {
            errno = -ret;
        }
        return -1;
    }
    return ret;
}
int epoll_ctl(int epid, int op, int fd, struct epoll_event *event){
    epoll_ctl_data_t data;
    int ret = -1;

    if (epid < 0 || fd < 0) {
        errno = EINVAL;
        return -1;
    }
    memset(&data, 0, sizeof(epoll_ctl_data_t));
    data.op = op;
    data.fd = fd;
    data.event = event;

    ret = ioctl(epid, EPOLL_VFS_CTL, (uint32_t)&data);
    if (ret < 0) {
        if (ret < -1) {
            errno = -ret;
        }
        return -1;
    }
    return ret;
}
int epoll_wait(int epid, struct epoll_event *events, int maxevents, int timeout){
    int ret = -1;
    epoll_wait_data_t data;
    if (events == NULL || epid < 0 || maxevents <= 0) {
        errno = EINVAL;
        return -1;
    }
    memset(&data, 0, sizeof(epoll_wait_data_t));
    data.events = events;
    data.maxevents = maxevents;
    data.timeout = timeout;
    ret = ioctl(epid, EPOLL_VFS_WAIT, (uint32_t)&data);
    if (ret < 0) {
        if (ret < -1) {
            errno = -ret;
        }
        return -1;
    }
    return ret;
}

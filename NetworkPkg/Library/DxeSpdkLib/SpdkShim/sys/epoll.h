/** @file
  epoll.h - EDK shim definitions from sys/epoll.h for compiling SPDK.

Copyright (c) 2024, Dell Inc. or its subsidiaries. All Rights Reserved.<BR>
SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef _SYS_EPOLL_H
#define _SYS_EPOLL_H

enum EPOLL_EVENTS {
  EPOLLIN = 0x001,
  #define EPOLLIN  EPOLLIN
  EPOLLPRI = 0x002,
  #define EPOLLPRI  EPOLLPRI
  EPOLLOUT = 0x004,
  #define EPOLLOUT  EPOLLOUT
  EPOLLRDNORM = 0x040,
  #define EPOLLRDNORM  EPOLLRDNORM
  EPOLLRDBAND = 0x080,
  #define EPOLLRDBAND  EPOLLRDBAND
  EPOLLWRNORM = 0x100,
  #define EPOLLWRNORM  EPOLLWRNORM
  EPOLLWRBAND = 0x200,
  #define EPOLLWRBAND  EPOLLWRBAND
  EPOLLMSG = 0x400,
  #define EPOLLMSG  EPOLLMSG
  EPOLLERR = 0x008,
  #define EPOLLERR  EPOLLERR
  EPOLLHUP = 0x010,
  #define EPOLLHUP  EPOLLHUP
  EPOLLRDHUP = 0x2000,
  #define EPOLLRDHUP  EPOLLRDHUP
  EPOLLEXCLUSIVE = 1u << 28,
  #define EPOLLEXCLUSIVE  EPOLLEXCLUSIVE
  EPOLLWAKEUP = 1u << 29,
  #define EPOLLWAKEUP  EPOLLWAKEUP
  EPOLLONESHOT = 1u << 30,
  #define EPOLLONESHOT  EPOLLONESHOT
  EPOLLET = 1u << 31
            #define EPOLLET  EPOLLET
};

#endif /* _SYS_EPOLL_H */

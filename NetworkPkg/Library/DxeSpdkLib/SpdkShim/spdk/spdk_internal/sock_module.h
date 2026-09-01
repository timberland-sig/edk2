/*   SPDX-License-Identifier: BSD-3-Clause
 *   Copyright (C) 2018 Intel Corporation. All rights reserved.
 *   Copyright (c) 2020 Mellanox Technologies LTD. All rights reserved.
 */

/** \file
 * TCP network implementation abstraction layer
 *
 * EDK2 shim override: includes the shim's sock.h which provides
 * struct definitions and inline functions matching the EDK2 socket
 * implementation layout.  This file also provides additional
 * declarations required by the upstream sock.c.
 */

#ifndef SPDK_INTERNAL_SOCK_MODULE_H
#define SPDK_INTERNAL_SOCK_MODULE_H

#include "spdk_internal/sock.h"
#include "spdk/log.h"
#include "spdk/trace.h"
#include "spdk_internal/trace_defs.h"

struct addrinfo *spdk_sock_posix_getaddrinfo (const char *ip, int port);

int spdk_sock_posix_fd_create (struct addrinfo *res, struct spdk_sock_opts *opts,
                               struct spdk_sock_impl_opts *impl_opts);

int spdk_sock_posix_fd_connect (int fd, struct addrinfo *res, struct spdk_sock_opts *opts);

int spdk_sock_posix_fd_connect_async (int fd, struct addrinfo *res, struct spdk_sock_opts *opts);

int spdk_sock_posix_fd_connect_poll_async (int fd);

#endif /* SPDK_INTERNAL_SOCK_MODULE_H */

/** @file
  edk_nvme.h - Header file for EDK NVMe data structures.

Copyright (c) 2024, Dell Inc. or its subsidiaries. All Rights Reserved.<BR>
SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef _EDK_NVME_H_
#define _EDK_NVME_H_

#include "nvme_internal.h"
#include "spdk/stdinc.h"
#include "spdk/log.h"
#include "spdk/nvme.h"
#include "spdk/util.h"

#include <Library/NetLib.h>
#include <Library/TcpIoLib.h>
#include "edk_sock.h"

struct edk_spdk_nvme_ctrlr_opts {
  // NVMe controller initialization options.
  struct spdk_nvme_ctrlr_opts    *base;

  // This struct lives on edk_nvme_ctrlr_probe()'s stack for exactly one probe, which is the
  // lifetime a probe need. Do not use a pointer here that requires the caller to free.
  struct spdk_edk_sock_ctx        sock_ctx;
};

#define __edk_opts(opts)   (struct edk_spdk_nvme_ctrlr_opts *)opts
#define __spdk_opts(opts)  (struct spdk_nvme_ctrlr_opts *)opts->base

/**
  The TCP_IO of the socket this controller connected with, or NULL if it has none.

  The socket writes it here at connect and clears it at close, so a caller must not cache it
  across a disconnect.
**/
TCP_IO *
edk_nvme_tcp_ctrlr_get_tcpio (
  struct spdk_nvme_ctrlr  *ctrlr
  );

#endif

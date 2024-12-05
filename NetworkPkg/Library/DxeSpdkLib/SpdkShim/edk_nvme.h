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

#pragma pack(push, 1)
struct edk_spdk_nvme_ctrlr_opts {
  //
  // NVMe controller initialization options.
  //
  struct spdk_nvme_ctrlr_opts    *base;

  //
  // Additional context for socket creation.
  // In UEFI use case, this field allows to provide additional information
  // required for TcpIo instance creation.
  //
  void                           *sock_ctx;
};

#pragma pack(pop)

#define __edk_opts(opts)   (struct edk_spdk_nvme_ctrlr_opts *)opts
#define __spdk_opts(opts)  (struct spdk_nvme_ctrlr_opts *)opts->base

#endif

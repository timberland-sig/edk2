/** @file
  edk_nvme_internal.h - Header file for EDK NVMe internal.

Copyright (c) 2024, Dell Inc. or its subsidiaries. All Rights Reserved.<BR>
SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef _EDK_NVME_INTERNAL_H_
#define _EDK_NVME_INTERNAL_H_

#include "edk_nvme.h"
#include "spdk/stdinc.h"
#include "nvme_internal.h"

extern LIST_ENTRY  fail_conn;

struct spdk_nvme_fail_trid {
  struct spdk_nvme_transport_id    trid;
  LIST_ENTRY                       link;
};

void
nvme_fabric_insert_fail_trid (
  struct spdk_nvme_transport_id  *trid
  );

int
edk_fabric_ctrlr_scan (
  struct spdk_nvme_probe_ctx  *probe_ctx,
  bool                        direct_connect
  );

int
edk_nvme_ctrlr_probe (
  const struct spdk_nvme_transport_id  *trid,
  struct spdk_nvme_probe_ctx           *probe_ctx,
  void                                 *devhandle
  );

#endif

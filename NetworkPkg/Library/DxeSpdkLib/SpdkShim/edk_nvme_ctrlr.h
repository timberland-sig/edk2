/** @file
  edk_nvme_ctrlr.h - Header file for EDK NVMe controller.

Copyright (c) 2024, Dell Inc. or its subsidiaries. All Rights Reserved.<BR>
SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef _EDK_NVME_CTRLR_H_
#define _EDK_NVME_CTRLR_H_

#include "nvme_internal.h"

int
nvme_ctrlr_keep_alive (
  struct spdk_nvme_ctrlr  *ctrlr
  );

#endif

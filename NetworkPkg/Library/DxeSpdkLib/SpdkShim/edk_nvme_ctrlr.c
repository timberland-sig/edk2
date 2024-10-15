/** @file
  edk_nvme_ctrlr.c - Implements EDK NVMe controller for SPDK

Copyright (c) 2024, Dell Inc. or its subsidiaries. All Rights Reserved.<BR>
SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "edk_nvme_ctrlr.h"

void
nvme_keep_alive_completion (
  void                        *cb_ctx,
  const struct spdk_nvme_cpl  *cpl
  )
{
  /* Do nothing */
}

int
nvme_ctrlr_keep_alive (
  struct spdk_nvme_ctrlr  *ctrlr
  )
{
  uint64_t              now;
  struct nvme_request   *req;
  struct spdk_nvme_cmd  *cmd;
  int                   rc = 0;

  now = spdk_get_ticks ();

  if (now < ctrlr->next_keep_alive_tick) {
    return rc;
  }

  req = nvme_allocate_request_null (ctrlr->adminq, nvme_keep_alive_completion, NULL);
  if (req == NULL) {
    return rc;
  }

  cmd      = &req->cmd;
  cmd->opc = SPDK_NVME_OPC_KEEP_ALIVE;

  rc = nvme_ctrlr_submit_admin_request (ctrlr, req);
  if (rc != 0) {
    DEBUG ((DEBUG_ERROR, "Submitting Keep Alive failed\n"));
    rc = -ENXIO;
  }

  ctrlr->next_keep_alive_tick = now + ctrlr->keep_alive_interval_ticks;

  return rc;
}

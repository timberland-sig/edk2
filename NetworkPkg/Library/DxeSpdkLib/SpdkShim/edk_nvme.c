/** @file
  edk_nvme.c - Implements EDK NVMe for SPDK

Copyright (c) 2024, Dell Inc. or its subsidiaries. All Rights Reserved.<BR>
SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "edk_nvme.h"

int
edk_nvme_ctrlr_probe (
  const struct spdk_nvme_transport_id  *trid,
  struct spdk_nvme_probe_ctx           *probe_ctx,
  void                                 *devhandle
  )
{
  struct spdk_nvme_ctrlr       *ctrlr;
  struct spdk_nvme_ctrlr_opts  opts;

  struct edk_spdk_nvme_ctrlr_opts  edk_opts;

  assert (trid != NULL);
  DEBUG ((DEBUG_INFO, "Probe trid: %s\n", trid->traddr));
  spdk_nvme_ctrlr_get_default_ctrlr_opts (&opts, sizeof (opts));
  edk_opts.base     = &opts;
  edk_opts.sock_ctx = NULL;
  if (!probe_ctx->probe_cb || probe_ctx->probe_cb (probe_ctx->cb_ctx, trid, (struct spdk_nvme_ctrlr_opts *)&edk_opts)) {
    ctrlr = nvme_get_ctrlr_by_trid_unsafe (trid);
    if (ctrlr) {
      /* This ctrlr already exists. */

      if (ctrlr->is_destructed) {
        /* This ctrlr is being destructed asynchronously. */
        SPDK_ERRLOG (
          "NVMe controller for SSD: %s is being destructed\n",
          trid->traddr
          );
        return -EBUSY;
      }

      /* Increase the ref count before calling attach_cb() as the user may
      * call nvme_detach() immediately. */
      nvme_ctrlr_proc_get_ref (ctrlr);

      if (probe_ctx->attach_cb) {
        nvme_robust_mutex_unlock (&g_spdk_nvme_driver->lock);
        probe_ctx->attach_cb (probe_ctx->cb_ctx, &ctrlr->trid, ctrlr, &ctrlr->opts);
        nvme_robust_mutex_lock (&g_spdk_nvme_driver->lock);
      }

      return 0;
    }

    ctrlr = nvme_transport_ctrlr_construct (trid, (struct spdk_nvme_ctrlr_opts *)&edk_opts, devhandle);
    if (ctrlr == NULL) {
      SPDK_ERRLOG ("Failed to construct NVMe controller for SSD: %s\n", trid->traddr);
      return -1;
    }

    ctrlr->remove_cb = probe_ctx->remove_cb;
    ctrlr->cb_ctx    = probe_ctx->cb_ctx;

    nvme_qpair_set_state (ctrlr->adminq, NVME_QPAIR_ENABLED);
    TAILQ_INSERT_TAIL (&probe_ctx->init_ctrlrs, ctrlr, tailq);
    return 0;
  }

  return 1;
}

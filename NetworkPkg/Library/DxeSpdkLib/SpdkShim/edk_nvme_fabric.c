/** @file
  edk_nvme_fabric.c - Implements EDK NVMe fabric shim

Copyright (c) 2024, Dell Inc. or its subsidiaries. All Rights Reserved.<BR>
SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "edk_nvme_internal.h"
#include "spdk/endian.h"

LIST_ENTRY  fail_conn;

void
nvme_fabric_insert_fail_trid (
  struct spdk_nvme_transport_id  *trid
  )
{
  struct spdk_nvme_fail_trid  *fail_trid;

  fail_trid = calloc (1, sizeof (*fail_trid));
  if (fail_trid == NULL) {
    SPDK_ERRLOG ("Failed to allocate for failed trid info \n");
    return;
  }

  memcpy (&fail_trid->trid, trid, sizeof (fail_trid->trid));
  InsertTailList (&fail_conn, &fail_trid->link);
}

void
nvme_fabric_discover_probe (
  struct spdk_nvmf_discovery_log_page_entry  *entry,
  struct spdk_nvme_probe_ctx                 *probe_ctx,
  int                                        discover_priority
  )
{
  struct spdk_nvme_transport_id  trid;
  uint8_t                        *end;
  size_t                         len;
  int                            ret = -1;

  memset (&trid, 0, sizeof (trid));

  if (entry->subtype == SPDK_NVMF_SUBTYPE_DISCOVERY) {
    SPDK_WARNLOG ("Skipping unsupported discovery service referral\n");
    return;
  } else if (entry->subtype != SPDK_NVMF_SUBTYPE_NVME) {
    SPDK_WARNLOG ("Skipping unknown subtype %u\n", entry->subtype);
    return;
  }

  trid.trtype = entry->trtype;
  spdk_nvme_transport_id_populate_trstring (&trid, spdk_nvme_transport_id_trtype_str (entry->trtype));
  if (!spdk_nvme_transport_available_by_name (trid.trstring)) {
    SPDK_WARNLOG (
      "NVMe transport type %u not available; skipping probe\n",
      trid.trtype
      );
    return;
  }

  trid.adrfam = entry->adrfam;

  /* Ensure that subnqn is null terminated. */
  end = memchr (entry->subnqn, '\0', SPDK_NVMF_NQN_MAX_LEN + 1);
  if (!end) {
    SPDK_ERRLOG ("Discovery entry SUBNQN is not null terminated\n");
    return;
  }

  len = end - entry->subnqn;
  if (len > SPDK_NVMF_NQN_MAX_LEN) {
    SPDK_ERRLOG ("Discovery entry SUBNQN is too long\n");
    len = SPDK_NVMF_NQN_MAX_LEN;
  }

  memcpy (trid.subnqn, entry->subnqn, len);
  trid.subnqn[len] = '\0';

  /* Convert traddr to a null terminated string. */
  len = spdk_strlen_pad (entry->traddr, sizeof (entry->traddr), ' ');
  if (len > SPDK_NVMF_TRADDR_MAX_LEN) {
    SPDK_ERRLOG ("Discovery entry TRADDR is too long\n");
    len = SPDK_NVMF_TRADDR_MAX_LEN;
  }

  memcpy (trid.traddr, entry->traddr, len);
  if (spdk_str_chomp (trid.traddr) != 0) {
    SPDK_DEBUGLOG (nvme, "Trailing newlines removed from discovery TRADDR\n");
  }

  /* Convert trsvcid to a null terminated string. */
  len = spdk_strlen_pad (entry->trsvcid, sizeof (entry->trsvcid), ' ');
  if (len > SPDK_NVMF_TRSVCID_MAX_LEN) {
    SPDK_ERRLOG ("Discovery entry TRSVCID is too long\n");
    len = SPDK_NVMF_TRSVCID_MAX_LEN;
  }

  memcpy (trid.trsvcid, entry->trsvcid, len);
  if (spdk_str_chomp (trid.trsvcid) != 0) {
    SPDK_DEBUGLOG (nvme, "Trailing newlines removed from discovery TRSVCID\n");
  }

  SPDK_DEBUGLOG (
    nvme,
    "subnqn=%s, trtype=%u, traddr=%s, trsvcid=%s\n",
    trid.subnqn,
    trid.trtype,
    trid.traddr,
    trid.trsvcid
    );

  /* Copy the priority from the discovery ctrlr */
  trid.priority = discover_priority;

  ret = edk_nvme_ctrlr_probe (&trid, probe_ctx, NULL);
  if (ret == -1) {
    /* Inserting failed discovered subsystem info for NBFT */
    nvme_fabric_insert_fail_trid (&trid);
  }
}

static int
nvme_fabric_get_discovery_log_page (
  struct spdk_nvme_ctrlr  *ctrlr,
  void                    *log_page,
  uint32_t                size,
  uint64_t                offset
  )
{
  struct nvme_completion_poll_status  *status;
  int                                 rc;

  status = calloc (1, sizeof (*status));
  if (!status) {
    SPDK_ERRLOG ("Failed to allocate status tracker\n");
    return -ENOMEM;
  }

  rc = spdk_nvme_ctrlr_cmd_get_log_page (
         ctrlr,
         SPDK_NVME_LOG_DISCOVERY,
         0,
         log_page,
         size,
         offset,
         nvme_completion_poll_cb,
         status
         );
  if (rc < 0) {
    free (status);
    return -1;
  }

  if (nvme_wait_for_completion (ctrlr->adminq, status)) {
    if (!status->timed_out) {
      free (status);
    }

    return -1;
  }

  free (status);

  return 0;
}

int
edk_nvme_fabric_ctrlr_discover (
  struct spdk_nvme_ctrlr      *ctrlr,
  struct spdk_nvme_probe_ctx  *probe_ctx
  )
{
  struct spdk_nvmf_discovery_log_page        *log_page;
  struct spdk_nvmf_discovery_log_page_entry  *log_page_entry;
  char                                       buffer[4096];
  int                                        rc;
  uint64_t                                   i, numrec, buffer_max_entries_first, buffer_max_entries, log_page_offset = 0;
  uint64_t                                   remaining_num_rec = 0;
  uint16_t                                   recfmt;

  memset (buffer, 0x0, 4096);
  buffer_max_entries_first = (sizeof (buffer) - offsetof (
                                                  struct spdk_nvmf_discovery_log_page,
                                                  entries[0]
                                                  )) /
                             sizeof (struct spdk_nvmf_discovery_log_page_entry);
  buffer_max_entries = sizeof (buffer) / sizeof (struct spdk_nvmf_discovery_log_page_entry);
  do {
    rc = nvme_fabric_get_discovery_log_page (ctrlr, buffer, sizeof (buffer), log_page_offset);
    if (rc < 0) {
      SPDK_DEBUGLOG (nvme, "Get Log Page - Discovery error\n");
      return rc;
    }

    if (!remaining_num_rec) {
      log_page = (struct spdk_nvmf_discovery_log_page *)buffer;
      recfmt   = from_le16 (&log_page->recfmt);
      if (recfmt != 0) {
        SPDK_ERRLOG ("Unrecognized discovery log record format %" PRIu16 "\n", recfmt);
        return -EPROTO;
      }

      remaining_num_rec = log_page->numrec;
      log_page_offset   = offsetof (struct spdk_nvmf_discovery_log_page, entries[0]);
      log_page_entry    = &log_page->entries[0];
      numrec            = spdk_min (remaining_num_rec, buffer_max_entries_first);
    } else {
      numrec         = spdk_min (remaining_num_rec, buffer_max_entries);
      log_page_entry = (struct spdk_nvmf_discovery_log_page_entry *)buffer;
    }

    for (i = 0; i < numrec; i++) {
      nvme_fabric_discover_probe (log_page_entry++, probe_ctx, ctrlr->trid.priority);
    }

    remaining_num_rec -= numrec;
    log_page_offset   += numrec * sizeof (struct spdk_nvmf_discovery_log_page_entry);
  } while (remaining_num_rec != 0);

  return 0;
}

int
edk_fabric_ctrlr_scan (
  struct spdk_nvme_probe_ctx  *probe_ctx,
  bool                        direct_connect
  )
{
  struct spdk_nvme_ctrlr_opts         discovery_opts;
  struct spdk_nvme_ctrlr              *discovery_ctrlr;
  int                                 rc;
  struct nvme_completion_poll_status  *status;

  struct edk_spdk_nvme_ctrlr_opts  edk_opts;

  if (strcmp (probe_ctx->trid.subnqn, SPDK_NVMF_DISCOVERY_NQN) != 0) {
    /* It is not a discovery_ctrlr info and try to directly connect it */
    rc = edk_nvme_ctrlr_probe (&probe_ctx->trid, probe_ctx, NULL);
    return rc;
  }

  spdk_nvme_ctrlr_get_default_ctrlr_opts (&discovery_opts, sizeof (discovery_opts));
  edk_opts.base     = &discovery_opts;
  edk_opts.sock_ctx = NULL;
  if (probe_ctx->probe_cb) {
    probe_ctx->probe_cb (probe_ctx->cb_ctx, &probe_ctx->trid, (struct spdk_nvme_ctrlr_opts *)&edk_opts);
  }

  discovery_ctrlr = nvme_transport_ctrlr_construct (&probe_ctx->trid, (struct spdk_nvme_ctrlr_opts *)&edk_opts, NULL);
  if (discovery_ctrlr == NULL) {
    return -1;
  }

  while (discovery_ctrlr->state != NVME_CTRLR_STATE_READY) {
    if (nvme_ctrlr_process_init (discovery_ctrlr) != 0) {
      nvme_ctrlr_destruct (discovery_ctrlr);
      return -1;
    }
  }

  status = calloc (1, sizeof (*status));
  if (!status) {
    SPDK_ERRLOG ("Failed to allocate status tracker\n");
    nvme_ctrlr_destruct (discovery_ctrlr);
    return -ENOMEM;
  }

  /* get the cdata info */
  rc = nvme_ctrlr_cmd_identify (
         discovery_ctrlr,
         SPDK_NVME_IDENTIFY_CTRLR,
         0,
         0,
         0,
         &discovery_ctrlr->cdata,
         sizeof (discovery_ctrlr->cdata),
         nvme_completion_poll_cb,
         status
         );
  if (rc != 0) {
    SPDK_ERRLOG ("Failed to identify cdata\n");
    nvme_ctrlr_destruct (discovery_ctrlr);
    free (status);
    return rc;
  }

  if (nvme_wait_for_completion (discovery_ctrlr->adminq, status)) {
    SPDK_ERRLOG ("nvme_identify_controller failed!\n");
    nvme_ctrlr_destruct (discovery_ctrlr);
    if (!status->timed_out) {
      free (status);
    }

    return -ENXIO;
  }

  free (status);

  /* Direct attach through spdk_nvme_connect() API */
  if (direct_connect == true) {
    /* Set the ready state to skip the normal init process */
    discovery_ctrlr->state = NVME_CTRLR_STATE_READY;
    nvme_ctrlr_connected (probe_ctx, discovery_ctrlr);
    nvme_ctrlr_add_process (discovery_ctrlr, 0);
    return 0;
  }

  rc = edk_nvme_fabric_ctrlr_discover (discovery_ctrlr, probe_ctx);
  nvme_ctrlr_destruct (discovery_ctrlr);
  return rc;
}

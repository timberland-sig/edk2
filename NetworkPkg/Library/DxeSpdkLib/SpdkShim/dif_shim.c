/** @file
  edk_dif.c - EDK DIF shim for compatibility with SPDK

Copyright (c) 2024, Dell Inc. or its subsidiaries. All Rights Reserved.<BR>
SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "stdlib_types.h"
#include "spdk/stdinc.h"
#include "spdk/dif.h"

void
spdk_dif_ctx_set_data_offset (
  struct spdk_dif_ctx  *ctx,
  uint32_t             data_offset
  )
{
  return;
}

void
spdk_dif_get_range_with_md (
  uint32_t                   data_offset,
  uint32_t                   data_len,
  uint32_t                   *_buf_offset,
  uint32_t                   *_buf_len,
  const struct spdk_dif_ctx  *ctx
  )
{
  return;
}

int
spdk_dif_update_crc32c_stream (
  struct iovec               *iovs,
  int                        iovcnt,
  uint32_t                   data_offset,
  uint32_t                   data_len,
  uint32_t                   *_crc32c,
  const struct spdk_dif_ctx  *ctx
  )
{
  return 0;
}

int
spdk_dif_set_md_interleave_iovs (
  struct iovec               *iovs,
  int                        iovcnt,
  struct iovec               *buf_iovs,
  int                        buf_iovcnt,
  uint32_t                   data_offset,
  uint32_t                   data_len,
  uint32_t                   *_mapped_len,
  const struct spdk_dif_ctx  *ctx
  )
{
  return 0;
}

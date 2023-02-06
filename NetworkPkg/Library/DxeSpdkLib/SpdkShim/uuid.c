/*-
 *
 * Copyright (c) 2020, Dell EMC All rights reserved
 * SPDX-License-Identifier: BSD-2-Clause-Patent
 
 */

#include "spdk/uuid.h"

SPDK_STATIC_ASSERT (sizeof (struct spdk_uuid) == sizeof (uuid_t), "Size mismatch");

static char const hexdigits_lower[16] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f' };

static void 
uuid_fmt (
  const uuid_t *uuid, 
  char *buf, 
  char const fmt[]
  )
{
  int i;
  char *p = buf;
  uint8_t tmp;

  for (i = 0; i < 16; i++) {
    if (i == 4 || i == 6 || i == 8 || i == 10) {
      *p++ = '-';
    }

    tmp = uuid->b[i];
    *p++ = fmt[tmp >> 4];
    *p++ = fmt[tmp & 15];
  }
  *p = '\0';
}

int
spdk_uuid_fmt_lower (
  char *uuid_str,
  size_t uuid_str_size, 
  const struct spdk_uuid *uuid
  )
{
  if (uuid_str_size < SPDK_UUID_STRING_LEN) {
    return -EINVAL;
  }
  uuid_fmt ((const uuid_t*)uuid, uuid_str, hexdigits_lower);
  return 0;
}

void
spdk_uuid_generate (
  struct spdk_uuid *uuid
  )
{
  return;
}

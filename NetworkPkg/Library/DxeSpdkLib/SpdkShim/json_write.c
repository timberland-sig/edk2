/*-
 * json_write.c - Implements stubs for Json write functions

 * Copyright (c) 2020, Dell EMC All rights reserved
 * SPDX-License-Identifier: BSD-2-Clause-Patent
 */
#include "spdk/stdinc.h"

int 
spdk_json_write_array_begin (
  struct spdk_json_write_ctx *w
  )
{ 
  return 0;
}

int 
spdk_json_write_array_end (
  struct spdk_json_write_ctx *w
  ) 
{
  return 0;
}

int 
spdk_json_write_object_begin (
  struct spdk_json_write_ctx *w
  )
{
  return 0;
}

int 
spdk_json_write_object_end (
  struct spdk_json_write_ctx *w
  )
{
  return 0;
}

int 
spdk_json_write_name (
  struct spdk_json_write_ctx *w, 
  const char *name
  )
{
  return 0;
}

int 
spdk_json_write_name_raw (
  struct spdk_json_write_ctx *w, 
  const char *name, 
  size_t len
  ) 
{
  return 0;
}

int 
spdk_json_write_named_bool (
  struct spdk_json_write_ctx *w, 
  const char *name, 
  bool val
  ) 
{
  return 0;
}

int 
spdk_json_write_named_uint8 (
  struct spdk_json_write_ctx *w, 
  const char *name, 
  uint8_t val
  )
{
  return 0;
}

int 
spdk_json_write_named_uint16 (
  struct spdk_json_write_ctx *w, 
  const char *name, 
  uint16_t val
  ) 
{
  return 0;
}

int 
spdk_json_write_named_int32 (
  struct spdk_json_write_ctx *w, 
  const char *name, 
  int32_t val
  ) 
{
  return 0;
}

int 
spdk_json_write_named_uint32 (
  struct spdk_json_write_ctx *w, 
  const char *name, 
  uint32_t val
  )
{
  return 0;
}

int
spdk_json_write_named_string (
  struct spdk_json_write_ctx *w, 
  const char *name, 
  const char *val
  ) 
{
  return 0;
}

int 
spdk_json_write_named_object_begin (
  struct spdk_json_write_ctx *w, 
  const char *name
  ) 
{
  return 0;
}

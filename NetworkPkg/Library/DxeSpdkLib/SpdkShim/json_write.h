/*-
 * json_write.h - Header for  Json write functions stubs

 * Copyright (c) 2020, Dell EMC All rights reserved
 * SPDX-License-Identifier: BSD-2-Clause-Patent
 */


#ifndef JSONWRITE_H
#define JSONWRITE_H

typedef int (*spdk_json_write_cb)(void *cb_ctx, const void *data, size_t size);

struct spdk_json_write_ctx {
  spdk_json_write_cb write_cb;
  void *cb_ctx;
  size_t buf_filled;
  uint8_t buf[4096];
};

int 
spdk_json_write_array_begin (
  struct spdk_json_write_ctx *w
  );
  
int 
spdk_json_write_array_end (
  struct spdk_json_write_ctx *w
  );
  
int 
spdk_json_write_object_begin (
  struct spdk_json_write_ctx *w
  );
  
int 
spdk_json_write_object_end (
  struct spdk_json_write_ctx *w
  );

int 
spdk_json_write_name (
  struct spdk_json_write_ctx *w, 
  const char *name
  );

int 
spdk_json_write_name_raw (
  struct spdk_json_write_ctx *w, 
  const char *name, 
  size_t len
  );

int 
spdk_json_write_named_bool (
  struct spdk_json_write_ctx *w, 
  const char *name, 
  bool val
  );

int 
spdk_json_write_named_uint8 (
  struct spdk_json_write_ctx *w, 
  const char *name, 
  uint8_t val
  );

int 
spdk_json_write_named_uint16 (
  struct spdk_json_write_ctx *w, 
  const char *name, 
  uint16_t val
  );

int 
spdk_json_write_named_int32 (
  struct spdk_json_write_ctx *w, 
  const char *name, 
  int32_t val
  );

int 
spdk_json_write_named_uint32 (
  struct spdk_json_write_ctx *w, 
  const char *name, 
  uint32_t val
  );

int 
spdk_json_write_named_string (
  struct spdk_json_write_ctx *w, 
  const char *name, 
  const char *val
  );
  
int 
spdk_json_write_named_object_begin (
  struct spdk_json_write_ctx *w, 
  const char *name
  );

#endif
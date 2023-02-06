/*-
 *   sys_types.h - Contains definition of std vector and builtin variable to
 *   keep MSC compiler happy

 *   Copyright (c) 2020, Dell EMC All rights reserved
 *   SPDX-License-Identifier: BSD-2-Clause-Patent
 */


#ifndef SYS_TYPES_H
#define SYS_TYPES_H

#include "std_socket.h"

#ifdef __cplusplus
extern "C" {
#endif

struct iovec {
  void    *iov_base;  /* Starting address */
  size_t  iov_len;    /* Number of bytes to transfer */
};

uint32_t
spdk_u32log2 (
  uint32_t x
  );

uint64_t
spdk_u64log2 (
  uint64_t x
  );

int
getaddrinfo (
  const char* addr, 
  const char* service, 
  struct addrinfo* hints, 
  struct addrinfo** res
  );

void
freeaddrinfo (
  struct addrinfo* res
  );

int
gai_strerror (
  int ret
  );

#if defined(_MSC_VER)


uint32_t
__builtin_ctz (
  uint32_t value
  );
  
uint32_t
 __builtin_ctzll (
   uint64_t value
   );

uint32_t 
__builtin_clz (
  uint32_t value
  );
  
uint32_t 
__builtin_clzll (
  uint64_t value
  );
  
uint32_t 
__builtin_clzl (
  uint64_t value
  );

uint32_t 
__builtin_popcount (
  uint32_t x
  );
  
uint32_t 
__builtin_popcountll (
  uint64_t x
  );
  
#endif

#endif

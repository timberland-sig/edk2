/*-
 *  sys_types.c 

 *  Copyright (c) 2020, Dell EMC All rights reserved
 *  SPDX-License-Identifier: BSD-2-Clause-Patent
 */

#include "spdk/stdinc.h"
#include "spdk/util.h"

#if defined(_MSC_VER)

unsigned char 
_BitScanForward (
  unsigned long *index, 
  unsigned long mask
  );  // NOLINT
  
unsigned char 
_BitScanReverse (
  unsigned long *index, 
  unsigned long mask
  );  // NOLINT
  
unsigned int
__popcnt (
  unsigned int value
  );

uint32_t __builtin_ctz (uint32_t value) {
  uint32_t trailing_zero = 0;
  if (_BitScanForward (&trailing_zero, value))
    return trailing_zero;
  return 32;     // value is 0, return an undefined value.   
}
uint32_t __builtin_clz (uint32_t value) {
  uint32_t leading_zero = 0;
  if (_BitScanReverse (&leading_zero, value)) // _BitScanReverse is MSVC intrinsics
    return 31 - leading_zero;
  return 32;
}
uint32_t __builtin_popcount (uint32_t x) { 
  return __popcnt(x);
}

#if defined(_WIN64)

unsigned char 
_BitScanForward64 (
  unsigned long *index, 
  unsigned __int64 mask
  );  // NOLINT
  
unsigned char 
_BitScanReverse64 (
  unsigned long *index, 
  unsigned __int64 mask
  );  // NOLINT
  
unsigned 
__int64 __popcnt64 (
  unsigned __int64 value
  );

uint32_t 
__builtin_ctzll (
  uint64_t value
  )
{
  unsigned long trailing_zero = 0;
  if (_BitScanForward64 (&trailing_zero, value))
    return trailing_zero;
  return 64;
}

uint32_t
__builtin_clzll (
  uint64_t value
  )
{
  uint32_t leading_zero = 0;
  if (_BitScanReverse64 (&leading_zero, value))
    return 63 - leading_zero;
  return 64;
}

uint32_t 
__builtin_popcountll (
  uint64_t x
  ) 
{ 
  return __popcnt64 (x); 
}
#else 

uint32_t 
__builtin_ctzll (
  uint64_t value
  )
{
  if (value == 0)
    return 64;
  uint32_t msh = (uint32_t)(value >> 32);
  uint32_t lsh = (uint32_t)(value & 0xFFFFFFFF);
  if (msh == 0)
    return __builtin_clz (lsh);
  return 32 + __builtin_clz (msh);
}

uint32_t 
__builtin_clzll (
  uint64_t value
  ) 
{
  if (value == 0)
    return 64;
  uint32_t msh = (uint32_t)(value >> 32);
  uint32_t lsh = (uint32_t)(value & 0xFFFFFFFF);
  if (msh != 0)
    return __builtin_clz (msh);
  return 32 + __builtin_clz (lsh);
}

uint32_t
__builtin_popcountll (
  uint64_t value
  ) 
{
  if (value == 0)
    return 0;
  uint32_t msh = (uint32_t)(value >> 32);
  uint32_t lsh = (uint32_t)(value & 0xFFFFFFFF);
  return __popcnt (msh) + __popcnt (lsh);
}
#endif
#define __builtin_ctzl          __builtin_ctzll
#define __builtin_clzl          __builtin_clzll
#define __builtin_popcountl     __builtin_popcountll
#endif 

uint32_t
spdk_u32log2 (
  uint32_t x
  )
{
  if (x == 0) {
    /* log(0) is undefined */
    return 0;
  }
  return 31u - __builtin_clz (x);
}

uint64_t
spdk_u64log2 (
  uint64_t x
  )
{
  if (x == 0) {
    /* log(0) is undefined */
    return 0;
  }
  return 63u - __builtin_clzl (x);
}

int
getaddrinfo (
  const char* addr, 
  const char* service, 
  struct addrinfo *hints, 
  struct addrinfo **res
  )
{
  *res = AllocatePool (sizeof (struct addrinfo));
  (*res)->ai_addrlen = 0;

  return 0;
}

void
freeaddrinfo (
  struct addrinfo *res
  )
{
  FreePool (res);
}

int
gai_strerror (
int ret
)
{
  return 0;
}

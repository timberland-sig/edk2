/** @file
  barrier.h - Contains defines for spdk barriers.

Copyright (c) 2022, Dell Technologies. All rights reserved.<BR>
SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef SPDK_BARRIER_H
#define SPDK_BARRIER_H

#define spdk_compiler_barrier() MemoryFence()

#endif

/** @file
  barrier.h - Contains defines for spdk barriers.

Copyright (c) 2022 - 2023, Dell Inc. or its subsidiaries. All Rights Reserved.<BR>
SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef SPDK_BARRIER_H
#define SPDK_BARRIER_H

#define spdk_compiler_barrier()  MemoryFence()

#endif

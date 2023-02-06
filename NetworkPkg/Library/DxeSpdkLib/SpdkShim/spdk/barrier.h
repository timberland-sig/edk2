/*-
 *   barrier.h - Contains defines for spdk barriers

 *   Copyright (c) 2020, Dell EMC All rights reserved
 *   SPDX-License-Identifier: BSD-2-Clause-Patent
 */


/** \file
 * Memory barriers
 */

#ifndef SPDK_BARRIER_H
#define SPDK_BARRIER_H

#define spdk_compiler_barrier() MemoryFence()

#endif

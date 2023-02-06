/** \file
 * Likely/unlikely branch prediction macros

 *   Copyright (c) 2020, Dell EMC All rights reserved
 *   SPDX-License-Identifier: BSD-2-Clause-Patent
 */

#ifndef SPDK_LIKELY_H
#define SPDK_LIKELY_H

#include "spdk/stdinc.h"

#define spdk_unlikely(cond)   (cond)
#define spdk_likely(cond)     (cond)

#endif
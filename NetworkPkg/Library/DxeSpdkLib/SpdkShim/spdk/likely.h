/** @file
  Likely/unlikely branch prediction macros

Copyright (c) 2022 - 2023, Dell Inc. or its subsidiaries. All Rights Reserved.<BR>
SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef SPDK_LIKELY_H
#define SPDK_LIKELY_H

#include "spdk/stdinc.h"

#define spdk_unlikely(cond)  (cond)
#define spdk_likely(cond)    (cond)

#endif

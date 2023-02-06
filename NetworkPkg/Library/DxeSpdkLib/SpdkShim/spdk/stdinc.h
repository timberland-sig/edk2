/*-
 *   stdinc.h - Standard includes

 *   Copyright (c) 2020, Dell EMC All rights reserved
 *   SPDX-License-Identifier: BSD-2-Clause-Patent
 */


#ifndef SPDK_STDINC_H
#define SPDK_STDINC_H

#ifdef __cplusplus
extern "C" {
#endif

#include <Base.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/UefiLib.h>
#include <Library/UefiBootServicesTableLib.h> 
#include <Library/MemoryAllocationLib.h>
#include <Library/TimerLib.h>
#include <Library/RngLib.h>

#include "stdlib_types.h"
#include "std_string.h"
#include "std_error.h"
#include "sys_types.h"
#include "spdk/queue_extras.h"
#include "std_socket.h"
#include "pthread_shim.h"
#include "sys/queue.h"

#ifdef __cplusplus
}
#endif

//
// Global variables
//
extern int errno;

#endif /* SPDK_STDINC_H */

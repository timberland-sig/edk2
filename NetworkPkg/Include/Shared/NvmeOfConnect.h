/** @file
  Connect timeout and retry bounds shared by the NvmeOf driver and the socket shim.

  Copyright (c) 2026, Dell Inc. or its subsidiaries. All Rights Reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef _NVMEOF_CONNECT_H_
#define _NVMEOF_CONNECT_H_

//
// This header is included by NvmeOfConfigVfr.vfr. VfrCompile cannot parse C
// declarations, so keep it free of #include directives and type definitions.
//

//
// Connect timeout, in milliseconds.
//
#define CONNECT_MIN_TIMEOUT      100
#define CONNECT_MAX_TIMEOUT      20000
#define CONNECT_DEFAULT_TIMEOUT  10000

//
// Connect retries after the first attempt. Zero means no retry.
//
#define CONNECT_MIN_RETRY      0
#define CONNECT_MAX_RETRY      10
#define CONNECT_DEFAULT_RETRY  1

#endif

/*-
 *   std_error.h - Standard error code definition

 *   Copyright (c) 2020, Dell EMC All rights reserved
 *   SPDX-License-Identifier: BSD-2-Clause-Patent
 */


#ifndef STD_ERROR_H
#define STD_ERROR_H

#include "Uefi/UefiBaseType.h"

#ifdef __cplusplus
extern "C" {
#endif


#define ESUCCESS        0 //EFI_SUCCESS
#define EMINERRORVAL    1 //EFI_INVALID_PARAMETER
#define EPERM           2 //EFI_ACCESS_DENIED
#define ENOENT          3 //EFI_NOT_FOUND
#define ESRCH           4 //EFI_UNSUPPORTED
#define EIO             5 //EFI_UNSUPPORTED
#define ENXIO           6 //EFI_DEVICE_ERROR
#define ENOMEM          9 //EFI_OUT_OF_RESOURCES
#define EFAULT          7 //EFI_BAD_BUFFER_SIZE
#define EBUSY           16 //EFI_NO_RESPONSE
#define EEXIST          8 //EFI_END_OF_FILE
#define ENODEV          9 //EFI_UNSUPPORTED
#define EINVAL          2 //EFI_INVALID_PARAMETER
#define EAGAIN          9 //EFI_OUT_OF_RESOURCES
#define EWOULDBLOCK     9 //EFI_OUT_OF_RESOURCES
#define EINPROGRESS     10 //EFI_ALREADY_STARTED
#define EOPNOTSUPP      11 //EFI_UNSUPPORTED
#define ENOTSUP         12 //EFI_UNSUPPORTED
#define ECONNRESET      13 //EFI_NOT_READY
#define EISCONN         14 //EFI_ALREADY_STARTED
#define ECANCELED       21 //EFI_ABORTED
#define EPROTO          15 //EFI_PROTOCOL_ERROR
#define EOWNERDEAD      16 //EFI_INVALID_PARAMETER
#define EBADF           4 //EFI_BAD_BUFFER_SIZE
#define ERANGE          34 /* Result too large */
#define EAFNOSUPPORT    47 /* Address family not supported by protocol family */


#endif

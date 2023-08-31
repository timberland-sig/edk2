/** @file
  Cli Interface file for NvmeOf driver.

  Copyright (c) 2021 - 2023, Dell Inc. or its subsidiaries. All Rights Reserved.<BR>
  Copyright (c) 2023, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent 

**/

#ifndef _NVMEOF_INTERFACE_CLI_H_
#define _NVMEOF_INTERFACE_CLI_H_

#include <Protocol/NvmeOfPassthru.h>

#define NVMEOF_CLI_UUID \
  { \
    0xc3ccf809, 0x07ed, 0x4b0c, { 0xbf, 0x99, 0xb4, 0x56, 0xf0, 0x95, 0x4d, 0x6c} \
  }

extern EDKII_NVMEOF_PASSTHRU_PROTOCOL gNvmeofPassThroughInstance;

 /**
  Install Cantroller Handler

  @param  NVMEOF_CONNECT_COMMAND     connect data

  @retval EFI_SUCCESS                    All OK
  @retval EFI_OUT_OF_RESOURCES           Out of Resources
  @retval EFI_NOT_FOUND                  Resource Not Found
**/

EFI_STATUS
EFIAPI
InstallControllerHandler (IN NVMEOF_CONNECT_COMMAND *ConnectData);

#endif

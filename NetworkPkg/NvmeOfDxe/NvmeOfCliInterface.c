/** @file
  Interface between Cli Application and Driver

  Copyright (c) 2021 - 2023, Dell Inc. or its subsidiaries. All Rights Reserved.<BR>
  Copyright (c) 2022 - 2023, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Uefi.h>
#include "NvmeOfDriver.h"
#include <Protocol/NvmeOfPassthru.h>

#define CONNECT_TIMEOUT  5000

extern NVMEOF_NBFT  gNvmeOfNbftList[NID_MAX];
extern UINT8        gNvmeOfNbftListIndex;

EFI_STATUS
EFIAPI
NvmeOfGetBootDesc (
  IN  EDKII_NVMEOF_PASSTHRU_PROTOCOL  *This,
  IN  EFI_HANDLE                      Handle,
  OUT CHAR16                          **Description
  )
{
  UINTN    Index;
  BOOLEAN  Found = FALSE;

  for (Index = 0; Index < gNvmeOfNbftListIndex; Index++) {
    if (gNvmeOfNbftList[Index].Device->DeviceHandle == Handle) {
      Found = TRUE;
      break;
    }
  }

  if (Found) {
    CopyMem (
      Description,
      gNvmeOfNbftList[Index].Device->ModelName,
      sizeof (gNvmeOfNbftList[Index].Device->ModelName)
      );
  } else {
    return EFI_NOT_FOUND;
  }

  return EFI_SUCCESS;
}

EDKII_NVMEOF_PASSTHRU_PROTOCOL  gNvmeofPassThroughInstance = {
  NvmeOfGetBootDesc
};

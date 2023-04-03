/** @file
  Header for NVMe-oF Passthrough protocol.

  It is meant to be implemented by EDKII NVMe-oF drivers
  to allow for external control over the driver (e.g. from CLI utilities).
  It also shall allow boot manager libraries to obtain an NVMe-oF-specific
  boot description of a given, supported handle.

  Copyright (c) 2023, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef _EDKII_NVMEOF_PASSTHRU_H_
#define _EDKII_NVMEOF_PASSTHRU_H_

typedef struct _EDKII_NVMEOF_PASSTHRU_PROTOCOL EDKII_NVMEOF_PASSTHRU_PROTOCOL;

/**
  Obtain NVMe-oF-specific boot description for a given supported handle.

  Caller is responsible for freeing the memory for the description allocated
  by this function.

  @param[in]  This                Pointer to EDKII_NVMEOF_PASSTHRU_PROTOCOL instance.
  @param[in]  Handle              Handle for which to obtain boot description.
  @param[out] Description         Resultant boot description.

  @retval EFI_SUCCESS             Boot description was successfully created.
  @retval EFI_UNSUPPORTED         Handle is not related to an NVMe-oF namespace.
**/
typedef
EFI_STATUS
(EFIAPI *EDKII_NVMEOF_PASSTHRU_GET_BOOT_DESC)(
  IN  EDKII_NVMEOF_PASSTHRU_PROTOCOL   *This,
  IN  EFI_HANDLE                       Handle,
  OUT CHAR16                           **Description
  );

typedef struct _EDKII_NVMEOF_PASSTHRU_PROTOCOL {
  EDKII_NVMEOF_PASSTHRU_GET_BOOT_DESC     GetBootDesc;
} EDKII_NVMEOF_PASSTHRU_PROTOCOL;

extern EFI_GUID gEdkiiNvmeofPassThruProtocolGuid;

#endif /* _EDKII_NVMEOF_PASSTHRU_H_ */

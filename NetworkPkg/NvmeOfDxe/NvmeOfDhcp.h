/** @file
  The head file of NVMeOF DHCP4 related configuration routines.

  Copyright (c) 2020, Dell EMC All rights reserved
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef _NVMEOF_DHCP_H_
#define _NVMEOF_DHCP_H_

//As defined in nBFT Spec version 0.32
#define NVMEOF_ROOT_PATH_ID              "NVME+"
#define NVMEOF_ROOT_PATH_FIELD_DELIMITER '/'

#define RP_FIELD_IDX_PROTOCOL           0
#define RP_FIELD_IDX_SERVERNAME         1
#define RP_FIELD_IDX_PORT               2
#define RP_FIELD_IDX_TARGETNAME         3
#define RP_FIELD_IDX_NID                4
#define RP_FIELD_IDX_MAX                5

typedef struct _NVMEOF_ROOT_PATH_FIELD {
  CHAR8 *Str;
  UINT8 Len;
} NVMEOF_ROOT_PATH_FIELD;

/**
  Parse the DHCP ACK to get the address configuration and DNS information.

  @param[in]       Image            The handle of the driver image.
  @param[in]       Controller       The handle of the controller.
  @param[in, out]  ConfigData       The attempt configuration data.

  @retval EFI_SUCCESS           The DNS information is got from the DHCP ACK.
  @retval EFI_OUT_OF_RESOURCES  Failed to allocate memory.
  @retval EFI_NO_MEDIA          There was a media error.
  @retval Others                Other errors as indicated.

**/
EFI_STATUS
NvmeOfDoDhcp (
  IN     EFI_HANDLE                   Image,
  IN     EFI_HANDLE                   Controller,
  IN OUT NVMEOF_ATTEMPT_CONFIG_NVDATA *ConfigData
  );

EFI_STATUS
NvmeOfDhcpExtractRootPath (
  IN      CHAR8                        *RootPath,
  IN      UINT16                       Length,
  IN OUT  NVMEOF_ATTEMPT_CONFIG_NVDATA *ConfigData
  );

/**
  Up to a specified length, convert lower case ASCII characters to
  upper case ASCII characters in a string.

  @param[in, out]  Str     A pointer to a Null-terminated ASCII string.
  @param[in]       Length  The maximum number of ASCII characters to be converted.

**/
VOID
ConvertStrnAsciiCharLowerToUpper (
  IN OUT CHAR8 *Str,
  IN     UINT16 Length
  );
#endif

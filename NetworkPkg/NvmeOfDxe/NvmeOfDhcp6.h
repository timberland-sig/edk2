/** @file
  The header file of NVMeOF DHCP6 related configuration routines.

  Copyright (c) 2020, Dell EMC All rights reserved
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef _NVMEOF_DHCP6_H_
#define _NVMEOF_DHCP6_H_

//As defined in nBFT Spec version 0.32
#define NVMEOF_ROOT_PATH_ID                     "NVME+"
#define NVMEOF_ROOT_PATH_TCP_PROTOCOL_ID        "TCP"
#define NVMEOF_ROOT_PATH_FIELD_DELIMITER        '/'
#define NVMEOF_ROOT_PATH_PROTOCOL_END_DELIMITER ':'
#define NVMEOF_ROOT_PATH_ADDR_START_DELIMITER   '['
#define NVMEOF_ROOT_PATH_ADDR_END_DELIMITER     ']'
#define NVMEOF_ROOT_PATH_PORT_START_DELIMITER   ':'
#define NVMEOF_ROOT_PATH_URN_END_DELIMITER      ':'
#define NVMEOF_ROOT_PATH_NIDTYPE_END_DELIMITER  ':'
#define NVMEOF_ROOT_PATH_NID_EMPTY              '0'
#define NVMEOF_ROOT_PATH_URN_ID                 "URN"
#define NVMEOF_ROOT_PATH_NID_TYPE_UUID          "UUID"
#define NVMEOF_ROOT_PATH_NID_TYPE_EUI64         "EUI64"
#define NVMEOF_ROOT_PATH_NID_TYPE_NGUID         "NGUID"

/**
  Extract the Root Path option and get the required target information from
  Boot File Uniform Resource Locator (URL) Option.

  @param[in]       RootPath      The RootPath string.
  @param[in]       Length        Length of the RootPath option payload.
  @param[in, out]  ConfigData    The Nvmeof session configuration data read from
                                 nonvolatile device.

  @retval EFI_SUCCESS            All required information is extracted from the
                                 RootPath option.
  @retval EFI_NOT_FOUND          The RootPath is not an NVMeOF RootPath.
  @retval EFI_OUT_OF_RESOURCES   Failed to allocate memory.
  @retval EFI_INVALID_PARAMETER  The RootPath is malformatted.

**/
EFI_STATUS
NvmeOfDhcp6ExtractRootPath (
  IN     CHAR8                         *RootPath,
  IN     UINT16                        Length,
  IN OUT NVMEOF_ATTEMPT_CONFIG_NVDATA  *ConfigData
  );

/**
  Parse the DHCP ACK to get the address configuration and DNS information.

  @param[in]       Image         The handle of the driver image.
  @param[in]       Controller    The handle of the controller;
  @param[in, out]  ConfigData    The attempt configuration data.

  @retval EFI_SUCCESS            The DNS information is got from the DHCP ACK.
  @retval EFI_NO_MAPPING         DHCP failed to acquire address and other
                                 information.
  @retval EFI_INVALID_PARAMETER  The DHCP ACK's DNS option is malformatted.
  @retval EFI_DEVICE_ERROR       Some unexpected error happened.
  @retval EFI_OUT_OF_RESOURCES   There is no sufficient resource to finish the
                                 operation.
  @retval EFI_NO_MEDIA           There was a media error.

**/
EFI_STATUS
NvmeOfDoDhcp6 (
  IN     EFI_HANDLE                    Image,
  IN     EFI_HANDLE                    Controller,
  IN OUT NVMEOF_ATTEMPT_CONFIG_NVDATA  *ConfigData
  );

#endif

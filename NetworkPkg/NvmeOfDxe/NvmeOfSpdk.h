/** @file
  Header file containing SPDK API's for NVMeOF driver.

  Copyright (c) 2020, Dell EMC All rights reserved
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/
#ifndef _NVMEOF_SPDK_H_
#define _NVMEOF_SPDK_H_


#include "NvmeOfDriver.h"
#include "NvmeOfImpl.h"

#define IPV4_STRING_SIZE  15
#define IPV6_STRING_SIZE  sizeof("ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff")
#define PORT_STRING_LEN   4
#define MAX_DISCOVERY_LOG_ENTRIES	((uint64_t)1000)


/**
  Populates transport data and calls SPDK probe

  @param  NVMEOF_DRIVER_DATA             Driver Private context.
  @param  NVMEOF_ATTEMPT_CONFIG_NVDATA   Attempt data
  @param  IpVersion                      IP version

  @retval EFI_SUCCESS                     All OK
  @retval EFI_OUT_OF_RESOURCES           Out of Resources
  @retval EFI_NOT_FOUND                  Resource Not Found
**/
EFI_STATUS
NvmeOfProbeControllers (
  IN NVMEOF_DRIVER_DATA            *Private,
  IN NVMEOF_ATTEMPT_CONFIG_NVDATA  *AttemptConfigData,
  IN UINT8                         IpVersion
  );

/**
  Function to fetch ASQSZ value required for nBFT
  @param[in]  Controller           The handle of the controller.

**/
VOID
NVMeOfGetAsqz (
  IN  struct spdk_nvme_ctrlr  *Ctrlr
  );

#endif

/** @file
  This file is used to implement the EFI_DISK_INFO_PROTOCOL interface.

  Copyright (c) 2020, Dell EMC All rights reserved
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/
#include "NvmeOfDriver.h"
#include "NvmeOfDeviceInfo.h"
#include "spdk/nvme.h"

EFI_DISK_INFO_PROTOCOL gNvmeOfDiskInfoProtocolTemplate = {
  EFI_DISK_INFO_NVMEOF_INTERFACE_GUID,
  NvmeOfDiskInfoInquiry,
  NvmeOfDiskInfoIdentify,
  NvmeOfDiskInfoSenseData,
  NvmeOfDiskInfoWhichIde
};

/**
  Initialize the installation of DiskInfo protocol.

  This function prepares for the installation of DiskInfo protocol on the child handle.
  By default, it installs DiskInfo protocol with NVMeOF interface GUID.

  @param[in]  Device  The pointer of NVMEOF_DEVICE_PRIVATE_DATA.

**/
VOID
InitializeDiskInfo (
  IN  NVMEOF_DEVICE_PRIVATE_DATA    *Device
  )
{
  CopyMem (&Device->DiskInfo, &gNvmeOfDiskInfoProtocolTemplate, sizeof (EFI_DISK_INFO_PROTOCOL));
}

/**
  Provides inquiry information for the controller type.

  This function is used to get inquiry data.  Data format
  of Identify data is defined by the Interface GUID.

  @param[in]      This              Pointer to the EFI_DISK_INFO_PROTOCOL instance.
  @param[in, out] InquiryData       Pointer to a buffer for the inquiry data.
  @param[in, out] InquiryDataSize   Pointer to the value for the inquiry data size.

  @retval EFI_SUCCESS            The command was accepted without any errors.
  @retval EFI_NOT_FOUND          Device does not support this data class
  @retval EFI_DEVICE_ERROR       Error reading InquiryData from device
  @retval EFI_BUFFER_TOO_SMALL   InquiryDataSize not big enough

**/
EFI_STATUS
EFIAPI
NvmeOfDiskInfoInquiry (
  IN     EFI_DISK_INFO_PROTOCOL   *This,
  IN OUT VOID                     *InquiryData,
  IN OUT UINT32                   *InquiryDataSize
  )
{
  return EFI_NOT_FOUND;
}


/**
  Provides identify information for the controller type.

  This function is used to get identify data.  Data format
  of Identify data is defined by the Interface GUID.

  @param[in]      This              Pointer to the EFI_DISK_INFO_PROTOCOL
                                    instance.
  @param[in, out] IdentifyData      Pointer to a buffer for the identify data.
  @param[in, out] IdentifyDataSize  Pointer to the value for the identify data
                                    size.

  @retval EFI_SUCCESS            The command was accepted without any errors.
  @retval EFI_NOT_FOUND          Device does not support this data class
  @retval EFI_DEVICE_ERROR       Error reading IdentifyData from device
  @retval EFI_BUFFER_TOO_SMALL   IdentifyDataSize not big enough

**/
EFI_STATUS
EFIAPI
NvmeOfDiskInfoIdentify (
  IN     EFI_DISK_INFO_PROTOCOL   *This,
  IN OUT VOID                     *IdentifyData,
  IN OUT UINT32                   *IdentifyDataSize
  )
{
  EFI_STATUS                      Status;
  NVMEOF_DEVICE_PRIVATE_DATA      *Device;
  const struct spdk_nvme_ns_data  *NamespaceData;

  Device = NVMEOF_DEVICE_PRIVATE_DATA_FROM_DISK_INFO (This);
  NamespaceData = spdk_nvme_ns_get_data (Device->NameSpace);

  Status = EFI_BUFFER_TOO_SMALL;
  if (*IdentifyDataSize >= sizeof (NamespaceData)) {
    Status = EFI_SUCCESS;
    CopyMem (IdentifyData, &NamespaceData, sizeof (NamespaceData));
  }
  *IdentifyDataSize = sizeof (NamespaceData);
  return Status;
}

/**
  Provides sense data information for the controller type.

  This function is used to get sense data.
  Data format of Sense data is defined by the Interface GUID.

  @param[in]      This              Pointer to the EFI_DISK_INFO_PROTOCOL instance.
  @param[in, out] SenseData         Pointer to the SenseData.
  @param[in, out] SenseDataSize     Size of SenseData in bytes.
  @param[out]     SenseDataNumber   Pointer to the value for the sense data size.

  @retval EFI_SUCCESS            The command was accepted without any errors.
  @retval EFI_NOT_FOUND          Device does not support this data class.
  @retval EFI_DEVICE_ERROR       Error reading SenseData from device.
  @retval EFI_BUFFER_TOO_SMALL   SenseDataSize not big enough.

**/
EFI_STATUS
EFIAPI
NvmeOfDiskInfoSenseData (
  IN     EFI_DISK_INFO_PROTOCOL   *This,
  IN OUT VOID                     *SenseData,
  IN OUT UINT32                   *SenseDataSize,
  OUT    UINT8                    *SenseDataNumber
  )
{
  return EFI_NOT_FOUND;
}


/**
  This function is used to get controller information.

  @param[in]  This         Pointer to the EFI_DISK_INFO_PROTOCOL instance.
  @param[out] IdeChannel   Pointer to the Ide Channel number.  Primary or secondary.
  @param[out] IdeDevice    Pointer to the Ide Device number.  Master or slave.

  @retval EFI_SUCCESS       IdeChannel and IdeDevice are valid.
  @retval EFI_UNSUPPORTED   This is not an IDE device.

**/
EFI_STATUS
EFIAPI
NvmeOfDiskInfoWhichIde (
  IN  EFI_DISK_INFO_PROTOCOL   *This,
  OUT UINT32                   *IdeChannel,
  OUT UINT32                   *IdeDevice
  )
{
  return EFI_UNSUPPORTED;
}
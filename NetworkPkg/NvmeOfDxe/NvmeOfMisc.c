/** @file
  Miscellaneous routines for NVMeOF driver.

  Copyright (c) 2020, Dell EMC All rights reserved
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "NvmeOfImpl.h"
#include "NvmeOfDriver.h"
#include "spdk/nvme.h"
#include "spdk/uuid.h"
#include "nvme_internal.h"
#include "NvmeOfSpdk.h"

GLOBAL_REMOVE_IF_UNREFERENCED CONST CHAR8  NvmeOfHexString[] = "0123456789ABCDEFabcdef";
extern CHAR8                               *gNvmeOfImagePath;
extern EFI_EVENT                           KatoEvent;


/**
  Tests whether a controller handle is being managed by Nvmeof driver.

  This function tests whether the driver specified by DriverBindingHandle is
  currently managing the controller specified by ControllerHandle.  This test
  is performed by evaluating if the protocol specified by ProtocolGuid is
  present on ControllerHandle and is was opened by DriverBindingHandle and Nic
  Device handle with an attribute of EFI_OPEN_PROTOCOL_BY_DRIVER.
  If ProtocolGuid is NULL, then ASSERT().

  @param  ControllerHandle     A handle for a controller to test.
  @param  DriverBindingHandle  Specifies the driver binding handle for the
  driver.
  @param  ProtocolGuid         Specifies the protocol that the driver specified
  by DriverBindingHandle opens in its Start()
  function.

  @retval EFI_SUCCESS          ControllerHandle is managed by the driver
  specified by DriverBindingHandle.
  @retval EFI_UNSUPPORTED      ControllerHandle is not managed by the driver
  specified by DriverBindingHandle.

 **/
EFI_STATUS
EFIAPI
NvmeOfTestManagedDevice (
  IN  EFI_HANDLE       ControllerHandle,
  IN  EFI_HANDLE       DriverBindingHandle,
  IN  EFI_GUID         *ProtocolGuid
  )
{
  EFI_STATUS     Status;
  VOID           *ManagedInterface;
  EFI_HANDLE     NicControllerHandle;

  ASSERT (ProtocolGuid != NULL);

  NicControllerHandle = NetLibGetNicHandle (ControllerHandle, ProtocolGuid);
  if (NicControllerHandle == NULL) {
    return EFI_UNSUPPORTED;
  }

  Status = gBS->OpenProtocol (
                  ControllerHandle,
                  (EFI_GUID *)ProtocolGuid,
                  &ManagedInterface,
                  DriverBindingHandle,
                  NicControllerHandle,
                  EFI_OPEN_PROTOCOL_BY_DRIVER
                  );
  if (!EFI_ERROR (Status)) {
    gBS->CloseProtocol (
           ControllerHandle,
           (EFI_GUID *)ProtocolGuid,
           DriverBindingHandle,
           NicControllerHandle
           );
    return EFI_UNSUPPORTED;
  }

  if (Status != EFI_ALREADY_STARTED) {
    return EFI_UNSUPPORTED;
  }

  return EFI_SUCCESS;
}

/**
  Delete the recorded NIC info from global structure. Also delete corresponding
  attempts.

  @param[in]  Controller         The handle of the controller.

  @retval EFI_SUCCESS            The operation is completed.
  @retval EFI_NOT_FOUND          The NIC info to be deleted is not recorded.

 **/
EFI_STATUS
NvmeOfRemoveNic (
  IN EFI_HANDLE  Controller
  )
{
  EFI_STATUS                    Status;
  NVMEOF_NIC_INFO               *NicInfo;
  LIST_ENTRY                    *Entry;
  LIST_ENTRY                    *NextEntry;
  NVMEOF_NIC_INFO               *ThisNic;
  NVMEOF_ATTEMPT_ENTRY          *AttemptEntry;
  EFI_MAC_ADDRESS               MacAddr;
  UINTN                         HwAddressSize;
  UINT16                        VlanId;

  //
  // Get MAC address of this network device.
  //
  Status = NetLibGetMacAddress (Controller, &MacAddr, &HwAddressSize);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  //
  // Get VLAN ID of this network device.
  //
  VlanId = NetLibGetVlanId (Controller);

  //
  // Check whether the NIC information exists.
  //
  ThisNic = NULL;

  NET_LIST_FOR_EACH (Entry, &mNicPrivate->NicInfoList) {
    NicInfo = NET_LIST_USER_STRUCT (Entry, NVMEOF_NIC_INFO, Link);
    if (NicInfo->HwAddressSize == HwAddressSize &&
        CompareMem (&NicInfo->PermanentAddress, MacAddr.Addr, HwAddressSize) == 0 &&
        NicInfo->VlanId == VlanId) {

      ThisNic = NicInfo;
      break;
    }
  }

  if (ThisNic == NULL) {
    return EFI_NOT_FOUND;
  }

  mNicPrivate->CurrentNic = ThisNic->NicIndex;

  RemoveEntryList (&ThisNic->Link);
  FreePool (ThisNic);
  mNicPrivate->NicCount--;

  //
  // Remove all attempts related to this NIC.
  //
  NET_LIST_FOR_EACH_SAFE (Entry, NextEntry, &mNicPrivate->AttemptConfigs) {
    AttemptEntry = NET_LIST_USER_STRUCT (Entry, NVMEOF_ATTEMPT_ENTRY, Link);
    if (AttemptEntry->Data.NicIndex == mNicPrivate->CurrentNic) {
      RemoveEntryList (&AttemptEntry->Link);
      mNicPrivate->AttemptCount--;
      FreePool (AttemptEntry);
    }
  }

  return EFI_SUCCESS;
}

/**
   Get the NIC's PCI location and return it according to the composited
   format defined in NVMEOF Boot Firmware Table.

   @param[in]   Controller        The handle of the controller.
   @param[out]  Bus               The bus number.
   @param[out]  Device            The device number.
   @param[out]  Function          The function number.

   @return      The composited representation of the NIC PCI location.

 **/
VOID
NvmeOfGetNicPciLocation (
  IN EFI_HANDLE  Controller,
  OUT UINTN      *Bus,
  OUT UINTN      *Device,
  OUT UINTN      *Function
  )
{
  EFI_STATUS                Status;
  EFI_DEVICE_PATH_PROTOCOL  *DevicePath;
  EFI_HANDLE                PciIoHandle;
  EFI_PCI_IO_PROTOCOL       *PciIo;
  UINTN                     Segment = 0;

  Status = gBS->HandleProtocol (
                  Controller,
                  &gEfiDevicePathProtocolGuid,
                  (VOID **)&DevicePath
                  );
  if (EFI_ERROR (Status)) {
    return;
  }

  Status = gBS->LocateDevicePath (
                  &gEfiPciIoProtocolGuid,
                  &DevicePath,
                  &PciIoHandle
                  );
  if (EFI_ERROR (Status)) {
    return;
  }

  Status = gBS->HandleProtocol (PciIoHandle, &gEfiPciIoProtocolGuid, (VOID **)&PciIo);
  if (EFI_ERROR (Status)) {
    return;
  }

  Status = PciIo->GetLocation (PciIo, &Segment, Bus, Device, Function);
  if (EFI_ERROR (Status)) {
    return;
  }
}

/**
  Record the NIC info in global structure.

  @param[in]  Controller         The handle of the controller.
  @param[in]  Image              Handle of the image.

  @retval EFI_SUCCESS            The operation is completed.
  @retval EFI_OUT_OF_RESOURCES   Do not have sufficient resources to finish this
                                 operation.

**/
EFI_STATUS
NvmeOfSaveNic (
  IN EFI_HANDLE  Controller,
  IN EFI_HANDLE  Image
  )
{
  EFI_STATUS                  Status;
  NVMEOF_NIC_INFO             *NicInfo;
  LIST_ENTRY                  *Entry;
  EFI_MAC_ADDRESS             MacAddr;
  UINTN                       HwAddressSize = 0;
  UINT16                      VlanId = 0;

  //
  // Get MAC address of this network device.
  //
  Status = NetLibGetMacAddress (Controller, &MacAddr, &HwAddressSize);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  //
  // Get VLAN ID of this network device.
  //
  VlanId = NetLibGetVlanId (Controller);

  //
  // Check whether the NIC info already exists. Return directly if so.
  //
  NET_LIST_FOR_EACH (Entry, &mNicPrivate->NicInfoList) {
    NicInfo = NET_LIST_USER_STRUCT (Entry, NVMEOF_NIC_INFO, Link);
    if (NicInfo->HwAddressSize == HwAddressSize &&
        CompareMem (&NicInfo->PermanentAddress, MacAddr.Addr, HwAddressSize) == 0 &&
        NicInfo->VlanId == VlanId) {
        mNicPrivate->CurrentNic = NicInfo->NicIndex;

      return EFI_SUCCESS;
    }

    if (mNicPrivate->MaxNic < NicInfo->NicIndex) {
      mNicPrivate->MaxNic = NicInfo->NicIndex;
    }
  }

  //
  // Record the NIC info in private structure.
  //
  NicInfo = AllocateZeroPool (sizeof (NVMEOF_NIC_INFO));
  if (NicInfo == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  CopyMem (&NicInfo->PermanentAddress, MacAddr.Addr, HwAddressSize);
  NicInfo->HwAddressSize  = (UINT32)HwAddressSize;
  NicInfo->VlanId         = VlanId;
  NicInfo->NicIndex       = (UINT8)(mNicPrivate->MaxNic + 1);
  mNicPrivate->MaxNic     = NicInfo->NicIndex;

  //
  // Get the PCI location.
  //
  NvmeOfGetNicPciLocation (
    Controller,
    &NicInfo->BusNumber,
    &NicInfo->DeviceNumber,
    &NicInfo->FunctionNumber
    );

  InsertTailList (&mNicPrivate->NicInfoList, &NicInfo->Link);
  mNicPrivate->NicCount++;

  mNicPrivate->CurrentNic = NicInfo->NicIndex;
  return EFI_SUCCESS;
}

/**
  Create the NvmeOf driver data.

  @param[in] Image      The handle of the driver image.
  @param[in] Controller The handle of the controller.

  @return The NvmeOf driver data created.
  @retval NULL Other errors as indicated.

**/
NVMEOF_DRIVER_DATA *
NvmeOfCreateDriverData (
  IN EFI_HANDLE  Image,
  IN EFI_HANDLE  Controller
  )
{
  NVMEOF_DRIVER_DATA *Private;

  Private = AllocateZeroPool (sizeof (NVMEOF_DRIVER_DATA));
  if (Private == NULL) {
    return NULL;
  }

  Private->Signature  = NVMEOF_DRIVER_DATA_SIGNATURE;
  Private->Image      = Image;
  Private->Controller = Controller;
  Private->NvmeOfIdentifier.Reserved = 0;
  InitializeListHead (&Private->UnsubmittedSubtasks);

  return Private;
}


/**
  Check wheather the Controller handle is configured to use DHCP protocol.

  @param[in]  Controller           The handle of the controller.
  @param[in]  IpVersion            IP_VERSION_4 or IP_VERSION_6.
  @param[in]  AttemptConfigData    Pointer containing all attempts.
  @param[in]  TargetCount          Number of attempts.

  @retval TRUE                     The handle of the controller need the Dhcp protocol.
  @retval FALSE                    The handle of the controller does not need the Dhcp protocol.

**/
BOOLEAN
NvmeOfDhcpIsConfigured (
  IN EFI_HANDLE                     Controller,
  IN UINT8                          IpVersion,
  IN NVMEOF_ATTEMPT_CONFIG_NVDATA  *AttemptConfigData,
  IN UINT8                          TargetCount
  )
{
  NVMEOF_ATTEMPT_CONFIG_NVDATA  *AttemptTmp = NULL;
  EFI_STATUS                    Status;
  EFI_MAC_ADDRESS               MacAddr;
  UINTN                         HwAddressSize = 0;
  UINT16                        VlanId = 0;
  CHAR16                        MacString[NVMEOF_MAX_MAC_STRING_LEN];
  CHAR16                        AttemptMacString[NVMEOF_MAX_MAC_STRING_LEN];
  UINT8                         Index = 0;

  //
  // Get MAC address of this network device.
  //
  Status = NetLibGetMacAddress (Controller, &MacAddr, &HwAddressSize);
  if (EFI_ERROR (Status)) {
    return FALSE;
  }
  //
  // Get VLAN ID of this network device.
  //
  VlanId = NetLibGetVlanId (Controller);
  NvmeOfMacAddrToStr (&MacAddr, (UINT32)HwAddressSize, VlanId, MacString);

  // Iterate over the list of attempts.
  for (Index = 0; Index < TargetCount; Index++) {
    AttemptTmp = (NVMEOF_ATTEMPT_CONFIG_NVDATA *)(AttemptConfigData + Index);

    // Check if the attempt is enabled.
    if (AttemptTmp->SubsysConfigData.Enabled == 0) {
      continue;
    }

    AsciiStrToUnicodeStrS (AttemptTmp->MacString, AttemptMacString,
      sizeof (AttemptMacString) / sizeof (AttemptMacString[0]));

    // Check if the attempt is for the same controller under consideration.
    if (StrCmp (MacString, AttemptMacString)) {
      continue;
    }

    // Check if IP mode is set to autoconfig.
    if (AttemptTmp->SubsysConfigData.NvmeofIpMode != IP_MODE_AUTOCONFIG &&
        AttemptTmp->SubsysConfigData.NvmeofIpMode != 
          ((IpVersion == IP_VERSION_4) ? IP_MODE_IP4 : IP_MODE_IP6)) {
      continue;
    }

    // Check if configuration to be fetched from the DHCP server.
    if (AttemptTmp->SubsysConfigData.NvmeofIpMode == IP_MODE_AUTOCONFIG ||
        AttemptTmp->SubsysConfigData.HostInfoDhcp == TRUE ||
        AttemptTmp->SubsysConfigData.NvmeofSubsysInfoDhcp == TRUE) {
      return TRUE;
    }
  }

  return FALSE;
}

/**
  Check whether the Controller handle is configured to use DNS protocol.

  @param[in]  Controller           The handle of the controller.
  @param[in]  AttemptConfigData    Pointer containing all attempts.
  @param[in]  TargetCount          Number of attempts.

  @retval TRUE                     The handle of the controller need the Dns protocol.
  @retval FALSE                    The handle of the controller does not need the Dns protocol.

**/
BOOLEAN
NvmeOfDnsIsConfigured (
  IN EFI_HANDLE                     Controller,
  IN NVMEOF_ATTEMPT_CONFIG_NVDATA  *AttemptConfigData,
  IN UINT8                          TargetCount
  )
{
  NVMEOF_ATTEMPT_CONFIG_NVDATA  *AttemptTmp;
  EFI_STATUS                    Status;
  EFI_MAC_ADDRESS               MacAddr;
  UINTN                         HwAddressSize = 0;
  UINT16                        VlanId = 0;
  CHAR16                        MacString[NVMEOF_MAX_MAC_STRING_LEN];
  CHAR16                        AttemptMacString[NVMEOF_MAX_MAC_STRING_LEN];
  UINT8                         Index = 0;

  //
  // Get MAC address of this network device.
  //
  Status = NetLibGetMacAddress (Controller, &MacAddr, &HwAddressSize);
  if (EFI_ERROR (Status)) {
    return FALSE;
  }
  //
  // Get VLAN ID of this network device.
  //
  VlanId = NetLibGetVlanId (Controller);
  NvmeOfMacAddrToStr (&MacAddr, (UINT32)HwAddressSize, VlanId, MacString);

  // Iterate over the list of attempts.
  for (Index = 0; Index < TargetCount; Index++) {
    AttemptTmp = (NVMEOF_ATTEMPT_CONFIG_NVDATA *)(AttemptConfigData + Index);

    // Check if the attempt is enabled.
    if (AttemptTmp->SubsysConfigData.Enabled == 0) {
      continue;
    }

    AsciiStrToUnicodeStrS (AttemptTmp->MacString, AttemptMacString,
      sizeof (AttemptMacString) / sizeof (AttemptMacString[0]));

    // Check if the attempt is for the same controller under consideration.
    if (StrCmp (MacString, AttemptMacString)) {
      continue;
    }

    // Check if configuration to be fetched from the DNS server.
    if (AttemptTmp->SubsysConfigData.NvmeofDnsMode ||
        AttemptTmp->SubsysConfigData.NvmeofSubsysInfoDhcp) {
      return TRUE;
    }
  }

  return FALSE;
}

/**
  Read the EFI variable (VendorGuid/Name) and return a dynamically allocated
  buffer, and the size of the buffer. If failure, return NULL.

  @param[in]   Name                   String part of EFI variable name.
  @param[in]   VendorGuid             GUID part of EFI variable name.
  @param[out]  VariableSize           Returns the size of the EFI variable that was read.

  @return Dynamically allocated memory that contains a copy of the EFI variable.
  @return Caller is responsible freeing the buffer.
  @retval NULL                   Variable was not read.

**/
VOID *
NvmeOfGetVariableAndSize (
  IN  CHAR16   *Name,
  IN  EFI_GUID *VendorGuid,
  OUT UINTN    *VariableSize
  )
{
  EFI_STATUS  Status;
  UINTN       BufferSize = 0;
  VOID        *Buffer;

  Buffer = NULL;

  //
  // Pass in a zero size buffer to find the required buffer size.
  //
  Status = gRT->GetVariable (Name, VendorGuid, NULL, &BufferSize, Buffer);
  if (Status == EFI_BUFFER_TOO_SMALL) {
    //
    // Allocate the buffer to return
    //
    Buffer = AllocateZeroPool (BufferSize);
    if (Buffer == NULL) {
      return NULL;
    }
    //
    // Read variable into the allocated buffer.
    //
    Status = gRT->GetVariable (Name, VendorGuid, NULL, &BufferSize, Buffer);
    if (EFI_ERROR (Status)) {
      BufferSize = 0;
    }
  }

  *VariableSize = BufferSize;
  return Buffer;
}

/**
  Checks the driver enable condition

  @retval TRUE - Driver loading enabled
  @retval FALSE - Driver loading disabled
**/
BOOLEAN
EFIAPI
NvmeOfGetDriverEnable (void)
{
 //
 // Get the input configuration parameters
 //
  NVMEOF_GLOBAL_DATA            *NvmeOfData;
  UINTN                         NvmeOfDataSize = 0;
  BOOLEAN                       EnableNvmeOf = false;

  NvmeOfData = NvmeOfGetVariableAndSize (
                 L"NvmeofGlobalData",
                 &gNvmeOfConfigGuid,
                 &NvmeOfDataSize
                 );

  if (NvmeOfData == NULL || NvmeOfDataSize == 0) {
    return FALSE;
  } else {
      EnableNvmeOf = NvmeOfData->NvmeOfEnabled;
      FreePool (NvmeOfData);
      return EnableNvmeOf;
  }
}

/**
  Read the attempt configuration data from the UEFI variable.

  @retval EFI_NOT_FOUND - No config data in UEFI variable
  @retval EFI_OUT_OF_RESOURCES - Unable to allocate buffer to read data
  @retval EFI_SUCCESS - Read attempt data successfully
**/
EFI_STATUS
EFIAPI
NvmeOfReadConfigData (void)
{
  EFI_STATUS                    Status;
  NVMEOF_ATTEMPT_CONFIG_NVDATA  *AttemptConfigData;
  NVMEOF_ATTEMPT_CONFIG_NVDATA  *SrcAttemptData;
  NVMEOF_ATTEMPT_ENTRY          *Attempt;
  UINTN                         AttemptConfigSize = 0;
  UINTN                         Index = 0;
  UINTN                         NumOfAttempts = 0;
  NVMEOF_GLOBAL_DATA            *NvmeOfData;
  UINTN                         NvmeOfDataSize = 0;

  DEBUG ((EFI_D_INFO, "Get Configuration Data from UEFI variable!\n"));

  NvmeOfData          = NULL;
  AttemptConfigData   = NULL;

  //
  // Get the input configuration parameters
  //
  NvmeOfData = NvmeOfGetVariableAndSize (
                 L"NvmeofGlobalData",
                 &gNvmeOfConfigGuid,
                 &NvmeOfDataSize
                 );
  if (NvmeOfData == NULL || NvmeOfDataSize == 0) {
    DEBUG ((EFI_D_ERROR, "NvmeOfData Read Failed\n"));
    return EFI_NOT_FOUND;
  }

  AttemptConfigData = NvmeOfGetVariableAndSize (
                        L"Nvmeof_Attemptconfig",
                        &gNvmeOfConfigGuid,
                        &AttemptConfigSize
                        );
  if (AttemptConfigData == NULL || AttemptConfigSize == 0) {
    DEBUG ((EFI_D_ERROR, "AttemptConfigData Read is Blank\n"));
    Status = EFI_NOT_FOUND;
    goto Exit;
  }

  NumOfAttempts = NvmeOfData->NvmeOfTargetCount;

  for (Index = 0; Index < NumOfAttempts; Index++) {
    // Check if the attempt is enabled.
    SrcAttemptData = &AttemptConfigData[Index];
    if (SrcAttemptData->SubsysConfigData.Enabled == 0) {
      continue;
    }

    Attempt = AllocateZeroPool (sizeof (NVMEOF_ATTEMPT_ENTRY));
    if (Attempt == NULL) {
      Status = EFI_OUT_OF_RESOURCES;
      goto Exit;
    }

    InitializeListHead (&Attempt->Link);
    CopyMem (&Attempt->Data, SrcAttemptData, sizeof (NVMEOF_ATTEMPT_CONFIG_NVDATA));

    //
    // Insert new created attempt to array.
    //
    InsertTailList (&mNicPrivate->AttemptConfigs, &Attempt->Link);
    mNicPrivate->AttemptCount++;
  }

 Status = EFI_SUCCESS;

Exit:
  if (NvmeOfData != NULL) {
    FreePool (NvmeOfData);
  }
  if (AttemptConfigData != NULL) {
    FreePool (AttemptConfigData);
  }
  return Status;
}

/**
  Read the UEFI variable.

  @param [OUT] Contents          Contents read from UEFI variable
  @param [OUT] Size              TargetCount of contents read from UEFI variable

  @retval EFI_NOT_FOUND - No config data in UEFI variable
  @retval EFI_SUCCESS - Read attempt data successfully
**/
EFI_STATUS
EFIAPI
NvmeOfReadAttemptVariables (
  OUT VOID      **Contents,
  OUT UINT8     *TargetCount
  )
{
  NVMEOF_GLOBAL_DATA *NvmeOfData = NULL;
  UINTN              Size = 0;
  if (Contents == NULL) {
    return EFI_UNSUPPORTED;
  }
  // Read the NvmeofData to check target count
  NvmeOfData = NvmeOfGetVariableAndSize (
                 L"NvmeofGlobalData",
                 &gNvmeOfConfigGuid,
                 &Size
                 );
  if (NvmeOfData == NULL || Size == 0) {
    return EFI_UNSUPPORTED;
  }

  if (NvmeOfData->DriverVersion != NVMEOF_DRIVER_VERSION) {
    DEBUG ((EFI_D_ERROR, "Driver version %d is not matching with Attempt version %d\n", NVMEOF_DRIVER_VERSION,
      NvmeOfData->DriverVersion));
    FreePool (NvmeOfData);
    return EFI_UNSUPPORTED;
  }

  *TargetCount = NvmeOfData->NvmeOfTargetCount;
  // Return if target count is 0
  if (*TargetCount == 0) {
    FreePool (NvmeOfData);
    return EFI_UNSUPPORTED;
  }

  // Read the attempts
  *Contents = NvmeOfGetVariableAndSize (
                L"Nvmeof_Attemptconfig",
                &gNvmeOfConfigGuid,
                &Size
                );
  if (Contents == NULL || Size == 0) {
    DEBUG ((EFI_D_ERROR, "AttemptConfigData Read Failed\n"));
    FreePool (NvmeOfData);
    return EFI_UNSUPPORTED;
  }
  FreePool (NvmeOfData);
  return EFI_SUCCESS;
}

/**
  Convert the mac address into a hexadecimal encoded "-" separated string.

  @param[in]  Mac     The mac address.
  @param[in]  Len     Length in bytes of the mac address.
  @param[in]  VlanId  VLAN ID of the network device.
  @param[out] Str     The storage to return the mac string.

**/
VOID
NvmeOfMacAddrToStr (
  IN  EFI_MAC_ADDRESS  *Mac,
  IN  UINT32           Len,
  IN  UINT16           VlanId,
  OUT CHAR16           *Str
  )
{
  UINT32  Index = 0;
  CHAR16  *String = NULL;

  for (Index = 0; Index < Len; Index++) {
    Str[3 * Index] = (CHAR16)NvmeOfHexString[(Mac->Addr[Index] >> 4) & 0x0F];
    Str[3 * Index + 1] = (CHAR16)NvmeOfHexString[Mac->Addr[Index] & 0x0F];
    Str[3 * Index + 2] = L':';
  }

  String = &Str[3 * Index - 1];
  if (VlanId != 0) {
    String += UnicodeSPrint (String, 6 * sizeof (CHAR16), L"\\%04x", (UINTN)VlanId);
  }

  *String = L'\0';
}

/**
  Get the recorded NIC info from global structure by the Index.

  @param[in]  NicIndex          The index indicates the position of NIC info.

  @return Pointer to the NIC info, or NULL if not found.

**/
NVMEOF_NIC_INFO *
NvmeOfGetNicInfoByIndex (
  IN UINT8      NicIndex
  )
{
  LIST_ENTRY         *Entry;
  NVMEOF_NIC_INFO    *NicInfo;

  NET_LIST_FOR_EACH (Entry, &mNicPrivate->NicInfoList) {
    NicInfo = NET_LIST_USER_STRUCT (Entry, NVMEOF_NIC_INFO, Link);
    if (NicInfo->NicIndex == NicIndex) {
      return NicInfo;
    }
  }

  return NULL;
}

/**
  Get the various configuration data.

  @param[in] Image      The handle of the driver image.
  @param[in] Controller The handle of the controller.
  @param[in/out]  AttemptConfigData   Attempt data.

  @retval EFI_SUCCESS            The configuration data is retrieved.
  @retval EFI_NOT_FOUND          This NVMeOF driver is not configured yet.
  @retval EFI_OUT_OF_RESOURCES   Failed to allocate memory.

**/
EFI_STATUS
NvmeOfGetConfigData (
  IN EFI_HANDLE                        Image,
  IN EFI_HANDLE                        Controller,
  IN OUT NVMEOF_ATTEMPT_CONFIG_NVDATA  *AttemptConfigData
  )
{
  EFI_STATUS Status = EFI_SUCCESS;

  if (AttemptConfigData->SubsysConfigData.NvmeofIpMode == IP_MODE_AUTOCONFIG) {
    AttemptConfigData->SubsysConfigData.HostInfoDhcp = TRUE;
    AttemptConfigData->SubsysConfigData.NvmeofSubsysInfoDhcp = TRUE;

    AttemptConfigData->AutoConfigureMode =
      (UINT8)(mNicPrivate->Ipv6Flag ? IP_MODE_AUTOCONFIG_IP6 : IP_MODE_AUTOCONFIG_IP4);
    AttemptConfigData->AutoConfigureSuccess = FALSE;
  }

  //
  // Get some information from dhcp server.
  //
  if (AttemptConfigData->SubsysConfigData.HostInfoDhcp || AttemptConfigData->SubsysConfigData.NvmeofSubsysInfoDhcp) {
    if (!mNicPrivate->Ipv6Flag &&
      (AttemptConfigData->SubsysConfigData.NvmeofIpMode == IP_MODE_IP4 ||
        AttemptConfigData->AutoConfigureMode == IP_MODE_AUTOCONFIG_IP4)) {
      Status = NvmeOfDoDhcp (Image, Controller, AttemptConfigData);
      if (!EFI_ERROR (Status)) {
        AttemptConfigData->DhcpSuccess = TRUE;
      }
    } else if (mNicPrivate->Ipv6Flag &&
      (AttemptConfigData->SubsysConfigData.NvmeofIpMode == IP_MODE_IP6 ||
        AttemptConfigData->AutoConfigureMode == IP_MODE_AUTOCONFIG_IP6)) {
      Status = NvmeOfDoDhcp6 (Image, Controller, AttemptConfigData);
      if (!EFI_ERROR (Status)) {
        AttemptConfigData->DhcpSuccess = TRUE;
      }
    } else {
      Status = EFI_NOT_FOUND;
    }
  }

  return Status;
}

/**
  Convert the formatted IP address into the binary IP address.

  @param[in]   Str               The UNICODE string.
  @param[in]   IpMode            Indicates whether the IP address is v4 or v6.
  @param[out]  Ip                The storage to return the ASCII string.

  @retval EFI_SUCCESS            The binary IP address is returned in Ip.
  @retval EFI_INVALID_PARAMETER  The IP string is malformatted or IpMode is
                                 invalid.

**/
EFI_STATUS
NvmeOfAsciiStrToIp (
  IN  CHAR8             *Str,
  IN  UINT8             IpMode,
  OUT EFI_IP_ADDRESS    *Ip
  )
{
  EFI_STATUS            Status;

  if (IpMode == IP_MODE_IP4 || IpMode == IP_MODE_AUTOCONFIG_IP4) {
    return NetLibAsciiStrToIp4 (Str, &Ip->v4);
  } else if (IpMode == IP_MODE_IP6 || IpMode == IP_MODE_AUTOCONFIG_IP6) {
    return NetLibAsciiStrToIp6 (Str, &Ip->v6);
  } else if (IpMode == IP_MODE_AUTOCONFIG) {
    Status = NetLibAsciiStrToIp4 (Str, &Ip->v4);
    if (!EFI_ERROR (Status)) {
      return Status;
    }
    return NetLibAsciiStrToIp6 (Str, &Ip->v6);
  }

  return EFI_INVALID_PARAMETER;
}

/**
  Convert the hexadecimal encoded NID string into the 16-byte NID.

  @param[in]   Str             The hexadecimal encoded NID string.
  @param[out]  NID             Storage to return the 16 byte NID.

  @retval EFI_SUCCESS            The 16-byte NID is stored in NID.
  @retval EFI_INVALID_PARAMETER  The string is malformatted.

**/
EFI_STATUS
NvmeOfAsciiStrToNid (
  IN  CHAR8  *Str,
  OUT CHAR8  *NID
  )
{
  UINTN   Index, IndexValue, IndexNum, SizeStr;
  CHAR8   TemStr[2];
  UINT8   TemValue = 0;
  UINT16  Value[4];

  ZeroMem (NID, 64);
  ZeroMem (TemStr, 2);
  ZeroMem ((UINT8 *)Value, sizeof (Value));
  SizeStr = AsciiStrLen (Str);
  IndexValue = 0;
  IndexNum = 0;

  for (Index = 0; Index < SizeStr; Index++) {
    TemStr[0] = Str[Index];
    TemValue = (UINT8)AsciiStrHexToUint64 (TemStr);
    if (TemValue == 0 && TemStr[0] != '0') {
      if ((TemStr[0] != '-') || (IndexNum == 0)) {
        //
        // Invalid NID Char.
        //
        return EFI_INVALID_PARAMETER;
      }
    }

    if ((TemValue == 0) && (TemStr[0] == '-')) {
      //
      // Next NID value.
      //
      if (++IndexValue >= 32) {
        //
        // Max 32 NID value.
        //
        return EFI_INVALID_PARAMETER;
      }
      //
      // Restart str index for the next NID value.
      //
      IndexNum = 0;
      continue;
    }

    if (++IndexNum > 32) {
      //
      // Each NID Str can't exceed size 32, because it will be as UINT16 value.
      //
      return EFI_INVALID_PARAMETER;
    }

    //
    // Combine UINT16 value.
    //
    Value[IndexValue] = (UINT16)((Value[IndexValue] << 4) + TemValue);
  }

  for (Index = 0; Index <= IndexValue; Index++) {
    *((UINT16 *)&NID[Index * 2]) = HTONS (Value[Index]);
  }

  return EFI_SUCCESS;
}

/**
Get the attempt for NIC being used.

@param[out]  AttemptData       Pointer to attempt structure for the NIC

@retval EFI_SUCCESS            Found attempt for the NIC
@retval EFI_NOT_FOUND          No attempt for current NIC

**/
EFI_STATUS
NvmeOfGetAttemptForCurrentNic (
  OUT NVMEOF_ATTEMPT_CONFIG_NVDATA **AttemptData
  )
{
  CHAR16                        AttemptMacString[NVMEOF_MAX_MAC_STRING_LEN];
  NVMEOF_ATTEMPT_ENTRY          *AttemptEntry;
  CHAR16                        MacString[NVMEOF_MAX_MAC_STRING_LEN];
  NVMEOF_NIC_INFO               *NicInfo;
  LIST_ENTRY                    *Entry;
  LIST_ENTRY                    *NextEntry;

  NET_LIST_FOR_EACH_SAFE (Entry, NextEntry, &mNicPrivate->AttemptConfigs) {
    AttemptEntry = NET_LIST_USER_STRUCT (Entry, NVMEOF_ATTEMPT_ENTRY, Link);

    // Get the Nic information from the NIC list
    NicInfo = NvmeOfGetNicInfoByIndex (mNicPrivate->CurrentNic);
    ASSERT (NicInfo != NULL);
    NvmeOfMacAddrToStr (
      &NicInfo->PermanentAddress,
      NicInfo->HwAddressSize,
      NicInfo->VlanId,
      MacString
      );

    AsciiStrToUnicodeStrS (AttemptEntry->Data.MacString, AttemptMacString,
      sizeof (AttemptMacString) / sizeof (AttemptMacString[0]));
    if (StrCmp (MacString, AttemptMacString) != 0) {
      continue;
    } else {
      *AttemptData = &AttemptEntry->Data;
      return EFI_SUCCESS;
    }
  }

  return EFI_NOT_FOUND;
}

/**
  Get the device path of the NvmeOf tcp connection and update it.

  @param  Private       Drivers private structure.
  @param  AttemptData   Attempt configuration data

  @return The updated device path.
  @retval NULL Other errors as indicated.
**/
EFI_DEVICE_PATH_PROTOCOL *
NvmeOfGetTcpConnectionDevicePath (
  NVMEOF_DEVICE_PRIVATE_DATA       *Device,
  IN NVMEOF_ATTEMPT_CONFIG_NVDATA  *AttemptData
  )
{
  EFI_DEVICE_PATH_PROTOCOL  *DevicePath;
  EFI_STATUS                Status;
  EFI_DEV_PATH              *DPathNode;
  UINTN                     PathLen;

  Status = gBS->HandleProtocol (
                  Device->TcpIo->Handle,
                  &gEfiDevicePathProtocolGuid,
                  (VOID **)&DevicePath
                  );
  if (EFI_ERROR (Status)) {
    return NULL;
  }

  // Duplicate the device path node.
  DevicePath = DuplicateDevicePath (DevicePath);
  if (DevicePath == NULL) {
    return NULL;
  }

  DPathNode = (EFI_DEV_PATH *)DevicePath;

  while (!IsDevicePathEnd (&DPathNode->DevPath)) {
    if (DevicePathType (&DPathNode->DevPath) == MESSAGING_DEVICE_PATH) {
      if (!mNicPrivate->Ipv6Flag && DevicePathSubType (&DPathNode->DevPath) == MSG_IPv4_DP) {
        DPathNode->Ipv4.LocalPort = 0;

        DPathNode->Ipv4.StaticIpAddress =
          (BOOLEAN)(!AttemptData->SubsysConfigData.HostInfoDhcp);

        PathLen = DevicePathNodeLength (&DPathNode->Ipv4);

        if (PathLen == IP4_NODE_LEN_NEW_VERSIONS) {

          IP4_COPY_ADDRESS (
            &DPathNode->Ipv4.GatewayIpAddress,
            &AttemptData->SubsysConfigData.NvmeofSubsysHostGateway
          );

          IP4_COPY_ADDRESS (
            &DPathNode->Ipv4.SubnetMask,
            &AttemptData->SubsysConfigData.NvmeofSubsysHostSubnetMask
          );
        }

        break;
      } else if (mNicPrivate->Ipv6Flag && DevicePathSubType (&DPathNode->DevPath) == MSG_IPv6_DP) {
        DPathNode->Ipv6.LocalPort = 0;
        PathLen = DevicePathNodeLength (&DPathNode->Ipv6);
        if (PathLen == IP6_NODE_LEN_NEW_VERSIONS) {
          DPathNode->Ipv6.IpAddressOrigin = 0;
          DPathNode->Ipv6.PrefixLength = IP6_PREFIX_LENGTH;
          ZeroMem (&DPathNode->Ipv6.GatewayIpAddress, sizeof (EFI_IPv6_ADDRESS));
        } else if (PathLen == IP6_NODE_LEN_OLD_VERSIONS) {
          *((UINT8 *)(&DPathNode->Ipv6) + IP6_OLD_IPADDRESS_OFFSET) =
            (BOOLEAN)(!AttemptData->SubsysConfigData.HostInfoDhcp);
        }

        break;
      }
    }
    DPathNode = (EFI_DEV_PATH *)NextDevicePathNode (&DPathNode->DevPath);
  }

  return &DPathNode->DevPath;
}

/**
  Create device path for the namespace.
   
  @param  Private         Drivers private structure.
  @param  Device          Device for which path has to be created
  @param  DevicePathNode  Device path for the namespace
  @param  AttemptData     Attempt configuration data

  @return EFI_SUCCESS     In case device path creation is successful
  @retval ERROR           In case of failures
**/
EFI_STATUS
NvmeOfBuildDevicePath (
  IN NVMEOF_DRIVER_DATA            *Private,
  NVMEOF_DEVICE_PRIVATE_DATA       *Device,
  OUT EFI_DEVICE_PATH_PROTOCOL     **DevicePathNode,
  IN NVMEOF_ATTEMPT_CONFIG_NVDATA  *AttemptData
  )
{
  NVMEOF_NIC_INFO           *NicInfo;
  EFI_DEVICE_PATH_PROTOCOL  *DevPathNode;
  ACPI_HID_DEVICE_PATH      AcpiNode;
  PCI_DEVICE_PATH           PciNode;
  MAC_ADDR_DEVICE_PATH      MacNode;
  VLAN_DEVICE_PATH          VlanNode;
  EFI_DEVICE_PATH_PROTOCOL  *TcpPathNode;
  EFI_DEV_PATH              *NvmeOfNode;
  UINTN                     DevPathNodeLen;

  // Get the Nic information from the NIC list
  NicInfo = NvmeOfGetNicInfoByIndex (mNicPrivate->CurrentNic);
  ASSERT (NicInfo != NULL);

  // Create ACPI device path for PciRoot
  AcpiNode.Header.Type = ACPI_DEVICE_PATH;
  AcpiNode.Header.SubType = ACPI_DP;
  AcpiNode.UID = NicInfo->BusNumber;
  AcpiNode.HID = EISA_PNP_ID (0x0a03);
  SetDevicePathNodeLength (&AcpiNode.Header, sizeof (AcpiNode));
  DevPathNode = AppendDevicePathNode (NULL, &AcpiNode.Header);
  if (DevPathNode == NULL) {
    DEBUG ((DEBUG_ERROR, "Append APCI device path failed\n"));
    return EFI_OUT_OF_RESOURCES;
  }

  // Create PCI device path
  PciNode.Header.Type = HARDWARE_DEVICE_PATH;
  PciNode.Header.SubType = HW_PCI_DP;
  SetDevicePathNodeLength (&PciNode.Header, sizeof (PciNode));

  PciNode.Device = NicInfo->DeviceNumber;
  PciNode.Function = NicInfo->FunctionNumber;
  DevPathNode = AppendDevicePathNode (DevPathNode, &PciNode.Header);
  if (DevPathNode == NULL) {
    DEBUG ((DEBUG_ERROR, "Append APCI device path failed\n"));
    return EFI_OUT_OF_RESOURCES;
  }

  // Create MAC address device path
  MacNode.Header.Type = MESSAGING_DEVICE_PATH;
  MacNode.Header.SubType = MSG_MAC_ADDR_DP;
  SetDevicePathNodeLength (&MacNode, sizeof (MacNode));
  CopyMem (&MacNode.MacAddress, &NicInfo->PermanentAddress, sizeof (EFI_MAC_ADDRESS));
  MacNode.IfType = NET_IFTYPE_ETHERNET;

  DevPathNode = AppendDevicePathNode (DevPathNode, &MacNode.Header);
  if (DevPathNode == NULL) {
    DEBUG ((DEBUG_ERROR, "Append MAC address device path failed\n"));
    return EFI_OUT_OF_RESOURCES;
  }

  // Create VLAN address device path
  if ((NicInfo->VlanId) > 0) {
    VlanNode.Header.Type = MESSAGING_DEVICE_PATH;
    VlanNode.Header.SubType = MSG_VLAN_DP;
    SetDevicePathNodeLength (&VlanNode, sizeof (VlanNode));
    VlanNode.VlanId = NicInfo->VlanId;
    DevPathNode = AppendDevicePathNode (DevPathNode, &VlanNode.Header);
    if (DevPathNode == NULL) {
      DEBUG ((DEBUG_ERROR, "Append VLAN address device path failed\n"));
      return EFI_OUT_OF_RESOURCES;
    }
  }

  // Create IP address device path
  TcpPathNode = NvmeOfGetTcpConnectionDevicePath (Device, AttemptData);
  if (TcpPathNode == NULL) {
    DEBUG ((DEBUG_ERROR, "Get TCP connection device path node failed\n"));
    return EFI_NOT_FOUND;
  }

  DevPathNode = AppendDevicePathNode (DevPathNode, TcpPathNode);
  if (DevPathNode == NULL) {
    DEBUG ((DEBUG_ERROR, "Append TCP connection device path failed\n"));
    return EFI_OUT_OF_RESOURCES;
  }

  // Create NvmeOf device path
  DevPathNodeLen = sizeof (NVMEOF_DEVICE_PATH) + 
                    AsciiStrLen (Device->NameSpace->ctrlr->trid.subnqn) + 1;
  NvmeOfNode = AllocateZeroPool (DevPathNodeLen);
  if (NvmeOfNode == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }
  NvmeOfNode->NvmeOf.Nidt = Device->NamespaceIdType;
  NvmeOfNode->DevPath.Type = MESSAGING_DEVICE_PATH;
  NvmeOfNode->DevPath.SubType = MSG_NVMEOF_DP;
  SetDevicePathNodeLength (&NvmeOfNode->DevPath, DevPathNodeLen);

  CopyMem (NvmeOfNode->NvmeOf.NamespaceUuid, Device->NamespaceUuid->u.raw,
            sizeof (Device->NamespaceUuid->u.raw));
  AsciiStrCpyS ((CHAR8 *)NvmeOfNode + sizeof (NVMEOF_DEVICE_PATH),
    AsciiStrLen (Device->NameSpace->ctrlr->trid.subnqn) + 1,
    Device->NameSpace->ctrlr->trid.subnqn);

  DevPathNode = AppendDevicePathNode (DevPathNode, (EFI_DEVICE_PATH_PROTOCOL *)NvmeOfNode);
  if (DevPathNode == NULL) {
    DEBUG ((DEBUG_ERROR, "Append NVMeOF device path failed\n"));
    return EFI_OUT_OF_RESOURCES;
  }

  *DevicePathNode = DevPathNode;
  FreePool (NvmeOfNode);
  return EFI_SUCCESS;
}

/**
  Create subsystem and NID/NSID map for the namespaces of the subsystem

  @param  Ctrlr           SPDK instance representing a subsystem
  @param  NqnNidMap       Structure containing fields for subsystem and NID/NSID

  @return NamespaceCount  Number of namespaces in the map. 
**/
UINT8
NvmeOfCreateNqnNamespaceMap (
  IN struct spdk_nvme_ctrlr *Ctrlr,
  OUT NVMEOF_NQN_NID *NqnNidMap
  )
{
  int                       Index = 0;
  const struct spdk_uuid    *Uuid;
  UINT32                    NumOfNamespaces;
  UINT32                    Nsid;
  struct spdk_nvme_ns       *Namespace;

  // Get the number of namespaces managed by the controller
  NumOfNamespaces = spdk_nvme_ctrlr_get_num_ns (Ctrlr);
  CopyMem (NqnNidMap->SubsystemNqn, Ctrlr->trid.subnqn, sizeof (Ctrlr->trid.subnqn));

  // Namespace IDs start at 1, not 0.
  for (Nsid = 1; Nsid <= NumOfNamespaces; Nsid++) {

    Namespace = spdk_nvme_ctrlr_get_ns (Ctrlr, Nsid);
    if (Namespace == NULL) {
      continue;
    }
    if (!spdk_nvme_ns_is_active (Namespace)) {
      continue;
    }
    Uuid = spdk_nvme_ns_get_uuid (Namespace);
    if (Uuid == NULL) {
        Uuid = (struct spdk_uuid *) spdk_nvme_ns_get_nguid (Namespace);
    }
    CopyMem (&NqnNidMap->Nid[Index][0], Uuid->u.raw, sizeof (Uuid->u.raw));
    NqnNidMap->Nsid[Index] = Nsid;
    NqnNidMap->NamespaceCount++;
    Index++;
  }

  return NqnNidMap->NamespaceCount;
}

/**
  Function to filter namespaces.

  @param  NqnNidMap       Structure contaning subsystem and its namespace ids.

  @return Filtered namespace map
**/
NVMEOF_NQN_NID*
NvmeOfFilterNamespaces (
  NVMEOF_NQN_NID      *NqnNidMap
  )
{
  NVMEOF_NQN_NID *FilteredNqnNidMap = NqnNidMap;
  return FilteredNqnNidMap;
}

/**
  Create UEFI variable containing the information discovered using the
  discovery NQN. Two variables will be exported: 1-Count of subsystems 
  discovred. 2-Array of structures containing the information.

  @return Status  Status returned by SetVariable function. 
**/
EFI_STATUS
NvmeOfSetDiscoveryInfo (VOID)
{
  EFI_STATUS Status = EFI_SUCCESS;

  if (NqnNidMapINdex > 0) {
    Status = gRT->SetVariable (
                    L"NvmeOfDiscoverySubsystemCount",
                    &gNvmeOfConfigGuid,
                    EFI_VARIABLE_BOOTSERVICE_ACCESS | EFI_VARIABLE_NON_VOLATILE,
                    sizeof (NqnNidMapINdex),
                    &NqnNidMapINdex
                    );
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "Error occured while setting NvmeOfDiscoverySubsystemCount\n"));
      return Status;
    }

    Status = gRT->SetVariable (
                    L"NvmeOfDiscoveryData",
                    &gNvmeOfConfigGuid,
                    EFI_VARIABLE_BOOTSERVICE_ACCESS | EFI_VARIABLE_NON_VOLATILE,
                    sizeof (NVMEOF_NQN_NID)*NqnNidMapINdex,
                    gNvmeOfNqnNidMap
                    );
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "Error occured while setting NvmeOfDiscoveryData\n"));
      return Status;
    }
  } else {
    DEBUG ((DEBUG_INFO, "No Discovery data to set\n"));
  }

  return Status;
}

/**
  Calculate the prefix length of the IPv4 subnet mask.

  @param[in]  SubnetMask The IPv4 subnet mask.

  @return     The prefix length of the subnet mask.
  @retval 0   Other errors as indicated.

**/
UINT8
NvmeOfGetSubnetMaskPrefixLength (
  IN EFI_IPv4_ADDRESS  *SubnetMask
  )
{
  UINT8   Len;
  UINT32  ReverseMask;

  //
  // The SubnetMask is in network byte order.
  //
  ReverseMask = (SubnetMask->Addr[0] << 24) | (SubnetMask->Addr[1] << 16) |
    (SubnetMask->Addr[2] << 8) | (SubnetMask->Addr[3]);

  //
  // Reverse it.
  //
  ReverseMask = ~ReverseMask;
  if ((ReverseMask & (ReverseMask + 1)) != 0) {
    return 0;
  }

  Len = 0;

  while (ReverseMask != 0) {
    ReverseMask = ReverseMask >> 1;
    Len++;
  }

  return (UINT8)(32 - Len);
}

/**
  Retrieve the IPv6 Address/Prefix/Gateway from the established TCP connection, these informations
  will be filled in the NVMeOF Boot Firmware Table.

  @param[in, out]  NvData      The connection data.

  @retval     EFI_SUCCESS      Get the NIC information successfully.
  @retval     Others           Other errors as indicated.

**/
EFI_STATUS
NvmeOfGetIp6NicInfo (
  IN OUT NVMEOF_SUBSYSTEM_CONFIG_NVDATA  *NvData,
  IN TCP_IO                              *TcpIo
  )
{
  EFI_TCP6_PROTOCOL            *Tcp6;
  EFI_IP6_MODE_DATA            Ip6ModeData;
  EFI_STATUS                   Status;
  EFI_IPv6_ADDRESS             *TargetIp;
  UINTN                        Index;
  UINT8                        SubnetPrefixLength;
  UINTN                        RouteEntry;

  TargetIp = &NvData->NvmeofSubSystemIp.v6;
  Tcp6 = TcpIo->Tcp.Tcp6;

  ZeroMem (&Ip6ModeData, sizeof (EFI_IP6_MODE_DATA));
  Status = Tcp6->GetModeData (
                   Tcp6,
                   NULL,
                   NULL,
                   &Ip6ModeData,
                   NULL,
                   NULL
                   );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  if (!Ip6ModeData.IsConfigured) {
    Status = EFI_ABORTED;
    goto ON_EXIT;
  }

  IP6_COPY_ADDRESS (&NvData->NvmeofSubsysHostIP, &Ip6ModeData.ConfigData.StationAddress);

  NvData->NvmeofPrefixLength = 0;
  for (Index = 0; Index < Ip6ModeData.AddressCount; Index++) {
    if (EFI_IP6_EQUAL (&NvData->NvmeofSubsysHostIP.v6, &Ip6ModeData.AddressList[Index].Address)) {
      NvData->NvmeofPrefixLength = Ip6ModeData.AddressList[Index].PrefixLength;
      break;
    }
  }

  SubnetPrefixLength = 0;
  RouteEntry = Ip6ModeData.RouteCount;
  for (Index = 0; Index < Ip6ModeData.RouteCount; Index++) {
    if (NetIp6IsNetEqual (TargetIp, &Ip6ModeData.RouteTable[Index].Destination, Ip6ModeData.RouteTable[Index].PrefixLength)) {
      if (SubnetPrefixLength < Ip6ModeData.RouteTable[Index].PrefixLength) {
        SubnetPrefixLength = Ip6ModeData.RouteTable[Index].PrefixLength;
        RouteEntry = Index;
      }
    }
  }
  if (RouteEntry != Ip6ModeData.RouteCount) {
    IP6_COPY_ADDRESS (&NvData->NvmeofSubsysHostGateway, &Ip6ModeData.RouteTable[RouteEntry].Gateway);
  }

ON_EXIT:
  if (Ip6ModeData.AddressList != NULL) {
    FreePool (Ip6ModeData.AddressList);
  }
  if (Ip6ModeData.GroupTable != NULL) {
    FreePool (Ip6ModeData.GroupTable);
  }
  if (Ip6ModeData.RouteTable != NULL) {
    FreePool (Ip6ModeData.RouteTable);
  }
  if (Ip6ModeData.NeighborCache != NULL) {
    FreePool (Ip6ModeData.NeighborCache);
  }
  if (Ip6ModeData.PrefixTable != NULL) {
    FreePool (Ip6ModeData.PrefixTable);
  }
  if (Ip6ModeData.IcmpTypeList != NULL) {
    FreePool (Ip6ModeData.IcmpTypeList);
  }

  return Status;
}

/**
  Save Root Path for NBFT.

  @param[in]  RootPath Root path string.
  @param[in]  Length   Length of Root path string.

**/
VOID
NvmeOfSaveRootPathForNbft (
  IN  CHAR8 *RootPath,
  IN  UINT32 Length
  )
{
  gNvmeOfRootPath = NULL;
  gNvmeOfRootPath = (CHAR8 *)AllocatePool (Length + 1);
  if (gNvmeOfRootPath != NULL) {
    CopyMem (gNvmeOfRootPath, RootPath, Length);
    gNvmeOfRootPath[Length] = '\0';
  }
}

/**
  Convert IP address to string

  @param[in]  IpAddr      IP to convert
  @param[out] IpAddrStr   IP string.
  @param[in]  Ipv6Flag    IP type.

**/
VOID
NvmeOfIpToStr (
  IN EFI_IP_ADDRESS IpAddr,
  OUT CHAR8         *IpAddrStr,
  IN BOOLEAN        Ipv6Flag
  )
{
  CHAR8   Ipv4Addr[IPV4_STRING_SIZE];
  CHAR16  Ipv6Addr[IPV6_STRING_SIZE];

  if (!Ipv6Flag) {
    sprintf (Ipv4Addr, "%d.%d.%d.%d", IpAddr.v4.Addr[0],
      IpAddr.v4.Addr[1],
      IpAddr.v4.Addr[2],
      IpAddr.v4.Addr[3]);
    CopyMem (IpAddrStr, Ipv4Addr, sizeof (Ipv4Addr));
  } else {
    NetLibIp6ToStr (&IpAddr.v6, Ipv6Addr, sizeof (Ipv6Addr));
    UnicodeStrToAsciiStrS (Ipv6Addr, IpAddrStr, sizeof (Ipv6Addr));
  }
}

/**
  Function to check if attempt configured for this NIC

  @param[in]  Controller           The handle of the controller.
  @param[in]  AttemptConfigData    Pointer containing all attempts.
  @param[in]  TargetCount          Number of attempts.

  @retval TRUE                     Attempt present
  @retval FALSE                    Attempt not present

**/
BOOLEAN
NvmeOfIsAttemptPresentForMac (
  IN EFI_HANDLE                    Controller,
  IN NVMEOF_ATTEMPT_CONFIG_NVDATA  *AttemptConfigData,
  IN UINT8                         TargetCount
  )
{
  NVMEOF_ATTEMPT_CONFIG_NVDATA  *AttemptTmp;
  EFI_STATUS                    Status;
  EFI_MAC_ADDRESS               MacAddr;
  UINTN                         HwAddressSize = 0;
  UINT16                        VlanId = 0;
  CHAR16                        MacString[NVMEOF_MAX_MAC_STRING_LEN];
  CHAR16                        AttemptMacString[NVMEOF_MAX_MAC_STRING_LEN];
  UINT8                         Index = 0;

  //
  // Get MAC address of this network device.
  //
  Status = NetLibGetMacAddress (Controller, &MacAddr, &HwAddressSize);
  if (EFI_ERROR (Status)) {
    return FALSE;
  }
  //
  // Get VLAN ID of this network device.
  //
  VlanId = NetLibGetVlanId (Controller);
  NvmeOfMacAddrToStr (&MacAddr, (UINT32)HwAddressSize, VlanId, MacString);

  // Iterate over the list of attempts.
  for (Index = 0; Index < TargetCount; Index++) {
    AttemptTmp = (NVMEOF_ATTEMPT_CONFIG_NVDATA *)(AttemptConfigData + Index);

    // Check if the attempt is enabled.
    if (AttemptTmp->SubsysConfigData.Enabled == 0) {
      continue;
    }

    AsciiStrToUnicodeStrS (AttemptTmp->MacString, AttemptMacString,
      sizeof (AttemptMacString) / sizeof (AttemptMacString[0]));

    // Check if the attempt is for the same controller under consideration.
    if (StrCmp (MacString, AttemptMacString)) {
      continue;
    }

    return TRUE;
  }

  return FALSE;
}

/**
  Function to determine if an integer represents character that is a hex digit

  @param[in]  c           Integer value to check

  @retval Non-Zero        Valid Hex digit
  @retval Zero            Invalid Hex digit
**/
UINT8
IsHexDigit (
  IN CHAR8 c
  )
{
  //
  // <hexdigit> ::= [0-9] | [a-f] | [A-F]
  //
  return ((('0' <= (c)) && ((c) <= '9')) ||
    (('a' <= (c)) && ((c) <= 'f')) ||
    (('A' <= (c)) && ((c) <= 'F')));
}

/**
  Function to check if valid UUID passed.

  @param[in]  Uuid           UUID string to be validated.

  @retval TRUE               Valid UUID
  @retval FALSE              Invalid UUID
**/
BOOLEAN
IsUuidValid (
  IN CHAR8 *Uuid
  )
{
  UINT8 i;
  for (i = 0; i < 36; i++) {
    if (i == 8 || i == 13 || i == 18 || i == 23) {
      if (Uuid[i] != '-')
        return FALSE;
    } else if (!IsHexDigit (Uuid[i])) {
      return FALSE;
    }
  }
  return TRUE;
}

/**
  Function to check if NIDs are valid and equal

  @param[in]  Nid1           NID1 to be checked
  @param[in]  Nid2           NID2 to be checked

  @retval TRUE               NIDs are valid and equal
  @retval FALSE              NIDs are invalid or unequal
**/
BOOLEAN
NvmeOfMatchNid (
  IN CHAR8 *Nid1,
  IN CHAR8 *Nid2
  )
{
  if (AsciiStriCmp (Nid1, Nid2) != 0) {
    return FALSE;
  }

  return TRUE;
}

/**
  Check whether an HBA adapter supports NVMeOF offload capability
  If yes, return EFI_SUCCESS.

  @retval EFI_SUCCESS              Offload capability supported.
  @retval EFI_NOT_FOUND            Offload capability not supported.
**/
EFI_STATUS
NvmeOfCheckOffloadCapableUsingAip (
  VOID
  )
{
  return EFI_NOT_FOUND;
}

/**
  Gets Driver Image Path
  @param[in] EFI_HANDLE   Handle,
  @param[out] CHAR16      **Name

  @retval EFI_SUCCESS     Getting Operation Status.
  @retval EFI_ERROR       Error in operation
**/

EFI_STATUS
NvmeOfGetDriverImageName (
  IN EFI_HANDLE   Handle,
  OUT CHAR16      **Name
  )
{
  EFI_STATUS                Status;
  EFI_LOADED_IMAGE_PROTOCOL *LoadedImage;
  EFI_DEVICE_PATH_PROTOCOL  *ImageDevicePath;
  EFI_DEVICE_PATH_PROTOCOL  *FinalPath;

  FinalPath = NULL;

  Status = gBS->OpenProtocol (
                  Handle,
                  &gEfiLoadedImageProtocolGuid,
                  (VOID**)&LoadedImage,
                  gImageHandle,
                  NULL,
                  EFI_OPEN_PROTOCOL_GET_PROTOCOL
                  );
  if (EFI_ERROR (Status)) {
    return (Status);
  }
  Status = gBS->OpenProtocol (
                  LoadedImage->DeviceHandle,
                  &gEfiDevicePathProtocolGuid,
                  (VOID**)&ImageDevicePath,
                  gImageHandle,
                  NULL,
                  EFI_OPEN_PROTOCOL_GET_PROTOCOL
                  );
  if (!EFI_ERROR (Status)) {
    FinalPath = AppendDevicePath (ImageDevicePath, LoadedImage->FilePath);
    gBS->CloseProtocol (
           LoadedImage->DeviceHandle,
           &gEfiDevicePathProtocolGuid,
           gImageHandle,
           NULL
           );
    gBS->CloseProtocol (
           Handle,
           &gEfiLoadedImageProtocolGuid,
           gImageHandle,
           NULL
           );
    *Name = ConvertDevicePathToText (FinalPath, TRUE, TRUE);
    if (FinalPath != NULL) {
      FreePool (FinalPath);
    }
    return EFI_SUCCESS;
  } else {
    gBS->CloseProtocol (
           Handle,
           &gEfiLoadedImageProtocolGuid,
           gImageHandle,
           NULL
           );
    return Status;
  }
}

/**
  Gets Namespace ID Type
  @param[in] const struct spdk_nvme_ns   *NameSpace,
  @param[in] const struct spdk_uuid      *NamespaceUuid

  @retval EFI_SUCCESS                    Return NID type.
**/

UINT8
NvmeOfFindNidType (
  const struct spdk_nvme_ns  *NameSpace,
  const struct spdk_uuid     *NamespaceUuid
  )
{
  const struct spdk_nvme_ns_id_desc  *Desc;
  UINT32                             Offset = 0;
  BOOLEAN                            NidMatched = FALSE;
  UINT8                              Index;

  while (Offset + NID_OFFSET_IN_STRUCT < sizeof (NameSpace->id_desc_list)) {
    Desc = (const struct spdk_nvme_ns_id_desc *)&NameSpace->id_desc_list[Offset];
    if (Desc->nidl == 0) {
      return 0;
    }
    /* Length of uuid/nguid : 16 bytes */
    for (Index = 0; Index < 16; Index++) {
      if ((Desc->nid[Index]) != (NamespaceUuid->u.raw[Index])) {
        NidMatched = FALSE;
        break;
      } else {
        NidMatched = TRUE;
      }
    }

    if (Offset + Desc->nidl + NID_OFFSET_IN_STRUCT > sizeof (NameSpace->id_desc_list)) {
      /* Descriptor longer than remaining space in list (invalid) */
      return 0;
    }
    Offset += NID_OFFSET_IN_STRUCT + Desc->nidl;

    if (NidMatched == TRUE) {
      return Desc->nidt;
    }

  }
  return 0;
}

/**
  Abort the session when the transition from BS to RT is initiated.

  @param[in]  Event  The event signaled.
  @param[in]  Context The NvmeOf driver data.

**/
VOID
EFIAPI
NvmeOfOnExitBootService (
  IN EFI_EVENT  Event,
  IN VOID       *Context
  )
{
  NVMEOF_DRIVER_DATA      *Private = NULL;
  NVMEOF_CONTROLLER_DATA  *CtrlrData = NULL;
  LIST_ENTRY              *Entry = NULL;
  LIST_ENTRY              *NextEntryProcessed = NULL;
  struct spdk_nvme_qpair  *qpair;
  struct spdk_nvme_qpair  *tmp;

  Private = (NVMEOF_DRIVER_DATA *)Context;

  if (Private->ExitBootServiceEvent != NULL) {
    gBS->CloseEvent (Private->ExitBootServiceEvent);
    Private->ExitBootServiceEvent = (EFI_EVENT)NULL;
  }

  if (KatoEvent != NULL) {
    gBS->CloseEvent (KatoEvent);
    KatoEvent = (EFI_EVENT)NULL;
  }

  NET_LIST_FOR_EACH_SAFE (Entry, NextEntryProcessed, &gNvmeOfControllerList) {
    CtrlrData = NET_LIST_USER_STRUCT (Entry, NVMEOF_CONTROLLER_DATA, Link);
    if (CtrlrData != NULL) {
      if (Private->Controller == CtrlrData->NicHandle) {
        TAILQ_FOREACH_SAFE (qpair, &CtrlrData->ctrlr->active_io_qpairs, tailq, tmp) {
          spdk_nvme_ctrlr_free_io_qpair (qpair);
        }
        nvme_transport_ctrlr_destruct (CtrlrData->ctrlr);
        if (CtrlrData->Link.ForwardLink != NULL
        && CtrlrData->Link.BackLink != NULL) {
          RemoveEntryList(&CtrlrData->Link);
          FreePool (CtrlrData);
        }
      }
    }
  }
}

/**
  Flip global flag to prevent NBFT changes in DB->Stop()
  during ExitBootServices().

  @param[in]  Event   The event signaled.
  @param[in]  Context NULL.

**/
VOID
EFIAPI
NvmeOfBeforeEBS (
  IN EFI_EVENT  Event,
  IN VOID       *Context
  )
{
  //
  // Flip the trigger that driver is currently in runtime stage.
  // This prevents NBFT from being modified during transition from
  // DXE stage to RT stage.
  //
  gDriverInRuntime = TRUE;
}

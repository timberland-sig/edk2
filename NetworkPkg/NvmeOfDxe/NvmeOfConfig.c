/** @file
  Helper functions for configuring or getting the parameters relating to NVMe-oF.

Copyright (c) 2023, Dell Technologies All rights reserved
SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "NvmeOfImpl.h"

CHAR16                      mVendorStorageName[] = L"NVMEOF_CONFIG_IFR_NVDATA";
NVMEOF_FORM_CALLBACK_INFO   *mCallbackInfo       = NULL;
NVMEOF_CONFIG_PRIVATE_DATA  *mConfigPrivate      = NULL;

HII_VENDOR_DEVICE_PATH  mNvmeOfHiiVendorDevicePath = {
  {
    {
      HARDWARE_DEVICE_PATH,
      HW_VENDOR_DP,
      {
        (UINT8)(sizeof (VENDOR_DEVICE_PATH)),
        (UINT8)((sizeof (VENDOR_DEVICE_PATH)) >> 8)
      }
    },
    NVMEOF_CONFIG_GUID
  },
  {
    END_DEVICE_PATH_TYPE,
    END_ENTIRE_DEVICE_PATH_SUBTYPE,
    {
      (UINT8)(END_DEVICE_PATH_LENGTH),
      (UINT8)((END_DEVICE_PATH_LENGTH) >> 8)
    }
  }
};

/**
  Convert EFI_IP_ADDRESS into a unicode string.

  @param[in]  Ip        The IP address.
  @param[in]  Ipv6Flag  Indicates whether the IP address is version 4 or version 6.
  @param[out] Str       The formatted IP string.

**/
VOID
NvmeOfIpToUnicodeStr (
  IN  EFI_IP_ADDRESS  *Ip,
  IN  BOOLEAN         Ipv6Flag,
  OUT CHAR16          *Str
  )
{
  CHAR8  IpAddr[IP_STR_MAX_SIZE];

  NvmeOfIpToStr (*Ip, IpAddr, Ipv6Flag);
  if (!Ipv6Flag) {
    AsciiStrToUnicodeStrS (
      IpAddr,
      Str,
      IP4_STR_MAX_SIZE
      );
  } else {
    AsciiStrToUnicodeStrS (
      IpAddr,
      Str,
      IP_STR_MAX_SIZE
      );
  }
}

/**
  Check whether the input IP address is valid.

  This function was duplicated from IScsiDxe and might benefit from being moved
  outside of both for use as a shared helper function.

  @param[in]  Ip        The IP address.
  @param[in]  IpMode    Indicates NVMe-oF running on IP4 or IP6 stack.

  @retval     TRUE      The input IP address is valid.
  @retval     FALSE     Otherwise

**/
BOOLEAN
IpIsUnicast (
  IN EFI_IP_ADDRESS  *Ip,
  IN  UINT8          IpMode
  )
{
  if (IpMode == IP_MODE_IP4) {
    if (IP4_IS_UNSPECIFIED (NTOHL (Ip->Addr[0])) || IP4_IS_LOCAL_BROADCAST (NTOHL (Ip->Addr[0]))) {
      return FALSE;
    }

    return TRUE;
  } else if (IpMode == IP_MODE_IP6) {
    return NetIp6IsValidUnicast (&Ip->v6);
  } else {
    DEBUG ((DEBUG_ERROR, "IpMode %d is invalid when configuring the NVMe-oF target IP!\n", IpMode));
    return FALSE;
  }
}

/**
  Check whether UNDI protocol supports IPv6.

  This function was duplicated from IScsiDxe and might benefit from being moved
  outside of both for use as a shared helper function.

  @param[in]   ControllerHandle    Controller handle.
  @param[out]  Ipv6Support         TRUE if UNDI supports IPv6.

  @retval EFI_SUCCESS   Get the result whether UNDI supports IPv6 by NII
                        or AIP protocol successfully.
  @retval EFI_NOT_FOUND Don't know whether UNDI supports IPv6 since NII
                        or AIP is not available.

**/
EFI_STATUS
NvmeOfConfigCheckIpv6Support (
  IN  EFI_HANDLE  ControllerHandle,
  OUT BOOLEAN     *Ipv6Support
  )
{
  EFI_HANDLE                                 Handle;
  EFI_ADAPTER_INFORMATION_PROTOCOL           *Aip;
  EFI_STATUS                                 Status;
  EFI_GUID                                   *InfoTypesBuffer;
  UINTN                                      InfoTypeBufferCount;
  UINTN                                      TypeIndex;
  BOOLEAN                                    Supported;
  VOID                                       *InfoBlock;
  UINTN                                      InfoBlockSize;
  EFI_NETWORK_INTERFACE_IDENTIFIER_PROTOCOL  *Nii;

  ASSERT (Ipv6Support != NULL);

  //
  // Check whether the UNDI supports IPv6 by NII protocol.
  //
  Status = gBS->OpenProtocol (
                  ControllerHandle,
                  &gEfiNetworkInterfaceIdentifierProtocolGuid_31,
                  (VOID **)&Nii,
                  gImageHandle,
                  ControllerHandle,
                  EFI_OPEN_PROTOCOL_GET_PROTOCOL
                  );
  if (Status == EFI_SUCCESS) {
    *Ipv6Support = Nii->Ipv6Supported;
    return EFI_SUCCESS;
  }

  //
  // Get the NIC handle by SNP protocol.
  //
  Handle = NetLibGetSnpHandle (ControllerHandle, NULL);
  if (Handle == NULL) {
    return EFI_NOT_FOUND;
  }

  Aip    = NULL;
  Status = gBS->HandleProtocol (
                  Handle,
                  &gEfiAdapterInformationProtocolGuid,
                  (VOID *)&Aip
                  );
  if (EFI_ERROR (Status) || (Aip == NULL)) {
    return EFI_NOT_FOUND;
  }

  InfoTypesBuffer     = NULL;
  InfoTypeBufferCount = 0;
  Status              = Aip->GetSupportedTypes (Aip, &InfoTypesBuffer, &InfoTypeBufferCount);
  if (EFI_ERROR (Status) || (InfoTypesBuffer == NULL)) {
    FreePool (InfoTypesBuffer);
    return EFI_NOT_FOUND;
  }

  Supported = FALSE;
  for (TypeIndex = 0; TypeIndex < InfoTypeBufferCount; TypeIndex++) {
    if (CompareGuid (&InfoTypesBuffer[TypeIndex], &gEfiAdapterInfoUndiIpv6SupportGuid)) {
      Supported = TRUE;
      break;
    }
  }

  FreePool (InfoTypesBuffer);
  if (!Supported) {
    return EFI_NOT_FOUND;
  }

  //
  // We now have adapter information block.
  //
  InfoBlock     = NULL;
  InfoBlockSize = 0;
  Status        = Aip->GetInformation (Aip, &gEfiAdapterInfoUndiIpv6SupportGuid, &InfoBlock, &InfoBlockSize);
  if (EFI_ERROR (Status) || (InfoBlock == NULL)) {
    FreePool (InfoBlock);
    return EFI_NOT_FOUND;
  }

  *Ipv6Support = ((EFI_ADAPTER_INFO_UNDI_IPV6_SUPPORT *)InfoBlock)->Ipv6Support;
  FreePool (InfoBlock);

  return EFI_SUCCESS;
}

/**
  Check an input NQN for validity based on the following:
    - begins with "nqn."
    - payload following "nqn." is non-zero
    - terminates with a null-character

  @param[in]    Nqn  The string to check for NQN-validity.

  @retval TRUE             NQN string is valid.
  @retval FALSE            NQN string is invalid.

**/
BOOLEAN
NvmeOfConfigIsNqnValid (
  IN CHAR16  *Nqn
  )
{
  UINT8  NqnLen;

  if (StrnCmp (Nqn, L"nqn.", 4)) {
    return FALSE;
  }

  //
  // Check that NQN payload is non-zero and ends with a null-character
  //
  NqnLen = StrnLenS (Nqn, NVMEOF_NQN_MAX_LEN);
  if ((NqnLen < 5) ||
      (Nqn[NqnLen] != '\0'))
  {
    return FALSE;
  }

  return TRUE;
}

/**
  Get attempt config data from HII private data structure.

  Searches for a matching index in AttemptInfoList and returns a pointer to the
  corresponding attempt data if found. Note that this pointer is to the data as
  stored in the list itself - it is not a newly-allocated buffer and should not
  be freed by the caller.

  @param[in]    AttemptConfigIndex   The zero-based index indicating which Attempt
                                     is to be located.

  @return       Pointer to an Attempt's configuration data stored in the configuration
                form's AttemptInfoList.
  @retval NULL  The attempt configuration data cannot be found.

**/
NVMEOF_ATTEMPT_CONFIG_NVDATA *
NvmeOfConfigGetAttemptByConfigIndex (
  IN UINT8  AttemptConfigIndex
  )
{
  NVMEOF_CONFIG_ATTEMPT_INFO  *AttemptInfo;
  LIST_ENTRY                  *Entry;

  NET_LIST_FOR_EACH (Entry, &mConfigPrivate->AttemptInfoList) {
    AttemptInfo = NET_LIST_USER_STRUCT (Entry, NVMEOF_CONFIG_ATTEMPT_INFO, Link);

    if (AttemptInfo->AttemptIndex == AttemptConfigIndex) {
      return &AttemptInfo->Data;
    }
  }
  return NULL;
}

/**
  Update the Configuration form's NicInfoList with information from available NIC device handles.

  @retval EFI_SUCCESS           NicInfoList updated with system NIC information.
  @retval EFI_NOT_FOUND         Failed to locate any supported NIC device handles.
  @retval EFI_OUT_OF_RESOURCES  Do not have sufficient resources to finish this
                                operation.

**/
EFI_STATUS
NvmeOfConfigUpdateNicInfoList (
  )
{
  LIST_ENTRY              *Entry;
  LIST_ENTRY              *NextEntry;
  NVMEOF_CONFIG_NIC_INFO  *NicInfo;
  EFI_HANDLE              *DeviceHandleBuffer;
  UINTN                   DeviceHandleCount;
  UINT8                   Index;
  EFI_STATUS              Status;
  EFI_MAC_ADDRESS         MacAddr;
  UINTN                   HwAddressSize;
  UINT16                  VlanId;
  BOOLEAN                 NewDevice;

  Status = gBS->LocateHandleBuffer (
                  ByProtocol,
                  &gEfiDevicePathProtocolGuid,
                  NULL,
                  &DeviceHandleCount,
                  &DeviceHandleBuffer
                  );
  if (EFI_ERROR (Status) || (DeviceHandleCount == 0)) {
    return EFI_NOT_FOUND;
  }

  for (Index = 0; Index < DeviceHandleCount; Index++) {
    NewDevice = TRUE;
    NicInfo   = NULL;

    Status = NetLibGetMacAddress (
               DeviceHandleBuffer[Index],
               &MacAddr,
               &HwAddressSize
               );

    if (EFI_ERROR (Status)) {
      continue;
    }

    VlanId = NetLibGetVlanId (DeviceHandleBuffer[Index]);

    //
    // Check whether the NIC info already exists.
    //
    NET_LIST_FOR_EACH (Entry, &mConfigPrivate->NicInfoList) {
      NicInfo = NET_LIST_USER_STRUCT (Entry, NVMEOF_CONFIG_NIC_INFO, Link);
      if ((NicInfo->HwAddressSize == HwAddressSize) &&
          (CompareMem (&NicInfo->PermanentAddress, MacAddr.Addr, HwAddressSize) == 0) &&
          (NicInfo->VlanId == VlanId))
      {
        NewDevice = FALSE;
      }
    }

    if (!NewDevice) {
      continue;
    }

    //
    // Add new NIC device info to the NicInfoList.
    //
    NicInfo = AllocateZeroPool (sizeof (NVMEOF_CONFIG_NIC_INFO));
    if (NicInfo == NULL) {
      FreePool (DeviceHandleBuffer);
      return EFI_OUT_OF_RESOURCES;
    }

    CopyMem (&NicInfo->PermanentAddress, MacAddr.Addr, HwAddressSize);
    CopyMem (&NicInfo->DeviceHandle, &DeviceHandleBuffer[Index], sizeof (EFI_HANDLE));
    NicInfo->HwAddressSize = (UINT32)HwAddressSize;
    NicInfo->VlanId        = VlanId;
    Status                 = NvmeOfConfigCheckIpv6Support (
                               NicInfo->DeviceHandle,
                               &NicInfo->Ipv6Support
                               );
    if (EFI_ERROR (Status)) {
      NicInfo->Ipv6Support = FALSE;
    }

    InsertTailList (&mConfigPrivate->NicInfoList, &NicInfo->Link);
  }

  FreePool (DeviceHandleBuffer);

  //
  // Update the NIC indexes and remove any invalid devices from the list.
  //
  mConfigPrivate->NicCount = 0;
  NET_LIST_FOR_EACH_SAFE (Entry, NextEntry, &mConfigPrivate->NicInfoList) {
    NicInfo = NET_LIST_USER_STRUCT (Entry, NVMEOF_CONFIG_NIC_INFO, Link);
    Status  = NetLibGetMacAddress (
                NicInfo->DeviceHandle,
                &MacAddr,
                &HwAddressSize
                );

    if (EFI_ERROR (Status)) {
      RemoveEntryList (&NicInfo->Link);
      FreePool (NicInfo);
      continue;
    }

    NicInfo->NicIndex = (UINT8)(mConfigPrivate->NicCount);
    mConfigPrivate->NicCount++;
  }

  return EFI_SUCCESS;
}

/**
  Update the Configuration form's AttemptInfoList with data from the
  Nvmeof_Attemptconfig NV variable.

  Function adds data from each enabled attempt in NV data to an internal list.
  Each node of the list also contains a corresponding zero-based index indicating
  where the attempt is stored in the NV variable.

  If an attempt already exists in the list (matched via index), it is updated
  in-place with the NV data.

  @retval EFI_SUCCESS           AttemptInfoList successfully updated with NV data
  @retval EFI_UNSUPPORTED       Located existing data with unexpected size.
  @retval EFI_NOT_FOUND         Failed to locate AttemptConfig NV data.
  @retval EFI_OUT_OF_RESOURCES  Do not have sufficient resources to finish this
                                operation.

**/
EFI_STATUS
NvmeOfConfigUpdateAttemptInfoList (
  )
{
  NVMEOF_CONFIG_ATTEMPT_INFO      *AttemptInfo;
  NVMEOF_ATTEMPT_CONFIG_NVDATA    *AttemptConfigData;
  NVMEOF_ATTEMPT_CONFIG_NVDATA    *NewAttempt;
  NVMEOF_SUBSYSTEM_CONFIG_NVDATA  *SubsysConfigData;
  UINTN                           Size;
  UINT8                           AttemptCount;
  UINT8                           Index;
  EFI_STATUS                      Status;
  LIST_ENTRY                      *Entry;

  Status      = EFI_SUCCESS;
  AttemptInfo = NULL;

  AttemptConfigData = NvmeOfGetVariableAndSize (
                        L"Nvmeof_Attemptconfig",
                        &gNvmeOfConfigGuid,
                        &Size
                        );
  if (Size % sizeof (NVMEOF_ATTEMPT_CONFIG_NVDATA) != 0) {
    Status = EFI_UNSUPPORTED;
    DEBUG ((
      DEBUG_ERROR,
      "%a: Located variable (Nvmeof_Attemptconfig) with Guid (%d) "
      "has unexpected data size.\n",
      __FUNCTION__,
      &gNvmeOfConfigGuid
      ));
    goto Exit;
  }

  AttemptCount = Size / sizeof (NVMEOF_ATTEMPT_CONFIG_NVDATA);
  if (AttemptCount > NVMEOF_MAX_ATTEMPTS_NUM) {
    DEBUG ((
      DEBUG_WARN,
      "%a: NV variable (Nvmeof_Attemptconfig) exceeds supported attempt count.\n",
      __FUNCTION__
      ));
  }

  for (Index = 0; Index < NVMEOF_MAX_ATTEMPTS_NUM; Index++) {
    //
    // Update existing node if list already contains data for the current index.
    //
    NET_LIST_FOR_EACH (Entry, &mConfigPrivate->AttemptInfoList) {
      AttemptInfo = NET_LIST_USER_STRUCT (Entry, NVMEOF_CONFIG_ATTEMPT_INFO, Link);
      if (AttemptInfo->AttemptIndex == Index) {
        break;
      }

      AttemptInfo = NULL;
    }

    //
    // Create new list node and allocate memory for attempt data.
    //
    if (AttemptInfo == NULL) {
      AttemptInfo = AllocateZeroPool (sizeof (NVMEOF_CONFIG_ATTEMPT_INFO));
      if (AttemptInfo == NULL) {
        Status = EFI_OUT_OF_RESOURCES;
        goto Exit;
      }

      InsertTailList (&mConfigPrivate->AttemptInfoList, &AttemptInfo->Link);
      mConfigPrivate->AttemptCount++;
    }

    //
    // Copy existing NV data to current list node or set attempt defaults.
    //
    if ((Index < AttemptCount) &&
        !IsZeroBuffer (&AttemptConfigData[Index], sizeof (NVMEOF_ATTEMPT_CONFIG_NVDATA)))
    {
      CopyMem (
        &AttemptInfo->Data,
        &AttemptConfigData[Index],
        sizeof (NVMEOF_ATTEMPT_CONFIG_NVDATA)
        );
    } else {
      NewAttempt                 = &AttemptInfo->Data;
      NewAttempt->NvmeofAuthType = NVMEOF_AUTH_TYPE_NONE;

      SubsysConfigData                     = &NewAttempt->SubsysConfigData;
      SubsysConfigData->NvmeofSubsysPortId = SUBSYS_PORT_DEFAULT;
      SubsysConfigData->NvmeofTimeout      = CONNECT_DEFAULT_TIMEOUT;
      SubsysConfigData->NvmeofRetryCount   = CONNECT_DEFAULT_RETRY;
    }

    //
    // Set the attempt name according to the order.
    //
    AttemptInfo->AttemptIndex = Index;
    UnicodeSPrint (
      mNicPrivate->PortString,
      (UINTN)NVMEOF_NAME_IFR_MAX_SIZE,
      L"Attempt %d",
      (UINTN)AttemptInfo->AttemptIndex + 1
      );

    UnicodeStrToAsciiStrS (
      mNicPrivate->PortString,
      AttemptInfo->Data.AttemptName,
      ATTEMPT_NAME_SIZE
      );
  }

Exit:
  if (AttemptConfigData != NULL) {
    FreePool (AttemptConfigData);
  }

  return Status;
}

/**
  Create Hii Extend Label OpCode as the start opcode and end opcode. It is
  a help function.

  This function was duplicated from IScsiDxe and might benefit from being moved
  outside of both for use as a shared helper function.

  @param[in]  StartLabelNumber   The number of start label.
  @param[out] StartOpCodeHandle  Points to the start opcode handle.
  @param[out] StartLabel         Points to the created start opcode.
  @param[out] EndOpCodeHandle    Points to the end opcode handle.
  @param[out] EndLabel           Points to the created end opcode.

  @retval EFI_OUT_OF_RESOURCES   Do not have sufficient resource to finish this
                                 operation.
  @retval EFI_INVALID_PARAMETER  Any input parameter is invalid.
  @retval EFI_SUCCESS            The operation is completed successfully.

**/
EFI_STATUS
NvmeOfCreateOpCode (
  IN  UINT16              StartLabelNumber,
  OUT VOID                **StartOpCodeHandle,
  OUT EFI_IFR_GUID_LABEL  **StartLabel,
  OUT VOID                **EndOpCodeHandle,
  OUT EFI_IFR_GUID_LABEL  **EndLabel
  )
{
  EFI_STATUS          Status;
  EFI_IFR_GUID_LABEL  *InternalStartLabel;
  EFI_IFR_GUID_LABEL  *InternalEndLabel;

  if ((StartOpCodeHandle == NULL) || (StartLabel == NULL) || (EndOpCodeHandle == NULL) || (EndLabel == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  *StartOpCodeHandle = NULL;
  *EndOpCodeHandle   = NULL;
  Status             = EFI_OUT_OF_RESOURCES;

  //
  // Initialize the container for dynamic opcodes.
  //
  *StartOpCodeHandle = HiiAllocateOpCodeHandle ();
  if (*StartOpCodeHandle == NULL) {
    return Status;
  }

  *EndOpCodeHandle = HiiAllocateOpCodeHandle ();
  if (*EndOpCodeHandle == NULL) {
    goto Exit;
  }

  //
  // Create Hii Extend Label OpCode as the start opcode.
  //
  InternalStartLabel = (EFI_IFR_GUID_LABEL *)HiiCreateGuidOpCode (
                                               *StartOpCodeHandle,
                                               &gEfiIfrTianoGuid,
                                               NULL,
                                               sizeof (EFI_IFR_GUID_LABEL)
                                               );
  if (InternalStartLabel == NULL) {
    goto Exit;
  }

  InternalStartLabel->ExtendOpCode = EFI_IFR_EXTEND_OP_LABEL;
  InternalStartLabel->Number       = StartLabelNumber;

  //
  // Create Hii Extend Label OpCode as the end opcode.
  //
  InternalEndLabel = (EFI_IFR_GUID_LABEL *)HiiCreateGuidOpCode (
                                             *EndOpCodeHandle,
                                             &gEfiIfrTianoGuid,
                                             NULL,
                                             sizeof (EFI_IFR_GUID_LABEL)
                                             );
  if (InternalEndLabel == NULL) {
    goto Exit;
  }

  InternalEndLabel->ExtendOpCode = EFI_IFR_EXTEND_OP_LABEL;
  InternalEndLabel->Number       = LABEL_END;

  *StartLabel = InternalStartLabel;
  *EndLabel   = InternalEndLabel;

  return EFI_SUCCESS;

Exit:

  if (*StartOpCodeHandle != NULL) {
    HiiFreeOpCodeHandle (*StartOpCodeHandle);
  }

  if (*EndOpCodeHandle != NULL) {
    HiiFreeOpCodeHandle (*EndOpCodeHandle);
  }

  return Status;
}

/**
  Update the MAIN form to display the configured attempts.

**/
VOID
NvmeOfConfigUpdateDisplayedAttempts (
  VOID
  )
{
  NVMEOF_ATTEMPT_CONFIG_NVDATA  *AttemptConfigData;
  NVMEOF_ATTEMPT_CONFIG_NVDATA  *Attempt;
  VOID                          *StartOpCodeHandle;
  EFI_IFR_GUID_LABEL            *StartLabel;
  VOID                          *EndOpCodeHandle;
  EFI_IFR_GUID_LABEL            *EndLabel;
  EFI_STATUS                    Status;
  EFI_STRING_ID                 AttemptTitleToken;
  EFI_STRING_ID                 AttemptTitleHelpToken;
  CHAR16                        *DevicePath;
  NVMEOF_CONFIG_NIC_INFO        *NicInfo;
  NVMEOF_CONFIG_ATTEMPT_INFO    *AttemptInfo;
  LIST_ENTRY                    *AttemptEntry;
  LIST_ENTRY                    *NicEntry;
  CHAR16                        MacString[NVMEOF_MAX_MAC_STRING_LEN];
  CHAR16                        MacString2[NVMEOF_MAX_MAC_STRING_LEN];

  Status = NvmeOfCreateOpCode (
             ATTEMPT_ENTRY_LABEL,
             &StartOpCodeHandle,
             &StartLabel,
             &EndOpCodeHandle,
             &EndLabel
             );
  if (EFI_ERROR (Status)) {
    return;
  }

  NET_LIST_FOR_EACH (AttemptEntry, &mConfigPrivate->AttemptInfoList) {
    AttemptInfo = NET_LIST_USER_STRUCT (AttemptEntry, NVMEOF_CONFIG_ATTEMPT_INFO, Link);
    Attempt     = &AttemptInfo->Data;

    UnicodeSPrint (
      mNicPrivate->PortString,
      (UINTN)NVMEOF_NAME_IFR_MAX_SIZE,
      L"%a",
      Attempt->AttemptName
      );
    AttemptTitleToken = HiiSetString (
                          mCallbackInfo->RegisteredHandle,
                          0,
                          mNicPrivate->PortString,
                          NULL
                          );
    if (AttemptTitleToken == 0) {
      return;
    }

    if (Attempt->SubsysConfigData.Enabled == NVMEOF_ATTEMPT_ENABLED) {
      //
      // Add NIC device path as Attempt help. Indicate if device is not found.
      //
      DevicePath = NULL;
      NET_LIST_FOR_EACH (NicEntry, &mConfigPrivate->NicInfoList) {
        NicInfo = NET_LIST_USER_STRUCT (NicEntry, NVMEOF_CONFIG_NIC_INFO, Link);

        NvmeOfMacAddrToStr (
          &NicInfo->PermanentAddress,
          NicInfo->HwAddressSize,
          NicInfo->VlanId,
          MacString
          );
        AsciiStrToUnicodeStrS (
          Attempt->MacString,
          MacString2,
          NVMEOF_MAX_MAC_STRING_LEN
          );

        if (!StrCmp (MacString, MacString2)) {
          DevicePath = ConvertDevicePathToText (
                         DevicePathFromHandle (NicInfo->DeviceHandle),
                         FALSE,                        // DisplayOnly
                         FALSE                         // AllowShortcuts
                         );
          break;
        }
      }
      if (DevicePath == NULL) {
        UnicodeSPrint (
          mNicPrivate->PortString,
          (UINTN)NVMEOF_NAME_IFR_MAX_SIZE,
          L"Device not found or configuration pending for MAC : %a",
          Attempt->MacString
          );
      } else {
        UnicodeSPrint (
          mNicPrivate->PortString,
          (UINTN)NVMEOF_NAME_IFR_MAX_SIZE,
          L"Device Path : %s",
          DevicePath
          );
        FreePool (DevicePath);
      }
    } else {
      UnicodeSPrint (
        mNicPrivate->PortString,
        (UINTN)NVMEOF_NAME_IFR_MAX_SIZE,
        L"Attempt is not enabled."
        );
    }

    AttemptTitleHelpToken = HiiSetString (
                              mCallbackInfo->RegisteredHandle,
                              0,
                              mNicPrivate->PortString,
                              NULL
                              );

    if (AttemptTitleHelpToken == 0) {
      FreePool (AttemptConfigData);
      return;
    }

    HiiCreateGotoOpCode (
      StartOpCodeHandle,                                             // Container for dynamic created opcodes
      FORMID_ATTEMPT_FORM,                                           // Form ID
      AttemptTitleToken,                                             // Prompt text
      AttemptTitleHelpToken,                                         // Help text
      EFI_IFR_FLAG_CALLBACK,                                         // Question flag
      (UINT16)(KEY_ATTEMPT_ENTRY_BASE + AttemptInfo->AttemptIndex)   // Question ID
      );
  }

  HiiUpdateForm (
    mCallbackInfo->RegisteredHandle,                // HII handle
    &gNvmeOfConfigGuid,                             // Formset GUID
    FORMID_MAIN_FORM,                               // Form ID
    StartOpCodeHandle,                              // Label for where to insert opcodes
    EndOpCodeHandle                                 // Replace data
    );

  HiiFreeOpCodeHandle (StartOpCodeHandle);
  HiiFreeOpCodeHandle (EndOpCodeHandle);
}

/**
  Convert the input attempt data to NVMe-oF configuration data.

  Input attempt is a pointer to NVMEOF_ATTEMPT_CONFIG_DATA stored in the
  HII's private AttemptInfoList. The index of this attempt data is located from
  the attempt list and used to update the corresponding index in the NV variable
  "Nvmeof_Attemptconfig" with input attempt data.

  Function will also set the Global NvmeOfTargetCount with the updated attempt
  count.

  @param[in]  Attempt            Pointer to Attempt data stored in the HII's
                                 AttemptInfoList.

  @retval EFI_OUT_OF_RESOURCES   Do not have sufficient resources to finish this
                                 operation.
  @retval EFI_NOT_FOUND          Input attempt data not found in configuration list.
  @retval EFI_SUCCESS            The operation completed successfully.

**/
EFI_STATUS
NvmeOfConfigSaveAttemptToNvData (
  NVMEOF_ATTEMPT_CONFIG_NVDATA  *Attempt
  )
{
  NVMEOF_ATTEMPT_CONFIG_NVDATA  *AttemptConfigData;
  NVMEOF_ATTEMPT_CONFIG_NVDATA  *AttemptConfigDataTmp;
  NVMEOF_CONFIG_ATTEMPT_INFO    *AttemptInfo;
  UINT8                         Index;
  LIST_ENTRY                    *Entry;
  UINTN                         Size;
  NVMEOF_GLOBAL_DATA            *NvmeOfGlobalData;
  EFI_STATUS                    Status;

  Status            = EFI_SUCCESS;
  AttemptConfigData = NULL;
  NvmeOfGlobalData  = NULL;

  //
  // Use AttemptInfoList to locate input attempt's corresponding index.
  //
  NET_LIST_FOR_EACH (Entry, &mConfigPrivate->AttemptInfoList) {
    AttemptInfo = NET_LIST_USER_STRUCT (Entry, NVMEOF_CONFIG_ATTEMPT_INFO, Link);
    if (&AttemptInfo->Data == Attempt) {
      Index = AttemptInfo->AttemptIndex;
      break;
    }

    AttemptInfo = NULL;
  }

  if (AttemptInfo == NULL) {
    Status = EFI_NOT_FOUND;
    goto Exit;
  }

  AttemptConfigData = NvmeOfGetVariableAndSize (
                        L"Nvmeof_Attemptconfig",
                        &gNvmeOfConfigGuid,
                        &Size
                        );

  //
  // Increase size of NV data if necessary.
  //
  if (Size < sizeof (NVMEOF_ATTEMPT_CONFIG_NVDATA) * mConfigPrivate->AttemptCount) {
    Size = sizeof (NVMEOF_ATTEMPT_CONFIG_NVDATA) * mConfigPrivate->AttemptCount;

    AttemptConfigDataTmp = AllocateZeroPool (Size);
    if (AttemptConfigDataTmp == NULL) {
      Status = EFI_OUT_OF_RESOURCES;
      goto Exit;
    }

    if (AttemptConfigData != NULL) {
      CopyMem (
        AttemptConfigDataTmp,
        AttemptConfigData,
        Size
        );

      //
      // Clear previous NV data.
      //
      Status = gRT->SetVariable (
                      L"Nvmeof_Attemptconfig",
                      &gNvmeOfConfigGuid,
                      NVMEOF_CONFIG_VAR_ATTR,
                      0,
                      NULL
                      );
      if (EFI_ERROR (Status)) {
        DEBUG ((
          DEBUG_ERROR,
          "%a: Failed to set variable (Nvmeof_Attemptconfig) with Guid (%g): "
          "%r\n",
          __FUNCTION__,
          &gNvmeOfConfigGuid,
          Status
          ));
        goto Exit;
      }

      FreePool (AttemptConfigData);
    }

    AttemptConfigData = AttemptConfigDataTmp;
  }

  //
  // Copy input attempt data to corresponding index in NV array.
  //
  CopyMem (
    &AttemptConfigData[Index],
    Attempt,
    sizeof (NVMEOF_ATTEMPT_CONFIG_NVDATA)
    );

  Status = gRT->SetVariable (
                  L"Nvmeof_Attemptconfig",
                  &gNvmeOfConfigGuid,
                  NVMEOF_CONFIG_VAR_ATTR,
                  Size,
                  AttemptConfigData
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Failed to set variable (Nvmeof_Attemptconfig) with Guid (%g): "
      "%r\n",
      __FUNCTION__,
      &gNvmeOfConfigGuid,
      Status
      ));
    goto Exit;
  }

  //
  // Update the Global TargetCount value.
  //
  NvmeOfGlobalData = NvmeOfGetVariableAndSize (
                       L"NvmeofGlobalData",
                       &gNvmeOfConfigGuid,
                       &Size
                       );
  if ((NvmeOfGlobalData == NULL) || (Size == 0)) {
    Status = EFI_NOT_FOUND;
    goto Exit;
  }

  NvmeOfGlobalData->NvmeOfTargetCount = mConfigPrivate->AttemptCount;

  Status = gRT->SetVariable (
                  L"NvmeofGlobalData",
                  &gNvmeOfConfigGuid,
                  NVMEOF_CONFIG_VAR_ATTR,
                  sizeof (*NvmeOfGlobalData),
                  NvmeOfGlobalData
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Failed to set variable (NvmeofGlobalData) with Guid (%g): "
      "%r\n",
      __FUNCTION__,
      &gNvmeOfConfigGuid,
      Status
      ));
  }

Exit:
  if (NvmeOfGlobalData != NULL) {
    FreePool (NvmeOfGlobalData);
  }

  if (AttemptConfigData != NULL) {
    FreePool (AttemptConfigData);
  }

  return Status;
}

/**
  Convert the NVMe-oF configuration data into the IFR data.

  @param[in]       Attempt                The NVMe-oF attempt config data.
  @param[in, out]  IfrNvData              The IFR nv data.

**/
VOID
NvmeOfConvertAttemptConfigDataToIfrNvData (
  IN NVMEOF_ATTEMPT_CONFIG_NVDATA  *Attempt,
  IN OUT NVMEOF_CONFIG_IFR_NVDATA  *IfrNvData
  )
{
  NVMEOF_SUBSYSTEM_CONFIG_NVDATA  *SubsysConfigData;
  EFI_IP_ADDRESS                  Ip;

  //
  // Normal session configuration parameters.
  //
  SubsysConfigData  = &Attempt->SubsysConfigData;
  IfrNvData->IpMode = SubsysConfigData->NvmeofIpMode;

  IfrNvData->HostInfoDhcp         = SubsysConfigData->HostInfoDhcp;
  IfrNvData->NvmeofSubsysInfoDhcp = SubsysConfigData->NvmeofSubsysInfoDhcp;
  IfrNvData->NvmeofTargetPort     = SubsysConfigData->NvmeofSubsysPortId;
  IfrNvData->ConnectRetryCount    = SubsysConfigData->NvmeofRetryCount;

  AsciiStrToUnicodeStrS (
    Attempt->MacString,
    IfrNvData->NvmeofSubsysMacString,
    NVMEOF_MAX_MAC_STRING_LEN
    );

  switch (IfrNvData->IpMode) {
    case IP_MODE_IP4:
      ZeroMem (IfrNvData->NvmeofSubsysHostIp, sizeof (IfrNvData->NvmeofSubsysHostIp));
      if (SubsysConfigData->NvmeofSubsysHostIP.v4.Addr[0] != '\0') {
        CopyMem (&Ip.v4, &SubsysConfigData->NvmeofSubsysHostIP, sizeof (EFI_IPv4_ADDRESS));
        NvmeOfIpToUnicodeStr (&Ip, FALSE, IfrNvData->NvmeofSubsysHostIp);
      }

      ZeroMem (IfrNvData->NvmeofSubsysHostSubnetMask, sizeof (IfrNvData->NvmeofSubsysHostSubnetMask));
      if (SubsysConfigData->NvmeofSubsysHostSubnetMask.v4.Addr[0] != '\0') {
        CopyMem (&Ip.v4, &SubsysConfigData->NvmeofSubsysHostSubnetMask, sizeof (EFI_IPv4_ADDRESS));
        NvmeOfIpToUnicodeStr (&Ip, FALSE, IfrNvData->NvmeofSubsysHostSubnetMask);
      }

      ZeroMem (IfrNvData->NvmeofSubsysHostGateway, sizeof (IfrNvData->NvmeofSubsysHostGateway));
      if (SubsysConfigData->NvmeofSubsysHostGateway.v4.Addr[0] != '\0') {
        CopyMem (&Ip.v4, &SubsysConfigData->NvmeofSubsysHostGateway, sizeof (EFI_IPv4_ADDRESS));
        NvmeOfIpToUnicodeStr (&Ip, FALSE, IfrNvData->NvmeofSubsysHostGateway);
      }

      ZeroMem (IfrNvData->NvmeofSubsysIp, sizeof (IfrNvData->NvmeofSubsysIp));
      if (SubsysConfigData->NvmeofSubSystemIp.v4.Addr[0] != '\0') {
        CopyMem (&Ip.v4, &SubsysConfigData->NvmeofSubSystemIp, sizeof (EFI_IPv4_ADDRESS));
        NvmeOfIpToUnicodeStr (&Ip, FALSE, IfrNvData->NvmeofSubsysIp);
      }

      break;

    case IP_MODE_IP6:
      ZeroMem (IfrNvData->NvmeofSubsysHostIp, sizeof (IfrNvData->NvmeofSubsysHostIp));
      if (SubsysConfigData->NvmeofSubsysHostIP.v6.Addr[0] != '\0') {
        IP6_COPY_ADDRESS (&Ip.v6, &SubsysConfigData->NvmeofSubsysHostIP);
        NvmeOfIpToUnicodeStr (&Ip, TRUE, IfrNvData->NvmeofSubsysHostIp);
      }

      ZeroMem (IfrNvData->NvmeofSubsysHostGateway, sizeof (IfrNvData->NvmeofSubsysHostGateway));
      if (SubsysConfigData->NvmeofSubsysHostGateway.v6.Addr[0] != '\0') {
        IP6_COPY_ADDRESS (&Ip.v6, &SubsysConfigData->NvmeofSubsysHostGateway);
        NvmeOfIpToUnicodeStr (&Ip, TRUE, IfrNvData->NvmeofSubsysHostGateway);
      }

      ZeroMem (IfrNvData->NvmeofSubsysIp, sizeof (IfrNvData->NvmeofSubsysIp));
      if (SubsysConfigData->NvmeofSubSystemIp.v6.Addr[0] != '\0') {
        IP6_COPY_ADDRESS (&Ip.v6, &SubsysConfigData->NvmeofSubSystemIp);
        NvmeOfIpToUnicodeStr (&Ip, TRUE, IfrNvData->NvmeofSubsysIp);
      }

      break;

    default:
      break;
  }

  AsciiStrToUnicodeStrS (
    SubsysConfigData->NvmeofSubsysNqn,
    IfrNvData->NvmeofSubsysNqn,
    sizeof (IfrNvData->NvmeofSubsysNqn) / sizeof (IfrNvData->NvmeofSubsysNqn[0])
    );

  AsciiStrToUnicodeStrS (
    SubsysConfigData->NvmeofSubsysNid,
    IfrNvData->NvmeofSubsysNid,
    sizeof (IfrNvData->NvmeofSubsysNid) / sizeof (IfrNvData->NvmeofSubsysNid[0])
    );

  if (SubsysConfigData->NvmeofDnsMode) {
    AsciiStrToUnicodeStrS (
      SubsysConfigData->ServerName,
      IfrNvData->NvmeofSubsysIp,
      sizeof (IfrNvData->NvmeofSubsysIp) / sizeof (IfrNvData->NvmeofSubsysIp[0])
      );
  }

  IfrNvData->NvmeofAuthType = Attempt->NvmeofAuthType;
  AsciiStrToUnicodeStrS (
    SubsysConfigData->SecurityKeyPath,
    IfrNvData->NvmeofSubsysSecurityPath,
    sizeof (IfrNvData->NvmeofSubsysSecurityPath) / sizeof (IfrNvData->NvmeofSubsysSecurityPath[0])
    );

  //
  // Other parameters.
  //
  IfrNvData->Enabled = SubsysConfigData->Enabled;

  AsciiStrToUnicodeStrS (
    Attempt->AttemptName,
    IfrNvData->AttemptName,
    sizeof (IfrNvData->AttemptName) / sizeof (IfrNvData->AttemptName[0])
    );
}

/**
  Convert the IFR data to NVMe-oF configuration data.

  @param[in]       IfrNvData              Point to NVMEOF_CONFIG_IFR_NVDATA.
  @param[in, out]  Attempt                The NVMe-oF attempt config data.

  @retval EFI_INVALID_PARAMETER  Any input or configured parameter is invalid.
  @retval EFI_NOT_FOUND          Cannot find the corresponding variable.
  @retval EFI_OUT_OF_RESOURCES   The operation is failed due to lack of resources.
  @retval EFI_ABORTED            The operation is aborted.
  @retval EFI_SUCCESS            The operation is completed successfully.

**/
EFI_STATUS
NvmeOfConvertIfrNvDataToAttemptConfigData (
  IN NVMEOF_CONFIG_IFR_NVDATA          *IfrNvData,
  IN OUT NVMEOF_ATTEMPT_CONFIG_NVDATA  *Attempt
  )
{
  EFI_IP_ADDRESS                  IpAddr;
  EFI_IP_ADDRESS                  SubnetMask;
  EFI_IP_ADDRESS                  Gateway;
  EFI_INPUT_KEY                   Key;
  EFI_STATUS                      Status;
  NVMEOF_SUBSYSTEM_CONFIG_NVDATA  *SubsysConfigData;

  if (IfrNvData->NvmeofSubsysMacString[0] == '\0') {
    CreatePopUp (
      EFI_LIGHTGRAY | EFI_BACKGROUND_BLUE,
      &Key,
      L"No network device selected!",
      NULL
      );
    return EFI_INVALID_PARAMETER;
  }
  UnicodeStrToAsciiStrS (
    IfrNvData->NvmeofSubsysMacString,
    Attempt->MacString,
    NVMEOF_MAX_MAC_STRING_LEN
    );

  SubsysConfigData                   = &Attempt->SubsysConfigData;
  SubsysConfigData->NvmeofRetryCount = IfrNvData->ConnectRetryCount;
  SubsysConfigData->NvmeofIpMode     = IfrNvData->IpMode;

  switch (IfrNvData->IpMode) {
    case IP_MODE_AUTOCONFIG:
      SubsysConfigData->HostInfoDhcp         = NVMEOF_DHCP_ENABLED;
      SubsysConfigData->NvmeofSubsysInfoDhcp = NVMEOF_DHCP_ENABLED;
      SubsysConfigData->NvmeofDnsMode        = NVMEOF_DNS_ENABLED;

      break;

    default:
      SubsysConfigData->HostInfoDhcp         = IfrNvData->HostInfoDhcp;
      SubsysConfigData->NvmeofSubsysInfoDhcp = IfrNvData->NvmeofSubsysInfoDhcp;

      SubsysConfigData->NvmeofSubsysPortId = IfrNvData->NvmeofTargetPort;

      if (SubsysConfigData->NvmeofSubsysPortId == 0) {
        SubsysConfigData->NvmeofSubsysPortId = SUBSYS_PORT_DEFAULT;
      }

      break;
  }

  //
  // Validate the host IPv4 configuration if DHCP is not deployed.
  //
  if (!IfrNvData->HostInfoDhcp && (IfrNvData->IpMode == IP_MODE_IP4)) {
    CopyMem (&IpAddr.v4, &SubsysConfigData->NvmeofSubsysHostIP, sizeof (IpAddr.v4));
    CopyMem (&SubnetMask.v4, &SubsysConfigData->NvmeofSubsysHostSubnetMask, sizeof (SubnetMask.v4));
    CopyMem (&Gateway.v4, &SubsysConfigData->NvmeofSubsysHostGateway, sizeof (Gateway.v4));

    if (  (Gateway.Addr[0] != 0)
       && (SubnetMask.Addr[0] == 0))
    {
      CreatePopUp (
        EFI_LIGHTGRAY | EFI_BACKGROUND_BLUE,
        &Key,
        L"Host gateway address is set but subnet mask is zero.",
        NULL
        );

      return EFI_INVALID_PARAMETER;
    }

    if (!IP4_NET_EQUAL (IpAddr.Addr[0], Gateway.Addr[0], SubnetMask.Addr[0])) {
      CreatePopUp (
        EFI_LIGHTGRAY | EFI_BACKGROUND_BLUE,
        &Key,
        L"Host IP and Gateway are not in the same subnet.",
        NULL
        );

      return EFI_INVALID_PARAMETER;
    }
  }

  //
  // Validate target configuration if DHCP isn't deployed.
  //
  if (!IfrNvData->NvmeofSubsysInfoDhcp && (IfrNvData->IpMode < IP_MODE_AUTOCONFIG)) {
    if (!SubsysConfigData->NvmeofDnsMode) {
      if (!IpIsUnicast (&SubsysConfigData->NvmeofSubSystemIp, IfrNvData->IpMode)) {
        CreatePopUp (
          EFI_LIGHTGRAY | EFI_BACKGROUND_BLUE,
          &Key,
          L"Target IP is invalid!",
          NULL
          );
        return EFI_INVALID_PARAMETER;
      }
    } else {
      if (SubsysConfigData->ServerName[0] == '\0') {
        CreatePopUp (
          EFI_LIGHTGRAY | EFI_BACKGROUND_BLUE,
          &Key,
          L"NVMe-oF target Url should not be NULL!",
          NULL
          );
        return EFI_INVALID_PARAMETER;
      }
    }
  }

  SubsysConfigData->Enabled = IfrNvData->Enabled;

  //
  // Record the user configuration information in NVR.
  //
  Status = NvmeOfConfigSaveAttemptToNvData (Attempt);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  NvmeOfConfigUpdateAttemptInfoList ();
  NvmeOfConfigUpdateDisplayedAttempts ();
  return Status;
}

/**
  Callback function when user presses "Select network device" from the
  "Add an Attempt" configuration form.

  @retval EFI_OUT_OF_RESOURCES   Does not have sufficient resources to finish this
                                 operation.
  @retval EFI_NOT_FOUND          Failed to locate any supported network device handles.
  @retval EFI_SUCCESS            The operation is completed successfully.
  @retval Others                 Failed to add opcodes for available network devices.

**/
EFI_STATUS
NvmeOfConfigSelectMac (
  VOID
  )
{
  EFI_STATUS              Status;
  VOID                    *StartOpCodeHandle;
  EFI_IFR_GUID_LABEL      *StartLabel;
  VOID                    *EndOpCodeHandle;
  EFI_IFR_GUID_LABEL      *EndLabel;
  EFI_STRING_ID           PortTitleToken;
  EFI_STRING_ID           PortTitleHelpToken;
  CHAR16                  MacString[NVMEOF_MAX_MAC_STRING_LEN];
  CHAR16                  *DevicePath;
  LIST_ENTRY              *Entry;
  NVMEOF_CONFIG_NIC_INFO  *NicInfo;

  Status = NvmeOfCreateOpCode (
             MAC_ENTRY_LABEL,
             &StartOpCodeHandle,
             &StartLabel,
             &EndOpCodeHandle,
             &EndLabel
             );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = NvmeOfConfigUpdateNicInfoList ();
  if (EFI_ERROR (Status)) {
    Status = EFI_NOT_FOUND;
    goto Exit;
  }

  NET_LIST_FOR_EACH (Entry, &mConfigPrivate->NicInfoList) {
    NicInfo = NET_LIST_USER_STRUCT (Entry, NVMEOF_CONFIG_NIC_INFO, Link);

    NvmeOfMacAddrToStr (
      &NicInfo->PermanentAddress,
      NicInfo->HwAddressSize,
      NicInfo->VlanId,
      MacString
      );
    UnicodeSPrint (
      mNicPrivate->PortString,
      (UINTN)NVMEOF_NAME_IFR_MAX_SIZE,
      L"MAC %s",
      MacString
      );
    PortTitleToken = HiiSetString (
                       mCallbackInfo->RegisteredHandle,
                       0,
                       mNicPrivate->PortString,
                       NULL
                       );
    if (PortTitleToken == 0) {
      Status = EFI_INVALID_PARAMETER;
      goto Exit;
    }

    DevicePath = ConvertDevicePathToText (
                   DevicePathFromHandle (NicInfo->DeviceHandle),
                   FALSE,                             // DisplayOnly
                   FALSE                              // AllowShortcuts
                   );
    UnicodeSPrint (
      mNicPrivate->PortString,
      (UINTN)NVMEOF_NAME_IFR_MAX_SIZE,
      L"Device Path : %s",
      DevicePath
      );
    FreePool (DevicePath);

    PortTitleHelpToken = HiiSetString (mCallbackInfo->RegisteredHandle, 0, mNicPrivate->PortString, NULL);
    if (PortTitleHelpToken == 0) {
      Status = EFI_INVALID_PARAMETER;
      goto Exit;
    }

    HiiCreateGotoOpCode (
      StartOpCodeHandle,                        // Container for dynamic created opcodes
      FORMID_ATTEMPT_FORM,                      // Destination form
      PortTitleToken,                           // Prompt text
      PortTitleHelpToken,                       // Help text
      EFI_IFR_FLAG_CALLBACK,                    // Question flag
      (UINT16)(KEY_MAC_ENTRY_BASE + NicInfo->NicIndex)
      );
  }

  Status = HiiUpdateForm (
             mCallbackInfo->RegisteredHandle,                // HII handle
             &gNvmeOfConfigGuid,                             // Formset GUID
             FORMID_MAC_FORM,                                // Form ID
             StartOpCodeHandle,                              // Label for where to insert opcodes
             EndOpCodeHandle                                 // Replace data
             );

Exit:

  HiiFreeOpCodeHandle (StartOpCodeHandle);
  HiiFreeOpCodeHandle (EndOpCodeHandle);

  return Status;
}

/**
  Callback function for dynamic opcodes. Used when a user presses "Attempt *" or "Select a network device".

  @param[in]  KeyValue           A unique value which is sent to the original
                                 exporting driver so that it can identify the type
                                 of data to expect.
  @param[in]  IfrNvData          The IFR nv data.

  @retval EFI_OUT_OF_RESOURCES   Does not have sufficient resources to finish this
                                 operation.
  @retval EFI_NOT_FOUND          Cannot find the corresponding variable.
  @retval EFI_SUCCESS            The operation is completed successfully.

**/
EFI_STATUS
NvmeOfConfigProcessDefault (
  IN  EFI_QUESTION_ID           KeyValue,
  IN  NVMEOF_CONFIG_IFR_NVDATA  *IfrNvData
  )
{
  EFI_STATUS                    Status;
  NVMEOF_ATTEMPT_CONFIG_NVDATA  *AttemptConfigData;
  UINT8                         CurrentAttemptConfigIndex;
  UINT8                         NicIndex;
  CHAR16                        MacString[NVMEOF_MAX_MAC_STRING_LEN];
  NVMEOF_CONFIG_NIC_INFO        *NicInfo;
  LIST_ENTRY                    *Entry;

  Status = EFI_SUCCESS;

  if ((KeyValue >= KEY_ATTEMPT_ENTRY_BASE) &&
      (KeyValue < (NVMEOF_MAX_ATTEMPTS_NUM + KEY_ATTEMPT_ENTRY_BASE)))
  {
    //
    // User has pressed "Attempt *". Get the attempt's configuration data.
    //
    AttemptConfigData = NULL;

    CurrentAttemptConfigIndex = (UINT8)(KeyValue - KEY_ATTEMPT_ENTRY_BASE);
    AttemptConfigData         = NvmeOfConfigGetAttemptByConfigIndex (CurrentAttemptConfigIndex);
    if (AttemptConfigData == NULL) {
      DEBUG ((DEBUG_ERROR, "Corresponding configuration data can not be retrieved!\n"));
      return EFI_NOT_FOUND;
    }

    NvmeOfConvertAttemptConfigDataToIfrNvData (AttemptConfigData, IfrNvData);

    //
    // Update current attempt with the found Attempt data.
    //
    mCallbackInfo->Current = AttemptConfigData;

    return Status;
  }

  if ((KeyValue >= KEY_MAC_ENTRY_BASE) &&
      (KeyValue <= (UINT16)(mConfigPrivate->NicCount + KEY_MAC_ENTRY_BASE)))
  {
    //
    // User has pressed "Add an Attempt" and then "Select network device".
    //
    NicIndex = (UINT8)(KeyValue - KEY_MAC_ENTRY_BASE);
    NET_LIST_FOR_EACH (Entry, &mConfigPrivate->NicInfoList) {
      NicInfo = NET_LIST_USER_STRUCT (Entry, NVMEOF_CONFIG_NIC_INFO, Link);
      if (NicInfo->NicIndex == NicIndex) {
        break;
      }
    }
    NvmeOfMacAddrToStr (
      &NicInfo->PermanentAddress,
      NicInfo->HwAddressSize,
      NicInfo->VlanId,
      MacString
      );

    //
    // Update the form to display the selected MAC address.
    //
    StrCpyS (
      IfrNvData->NvmeofSubsysMacString,
      NVMEOF_MAX_MAC_STRING_LEN,
      MacString
      );

    return Status;
  }

  //
  // Otherwise, don't process anything.
  //
  return Status;
}

/**

  This function allows the caller to request the current
  configuration for one or more named elements. The resulting
  string is in <ConfigAltResp> format. Also, any and all alternative
  configuration strings shall be appended to the end of the
  current configuration string. If they are, they must appear
  after the current configuration. They must contain the same
  routing (GUID, NAME, PATH) as the current configuration string.
  They must have an additional description indicating the type of
  alternative configuration the string represents,
  "ALTCFG=<StringToken>". That <StringToken> (when
  converted from Hex UNICODE to binary) is a reference to a
  string in the associated string pack.

  @param[in]  This       Points to the EFI_HII_CONFIG_ACCESS_PROTOCOL.

  @param[in]  Request    A null-terminated Unicode string in
                         <ConfigRequest> format. Note that this
                         includes the routing information as well as
                         the configurable name / value pairs. It is
                         invalid for this string to be in
                         <MultiConfigRequest> format.

  @param[out] Progress   On return, points to a character in the
                         Request string. Points to the string's null
                         terminator if request was successful. Points
                         to the most recent "&" before the first
                         failing name / value pair (or the beginning
                         of the string if the failure is in the first
                         name / value pair) if the request was not successful.

  @param[out] Results    A null-terminated Unicode string in
                         <ConfigAltResp> format which has all values
                         filled in for the names in the Request string.
                         String to be allocated by the called function.

  @retval EFI_SUCCESS             The Results string is filled with the
                                  values corresponding to all requested
                                  names.

  @retval EFI_OUT_OF_RESOURCES    Not enough memory to store the
                                  parts of the results that must be
                                  stored awaiting possible future
                                  protocols.

  @retval EFI_INVALID_PARAMETER   For example, passing in a NULL
                                  for the Request parameter
                                  would result in this type of
                                  error. In this case, the
                                  Progress parameter would be
                                  set to NULL.

  @retval EFI_NOT_FOUND           Routing data doesn't match any
                                  known driver. Progress set to the
                                  first character in the routing header.
                                  Note: There is no requirement that the
                                  driver validate the routing data. It
                                  must skip the <ConfigHdr> in order to
                                  process the names.

  @retval EFI_INVALID_PARAMETER   Illegal syntax. Progress set
                                  to most recent "&" before the
                                  error or the beginning of the
                                  string.

  @retval EFI_INVALID_PARAMETER   Unknown name. Progress points
                                  to the & before the name in
                                  question.

**/
EFI_STATUS
EFIAPI
NvmeOfFormExtractConfig (
  IN  CONST EFI_HII_CONFIG_ACCESS_PROTOCOL  *This,
  IN  CONST EFI_STRING                      Request,
  OUT EFI_STRING                            *Progress,
  OUT EFI_STRING                            *Results
  )
{
  EFI_STATUS                 Status;
  NVMEOF_GLOBAL_DATA         *NvmeOfGlobalData;
  NVMEOF_CONFIG_IFR_NVDATA   *IfrNvData;
  UINTN                      BufferSize;
  NVMEOF_FORM_CALLBACK_INFO  *Private;
  EFI_STRING                 ConfigRequestHdr;
  EFI_STRING                 ConfigRequest;
  BOOLEAN                    AllocatedRequest;
  UINTN                      Size;

  if ((This == NULL) || (Progress == NULL) || (Results == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  *Progress = Request;
  if ((Request != NULL) && !HiiIsConfigHdrMatch (Request, &gNvmeOfConfigGuid, mVendorStorageName)) {
    return EFI_NOT_FOUND;
  }

  ConfigRequestHdr = NULL;
  ConfigRequest    = NULL;
  AllocatedRequest = FALSE;
  Size             = 0;

  Private   = NVMEOF_FORM_CALLBACK_INFO_FROM_FORM_CALLBACK (This);
  IfrNvData = AllocateZeroPool (sizeof (NVMEOF_CONFIG_IFR_NVDATA));
  if (IfrNvData == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  if (Private->Current != NULL) {
    NvmeOfConvertAttemptConfigDataToIfrNvData (Private->Current, IfrNvData);
  }

  NvmeOfGlobalData = NvmeOfGetVariableAndSize (
                       L"NvmeofGlobalData",
                       &gNvmeOfConfigGuid,
                       &Size
                       );
  if ((NvmeOfGlobalData == NULL) || (Size == 0)) {
    IfrNvData->NvmeofHostNqn[0] = L'\0';
    IfrNvData->NvmeofHostId[0]  = L'\0';
  } else {
    AsciiStrToUnicodeStrS (
      NvmeOfGlobalData->NvmeofHostNqn,
      IfrNvData->NvmeofHostNqn,
      NVMEOF_NAME_MAX_SIZE
      );

    if (!IsZeroGuid ((EFI_GUID *)NvmeOfGlobalData->NvmeofHostId)) {
      UnicodeSPrint (
        IfrNvData->NvmeofHostId,
        sizeof (IfrNvData->NvmeofHostId),
        L"%g",
        NvmeOfGlobalData->NvmeofHostId
        );
    }

    FreePool (NvmeOfGlobalData);
  }

  //
  // Convert buffer data to <ConfigResp> by helper function BlockToConfig().
  //
  BufferSize    = sizeof (NVMEOF_CONFIG_IFR_NVDATA);
  ConfigRequest = Request;
  if ((Request == NULL) || (StrStr (Request, L"OFFSET") == NULL)) {
    //
    // Request has no request element, construct full request string.
    // Allocate and fill a buffer large enough to hold the <ConfigHdr> template
    // followed by "&OFFSET=0&WIDTH=WWWWWWWWWWWWWWWW" followed by a Null-terminator
    //
    ConfigRequestHdr = HiiConstructConfigHdr (&gNvmeOfConfigGuid, mVendorStorageName, Private->DriverHandle);
    Size             = (StrLen (ConfigRequestHdr) + 32 + 1) * sizeof (CHAR16);
    ConfigRequest    = AllocateZeroPool (Size);
    if (ConfigRequest == NULL) {
      FreePool (IfrNvData);
      return EFI_OUT_OF_RESOURCES;
    }

    AllocatedRequest = TRUE;
    UnicodeSPrint (ConfigRequest, Size, L"%s&OFFSET=0&WIDTH=%016LX", ConfigRequestHdr, (UINT64)BufferSize);
    FreePool (ConfigRequestHdr);
  }

  Status = gHiiConfigRouting->BlockToConfig (
                                gHiiConfigRouting,
                                ConfigRequest,
                                (UINT8 *)IfrNvData,
                                BufferSize,
                                Results,
                                Progress
                                );
  FreePool (IfrNvData);

  //
  // Free the allocated config request string.
  //
  if (AllocatedRequest) {
    FreePool (ConfigRequest);
    ConfigRequest = NULL;
  }

  //
  // Set Progress string to the original request string.
  //
  if (Request == NULL) {
    *Progress = NULL;
  } else if (StrStr (Request, L"OFFSET") == NULL) {
    *Progress = Request + StrLen (Request);
  }

  return Status;
}

/**

  This function applies changes in a driver's configuration.
  Input is a Configuration, which has the routing data for this
  driver followed by name / value configuration pairs. The driver
  must apply those pairs to its configurable storage. If the
  driver's configuration is stored in a linear block of data
  and the driver's name / value pairs are in <BlockConfig>
  format, it may use the ConfigToBlock helper function (above) to
  simplify the job.

  @param[in]  This           Points to the EFI_HII_CONFIG_ACCESS_PROTOCOL.

  @param[in]  Configuration  A null-terminated Unicode string in
                             <ConfigString> format.

  @param[out] Progress       A pointer to a string filled in with the
                             offset of the most recent '&' before the
                             first failing name / value pair (or the
                             beginning of the string if the failure
                             is in the first name / value pair) or
                             the terminating NULL if all was
                             successful.

  @retval EFI_SUCCESS             The results have been distributed or are
                                  awaiting distribution.

  @retval EFI_OUT_OF_RESOURCES    Not enough memory to store the
                                  parts of the results that must be
                                  stored awaiting possible future
                                  protocols.

  @retval EFI_INVALID_PARAMETERS  Passing in a NULL for the
                                  Results parameter would result
                                  in this type of error.

  @retval EFI_NOT_FOUND           Target for the specified routing data
                                  was not found.

**/
EFI_STATUS
EFIAPI
NvmeOfFormRouteConfig (
  IN  CONST EFI_HII_CONFIG_ACCESS_PROTOCOL  *This,
  IN  CONST EFI_STRING                      Configuration,
  OUT EFI_STRING                            *Progress
  )
{
  EFI_STATUS                Status;
  NVMEOF_CONFIG_IFR_NVDATA  *IfrNvData;
  UINTN                     BufferSize;

  Status = EFI_SUCCESS;

  if ((This == NULL) || (Configuration == NULL) || (Progress == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  //
  // Check routing data in <ConfigHdr>.
  // Note: if only one Storage is used, then this checking could be skipped.
  //
  if (!HiiIsConfigHdrMatch (Configuration, &gNvmeOfConfigGuid, mVendorStorageName)) {
    *Progress = Configuration;
    return EFI_NOT_FOUND;
  }

  IfrNvData = AllocateZeroPool (sizeof (NVMEOF_CONFIG_IFR_NVDATA));
  if (IfrNvData == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  //
  // Convert <ConfigResp> to buffer data by helper function ConfigToBlock().
  //
  BufferSize = sizeof (NVMEOF_CONFIG_IFR_NVDATA);
  Status     = gHiiConfigRouting->ConfigToBlock (
                                    gHiiConfigRouting,
                                    Configuration,
                                    (UINT8 *)IfrNvData,
                                    &BufferSize,
                                    Progress
                                    );
  if (EFI_ERROR (Status)) {
    goto Exit;
  }

Exit:
  if (IfrNvData != NULL) {
    FreePool (IfrNvData);
  }

  return Status;
}

/**

  This function is called to provide results data to the driver.
  This data consists of a unique key that is used to identify
  which data is either being passed back or being asked for.

  @param[in]       This          Points to the EFI_HII_CONFIG_ACCESS_PROTOCOL.
  @param[in]       Action        Specifies the type of action taken by the browser.
  @param[in]       QuestionId    A unique value which is sent to the original
                                 exporting driver so that it can identify the type
                                 of data to expect. The format of the data tends to
                                 vary based on the opcode that generated the callback.
  @param[in]       Type          The type of value for the question.
  @param[in, out]  Value         A pointer to the data being sent to the original
                                 exporting driver.
  @param[out]      ActionRequest On return, points to the action requested by the
                                 callback function.

  @retval EFI_SUCCESS            The callback successfully handled the action.
  @retval EFI_OUT_OF_RESOURCES   Not enough storage is available to hold the
                                 variable and its data.
  @retval EFI_DEVICE_ERROR       The variable could not be saved.
  @retval EFI_UNSUPPORTED        The specified Action is not supported by the
                                 callback.
**/
EFI_STATUS
EFIAPI
NvmeOfFormCallback (
  IN CONST  EFI_HII_CONFIG_ACCESS_PROTOCOL  *This,
  IN        EFI_BROWSER_ACTION              Action,
  IN        EFI_QUESTION_ID                 QuestionId,
  IN        UINT8                           Type,
  IN OUT    EFI_IFR_TYPE_VALUE              *Value,
  OUT       EFI_BROWSER_ACTION_REQUEST      *ActionRequest
  )
{
  NVMEOF_FORM_CALLBACK_INFO  *Private;
  UINTN                      BufferSize;
  CHAR8                      IpString[NVMEOF_NAME_MAX_SIZE];
  CHAR8                      NidString[NVMEOF_NID_MAX_LEN];
  EFI_IP_ADDRESS             IpAddr;
  EFI_IP_ADDRESS             SubnetMask;
  EFI_IP_ADDRESS             Gateway;
  NVMEOF_CONFIG_IFR_NVDATA   *IfrNvData;
  NVMEOF_CONFIG_IFR_NVDATA   OldIfrNvData;
  NVMEOF_GLOBAL_DATA         *NvmeOfGlobalData;
  EFI_STATUS                 Status;
  EFI_INPUT_KEY              Key;
  CHAR16                     MacString[NVMEOF_MAX_MAC_STRING_LEN];
  NVMEOF_CONFIG_NIC_INFO     *NicInfo;
  LIST_ENTRY                 *Entry;

  if ((Action == EFI_BROWSER_ACTION_FORM_OPEN) && mCallbackInfo->InitialFormLoad) {
    //
    // Populate the NicInfoList on initial form load.
    //
    NvmeOfConfigUpdateNicInfoList ();

    //
    // Populate internal AttemptConfig list from NV data on initial form load.
    //
    NvmeOfConfigUpdateAttemptInfoList ();

    //
    // Update main form first time the form loads.
    //
    NvmeOfConfigUpdateDisplayedAttempts ();
    mCallbackInfo->InitialFormLoad = false;
  }

  if ((Action == EFI_BROWSER_ACTION_FORM_OPEN) || (Action == EFI_BROWSER_ACTION_FORM_CLOSE)) {
    //
    // Do nothing for UEFI OPEN/CLOSE Action
    //
    return EFI_SUCCESS;
  }

  if ((Action != EFI_BROWSER_ACTION_CHANGING) && (Action != EFI_BROWSER_ACTION_CHANGED)) {
    //
    // All other type return unsupported.
    //
    return EFI_UNSUPPORTED;
  }

  if ((Value == NULL) || (ActionRequest == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  Private = NVMEOF_FORM_CALLBACK_INFO_FROM_FORM_CALLBACK (This);

  //
  // Retrieve uncommitted data from Browser
  //

  BufferSize = sizeof (NVMEOF_CONFIG_IFR_NVDATA);
  IfrNvData  = AllocateZeroPool (BufferSize);
  if (IfrNvData == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  Status = EFI_SUCCESS;

  ZeroMem (&OldIfrNvData, BufferSize);

  HiiGetBrowserData (NULL, NULL, BufferSize, (UINT8 *)IfrNvData);

  CopyMem (&OldIfrNvData, IfrNvData, BufferSize);

  if (Action == EFI_BROWSER_ACTION_CHANGING) {
    switch (QuestionId) {
      case KEY_SELECT_MAC:
        Status = NvmeOfConfigSelectMac ();
        break;

      default:
        Status = NvmeOfConfigProcessDefault (QuestionId, IfrNvData);
        break;
    }
  } else if (Action == EFI_BROWSER_ACTION_CHANGED) {
    switch (QuestionId) {
      case KEY_HOST_NQN:
      case KEY_HOST_ID:
        //
        // Update modified Global Data values.
        //
        BufferSize       = 0;
        NvmeOfGlobalData = NvmeOfGetVariableAndSize (
                             L"NvmeofGlobalData",
                             &gNvmeOfConfigGuid,
                             &BufferSize
                             );
        if ((NvmeOfGlobalData == NULL) || (BufferSize == 0)) {
          Status = EFI_NOT_FOUND;
          break;
        }

        if (QuestionId == KEY_HOST_NQN) {
          if (!NvmeOfConfigIsNqnValid (IfrNvData->NvmeofHostNqn)) {
            CreatePopUp (
              EFI_LIGHTGRAY | EFI_BACKGROUND_BLUE,
              &Key,
              L"Invalid NQN format!",
              NULL
              );
            FreePool (NvmeOfGlobalData);
            break;
          }

          UnicodeStrToAsciiStrS (
            IfrNvData->NvmeofHostNqn,
            NvmeOfGlobalData->NvmeofHostNqn,
            NVMEOF_NAME_MAX_SIZE
            );
        } else if (QuestionId == KEY_HOST_ID) {
          Status = StrToGuid (
                     IfrNvData->NvmeofHostId,
                     (EFI_GUID *)NvmeOfGlobalData->NvmeofHostId
                     );
          if (EFI_ERROR (Status)) {
            CreatePopUp (
              EFI_LIGHTGRAY | EFI_BACKGROUND_BLUE,
              &Key,
              L"Invalid Host ID format!",
              NULL
              );
            FreePool (NvmeOfGlobalData);
            break;
          }
        }

        Status = gRT->SetVariable (
                        L"NvmeofGlobalData",
                        &gNvmeOfConfigGuid,
                        NVMEOF_CONFIG_VAR_ATTR,
                        sizeof (*NvmeOfGlobalData),
                        NvmeOfGlobalData
                        );
        if (EFI_ERROR (Status)) {
          CreatePopUp (
            EFI_LIGHTGRAY | EFI_BACKGROUND_BLUE,
            &Key,
            L"Error setting value!",
            NULL
            );
          FreePool (NvmeOfGlobalData);
          break;
        }

        *ActionRequest = EFI_BROWSER_ACTION_REQUEST_FORM_APPLY;
        FreePool (NvmeOfGlobalData);
        break;

      case KEY_IP_MODE:
        switch (Value->u8) {
          case IP_MODE_IP6:
            if (IfrNvData->NvmeofSubsysMacString[0] == '\0') {
              IfrNvData->IpMode = IP_MODE_IP4;

              CreatePopUp (
                EFI_LIGHTGRAY | EFI_BACKGROUND_BLUE,
                &Key,
                L"Please select network device first.",
                NULL
                );
            } else {
              NET_LIST_FOR_EACH (Entry, &mConfigPrivate->NicInfoList) {
                NicInfo = NET_LIST_USER_STRUCT (Entry, NVMEOF_CONFIG_NIC_INFO, Link);
                NvmeOfMacAddrToStr (&NicInfo->PermanentAddress, NicInfo->HwAddressSize, NicInfo->VlanId, MacString);
                if (!StrCmp (MacString, IfrNvData->NvmeofSubsysMacString)) {
                  break;
                }
              }

              if (!NicInfo->Ipv6Support) {
                IfrNvData->IpMode = IP_MODE_IP4;

                CreatePopUp (
                  EFI_LIGHTGRAY | EFI_BACKGROUND_BLUE,
                  &Key,
                  L"Current NIC doesn't Support IPv6!",
                  NULL
                  );
              }
            }

          case IP_MODE_IP4:
            ZeroMem (IfrNvData->NvmeofSubsysHostIp, sizeof (IfrNvData->NvmeofSubsysHostIp));
            ZeroMem (IfrNvData->NvmeofSubsysHostSubnetMask, sizeof (IfrNvData->NvmeofSubsysHostSubnetMask));
            ZeroMem (IfrNvData->NvmeofSubsysHostGateway, sizeof (IfrNvData->NvmeofSubsysHostGateway));
            ZeroMem (IfrNvData->NvmeofSubsysIp, sizeof (IfrNvData->NvmeofSubsysIp));
            Private->Current->AutoConfigureMode = 0;
            ZeroMem (&Private->Current->SubsysConfigData.NvmeofSubsysHostIP, sizeof (EFI_IP_ADDRESS));
            ZeroMem (&Private->Current->SubsysConfigData.NvmeofSubsysHostSubnetMask, sizeof (EFI_IPv4_ADDRESS));
            ZeroMem (&Private->Current->SubsysConfigData.NvmeofSubsysHostGateway, sizeof (EFI_IP_ADDRESS));
            ZeroMem (&Private->Current->SubsysConfigData.NvmeofSubSystemIp, sizeof (EFI_IP_ADDRESS));

            break;
        }

        break;

      case KEY_HOST_IP:
        if (IfrNvData->IpMode == IP_MODE_IP6) {
          Status = NetLibStrToIp6 (IfrNvData->NvmeofSubsysHostIp, &IpAddr.v6);
          if (EFI_ERROR (Status) ||  !IpIsUnicast (&IpAddr, IfrNvData->IpMode)) {
            CreatePopUp (
              EFI_LIGHTGRAY | EFI_BACKGROUND_BLUE,
              &Key,
              L"Invalid IP address!",
              NULL
              );

            Status = EFI_INVALID_PARAMETER;
          } else {
            CopyMem (&Private->Current->SubsysConfigData.NvmeofSubsysHostIP, &IpAddr.v6, sizeof (IpAddr.v6));
          }
        } else if (IfrNvData->IpMode == IP_MODE_IP4) {
          Status = NetLibStrToIp4 (IfrNvData->NvmeofSubsysHostIp, &IpAddr.v4);
          if (EFI_ERROR (Status) ||
              ((Private->Current->SubsysConfigData.NvmeofSubsysHostSubnetMask.Addr[0] != 0) &&
               !NetIp4IsUnicast (
                  NTOHL (IpAddr.Addr[0]),
                  NTOHL (*(UINT32 *)Private->Current->SubsysConfigData.NvmeofSubsysHostSubnetMask.Addr)
                  )))
          {
            CreatePopUp (
              EFI_LIGHTGRAY | EFI_BACKGROUND_BLUE,
              &Key,
              L"Invalid IP address!",
              NULL
              );

            Status = EFI_INVALID_PARAMETER;
          } else {
            CopyMem (&Private->Current->SubsysConfigData.NvmeofSubsysHostIP, &IpAddr.v4, sizeof (IpAddr.v4));
          }
        }

        break;

      case KEY_SUBNET_MASK:
        Status = NetLibStrToIp4 (IfrNvData->NvmeofSubsysHostSubnetMask, &SubnetMask.v4);
        if (EFI_ERROR (Status) || ((SubnetMask.Addr[0] != 0) && (NvmeOfGetSubnetMaskPrefixLength (&SubnetMask.v4) == 0))) {
          CreatePopUp (
            EFI_LIGHTGRAY | EFI_BACKGROUND_BLUE,
            &Key,
            L"Invalid Subnet Mask!",
            NULL
            );

          Status = EFI_INVALID_PARAMETER;
        } else {
          CopyMem (&Private->Current->SubsysConfigData.NvmeofSubsysHostSubnetMask, &SubnetMask.v4, sizeof (SubnetMask.v4));
        }

        break;

      case KEY_GATE_WAY:
        if (IfrNvData->IpMode == IP_MODE_IP6) {
          Status = NetLibStrToIp6 (IfrNvData->NvmeofSubsysHostGateway, &Gateway.v6);
          if (EFI_ERROR (Status) || !IpIsUnicast (&Gateway, IfrNvData->IpMode)) {
            CreatePopUp (
              EFI_LIGHTGRAY | EFI_BACKGROUND_BLUE,
              &Key,
              L"Invalid Gateway!",
              NULL
              );
            Status = EFI_INVALID_PARAMETER;
          } else {
            CopyMem (&Private->Current->SubsysConfigData.NvmeofSubsysHostGateway, &Gateway.v6, sizeof (Gateway.v6));
          }
        } else if (IfrNvData->IpMode == IP_MODE_IP4) {
          Status = NetLibStrToIp4 (IfrNvData->NvmeofSubsysHostGateway, &Gateway.v4);
          if (EFI_ERROR (Status) ||
              ((Gateway.Addr[0] != 0) &&
               (Private->Current->SubsysConfigData.NvmeofSubsysHostSubnetMask.Addr[0] != 0) &&
               !NetIp4IsUnicast (
                  NTOHL (Gateway.Addr[0]),
                  NTOHL (*(UINT32 *)Private->Current->SubsysConfigData.NvmeofSubsysHostSubnetMask.Addr)
                  )))
          {
            CreatePopUp (
              EFI_LIGHTGRAY | EFI_BACKGROUND_BLUE,
              &Key,
              L"Invalid Gateway!",
              NULL
              );
            Status = EFI_INVALID_PARAMETER;
          } else {
            CopyMem (&Private->Current->SubsysConfigData.NvmeofSubsysHostGateway, &Gateway.v4, sizeof (Gateway.v4));
          }
        }

        break;

      case KEY_SUBSYS_IP:
        UnicodeStrToAsciiStrS (IfrNvData->NvmeofSubsysIp, IpString, sizeof (IpString));
        Status = NvmeOfAsciiStrToIp (IpString, IfrNvData->IpMode, &IpAddr);
        if (EFI_ERROR (Status) || !IpIsUnicast (&IpAddr, IfrNvData->IpMode)) {
          //
          // The target is expressed in URL format or an invalid Ip address, just save.
          //
          Private->Current->SubsysConfigData.NvmeofDnsMode = NVMEOF_DNS_ENABLED;
          ZeroMem (&Private->Current->SubsysConfigData.NvmeofSubSystemIp, sizeof (Private->Current->SubsysConfigData.NvmeofSubSystemIp));
          UnicodeStrToAsciiStrS (IfrNvData->NvmeofSubsysIp, Private->Current->SubsysConfigData.ServerName, NVMEOF_NAME_MAX_SIZE);
        } else {
          Private->Current->SubsysConfigData.NvmeofDnsMode = NVMEOF_DNS_DISABLED;
          ZeroMem (&Private->Current->SubsysConfigData.ServerName, sizeof (Private->Current->SubsysConfigData.ServerName));
          CopyMem (&Private->Current->SubsysConfigData.NvmeofSubSystemIp, &IpAddr, sizeof (IpAddr));
        }

        break;

      case KEY_DHCP_ENABLE:
        if (IfrNvData->HostInfoDhcp == NVMEOF_DHCP_DISABLED) {
          IfrNvData->NvmeofSubsysInfoDhcp = NVMEOF_DHCP_DISABLED;
        }

        break;

      case KEY_SUBSYS_NQN:
        if (!NvmeOfConfigIsNqnValid (IfrNvData->NvmeofSubsysNqn)) {
          CreatePopUp (
            EFI_LIGHTGRAY | EFI_BACKGROUND_BLUE,
            &Key,
            L"Invalid NQN format!",
            NULL
            );
          break;
        }

        UnicodeStrToAsciiStrS (
          IfrNvData->NvmeofSubsysNqn,
          Private->Current->SubsysConfigData.NvmeofSubsysNqn,
          NVMEOF_NQN_MAX_LEN
          );
        break;

      case KEY_SUBSYS_NID:
        UnicodeStrToAsciiStrS (IfrNvData->NvmeofSubsysNid, NidString, NVMEOF_NID_MAX_LEN);
        if ((IfrNvData->NvmeofSubsysNid[0] != '\0') &&
            !NvmeOfIsNidValid (NidString))
        {
          CreatePopUp (
            EFI_LIGHTGRAY | EFI_BACKGROUND_BLUE,
            &Key,
            L"Invalid NID format!",
            NULL
            );
          break;
        }

        UnicodeStrToAsciiStrS (
          IfrNvData->NvmeofSubsysNid,
          Private->Current->SubsysConfigData.NvmeofSubsysNid,
          NVMEOF_NID_MAX_LEN
          );
        break;

      case KEY_SAVE_ATTEMPT_CONFIG:
        Status = NvmeOfConvertIfrNvDataToAttemptConfigData (IfrNvData, Private->Current);
        if (EFI_ERROR (Status)) {
          break;
        }

        *ActionRequest = EFI_BROWSER_ACTION_REQUEST_FORM_SUBMIT_EXIT;
        break;

      default:
        break;
    }
  }

  if (!EFI_ERROR (Status)) {
    //
    // Pass changed uncommitted data back to Form Browser.
    //
    BufferSize = sizeof (NVMEOF_CONFIG_IFR_NVDATA);
    HiiSetBrowserData (NULL, NULL, BufferSize, (UINT8 *)IfrNvData, NULL);
  }

  FreePool (IfrNvData);

  return Status;
}

/**
  Initialize the NVMe-oF configuration form.

  @param[in]  DriverBindingHandle The NVMe-oF driverbinding handle.

  @retval EFI_SUCCESS             The NVMe-oF configuration form is initialized.
  @retval EFI_OUT_OF_RESOURCES    Failed to allocate memory.

**/
EFI_STATUS
NvmeOfConfigFormInit (
  IN EFI_HANDLE  DriverBindingHandle
  )
{
  EFI_STATUS                 Status;
  NVMEOF_FORM_CALLBACK_INFO  *CallbackInfo;

  CallbackInfo = (NVMEOF_FORM_CALLBACK_INFO *)AllocateZeroPool (sizeof (NVMEOF_FORM_CALLBACK_INFO));
  if (CallbackInfo == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  CallbackInfo->Signature = NVMEOF_FORM_CALLBACK_INFO_SIGNATURE;
  CallbackInfo->Current   = NULL;

  CallbackInfo->ConfigAccess.ExtractConfig = NvmeOfFormExtractConfig;
  CallbackInfo->ConfigAccess.RouteConfig   = NvmeOfFormRouteConfig;
  CallbackInfo->ConfigAccess.Callback      = NvmeOfFormCallback;

  CallbackInfo->InitialFormLoad = true;

  //
  // Create the private data structures.
  //
  mConfigPrivate = AllocateZeroPool (sizeof (NVMEOF_CONFIG_PRIVATE_DATA));
  if (mConfigPrivate == NULL) {
    FreePool (CallbackInfo);
    return EFI_OUT_OF_RESOURCES;
  }

  InitializeListHead (&mConfigPrivate->NicInfoList);
  InitializeListHead (&mConfigPrivate->AttemptInfoList);

  //
  // Install Device Path Protocol and Config Access protocol to driver handle.
  //
  Status = gBS->InstallMultipleProtocolInterfaces (
                  &CallbackInfo->DriverHandle,
                  &gEfiDevicePathProtocolGuid,
                  &mNvmeOfHiiVendorDevicePath,
                  &gEfiHiiConfigAccessProtocolGuid,
                  &CallbackInfo->ConfigAccess,
                  NULL
                  );
  ASSERT_EFI_ERROR (Status);

  //
  // Publish our HII data.
  //
  CallbackInfo->RegisteredHandle = HiiAddPackages (
                                     &gNvmeOfConfigGuid,
                                     CallbackInfo->DriverHandle,
                                     NvmeOfDxeStrings,
                                     NvmeOfConfigVfrBin,
                                     NULL
                                     );
  if (CallbackInfo->RegisteredHandle == NULL) {
    gBS->UninstallMultipleProtocolInterfaces (
           CallbackInfo->DriverHandle,
           &gEfiDevicePathProtocolGuid,
           &mNvmeOfHiiVendorDevicePath,
           &gEfiHiiConfigAccessProtocolGuid,
           &CallbackInfo->ConfigAccess,
           NULL
           );
    FreePool (CallbackInfo);
    return EFI_OUT_OF_RESOURCES;
  }

  mCallbackInfo = CallbackInfo;

  return EFI_SUCCESS;
}

/**
  Unload the NVMe-oF configuration form, this includes: uninstall the form callback protocol, and free the resources used.

  @param[in]  DriverBindingHandle The NVMe-oF driverbinding handle.

  @retval EFI_SUCCESS             The NVMe-oF configuration form is unloaded.
  @retval Others                  Failed to unload the form.

**/
EFI_STATUS
NvmeOfConfigFormUnload (
  IN EFI_HANDLE  DriverBindingHandle
  )
{
  EFI_STATUS                  Status;
  NVMEOF_CONFIG_NIC_INFO      *NicInfo;
  NVMEOF_CONFIG_ATTEMPT_INFO  *AttemptInfo;
  LIST_ENTRY                  *Entry;

  while (!IsListEmpty (&mConfigPrivate->NicInfoList)) {
    Entry   = NetListRemoveHead (&mConfigPrivate->NicInfoList);
    NicInfo = NET_LIST_USER_STRUCT (Entry, NVMEOF_CONFIG_NIC_INFO, Link);
    FreePool (NicInfo);
    mConfigPrivate->NicCount--;
  }

  ASSERT (mConfigPrivate->NicCount == 0);

  while (!IsListEmpty (&mConfigPrivate->AttemptInfoList)) {
    Entry       = NetListRemoveHead (&mConfigPrivate->AttemptInfoList);
    AttemptInfo = NET_LIST_USER_STRUCT (Entry, NVMEOF_CONFIG_ATTEMPT_INFO, Link);
    FreePool (AttemptInfo);
    mConfigPrivate->AttemptCount--;
  }

  ASSERT (mConfigPrivate->AttemptCount == 0);

  FreePool (mConfigPrivate);
  mConfigPrivate = NULL;

  //
  // Remove HII package list.
  //
  HiiRemovePackages (mCallbackInfo->RegisteredHandle);

  //
  // Uninstall Device Path Protocol and Config Access protocol.
  //
  Status = gBS->UninstallMultipleProtocolInterfaces (
                  mCallbackInfo->DriverHandle,
                  &gEfiDevicePathProtocolGuid,
                  &mNvmeOfHiiVendorDevicePath,
                  &gEfiHiiConfigAccessProtocolGuid,
                  &mCallbackInfo->ConfigAccess,
                  NULL
                  );

  FreePool (mCallbackInfo);

  return Status;
}

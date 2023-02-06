/** @file
  NVMeOF DHCP4 related configuration routines.

  Copyright (c) 2020, Dell EMC All rights reserved
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "NvmeOfImpl.h"

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
  )
{
  UINT8 Index = 0;
  for (; (Str[Index] != '\0') && Index < Length; Index++) {
    Str[Index] = AsciiCharToUpper (Str[Index]);
  }
}

/**
  Extract the Root Path option and get the required target information.

  @param[in]        RootPath         The RootPath.
  @param[in]        Length           Length of the RootPath option payload.
  @param[in, out]   ConfigData       The attempt configuration data read
                                     from a nonvolatile device.

  @retval EFI_SUCCESS           All required information is extracted from the RootPath option.
  @retval EFI_NOT_FOUND         The RootPath is not an NVMeOF target.
  @retval EFI_OUT_OF_RESOURCES  Failed to allocate memory.
  @retval EFI_INVALID_PARAMETER The RootPath is malformatted.

**/
EFI_STATUS
NvmeOfDhcpExtractRootPath (
  IN      CHAR8                        *RootPath,
  IN      UINT16                       Length,
  IN OUT  NVMEOF_ATTEMPT_CONFIG_NVDATA *ConfigData
  )
{
  EFI_STATUS                     Status = 0 ;
  UINT16                         RootPathIdLen = 0;
  CHAR8                          *TmpStr = NULL;
  CHAR8                          *RootPathSubStr = NULL;
  NVMEOF_ROOT_PATH_FIELD         Fields[RP_FIELD_IDX_MAX] = { 0 };
  NVMEOF_ROOT_PATH_FIELD         *Field = NULL;
  UINT32                         FieldIndex = 0;
  UINT16                         Index = 0;
  NVMEOF_SUBSYSTEM_CONFIG_NVDATA *ConfigNvData = NULL;
  EFI_IP_ADDRESS                 Ip = { 0 };
  UINT8                          IpMode = 0;
  UINT8                          IsPortFieldEmpty = 1;

  ConfigNvData = &ConfigData->SubsysConfigData;

  //
  // "NVME<+PROTOCOL>://<SERVERNAME/IP>[:TRANSPORT PORT]/<SUBSYS NQN>/<NID>"
  //
  RootPathIdLen = sizeof (NVMEOF_ROOT_PATH_ID) - 1;
  ConvertStrnAsciiCharLowerToUpper (RootPath, RootPathIdLen);

  if ((Length <= RootPathIdLen) || (CompareMem (RootPath, NVMEOF_ROOT_PATH_ID, RootPathIdLen) != 0)) {
    return EFI_NOT_FOUND;
  }

  NvmeOfSaveRootPathForNbft (RootPath, Length);
  //
  // Skip the NVMeOF RootPath ID "NVME+".
  //
  RootPath += RootPathIdLen;
  Length  = (UINT16) (Length - RootPathIdLen);

  TmpStr  = (CHAR8 *) AllocatePool (Length + 1);
  if (TmpStr == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  CopyMem (TmpStr, RootPath, Length);
  TmpStr[Length]  = '\0';

  Index           = 0;
  FieldIndex      = RP_FIELD_IDX_PROTOCOL;
  ZeroMem (&Fields[0], sizeof (Fields));

  // Extract PROTOCOL field from Root Path.
  Fields[RP_FIELD_IDX_PROTOCOL].Str = &TmpStr[Index];

  while ((TmpStr[Index] != NVMEOF_ROOT_PATH_PROTOCOL_END_DELIMITER) && (Index < Length)) {
    Index++;
  }
  TmpStr[Index] = '\0';
  //  skip ://
  Index += 3;

  //
  // Check the protocol type.
  //
  Field = &Fields[RP_FIELD_IDX_PROTOCOL];
  if ((Field->Str != NULL) && AsciiStriCmp (Field->Str, NVMEOF_ROOT_PATH_TCP_PROTOCOL_ID)) {
    Status = EFI_INVALID_PARAMETER;
    goto ON_EXIT;
  }

  // Extract SERVERNAME field from Root Path.
  Fields[RP_FIELD_IDX_SERVERNAME].Str = &TmpStr[Index];

  while ((TmpStr[Index] != NVMEOF_ROOT_PATH_PORT_START_DELIMITER) &&
    (TmpStr[Index] != NVMEOF_ROOT_PATH_FIELD_DELIMITER) &&
    (Index < Length)) {
    Index++;
  }

  if (TmpStr[Index] == NVMEOF_ROOT_PATH_FIELD_DELIMITER) {
    // Port is empty.

    TmpStr[Index] = '\0'; // String termination for ServerName.
    Index++;
  } else if (TmpStr[Index] == NVMEOF_ROOT_PATH_PORT_START_DELIMITER &&
    TmpStr[Index + 1] == NVMEOF_ROOT_PATH_FIELD_DELIMITER) {
    // Port is empty.

    TmpStr[Index] = '\0'; // String termination for ServerName.
    // Skip ':' and '/'
    Index += 2;
  } else if (TmpStr[Index] == NVMEOF_ROOT_PATH_PORT_START_DELIMITER) {
    TmpStr[Index] = '\0'; // String termination for ServerName.
    Index++;

    //Port is not empty.
    IsPortFieldEmpty = 0;
  }

  if (Fields[RP_FIELD_IDX_SERVERNAME].Str != NULL) {
    Fields[RP_FIELD_IDX_SERVERNAME].Len = (UINT8) AsciiStrLen(Fields[RP_FIELD_IDX_SERVERNAME].Str);
  }

  //
  // Extract other fields in the Root Path option string.
  //
  for (FieldIndex = RP_FIELD_IDX_PORT; (FieldIndex < RP_FIELD_IDX_MAX) && (Index < Length); FieldIndex++) {

    if ((FieldIndex == RP_FIELD_IDX_PORT) && IsPortFieldEmpty) {
      continue;
    }

    if ((FieldIndex == RP_FIELD_IDX_NID) &&
      (TmpStr[Index] != NVMEOF_ROOT_PATH_FIELD_DELIMITER)) {
      // Get "urn".
      RootPathSubStr = &TmpStr[Index];
      while ((TmpStr[Index] != NVMEOF_ROOT_PATH_URN_END_DELIMITER) && (Index < Length)) {
        Index++;
      }
      TmpStr[Index] = '\0';
      Index++; // Skip NVMEOF_ROOT_PATH_URN_END_DELIMITER

      // Check "urn".
      if (0 != AsciiStriCmp (RootPathSubStr, NVMEOF_ROOT_PATH_URN_ID)) {
        break;
      }

      // Get NID Type.
      RootPathSubStr = &TmpStr[Index];
      while ((TmpStr[Index] != NVMEOF_ROOT_PATH_NIDTYPE_END_DELIMITER) && (Index < Length)) {
        Index++;
      }
      TmpStr[Index] = '\0';
      Index++; // Skip NVMEOF_ROOT_PATH_NIDTYPE_END_DELIMITER

      // Check NID Type.
      if ((0 != AsciiStriCmp (RootPathSubStr, NVMEOF_ROOT_PATH_NID_TYPE_UUID)) &&
        (0 != AsciiStriCmp (RootPathSubStr, NVMEOF_ROOT_PATH_NID_TYPE_EUI64)) &&
        (0 != AsciiStriCmp (RootPathSubStr, NVMEOF_ROOT_PATH_NID_TYPE_NGUID))) {
        break;
      }

      if ((TmpStr[Index] == NVMEOF_ROOT_PATH_FIELD_DELIMITER) || (Index == Length)) {
        //Invalid Format.
        break;
      }
    }

    if (TmpStr[Index] != NVMEOF_ROOT_PATH_FIELD_DELIMITER) {
      Fields[FieldIndex].Str = &TmpStr[Index];
    }

    while ((TmpStr[Index] != NVMEOF_ROOT_PATH_FIELD_DELIMITER) && (Index < Length)) {
      Index++;
    }

    if (TmpStr[Index] == NVMEOF_ROOT_PATH_FIELD_DELIMITER || (Index == Length)) {
      if (FieldIndex != RP_FIELD_IDX_NID) {
        TmpStr[Index] = '\0';
        Index++;
      }

      if (Fields[FieldIndex].Str != NULL) {
        Fields[FieldIndex].Len = (UINT8) AsciiStrLen (Fields[FieldIndex].Str);
      }
    }

    if ((FieldIndex == RP_FIELD_IDX_TARGETNAME) && (Index >= Length)) {
      FieldIndex = RP_FIELD_IDX_MAX;
      break;
    }
  }

  if (FieldIndex != RP_FIELD_IDX_MAX) {
    Status = EFI_INVALID_PARAMETER;
    goto ON_EXIT;
  }

  if ((Fields[RP_FIELD_IDX_SERVERNAME].Str == NULL) ||
      (Fields[RP_FIELD_IDX_TARGETNAME].Str == NULL) ||
      (Fields[RP_FIELD_IDX_PROTOCOL].Len > 1)
      ) {

    Status = EFI_INVALID_PARAMETER;
    goto ON_EXIT;
  }

  //
  // Get the IP address of the target.
  //
  Field   = &Fields[RP_FIELD_IDX_SERVERNAME];

  if (ConfigNvData->NvmeofIpMode < IP_MODE_AUTOCONFIG) {
    IpMode = ConfigNvData->NvmeofIpMode;
  } else {
    IpMode = ConfigData->AutoConfigureMode;
  }

  //
  // Server name is expressed as domain name, just save it.
  //
  if ((!NET_IS_DIGIT (*(Field->Str))) && (*(Field->Str) != '[')) {
    ConfigNvData->NvmeofDnsMode = TRUE;
    if ((Field->Len + 2) > sizeof (ConfigNvData->ServerName)) {
      return EFI_INVALID_PARAMETER;
    }
    CopyMem (&ConfigNvData->ServerName, Field->Str, Field->Len);
    ConfigNvData->ServerName[Field->Len + 1] = '\0';
  } else {
    ConfigNvData->NvmeofDnsMode = FALSE;
    ZeroMem (ConfigNvData->ServerName, sizeof (ConfigNvData->ServerName));
    Status = NvmeOfAsciiStrToIp (Field->Str, IpMode, &Ip);
    CopyMem (&ConfigNvData->NvmeofSubSystemIp, &Ip, sizeof (EFI_IP_ADDRESS));

    if (EFI_ERROR (Status)) {
      goto ON_EXIT;
    }
  }
  //
  // Get the port of the NvmeOF target.
  //
  Field = &Fields[RP_FIELD_IDX_PORT];
  if (Field->Str != NULL) {
    ConfigNvData->NvmeofSubsysPortId = (UINT16) AsciiStrDecimalToUintn (Field->Str);
  } else {
    if (AsciiStriCmp (Fields[RP_FIELD_IDX_TARGETNAME].Str, NVMEOF_DISCOVERY_NQN) == 0) {
      ConfigNvData->NvmeofSubsysPortId = 8009;
    } else {
      ConfigNvData->NvmeofSubsysPortId = 4420;
    }
  }
  //
  // Get the NID.
  //
  Field = &Fields[RP_FIELD_IDX_NID];
  if (Field->Str != NULL) {
    AsciiStrCpyS (ConfigNvData->NvmeofSubsysNid, NVMEOF_NID_LEN, Field->Str);
  } else {
    ZeroMem (ConfigNvData->NvmeofSubsysNid, sizeof (ConfigNvData->NvmeofSubsysNid));
  }
  //
  // Get the target Name.
  //
  Field = &Fields[RP_FIELD_IDX_TARGETNAME];

  if (AsciiStrLen (Field->Str) > NVMEOF_NAME_MAX_SIZE - 1) {
    Status = EFI_INVALID_PARAMETER;
    goto ON_EXIT;
  }

  AsciiStrCpyS (ConfigNvData->NvmeofSubsysNqn, NVMEOF_NAME_MAX_SIZE, Field->Str);

ON_EXIT:

  FreePool (TmpStr);

  return Status;
}

/**
  The callback function registered to the DHCP4 instance that is used to select
  the qualified DHCP OFFER.

  @param[in]  This         The DHCP4 protocol.
  @param[in]  Context      The context set when configuring the DHCP4 protocol.
  @param[in]  CurrentState The current state of the DHCP4 protocol.
  @param[in]  Dhcp4Event   The event occurs in the current state.
  @param[in]  Packet       The DHCP packet that is to be sent or was already received.
  @param[out] NewPacket    The packet used to replace the above Packet.

  @retval EFI_SUCCESS      Either the DHCP OFFER is qualified or we're not intereseted
                           in the Dhcp4Event.
  @retval EFI_NOT_READY    The DHCP OFFER packet doesn't match our requirements.
  @retval Others           Other errors as indicated.

**/
EFI_STATUS
EFIAPI
NvmeOfDhcpSelectOffer (
  IN  EFI_DHCP4_PROTOCOL  *This,
  IN  VOID                *Context,
  IN  EFI_DHCP4_STATE     CurrentState,
  IN  EFI_DHCP4_EVENT     Dhcp4Event,
  IN  EFI_DHCP4_PACKET    *Packet, OPTIONAL
  OUT EFI_DHCP4_PACKET    **NewPacket OPTIONAL
  )
{
  EFI_STATUS              Status;
  UINT32                  OptionCount = 0;
  EFI_DHCP4_PACKET_OPTION **OptionList;
  UINT32                  Index = 0;
  UINT32                  Index1 = 0;
  UINT32                  Index2 = 0;
  CHAR8                   *RootPath = NULL;
  VOID                    *RootPathTmp = NULL;
  UINTN                   RootPathLen = 0;
  UINT8                   OptionLength;

  if ((Dhcp4Event != Dhcp4RcvdOffer) && (Dhcp4Event != Dhcp4SelectOffer)) {
    return EFI_SUCCESS;
  }

  OptionCount = 0;
  Status      = This->Parse (This, Packet, &OptionCount, NULL);
  if (Status != EFI_BUFFER_TOO_SMALL) {
    return EFI_NOT_READY;
  }

  OptionList = AllocatePool (OptionCount * sizeof (EFI_DHCP4_PACKET_OPTION *));
  if (OptionList == NULL) {
    return EFI_NOT_READY;
  }

  Status = This->Parse (This, Packet, &OptionCount, OptionList);
  if (EFI_ERROR (Status)) {
    Status = EFI_NOT_READY;
    goto Exit;
  }

  for (Index = 0; Index < OptionCount; Index++) {
    if (OptionList[Index]->OpCode == DHCP4_TAG_ROOTPATH) {
      RootPathTmp = ReallocatePool (
                      RootPathLen + 1,
                      RootPathLen + (UINTN) OptionList[Index]->Length + 1,
                      (VOID *) RootPath
                      );
      if (RootPathTmp == NULL) {
        Status = EFI_OUT_OF_RESOURCES;
        goto Exit;
      }
      RootPath = (CHAR8 *) RootPathTmp;

      Index1 = RootPathLen;
      OptionLength = OptionList[Index]->Length;
      RootPathLen += (UINTN) OptionList[Index]->Length;
      for (Index2=0; Index1 < (UINT32) RootPathLen && (Index2 < (UINT32) OptionLength);) {
        RootPath[Index1++] = OptionList[Index]->Data[Index2++];
      }
    }
  }

  if (RootPath == NULL) {
    Status = EFI_NOT_READY;
    goto Exit;
  }

  Status = NvmeOfDhcpExtractRootPath (
             RootPath,
             (UINT16) RootPathLen,
             (NVMEOF_ATTEMPT_CONFIG_NVDATA *) Context
             );

Exit:

  if (RootPath != NULL) {
    FreePool (RootPath);
  }
  FreePool (OptionList);
  return Status;
}

/**
  Parse the DHCP ACK to get the address configuration and DNS information.

  @param[in]       Dhcp4        The DHCP4 protocol.
  @param[in, out]  ConfigData   The session configuration data.

  @retval EFI_SUCCESS           The DNS information is got from the DHCP ACK.
  @retval EFI_NO_MAPPING        DHCP failed to acquire address and other information.
  @retval EFI_INVALID_PARAMETER The DHCP ACK's DNS option is malformatted.
  @retval EFI_DEVICE_ERROR      Other errors as indicated.
  @retval EFI_OUT_OF_RESOURCES  Failed to allocate memory.

**/
EFI_STATUS
NvmeOfParseDhcpAck (
  IN     EFI_DHCP4_PROTOCOL           *Dhcp4,
  IN OUT NVMEOF_ATTEMPT_CONFIG_NVDATA *ConfigData
  )
{
  EFI_STATUS                       Status;
  EFI_DHCP4_MODE_DATA              Dhcp4ModeData;
  UINT32                           OptionCount = 0;
  EFI_DHCP4_PACKET_OPTION          **OptionList;
  UINT32                           Index = 0;
  NVMEOF_SUBSYSTEM_CONFIG_NVDATA   *NvData;

  Status = Dhcp4->GetModeData (Dhcp4, &Dhcp4ModeData);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  if (Dhcp4ModeData.State != Dhcp4Bound) {
    return EFI_NO_MAPPING;
  }

  NvData = &ConfigData->SubsysConfigData;

  CopyMem (&NvData->NvmeofSubsysHostIP, &Dhcp4ModeData.ClientAddress, sizeof (EFI_IPv4_ADDRESS));
  CopyMem (&NvData->NvmeofSubsysHostSubnetMask, &Dhcp4ModeData.SubnetMask, sizeof (EFI_IPv4_ADDRESS));
  CopyMem (&NvData->NvmeofSubsysHostGateway, &Dhcp4ModeData.RouterAddress, sizeof (EFI_IPv4_ADDRESS));

  OptionCount = 0;
  OptionList  = NULL;

  Status      = Dhcp4->Parse (Dhcp4, Dhcp4ModeData.ReplyPacket, &OptionCount, OptionList);
  if (Status != EFI_BUFFER_TOO_SMALL) {
    return EFI_DEVICE_ERROR;
  }

  OptionList = AllocatePool (OptionCount * sizeof (EFI_DHCP4_PACKET_OPTION *));
  if (OptionList == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  Status = Dhcp4->Parse (Dhcp4, Dhcp4ModeData.ReplyPacket, &OptionCount, OptionList);
  if (EFI_ERROR (Status)) {
    FreePool (OptionList);
    return EFI_DEVICE_ERROR;
  }

  for (Index = 0; Index < OptionCount; Index++) {
    //
    // Get DNS server addresses and DHCP server address from this offer.
    //
    if (OptionList[Index]->OpCode == DHCP4_TAG_DNS_SERVER) {

      if (((OptionList[Index]->Length & 0x3) != 0) || (OptionList[Index]->Length == 0)) {
        Status = EFI_INVALID_PARAMETER;
        break;
      }
      //
      // Primary DNS server address.
      //
      CopyMem (&ConfigData->PrimaryDns, &OptionList[Index]->Data[0], sizeof (EFI_IPv4_ADDRESS));

      if (OptionList[Index]->Length > 4) {
        //
        // Secondary DNS server address.
        //
        CopyMem (&ConfigData->SecondaryDns, &OptionList[Index]->Data[4], sizeof (EFI_IPv4_ADDRESS));
      }
    } else if (OptionList[Index]->OpCode == DHCP4_TAG_SERVER_ID) {
      if (OptionList[Index]->Length != 4) {
        Status = EFI_INVALID_PARAMETER;
        break;
      }

      CopyMem (&ConfigData->DhcpServer, &OptionList[Index]->Data[0], sizeof (EFI_IPv4_ADDRESS));
    }
  }

  FreePool (OptionList);

  return Status;
}

/**
  This function will switch the IP4 configuration policy to Static.

  @param[in]  Ip4Config2          Pointer to the IP4 configuration protocol.

  @retval     EFI_SUCCESS         The policy is already configured to static.
  @retval     Others              Other error as indicated.

**/
EFI_STATUS
NvmeOfSetIp4Policy (
  IN EFI_IP4_CONFIG2_PROTOCOL        *Ip4Config2
  )
{
  EFI_IP4_CONFIG2_POLICY          Policy;
  EFI_STATUS                      Status;
  UINTN                           DataSize = 0;

  DataSize = sizeof (EFI_IP4_CONFIG2_POLICY);
  Status = Ip4Config2->GetData (
                         Ip4Config2,
                         Ip4Config2DataTypePolicy,
                         &DataSize,
                         &Policy
                         );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  if (Policy != Ip4Config2PolicyStatic) {
    Policy = Ip4Config2PolicyStatic;
    Status = Ip4Config2->SetData (
                          Ip4Config2,
                          Ip4Config2DataTypePolicy,
                          sizeof (EFI_IP4_CONFIG2_POLICY),
                          &Policy
                          );
    if (EFI_ERROR (Status)) {
      return Status;
    }
  }

  return EFI_SUCCESS;
}

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
  IN     EFI_HANDLE                    Image,
  IN     EFI_HANDLE                    Controller,
  IN OUT NVMEOF_ATTEMPT_CONFIG_NVDATA  *ConfigData
  )
{
  EFI_HANDLE                      Dhcp4Handle;
  EFI_IP4_CONFIG2_PROTOCOL        *Ip4Config2;
  EFI_DHCP4_PROTOCOL              *Dhcp4;
  EFI_STATUS                      Status;
  EFI_DHCP4_PACKET_OPTION         *ParaList;
  EFI_DHCP4_CONFIG_DATA           Dhcp4ConfigData;
  NVMEOF_SUBSYSTEM_CONFIG_NVDATA  *NvData;
  EFI_STATUS                      MediaStatus;

  Dhcp4Handle = NULL;
  Ip4Config2  = NULL;
  Dhcp4       = NULL;
  ParaList    = NULL;

  //
  // Check media status before doing DHCP.
  //
  MediaStatus = EFI_SUCCESS;
  NetLibDetectMediaWaitTimeout (Controller, NVMEOF_CHECK_MEDIA_GET_DHCP_WAITING_TIME, &MediaStatus);
  if (MediaStatus!= EFI_SUCCESS) {
    AsciiPrint ("\n  Error: Could not detect network connection.\n");
    return EFI_NO_MEDIA;
  }

  //
  // DHCP4 service allows only one of its children to be configured in
  // the active state, If the DHCP4 D.O.R.A started by IP4 auto
  // configuration and has not been completed, the Dhcp4 state machine
  // will not be in the right state for the NVMeOF to start a new round D.O.R.A.
  // So, we need to switch its policy to static.
  //
  Status = gBS->HandleProtocol (Controller, &gEfiIp4Config2ProtocolGuid, (VOID **) &Ip4Config2);
  if (!EFI_ERROR (Status)) {
    Status = NvmeOfSetIp4Policy (Ip4Config2);
    if (EFI_ERROR (Status)) {
      return Status;
    }
  }

  //
  // Create a DHCP4 child instance and get the protocol.
  //
  Status = NetLibCreateServiceChild (
             Controller,
             Image,
             &gEfiDhcp4ServiceBindingProtocolGuid,
             &Dhcp4Handle
             );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = gBS->OpenProtocol (
                  Dhcp4Handle,
                  &gEfiDhcp4ProtocolGuid,
                  (VOID **) &Dhcp4,
                  Image,
                  Controller,
                  EFI_OPEN_PROTOCOL_BY_DRIVER
                  );
  if (EFI_ERROR (Status)) {
    goto ON_EXIT;
  }

  NvData   = &ConfigData->SubsysConfigData;

  ParaList = AllocatePool (sizeof (EFI_DHCP4_PACKET_OPTION) + 3);
  if (ParaList == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    goto ON_EXIT;
  }

  //
  // Ask the server to reply with Netmask, Router, DNS, and RootPath options.
  //
  ParaList->OpCode  = DHCP4_TAG_PARA_LIST;
  ParaList->Length  = (UINT8) (NvData->NvmeofSubsysInfoDhcp ? 4 : 3);
  ParaList->Data[0] = DHCP4_TAG_NETMASK;
  ParaList->Data[1] = DHCP4_TAG_ROUTER;
  ParaList->Data[2] = DHCP4_TAG_DNS_SERVER;
  ParaList->Data[3] = DHCP4_TAG_ROOTPATH;

  ZeroMem (&Dhcp4ConfigData, sizeof (EFI_DHCP4_CONFIG_DATA));
  Dhcp4ConfigData.OptionCount = 1;
  Dhcp4ConfigData.OptionList  = &ParaList;

  if (NvData->NvmeofSubsysInfoDhcp) {
    //
    // Use callback to select an offer that contains target information.
    //
    Dhcp4ConfigData.Dhcp4Callback   = NvmeOfDhcpSelectOffer;
    Dhcp4ConfigData.CallbackContext = ConfigData;
  }

  Status = Dhcp4->Configure (Dhcp4, &Dhcp4ConfigData);
  if (EFI_ERROR (Status)) {
    goto ON_EXIT;
  }

  Status = Dhcp4->Start (Dhcp4, NULL);
  if (EFI_ERROR (Status)) {
    goto ON_EXIT;
  }

  //
  // Parse the ACK to get required information.
  //
  Status = NvmeOfParseDhcpAck (Dhcp4, ConfigData);

ON_EXIT:

  if (ParaList != NULL) {
    FreePool (ParaList);
  }

  if (Dhcp4 != NULL) {
    Dhcp4->Stop (Dhcp4);
    Dhcp4->Configure (Dhcp4, NULL);

    gBS->CloseProtocol (
           Dhcp4Handle,
           &gEfiDhcp4ProtocolGuid,
           Image,
           Controller
           );
  }

  NetLibDestroyServiceChild (
    Controller,
    Image,
    &gEfiDhcp4ServiceBindingProtocolGuid,
    Dhcp4Handle
    );

  return Status;
}

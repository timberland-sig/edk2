/** @file
  NVMeOF DHCP6 related configuration routines.

  Copyright (c) 2020, Dell EMC All rights reserved
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "NvmeOfImpl.h"

/**
  Extract the Root Path option and get the required target information from
  Boot File Uniform Resource Locator (URL) Option.

  @param[in]       RootPath      The RootPath string.
  @param[in]       Length        Length of the RootPath option payload.
  @param[in, out]  ConfigData    The target configuration data read from
                                 nonvolatile device.

  @retval EFI_SUCCESS            All required information is extracted from the
                                 RootPath option.
  @retval EFI_NOT_FOUND          The RootPath is not NVMeOF RootPath.
  @retval EFI_OUT_OF_RESOURCES   Failed to allocate memory.
  @retval EFI_INVALID_PARAMETER  The RootPath is malformatted.

**/
EFI_STATUS
NvmeOfDhcp6ExtractRootPath (
  IN     CHAR8                        *RootPath,
  IN     UINT16                       Length,
  IN OUT NVMEOF_ATTEMPT_CONFIG_NVDATA *ConfigData
  )
{
  EFI_STATUS                     Status = EFI_SUCCESS;
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
  ConfigNvData->NvmeofDnsMode = FALSE;
  //
  // "NVME<+PROTOCOL>://<SERVERNAME/IP>[:TRANSPORT PORT]/<SUBSYS NQN>/<NID>"
  //
  RootPathIdLen = sizeof (NVMEOF_ROOT_PATH_ID) - 1;
  ConvertStrnAsciiCharLowerToUpper (RootPath, RootPathIdLen);

  if ((Length <= RootPathIdLen) ||
      (CompareMem (RootPath, NVMEOF_ROOT_PATH_ID, RootPathIdLen) != 0)) {
    return EFI_NOT_FOUND;
  }

  NvmeOfSaveRootPathForNbft (RootPath, Length);
  //
  // Skip the NVMeOF RootPath ID "NVME+".
  //
  RootPath = RootPath + RootPathIdLen;
  Length   = (UINT16) (Length - RootPathIdLen);

  TmpStr   = (CHAR8 *) AllocatePool (Length + 1);
  if (TmpStr == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  CopyMem (TmpStr, RootPath, Length);
  TmpStr[Length]  = '\0';

  Index           = 0;
  FieldIndex      = 0;
  ZeroMem (&Fields[0], sizeof (Fields));

  // Extract PROTOCOL
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

  //
  // Extract SERVERNAME field in the Root Path option.
  //
  if (TmpStr[Index] != NVMEOF_ROOT_PATH_ADDR_START_DELIMITER) {
    //
    // The servername is expressed as domain name.
    //
    ConfigNvData->NvmeofDnsMode = TRUE;
  } else {
    Index++;
  }

  Fields[RP_FIELD_IDX_SERVERNAME].Str = &TmpStr[Index];

  if (!ConfigNvData->NvmeofDnsMode) {
    while ((TmpStr[Index] != NVMEOF_ROOT_PATH_ADDR_END_DELIMITER) && (Index < Length)) {
      Index++;
    }

    //
    // Skip ']'
    //
    TmpStr[Index] = '\0';
    Index++;
  } else {
    while ((TmpStr[Index] != NVMEOF_ROOT_PATH_PORT_START_DELIMITER) &&
      (TmpStr[Index] != NVMEOF_ROOT_PATH_FIELD_DELIMITER) &&
      (Index < Length)) {
      Index++;
    }
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
    Fields[RP_FIELD_IDX_SERVERNAME].Len = (UINT8)AsciiStrLen(Fields[RP_FIELD_IDX_SERVERNAME].Str);
  }

  //
  // Extract others fields in the Root Path option string.
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

    if ((TmpStr[Index] == NVMEOF_ROOT_PATH_FIELD_DELIMITER) || (Index == Length)) {
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
  if (ConfigNvData->NvmeofDnsMode) {
    if ((Field->Len + 2) > sizeof (ConfigNvData->ServerName)) {
      return EFI_INVALID_PARAMETER;
    }
    CopyMem (&ConfigNvData->ServerName, Field->Str, Field->Len);
    ConfigNvData->ServerName[Field->Len + 1] = '\0';
  } else {
    ZeroMem (&ConfigNvData->ServerName, sizeof (ConfigNvData->ServerName));
    Status = NvmeOfAsciiStrToIp (Field->Str, IpMode, &Ip);
    CopyMem (&ConfigNvData->NvmeofSubSystemIp, &Ip, sizeof (EFI_IP_ADDRESS));

    if (EFI_ERROR (Status)) {
      goto ON_EXIT;
    }
  }
  //
  // Get the port of the Nvmeof target.
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

  //Get the NID.
  Field = &Fields[RP_FIELD_IDX_NID];
  if (Field->Str != NULL) {
    AsciiStrCpyS (ConfigNvData->NvmeofSubsysNid, NVMEOF_NID_LEN, Field->Str);
  } else {
    ZeroMem (ConfigNvData->NvmeofSubsysNid, sizeof (ConfigNvData->NvmeofSubsysNid));
  }
  //
  // Get the target Subsystem Name.
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
  EFI_DHCP6_INFO_CALLBACK is provided by the consumer of the EFI DHCPv6 Protocol
  instance to intercept events that occurs in the DHCPv6 Information Request
  exchange process.

  @param[in]  This              Pointer to the EFI_DHCP6_PROTOCOL instance that
                                is used to configure this  callback function.
  @param[in]  Context           Pointer to the context that is initialized in
                                the EFI_DHCP6_PROTOCOL.InfoRequest().
  @param[in]  Packet            Pointer to Reply packet that has been received.
                                The EFI DHCPv6 Protocol instance is responsible
                                for freeing the buffer.

  @retval EFI_SUCCESS           Tell the EFI DHCPv6 Protocol instance to finish
                                Information Request exchange process.
  @retval EFI_NOT_READY         Tell the EFI DHCPv6 Protocol instance to continue
                                Information Request exchange process.
  @retval EFI_ABORTED           Tell the EFI DHCPv6 Protocol instance to abort
                                the Information Request exchange process.
  @retval EFI_UNSUPPORTED       Tell the EFI DHCPv6 Protocol instance to finish
                                the Information Request exchange process because some
                                request information are not received.

**/
EFI_STATUS
EFIAPI
NvmeOfDhcp6ParseReply (
  IN EFI_DHCP6_PROTOCOL          *This,
  IN VOID                        *Context,
  IN EFI_DHCP6_PACKET            *Packet
  )
{
  EFI_STATUS                    Status;
  UINT32                        Index = 0;
  UINT32                        OptionCount = 0;
  EFI_DHCP6_PACKET_OPTION       *BootFileOpt;
  EFI_DHCP6_PACKET_OPTION       **OptionList;
  NVMEOF_ATTEMPT_CONFIG_NVDATA  *ConfigData;

  BootFileOpt = NULL;

  Status      = This->Parse (This, Packet, &OptionCount, NULL);
  if (Status != EFI_BUFFER_TOO_SMALL) {
    return EFI_NOT_READY;
  }

  OptionList = AllocateZeroPool (OptionCount * sizeof (EFI_DHCP6_PACKET_OPTION *));
  if (OptionList == NULL) {
    return EFI_NOT_READY;
  }

  Status = This->Parse (This, Packet, &OptionCount, OptionList);
  if (EFI_ERROR (Status)) {
    Status = EFI_NOT_READY;
    goto Exit;
  }

  ConfigData = (NVMEOF_ATTEMPT_CONFIG_NVDATA *) Context;

  for (Index = 0; Index < OptionCount; Index++) {
    OptionList[Index]->OpCode = NTOHS (OptionList[Index]->OpCode);
    OptionList[Index]->OpLen  = NTOHS (OptionList[Index]->OpLen);

    //
    // Get DNS server addresses from this reply packet.
    //
    if (OptionList[Index]->OpCode == DHCP6_OPT_DNS_SERVERS) {

      if (((OptionList[Index]->OpLen & 0xf) != 0) || (OptionList[Index]->OpLen == 0)) {
        Status = EFI_UNSUPPORTED;
        goto Exit;
      }
      //
      // Primary DNS server address.
      //
      CopyMem (&ConfigData->PrimaryDns, &OptionList[Index]->Data[0], sizeof (EFI_IPv6_ADDRESS));

      if (OptionList[Index]->OpLen > 16) {
        //
        // Secondary DNS server address
        //
        CopyMem (&ConfigData->SecondaryDns, &OptionList[Index]->Data[16], sizeof (EFI_IPv6_ADDRESS));
      }
    } else if (OptionList[Index]->OpCode == DHCP6_OPT_VENDOR_OPTS) {
      // The server sends this option to inform the client about an URL to a boot file.
      //
      BootFileOpt = OptionList[Index];
    }
  }

  if (BootFileOpt == NULL) {
    Status = EFI_UNSUPPORTED;
    goto Exit;
  }

  //
  // Get root path from Boot File Uniform Resource Locator (URL) Option
  //
  Status = NvmeOfDhcp6ExtractRootPath (
             (CHAR8 *) BootFileOpt->Data,
             BootFileOpt->OpLen,
             ConfigData
             );

Exit:

  FreePool (OptionList);
  return Status;
}


/**
  Parse the DHCP ACK to get the address configuration and DNS information.

  @param[in]       Image         The handle of the driver image.
  @param[in]       Controller    The handle of the controller;
  @param[in, out]  ConfigData    The attempt configuration data.

  @retval EFI_SUCCESS            The DNS information is got from the DHCP ACK.
  @retval EFI_NO_MAPPING         DHCP failed to acquire address and other
                                 information.
  @retval EFI_INVALID_PARAMETER  The DHCP ACK's DNS option is malformatted.
  @retval EFI_DEVICE_ERROR       Some unexpected error occurred.
  @retval EFI_OUT_OF_RESOURCES   There is no sufficient resource to finish the
                                 operation.
  @retval EFI_NO_MEDIA           There was a media error.

**/
EFI_STATUS
NvmeOfDoDhcp6 (
  IN     EFI_HANDLE                    Image,
  IN     EFI_HANDLE                    Controller,
  IN OUT NVMEOF_ATTEMPT_CONFIG_NVDATA  *ConfigData
  )
{
  EFI_HANDLE                Dhcp6Handle;
  EFI_DHCP6_PROTOCOL        *Dhcp6;
  EFI_STATUS                Status;
  EFI_STATUS                TimerStatus;
  EFI_DHCP6_PACKET_OPTION   *Oro;
  EFI_DHCP6_RETRANSMISSION  InfoReqReXmit;
  EFI_EVENT                 Timer;
  EFI_STATUS                MediaStatus;

  //
  // Check media status before doing DHCP.
  //
  MediaStatus = EFI_SUCCESS;
  NetLibDetectMediaWaitTimeout (Controller, NVMEOF_CHECK_MEDIA_GET_DHCP_WAITING_TIME, &MediaStatus);
  if (MediaStatus != EFI_SUCCESS) {
    AsciiPrint ("\n  Error: Could not detect network connection.\n");
    return EFI_NO_MEDIA;
  }

  //
  // NNMeOF will only request target info from DHCPv6 server.
  //
  if (!ConfigData->SubsysConfigData.NvmeofSubsysInfoDhcp) {
    return EFI_SUCCESS;
  }

  Dhcp6Handle = NULL;
  Dhcp6       = NULL;
  Oro         = NULL;
  Timer       = NULL;

  //
  // Create a DHCP6 child instance and get the protocol.
  //
  Status = NetLibCreateServiceChild (
             Controller,
             Image,
             &gEfiDhcp6ServiceBindingProtocolGuid,
             &Dhcp6Handle
             );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = gBS->OpenProtocol (
                  Dhcp6Handle,
                  &gEfiDhcp6ProtocolGuid,
                  (VOID **) &Dhcp6,
                  Image,
                  Controller,
                  EFI_OPEN_PROTOCOL_BY_DRIVER
                  );
  if (EFI_ERROR (Status)) {
    goto ON_EXIT;
  }

  Oro = AllocateZeroPool (sizeof (EFI_DHCP6_PACKET_OPTION) + 5);
  if (Oro == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    goto ON_EXIT;
  }

  //
  // Ask the server to reply with DNS and Boot File URL options by info request.
  // All members in EFI_DHCP6_PACKET_OPTION are in network order.
  //
  Oro->OpCode  = HTONS (DHCP6_OPT_ORO);
  Oro->OpLen   = HTONS (2 * 3);
  Oro->Data[1] = DHCP6_OPT_DNS_SERVERS;
  Oro->Data[3] = DHCP6_OPT_VENDOR_OPTS;

  InfoReqReXmit.Irt = 4;
  InfoReqReXmit.Mrc = 1;
  InfoReqReXmit.Mrt = 10;
  InfoReqReXmit.Mrd = 30;

  Status = Dhcp6->InfoRequest (
                    Dhcp6,
                    TRUE,
                    Oro,
                    0,
                    NULL,
                    &InfoReqReXmit,
                    NULL,
                    NvmeOfDhcp6ParseReply,
                    ConfigData
                    );
  if (Status == EFI_NO_MAPPING) {
    Status = gBS->CreateEvent (EVT_TIMER, TPL_CALLBACK, NULL, NULL, &Timer);
    if (EFI_ERROR (Status)) {
      goto ON_EXIT;
    }

    Status = gBS->SetTimer (
                    Timer,
                    TimerRelative,
                    NVMEOF_GET_MAPPING_TIMEOUT
                    );

    if (EFI_ERROR (Status)) {
      goto ON_EXIT;
    }

    do {
      TimerStatus = gBS->CheckEvent (Timer);
      if (!EFI_ERROR (TimerStatus)) {
        Status = Dhcp6->InfoRequest (
                          Dhcp6,
                          TRUE,
                          Oro,
                          0,
                          NULL,
                          &InfoReqReXmit,
                          NULL,
                          NvmeOfDhcp6ParseReply,
                          ConfigData
                          );
      }
    } while (TimerStatus == EFI_NOT_READY);
  }

ON_EXIT:

  if (Oro != NULL) {
    FreePool (Oro);
  }

  if (Timer != NULL) {
    gBS->CloseEvent (Timer);
  }

  if (Dhcp6 != NULL) {
    gBS->CloseProtocol (
           Dhcp6Handle,
           &gEfiDhcp6ProtocolGuid,
           Image,
           Controller
           );
  }

  NetLibDestroyServiceChild (
    Controller,
    Image,
    &gEfiDhcp6ServiceBindingProtocolGuid,
    Dhcp6Handle
    );

  return Status;
}


/** @file
  Implementation for NVMeOF Boot Firmware Table publication.

  Copyright (c) 2020, Dell EMC All rights reserved
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "NvmeOfImpl.h"
#include "NvmeOfDriver.h"
#include "NvmeOfNbft.h"
#include "spdk/nvme.h"
#include "nvme_internal.h"
#include "spdk/string.h"

BOOLEAN                     gNbftInstalled = FALSE;
UINTN                       gTableKey;
NVMEOF_NBFT_HEAP            gNbftHeap;
LIST_ENTRY                  gAddedAdaptersList;
NVMEOF_PROCESSED_NAMESPACE  gProcessedNamespaceList;
NVMEOF_PROCESSED_IP_ADDR    gProcessedIpConfigList;
NVMEOF_GLOBAL_DATA          *NvmeOfData;
extern EFI_HANDLE           mImageHandler;
extern CHAR8                *gNvmeOfImagePath;

/**
  Add one item into the heap.

  @param[in, out]  Heap  On input, the current address of the heap. On output, the address of
                         the heap after the item is added.
  @param[in]       Data  The data to add into the heap.
  @param[in]       Len   Length of the Data in byte.

**/
VOID
NvmeOfAddHeapItem (
  IN OUT UINT8  **Heap,
  IN     VOID   *Data,
  IN     UINTN  Len
  )
{
  if (Len != 0) {
    CopyMem (*Heap, Data, Len);
    *(*Heap + Len) = 0;

    *Heap += Len + 1;

    gNbftHeap.Length += Len + 1;
  }
}

/**
  Copy to the original heap and adjust offsets in the structures.

  @param[in]  Table       The ACPI table.
  @param[in]  HostOnly    Flag indicating only Host structure being populated to ACPI.

**/
VOID
NvmeOfFillHeapOffsets (
  IN  EFI_ACPI_NVMEOF_BFT_HEADER  *Table,
  IN  BOOLEAN                     HostOnly  
  )
{
  EFI_ACPI_NVMEOF_BFT_CONTROL_STRUCTURE                  *Control;
  EFI_ACPI_NVMEOF_BFT_HOST_DESCRIPTOR                    *Host;
  EFI_ACPI_NVMEOF_BFT_HFI_HEADER_DESCRIPTOR              *HfiHeader;
  EFI_ACPI_NVMEOF_BFT_HFI_TRANSPORT_INFO_DESCRIPTOR_TCP  *HfiTcpTransportInfo;
  EFI_ACPI_NVMEOF_BFT_SUBSYSTEM_NAMESPACE_DESCRIPTOR     *SubsystemNamespace;
  EFI_ACPI_NVMEOF_BFT_DISCOVERY_DESCRIPTOR               *Discovery;
  EFI_ACPI_NVMEOF_BFT_SUBSYSTEM_EXT_INFO_DESCRIPTOR      *SsnsExtInfo;

  UINT32  TableSize = 0;
  UINT8   *ActualHeap = NULL;
  UINT8   *HeapStart = NULL;
  UINT8   Index = 0;
  UINT8   RecordSeperator = 1;

  Control = (EFI_ACPI_NVMEOF_BFT_CONTROL_STRUCTURE *)(Table + 1);
  Discovery = (EFI_ACPI_NVMEOF_BFT_DISCOVERY_DESCRIPTOR *)((UINT8 *)Table + Control->DiscoDescOffset);


  TableSize = NBFT_ROUNDUP (sizeof (EFI_ACPI_NVMEOF_BFT_HEADER)) +
  NBFT_ROUNDUP (sizeof (EFI_ACPI_NVMEOF_BFT_CONTROL_STRUCTURE)) +
  NBFT_ROUNDUP (sizeof (EFI_ACPI_NVMEOF_BFT_HOST_DESCRIPTOR)) +
  (Control->NumHfis * NBFT_ROUNDUP (sizeof (EFI_ACPI_NVMEOF_BFT_HFI_HEADER_DESCRIPTOR))) +
  (Control->NumNamespaces * NBFT_ROUNDUP (sizeof (EFI_ACPI_NVMEOF_BFT_SUBSYSTEM_NAMESPACE_DESCRIPTOR))) +
  (Control->NumSecurityProfiles * NBFT_ROUNDUP (sizeof (EFI_ACPI_NVMEOF_BFT_SECURITY_DESCRIPTOR))) +
  (Control->NumDiscoveryEntires * NBFT_ROUNDUP (sizeof (EFI_ACPI_NVMEOF_BFT_DISCOVERY_DESCRIPTOR)));

  HeapStart = ActualHeap = (UINT8 *)Table + TableSize;

  // Copy the data to actual heap offset
  CopyMem (ActualHeap, gNbftHeap.Heap, gNbftHeap.Length);

  // Adjust the length and heap offset for Header structure
  Table->Length = TableSize + gNbftHeap.Length;
  Table->HeapOffset = (UINT32)((UINTN)ActualHeap - (UINTN)Table);

  // Adjust the heap offset for Header structure
  if (Table->DrvDevPathSignatureLength > 0) {
    Table->DrvDevPathSignatureOffset = (UINT32)((UINTN)ActualHeap - (UINTN)Table);
    ActualHeap = (UINT8 *)ActualHeap + Table->DrvDevPathSignatureLength + RecordSeperator;
  } else {
    Table->DrvDevPathSignatureOffset = (UINT32)0;
  }
  // Adjust the heap offset for Host structure
  Host = (EFI_ACPI_NVMEOF_BFT_HOST_DESCRIPTOR *)((UINT8 *)Table + Control->HostDescOffset);
  if (Host->HostNqnLen > 0) {
    Host->HostNqnOffset = (UINT32)((UINTN)ActualHeap - (UINTN)Table);
    ActualHeap = (UINT8 *)ActualHeap + Host->HostNqnLen + RecordSeperator;
  } else {
    Host->HostNqnOffset = (UINT32)0;
  }

  // Adjust the heap offset for HFI header and transport Info structure
  HfiHeader = (EFI_ACPI_NVMEOF_BFT_HFI_HEADER_DESCRIPTOR *)((UINT8 *)Table + Control->HfiDescOffset);
  for (Index = 0; Index < Control->NumHfis; Index++) {
    // Host Info structure offset
    if (HfiHeader->InfoStructureLen > 0) {
      HfiHeader->InfoStructureOffset = (UINT32)((UINTN)ActualHeap - (UINTN)Table);
      ActualHeap = (UINT8 *)ActualHeap + HfiHeader->InfoStructureLen + RecordSeperator;

      HfiTcpTransportInfo =
        (EFI_ACPI_NVMEOF_BFT_HFI_TRANSPORT_INFO_DESCRIPTOR_TCP *)((UINT8 *)Table + HfiHeader->InfoStructureOffset);

      if (HfiTcpTransportInfo->HostNameLength > 0) {
        HfiTcpTransportInfo->HostNameOffset = (UINT32)((UINTN)ActualHeap - (UINTN)Table);
        ActualHeap = (UINT8 *)ActualHeap + HfiTcpTransportInfo->HostNameLength + RecordSeperator;
      } else {
        HfiTcpTransportInfo->HostNameOffset = (UINT32)0;
      }
    } else {
      HfiHeader->InfoStructureOffset = (UINT32)0;
    }

    HfiHeader = (EFI_ACPI_NVMEOF_BFT_HFI_HEADER_DESCRIPTOR *)((UINT8 *)HfiHeader +
      NBFT_ROUNDUP (sizeof (EFI_ACPI_NVMEOF_BFT_HFI_HEADER_DESCRIPTOR)));
  }

  // Adjust the heap offset for subsystem structure
  SubsystemNamespace =
    (EFI_ACPI_NVMEOF_BFT_SUBSYSTEM_NAMESPACE_DESCRIPTOR *)((UINT8 *)Table + Control->SubSytemDescOffset);
  for (Index = 0; Index < Control->NumNamespaces; Index++) {

    if (SubsystemNamespace->SubsystemTransportAdressLength > 0) {
      SubsystemNamespace->SubsystemTransportAdressOffset = (UINT32)((UINTN)ActualHeap - (UINTN)Table);
      ActualHeap = (UINT8 *)ActualHeap + SubsystemNamespace->SubsystemTransportAdressLength + RecordSeperator;
    } else {
      SubsystemNamespace->SubsystemTransportAdressOffset = (UINT32)0;
    }

    if (SubsystemNamespace->SubsystemTransportServiceIdLength > 0) {
      SubsystemNamespace->SubsystemTransportServiceIdOffset = (UINT32)((UINTN)ActualHeap - (UINTN)Table);
      ActualHeap = (UINT8 *)ActualHeap + SubsystemNamespace->SubsystemTransportServiceIdLength + RecordSeperator;
    } else {
      SubsystemNamespace->SubsystemTransportServiceIdOffset = (UINT32)0;
    }

    if (SubsystemNamespace->HfiAssociationLen > 0) {
      SubsystemNamespace->HfiAssociationOffset = (UINT32)((UINTN)ActualHeap - (UINTN)Table);
      ActualHeap = (UINT8 *)ActualHeap + SubsystemNamespace->HfiAssociationLen + RecordSeperator;
    } else {
      SubsystemNamespace->HfiAssociationOffset = (UINT32)0;
    }

    if (SubsystemNamespace->SubsystemNamespaceNqnLen > 0) {
      SubsystemNamespace->SubsystemNamespaceNqnOffset = (UINT32)((UINTN)ActualHeap - (UINTN)Table);
      ActualHeap = (UINT8 *)ActualHeap + SubsystemNamespace->SubsystemNamespaceNqnLen + RecordSeperator;
    } else {
      SubsystemNamespace->SubsystemNamespaceNqnOffset = (UINT16)0;
    }

    if (SubsystemNamespace->SsnsExtendedInfoLength > 0) {
    SubsystemNamespace->SsnsExtendedInfoOffset = (UINT32)((UINTN)ActualHeap - (UINTN)Table);
    ActualHeap = (UINT8 *)ActualHeap + SubsystemNamespace->SsnsExtendedInfoLength + RecordSeperator;
      SsnsExtInfo = 
        (EFI_ACPI_NVMEOF_BFT_SUBSYSTEM_EXT_INFO_DESCRIPTOR  *)((UINT8 *)Table + SubsystemNamespace->SsnsExtendedInfoOffset);
      if (SsnsExtInfo->DhcpRootPathLength > 0) {
        SsnsExtInfo->DhcpRootPathOffset = (UINT32)((UINTN)ActualHeap - (UINTN)Table);
        ActualHeap = (UINT8 *)ActualHeap + SsnsExtInfo->DhcpRootPathLength + RecordSeperator;
      } else {
        SsnsExtInfo->DhcpRootPathOffset = (UINT32)0;
      } 
    } else {
      SubsystemNamespace->SsnsExtendedInfoOffset = (UINT32)0;
    }
    
    SubsystemNamespace =
      (EFI_ACPI_NVMEOF_BFT_SUBSYSTEM_NAMESPACE_DESCRIPTOR *)((UINT8 *)SubsystemNamespace +
        NBFT_ROUNDUP (sizeof (EFI_ACPI_NVMEOF_BFT_SUBSYSTEM_NAMESPACE_DESCRIPTOR)));
  }

  // Adjust the heap offset for discovery structure
  if (!HostOnly) {
    for (Index = 0; Index < Control->NumDiscoveryEntires; Index++) {
      if (Discovery->DiscoveryCtrlrAddrLen > 0) {
        Discovery->DiscoveryCtrlrAddrOffset = (UINT32)((UINTN)ActualHeap - (UINTN)Table);
        ActualHeap = (UINT8 *)ActualHeap + Discovery->DiscoveryCtrlrAddrLen + RecordSeperator;
      } else {
        Discovery->DiscoveryCtrlrAddrOffset = (UINT32)0;
      }

      if (Discovery->DiscoveryCtrlrNqnLen > 0) {
        Discovery->DiscoveryCtrlrNqnOffset = (UINT32)((UINTN)ActualHeap - (UINTN)Table);
        ActualHeap = (UINT8 *)ActualHeap + Discovery->DiscoveryCtrlrNqnLen + RecordSeperator;
      } else {
        Discovery->DiscoveryCtrlrNqnOffset = (UINT32)0;
      }

      Discovery = (EFI_ACPI_NVMEOF_BFT_DISCOVERY_DESCRIPTOR *)((UINT8 *)Discovery +
       NBFT_ROUNDUP (sizeof (EFI_ACPI_NVMEOF_BFT_DISCOVERY_DESCRIPTOR)));
    }
  }

  // Update heap length in header structure
  Table->HeapLength = (UINT32)((UINTN)ActualHeap - (UINTN)HeapStart);
}

/**
  Initialize the header of the NVMeOF Boot Firmware Table.

  @param[out]  Header      The header of the NVMeOF Boot Firmware Table.
  @param[in]   OemId       The OEM ID.
  @param[in]   OemTableId  The OEM table ID for the nBFT.

**/
VOID
NvmeOfInitIbfTableHeader (
  OUT EFI_ACPI_NVMEOF_BFT_HEADER  *Header,
  IN  UINT8                       *OemId,
  IN  UINT64                      *OemTableId,
  IN OUT UINT8                    **Heap
  )
{
  UINT16      Length;

  Header->Signature      = EFI_ACPI_3_0_NVMEOF_BFT_SIGNATURE;
  Header->Length         = sizeof (EFI_ACPI_NVMEOF_BFT_HEADER);
  Header->Revision       = EFI_ACPI_NVMEOF_BFT_REVISION;
  Header->MinorRevision  = EFI_ACPI_NVMEOF_BFT_MINOR_REVISION;
  Header->Checksum       = 0;

  CopyMem (Header->OemId, OemId, sizeof (Header->OemId));
  CopyMem (&Header->OemTableId, OemTableId, sizeof (UINT64));

  Header->CreatorId = PcdGet32 (PcdAcpiDefaultCreatorId);;
  Header->CreatorRevision = PcdGet32 (PcdAcpiDefaultCreatorRevision);
  Header->HeapLength = 0;
  Header->HeapOffset = 0;
  Header->DrvDevPathSignatureOffset = 0;
  Header->DrvDevPathSignatureLength = 0;
  if(gNvmeOfImagePath != NULL) {
    Length = AsciiStrLen (gNvmeOfImagePath);
    NvmeOfAddHeapItem (Heap, gNvmeOfImagePath, Length);
    Header->DrvDevPathSignatureLength = Length;
  }
}


/**
  Initialize the control section of the NVMeOF Boot Firmware Table.

  @param[in]  Table       The ACPI table.

**/
VOID
NvmeOfInitControlSection (
  IN EFI_ACPI_NVMEOF_BFT_HEADER  *Table
  )
{
  EFI_ACPI_NVMEOF_BFT_CONTROL_STRUCTURE  *Control;

  Control = (EFI_ACPI_NVMEOF_BFT_CONTROL_STRUCTURE *) (Table + 1);

  Control->StructureId       = EFI_ACPI_NVMEOF_BFT_CONTROL_STRUCTURE_ID;
  Control->MajorRevision     = EFI_ACPI_NVMEOF_BFT_CONTROL_STRUCTURE_VERSION;
  Control->MinorRevision     = EFI_ACPI_NVMEOF_BFT_CONTROL_STRUCTURE_MINOR_VERSION;
  Control->Length            = (UINT16) sizeof (EFI_ACPI_NVMEOF_BFT_CONTROL_STRUCTURE);
  Control->Flags             = EFI_ACPI_NVMEOF_BFT_CONTROL_STRUCTURE_FLAG_BLOCK_VALID;

  Control->NumHfis = 0;
  Control->NumNamespaces = 0;
  Control->NumSecurityProfiles = 0;
  Control->NumDiscoveryEntires = 0;
}

/**
  Fill the host section of the NVMeOF Boot Firmware Table.

  @param[in]       Table  The ACPI table.
  @param[in, out]  Heap   The heap.

**/
VOID
NvmeOfFillHostSection (
  IN     EFI_ACPI_NVMEOF_BFT_HEADER  *Table,
  IN OUT UINT8                       **Heap
  )
{
  EFI_ACPI_NVMEOF_BFT_CONTROL_STRUCTURE  *Control;
  EFI_ACPI_NVMEOF_BFT_HOST_DESCRIPTOR     *Host;
  UINTN                                  NvmeOfDataSize = 0;
  UINT16                                 Length;

  Control = (EFI_ACPI_NVMEOF_BFT_CONTROL_STRUCTURE *)(Table + 1);

  //
  // Host section immediately follows the control section.
  //
  Host = (EFI_ACPI_NVMEOF_BFT_HOST_DESCRIPTOR *)
    ((UINT8 *)Table  + NBFT_ROUNDUP (Table->Length) + NBFT_ROUNDUP (Control->Length));

  Control->HostDescOffset      = (UINT32)((UINTN)Host - (UINTN)Table);
  Host->StructureId            = EFI_ACPI_NVMEOF_BFT_HOST_DESCRIPTOR_ID;
  Control->HostDescVersion     = EFI_ACPI_NVMEOF_BFT_HOST_DESCRIPTOR_VERSION;
  Control->HostDescLength      = (UINT16) sizeof (EFI_ACPI_NVMEOF_BFT_HOST_DESCRIPTOR);
  Host->Flags                  = EFI_ACPI_NVMEOF_BFT_HOST_DESCRIPTOR_FLAG_BLOCK_VALID| \
    EFI_ACPI_NVMEOF_BFT_HOSTID_CONFIGURED|EFI_ACPI_NVMEOF_BFT_HOSTNQN_CONFIGURED;

  // Read the host data from UEFI variable
  NvmeOfData = NvmeOfGetVariableAndSize (
                 L"NvmeofGlobalData",
                 &gNvmeOfConfigGuid,
                 &NvmeOfDataSize
                 );
  if (NvmeOfData == NULL || NvmeOfDataSize == 0) {
    DEBUG ((EFI_D_ERROR, "NvmeOfData Read Failed\n"));
    return;
  }

  // Host identifier in UUID format
  CopyMem (Host->HostIdentifier, NvmeOfData->NvmeofHostId, sizeof (Host->HostIdentifier));

  //
  // Fill the NVMeOF Host NQN into the heap.
  //
  Length = (UINT16)AsciiStrLen (NvmeOfData->NvmeofHostNqn);
  NvmeOfAddHeapItem (Heap, NvmeOfData->NvmeofHostNqn, Length);
  Host->HostNqnLen = Length;
}

/**
  Map the v4 IP address into v6 IP address.

  @param[in]   V4 The v4 IP address.
  @param[out]  V6 The v6 IP address.

**/
VOID
NvmeOfMapV4ToV6Addr (
  IN  EFI_IPv4_ADDRESS *V4,
  OUT EFI_IPv6_ADDRESS *V6
  )
{
  UINTN Index;

  ZeroMem (V6, sizeof (EFI_IPv6_ADDRESS));

  V6->Addr[10]  = 0xff;
  V6->Addr[11]  = 0xff;

  for (Index = 0; Index < 4; Index++) {
    V6->Addr[12 + Index] = V4->Addr[Index];
  }
}

/**
  Fill the Host Fabric Interface sections in NVMeOF Boot Firmware Table.

  @param[in]       Table    The buffer of the ACPI table.
  @param[in, out]  Heap     The heap buffer used to store the variable length
                            parameters such as NVMeOF name.

**/
VOID
NvmeOfFillHostFabricInterfaceSections (
  IN     EFI_ACPI_NVMEOF_BFT_HEADER  *Table,
  IN OUT UINT8                       **Heap
  )
{
  EFI_ACPI_NVMEOF_BFT_CONTROL_STRUCTURE                  *Control;
  EFI_ACPI_NVMEOF_BFT_HFI_HEADER_DESCRIPTOR              *HfiHeader;
  EFI_ACPI_NVMEOF_BFT_HFI_TRANSPORT_INFO_DESCRIPTOR_TCP  HfiTcpTransportInfo = { 0 };
  NVMEOF_SUBSYSTEM_CONFIG_NVDATA                         *NvData;
  UINTN                                                  AdapterIndex = 0;
  UINTN                                                  Index;
  LIST_ENTRY                                             *Entry;
  LIST_ENTRY                                             *NextEntry;
  NVMEOF_ATTEMPT_ENTRY                                   *AttemptEntry;
  NVMEOF_ATTEMPT_CONFIG_NVDATA                           *Attempt;
  NVMEOF_NIC_INFO                                        *NicInfo;
  CHAR16                                                 AttemptMacString[NVMEOF_MAX_MAC_STRING_LEN] = { 0 };
  CHAR16                                                 MacString[NVMEOF_MAX_MAC_STRING_LEN];
  BOOLEAN                                                AlreadyProcessed;
  LIST_ENTRY                                             *ProcessedEntry;
  LIST_ENTRY                                             *NextEntryProcessed;
  NVMEOF_PROCESSED_MAC                                   *ProcessedAdapter;
  UINT8                                                  **HostOverridesHeapRef;
  UINT8                                                  NvmeofHostNqnOverrideTmp[NVMEOF_NAME_MAX_SIZE];

  //
  // Get the offset of the first hfi section.
  //
  Control   = (EFI_ACPI_NVMEOF_BFT_CONTROL_STRUCTURE *)(Table + 1);
  HfiHeader = (EFI_ACPI_NVMEOF_BFT_HFI_HEADER_DESCRIPTOR *)((UINTN)Table +
    Control->HostDescOffset + NBFT_ROUNDUP (sizeof (EFI_ACPI_NVMEOF_BFT_HOST_DESCRIPTOR)));

 // Fill the offset of 1st hfi header structure in control structure
  Control->HfiDescOffset = (UINT16)((UINTN)HfiHeader - (UINTN)Table);

  NET_LIST_FOR_EACH_SAFE (Entry, NextEntry, &mNicPrivate->AttemptConfigs) {
    AttemptEntry = NET_LIST_USER_STRUCT (Entry, NVMEOF_ATTEMPT_ENTRY, Link);
    Attempt = &AttemptEntry->Data;
    NvData = &Attempt->SubsysConfigData;
    SetMem (NvmeofHostNqnOverrideTmp, sizeof (NvmeofHostNqnOverrideTmp), 0);
    // Check if start invoked for this NIC
    NicInfo = NvmeOfGetNicInfoByIndex (Attempt->NicIndex);
    if (NicInfo == NULL) {
      continue;
    }

    // Logic to skip already processed adapter by comparing MAC
    AlreadyProcessed = FALSE;
    NET_LIST_FOR_EACH_SAFE (ProcessedEntry, NextEntryProcessed, &gAddedAdaptersList) {
      ProcessedAdapter = NET_LIST_USER_STRUCT (ProcessedEntry, NVMEOF_PROCESSED_MAC, Link);
      if (AsciiStrCmp (Attempt->MacString, ProcessedAdapter->MacString) == 0) {
        AlreadyProcessed = TRUE;
        break;
      }
    }
    if (AlreadyProcessed) {
      continue;
    }

    //
    // Fill the Host fabric interface header section.
    //
    HfiHeader->StructureId  = EFI_ACPI_NVMEOF_BFT_HFI_HEADER_DESCRIPTOR_ID;
    Control->HfiDescVersion = EFI_ACPI_NVMEOF_BFT_HFI_HEADER_DESCRIPTOR_VERSION;
    HfiHeader->Index        = (UINT16) AdapterIndex + 1 ;
    HfiHeader->Flags        = EFI_ACPI_NVMEOF_BFT_HFI_HEADER_DESCRIPTOR_FLAG_BLOCK_VALID;
    Control->HfiDescLength  = (UINT16) sizeof (EFI_ACPI_NVMEOF_BFT_HFI_HEADER_DESCRIPTOR);

    HostOverridesHeapRef = Heap;
    HfiHeader->HfiTransportType = NVMEOF_TRANSPORT_TCP; // NVMe/TCP (802.11 + TCP/IP)

    // Fill the Hfi transport info structure
    HfiTcpTransportInfo.Header.StructureId             = EFI_ACPI_NVMEOF_BFT_HFI_TRANSPORT_DESCRIPTOR_ID;
    HfiTcpTransportInfo.Header.HfiTransportType        = EFI_ACPI_NVMEOF_BFT_HFI_TRANSPORT_INFO_HFI_TYPE_TCPIP;
    HfiTcpTransportInfo.Header.Version                 = EFI_ACPI_NVMEOF_BFT_HFI_TRANSPORT_INFO_DESCRIPTOR_VERSION;
    HfiTcpTransportInfo.Header.HfiTransportInfoVersion = EFI_ACPI_NVMEOF_BFT_HFI_TRANSPORT_INFO_VERSION;
    HfiHeader->InfoStructureLen                        = sizeof (EFI_ACPI_NVMEOF_BFT_HFI_TRANSPORT_INFO_DESCRIPTOR_TCP);
    HfiTcpTransportInfo.Header.HfiIndex                = (UINT16) HfiHeader->Index;
    HfiTcpTransportInfo.TransportFlags                 = EFI_ACPI_NVMEOF_BFT_HFI_TRANSPORT_DESCRIPTOR_FLAG_BLOCK_VALID |
      EFI_ACPI_NVMEOF_BFT_HFI_TRANSPORT_DESCRIPTOR_FLAG_GLOBAL_LOCAL_ROUTE;
 
    if (NvData->HostInfoDhcp) {
      HfiTcpTransportInfo.TransportFlags |= EFI_ACPI_NVMEOF_BFT_HFI_TRANSPORT_DESCRIPTOR_FLAG_DHCP_OVERRIDE;
      HfiTcpTransportInfo.Origin = IpPrefixOriginDhcp;
    } else {
      HfiTcpTransportInfo.Origin = IpPrefixOriginManual;
    }

    if ((NvData->NvmeofIpMode == IP_MODE_IP4) ||
      (Attempt->AutoConfigureMode == IP_MODE_AUTOCONFIG_IP4)) {
      //
      // Get the subnet mask prefix length.
      //
      HfiTcpTransportInfo.SubnetMaskPrefix = NvmeOfGetSubnetMaskPrefixLength (&NvData->NvmeofSubsysHostSubnetMask.v4);

      //
      // Map the various v4 addresses into v6 addresses.
      //
      NvmeOfMapV4ToV6Addr (&NvData->NvmeofSubsysHostIP.v4, &HfiTcpTransportInfo.IpAddress);
      NvmeOfMapV4ToV6Addr (&NvData->NvmeofSubsysHostGateway.v4, &HfiTcpTransportInfo.Gateway);
      NvmeOfMapV4ToV6Addr (&Attempt->PrimaryDns.v4, &HfiTcpTransportInfo.PrimaryDns);
      NvmeOfMapV4ToV6Addr (&Attempt->SecondaryDns.v4, &HfiTcpTransportInfo.SecondaryDns);
      NvmeOfMapV4ToV6Addr (&Attempt->DhcpServer.v4, &HfiTcpTransportInfo.DhcpServer);
    } else if ((NvData->NvmeofIpMode == IP_MODE_IP6) ||
      (Attempt->AutoConfigureMode == IP_MODE_AUTOCONFIG_IP6)) {
      HfiTcpTransportInfo.SubnetMaskPrefix = NvData->NvmeofPrefixLength;
      CopyMem (&HfiTcpTransportInfo.IpAddress, &NvData->NvmeofSubsysHostIP, sizeof (EFI_IPv6_ADDRESS));
      CopyMem (&HfiTcpTransportInfo.Gateway, &NvData->NvmeofSubsysHostGateway, sizeof (EFI_IPv6_ADDRESS));
      CopyMem (&HfiTcpTransportInfo.PrimaryDns, &Attempt->PrimaryDns, sizeof (EFI_IPv6_ADDRESS));
      CopyMem (&HfiTcpTransportInfo.SecondaryDns, &Attempt->SecondaryDns, sizeof (EFI_IPv6_ADDRESS));
      CopyMem (&HfiTcpTransportInfo.DhcpServer, &Attempt->DhcpServer, sizeof (EFI_IPv6_ADDRESS));
    } else {
      ASSERT (FALSE);
    }

    HfiTcpTransportInfo.RouteMetric = NvData->RouteMetric;

    //
    // Update the length of hostname
    //
    HfiTcpTransportInfo.HostNameLength = (UINT16)AsciiStrLen (NvData->HostName);

    //
    // Adapter Info: VLAN tag, Mac address, PCI location.
    //
    HfiTcpTransportInfo.VLanTag = NicInfo->VlanId;
    CopyMem (HfiTcpTransportInfo.Mac, &NicInfo->PermanentAddress, sizeof (HfiTcpTransportInfo.Mac));
    HfiTcpTransportInfo.PciLocation = (UINT16)((NicInfo->BusNumber << 8) |
      (NicInfo->DeviceNumber << 3) | NicInfo->FunctionNumber);

    // Convert MAC to string.
    NvmeOfMacAddrToStr (&NicInfo->PermanentAddress, NicInfo->HwAddressSize,
      NicInfo->VlanId, MacString);

    for (Index = 0; Index < gNvmeOfNbftListIndex; Index++) {
      AsciiStrToUnicodeStrS (gNvmeOfNbftList[Index].AttemptData->MacString, AttemptMacString,
        sizeof (AttemptMacString) / sizeof (AttemptMacString[0]));
      if (StrCmp (MacString, AttemptMacString) != 0) {
        continue;
      } else {
        gNvmeOfNbftList[Index].DeviceAdapterIndex = HfiHeader->Index;
        if (AsciiStrCmp (gNvmeOfNbftList[Index].AttemptData->SubsysConfigData.NvmeofSubsysNqn,
          NVMEOF_DISCOVERY_NQN) == 0) {
          gNvmeOfNbftList[Index].IsDiscoveryNqn = TRUE;
        } else {
          gNvmeOfNbftList[Index].IsDiscoveryNqn = FALSE;
        }
      }
    }
    // Copy HFI tenasport info to heap and update the HFI header structure
    NvmeOfAddHeapItem (Heap, &HfiTcpTransportInfo, sizeof (EFI_ACPI_NVMEOF_BFT_HFI_TRANSPORT_INFO_DESCRIPTOR_TCP));
    HfiHeader->InfoStructureLen = sizeof (EFI_ACPI_NVMEOF_BFT_HFI_TRANSPORT_INFO_DESCRIPTOR_TCP);

    // Copy the NVMeOF Host name into the heap.
    NvmeOfAddHeapItem (Heap, NvData->HostName, HfiTcpTransportInfo.HostNameLength);

   // Add an entry to processed adapter list
    ProcessedAdapter = (NVMEOF_PROCESSED_MAC *)AllocateZeroPool (sizeof (NVMEOF_PROCESSED_MAC));
    if (ProcessedAdapter == NULL) {
      return;
    }
    CopyMem (ProcessedAdapter->MacString, Attempt->MacString, sizeof (ProcessedAdapter->MacString));
    ProcessedAdapter->HfiHeaderRef = HfiHeader;
    ProcessedAdapter->HostOverrideEnable = NvData->HostOverrideEnable;
    ProcessedAdapter->HeapRef = HostOverridesHeapRef;
    InsertTailList (&gAddedAdaptersList, &ProcessedAdapter->Link);
 
    //
    // Advance to the next adapter section
    //
    HfiHeader = (EFI_ACPI_NVMEOF_BFT_HFI_HEADER_DESCRIPTOR *)((UINTN)HfiHeader +
      NBFT_ROUNDUP(sizeof (EFI_ACPI_NVMEOF_BFT_HFI_HEADER_DESCRIPTOR)));

    AdapterIndex++;
  }

  // Populate the num of adapters in control section
  Control->NumHfis = AdapterIndex;
}

/**
  Fill the Subsystem Namespace sections in NVMeOF Boot Firmware Table.

  @param[in]       Table  The buffer of the ACPI table.
  @param[in, out]  Heap   The heap buffer used to store the variable length
                          parameters such as NVMeOF name.

**/
VOID
NvmeOfFillSubsystemNamespaceSection (
  IN     EFI_ACPI_NVMEOF_BFT_HEADER  *Table,
  UINT8                              **Heap
  )
{
  EFI_ACPI_NVMEOF_BFT_SUBSYSTEM_NAMESPACE_DESCRIPTOR  *SubsystemNamespace;
  EFI_ACPI_NVMEOF_BFT_CONTROL_STRUCTURE               *Control;
  EFI_ACPI_NVMEOF_BFT_SUBSYSTEM_EXT_INFO_DESCRIPTOR   SsnsExtInfo = { 0 };
  EFI_ACPI_NVMEOF_BFT_HFI_HEADER_DESCRIPTOR           *HfiHeader;
  UINTN                                               DeviceIndex = 0;
  UINT8                                               Index;
  LIST_ENTRY                                          *ProcessedEntry;
  LIST_ENTRY                                          *NextEntryProcessed;
  NVMEOF_PROCESSED_NAMESPACE                          *ProcessedNamespace;
  BOOLEAN                                             AlreadyProcessed;
  EFI_IPv6_ADDRESS                                    SubsytemTrasportAddress;
  UINT16                                              Len = 0;

  Control = (EFI_ACPI_NVMEOF_BFT_CONTROL_STRUCTURE *)(Table + 1);
  SubsystemNamespace = (EFI_ACPI_NVMEOF_BFT_SUBSYSTEM_NAMESPACE_DESCRIPTOR *)((UINTN)Table +
    Control->HfiDescOffset +
    (Control->NumHfis * NBFT_ROUNDUP (sizeof (EFI_ACPI_NVMEOF_BFT_HFI_HEADER_DESCRIPTOR))));
  HfiHeader = (EFI_ACPI_NVMEOF_BFT_HFI_HEADER_DESCRIPTOR *)((UINTN)Table +
      Control->HostDescOffset + NBFT_ROUNDUP(sizeof (EFI_ACPI_NVMEOF_BFT_HOST_DESCRIPTOR)));

  // Fill the offset of 1st namespace structure in control structure
  Control->SubSytemDescOffset = (UINT16)((UINTN)SubsystemNamespace - (UINTN)Table);

  // Iterate on each namespace and populate the details.
  for (Index = 0; Index < gNvmeOfNbftListIndex; Index++) {
    // Logic to skip already processed namespace by comparing NID
    AlreadyProcessed = FALSE;
    NET_LIST_FOR_EACH_SAFE (ProcessedEntry, NextEntryProcessed, &gProcessedNamespaceList.Link) {
      ProcessedNamespace = NET_LIST_USER_STRUCT (ProcessedEntry, NVMEOF_PROCESSED_NAMESPACE, Link);
      if (CompareMem (SubsystemNamespace->Nid, ProcessedNamespace->Nid, 16) == 0) {
        AlreadyProcessed = TRUE;
        break;
      }
    }
    if (AlreadyProcessed) {
      NvmeOfAddHeapItem (Heap, &(gNvmeOfNbftList[Index].DeviceAdapterIndex), sizeof (UINT8));
      SubsystemNamespace->HfiAssociationLen += sizeof (UINT8);
      continue;
    }
    //
    // Fill the Subsystem Namespace section.
    //
    SubsystemNamespace->StructureId  = EFI_ACPI_NVMEOF_BFT_SUBSYSTEM_NAMESPACE_DESCRIPTOR_ID;
    Control->SubSystemVersion        = EFI_ACPI_NVMEOF_BFT_SUBSYSTEM_DESCRIPTOR_VERSION;
    Control->SubSystemDescLength     = (UINT16) sizeof (EFI_ACPI_NVMEOF_BFT_SUBSYSTEM_NAMESPACE_DESCRIPTOR);
    SubsystemNamespace->Index        = (UINT16) DeviceIndex + 1;
    SubsystemNamespace->Flags        = EFI_ACPI_NVMEOF_BFT_SUBSYSTEM_DESCRIPTOR_FLAG_BLOCK_VALID;
    // Transport type
    SubsystemNamespace->TransportType = NVMEOF_TRANSPORT_TCP;

    if (gNvmeOfNbftList[Index].IsDiscoveryNqn) {
      SubsystemNamespace->PrimaryDiscoveryCtrlrIndex = gNvmeOfNbftList[Index].DeviceAdapterIndex;
      // Update flag
      SubsystemNamespace->Flags |= EFI_ACPI_NVMEOF_BFT_SUBSYSTEM_DESCRIPTOR_FLAG_DISCOVERED_NAMESPACE;
    }

    // Transport address
    SubsystemNamespace->SubsystemTransportAdressLength = sizeof(EFI_IPv6_ADDRESS);
    if ((gNvmeOfNbftList[Index].AttemptData->SubsysConfigData.NvmeofIpMode == IP_MODE_IP4) ||
      (gNvmeOfNbftList[Index].AttemptData->AutoConfigureMode == IP_MODE_AUTOCONFIG_IP4)) {
      EFI_IPv4_ADDRESS  v4;
      NetLibAsciiStrToIp4 (gNvmeOfNbftList[Index].Device->NameSpace->ctrlr->trid.traddr, &v4);
      NvmeOfMapV4ToV6Addr (&v4, &SubsytemTrasportAddress);
      NvmeOfAddHeapItem (Heap, &SubsytemTrasportAddress, sizeof (EFI_IPv6_ADDRESS));
    } else if ((gNvmeOfNbftList[Index].AttemptData->SubsysConfigData.NvmeofIpMode == IP_MODE_IP6) ||
      (gNvmeOfNbftList[Index].AttemptData->AutoConfigureMode == IP_MODE_AUTOCONFIG_IP6)) {
      NetLibAsciiStrToIp6 (gNvmeOfNbftList[Index].Device->NameSpace->ctrlr->trid.traddr,
        &SubsytemTrasportAddress);
      NvmeOfAddHeapItem (Heap, &SubsytemTrasportAddress, sizeof (EFI_IPv6_ADDRESS));
    } else {
      ASSERT (FALSE);
    }
    // Transport port
    Len = AsciiStrLen (gNvmeOfNbftList[Index].Device->NameSpace->ctrlr->trid.trsvcid);
    SubsystemNamespace->SubsystemTransportServiceIdLength = Len;
    NvmeOfAddHeapItem (Heap, gNvmeOfNbftList[Index].Device->NameSpace->ctrlr->trid.trsvcid, Len);
    // Subsystem Port ID
    SubsystemNamespace->SubsytemPortId = 0;
    // NSID
    SubsystemNamespace->Nsid = gNvmeOfNbftList[Index].Device->NamespaceId;
    // NID type
    SubsystemNamespace->NidType = gNvmeOfNbftList[Index].Device->NamespaceIdType;

    // Fill the NID
    CopyMem (SubsystemNamespace->Nid, gNvmeOfNbftList[Index].Device->NamespaceUuid,
      sizeof (SubsystemNamespace->Nid));

    SubsystemNamespace->PrimaryHfiDescriptorIndex = HfiHeader->Index;
    // HFI association
    NvmeOfAddHeapItem (Heap, &(gNvmeOfNbftList[Index].DeviceAdapterIndex), sizeof (UINT8));
    SubsystemNamespace->HfiAssociationLen = sizeof (UINT8);

    // Fill the subsystem NQN into the heap.
    Len = (UINT16)AsciiStrLen (gNvmeOfNbftList[Index].Device->NameSpace->ctrlr->trid.subnqn);
    NvmeOfAddHeapItem (Heap, gNvmeOfNbftList[Index].Device->NameSpace->ctrlr->trid.subnqn, Len);
    SubsystemNamespace->SubsystemNamespaceNqnLen = Len;

    SsnsExtInfo.StructureId      = EFI_ACPI_NVMEOF_BFT_SUBSYSTEM_NAMESPACE_INFO_EXT_DESCRIPTOR_ID;
    SsnsExtInfo.Version          = EFI_ACPI_NVMEOF_BFT_SUBSYSTEM_EXT_INFO_DESCRIPTOR_VERSION;
    SsnsExtInfo.SsnsIndex        = Index + 1;
    SsnsExtInfo.Flags            = EFI_ACPI_NVMEOF_BFT_SUBSYSTEM_EXT_INFO_DESCRIPTOR_VERSION_FLAG_STRUCTURE_VALID;
    // Controller ID
    SsnsExtInfo.ControllerId = gNvmeOfNbftList[Index].Device->NameSpace->ctrlr->cntlid;
    if (gNvmeOfNbftList[Index].IsDiscoveryNqn) {
    // ASQSZ
      SsnsExtInfo.Asqsz = gNvmeOfNbftList[Index].Device->Asqsz;
    } else {
      SsnsExtInfo.Asqsz = DEFAULT_ADMIN_QUEUE_SIZE;
    }
    // Root Path in heap
    if (gNvmeOfRootPath != NULL) {
      SsnsExtInfo.DhcpRootPathLength = (AsciiStrLen(gNvmeOfRootPath) - 1);
    } else {
      SsnsExtInfo.DhcpRootPathLength = 0;
    }
    // Copy SsnsExtInfo  to heap and update the SubSystem Namespace header structure
    NvmeOfAddHeapItem (Heap, &SsnsExtInfo, sizeof (EFI_ACPI_NVMEOF_BFT_SUBSYSTEM_EXT_INFO_DESCRIPTOR));
    SubsystemNamespace->SsnsExtendedInfoLength = sizeof (EFI_ACPI_NVMEOF_BFT_SUBSYSTEM_EXT_INFO_DESCRIPTOR);
    if (SsnsExtInfo.DhcpRootPathLength > 0) {
      NvmeOfAddHeapItem (Heap, gNvmeOfRootPath, SsnsExtInfo.DhcpRootPathLength);
    }
    SubsystemNamespace->Flags |=  EFI_ACPI_NVMEOF_BFT_SUBSYSTEM_DESCRIPTOR_FLAG_USE_SSNS_EXT_INFO;

    // Add an entry to processed namespace list
    ProcessedNamespace = AllocateZeroPool (sizeof (NVMEOF_PROCESSED_NAMESPACE));
    if (ProcessedNamespace == NULL) {
      return;
    }
    CopyMem (ProcessedNamespace->Nid, SubsystemNamespace->Nid, sizeof (ProcessedNamespace->Nid));
    InsertTailList (&gProcessedNamespaceList.Link, &ProcessedNamespace->Link);

    // Advance the subsystem namespace section
    SubsystemNamespace = (EFI_ACPI_NVMEOF_BFT_SUBSYSTEM_NAMESPACE_DESCRIPTOR *)((UINTN)SubsystemNamespace +
      NBFT_ROUNDUP (sizeof (EFI_ACPI_NVMEOF_BFT_SUBSYSTEM_NAMESPACE_DESCRIPTOR)));
      
    DeviceIndex++;
  }

  Control->NumNamespaces = DeviceIndex;
}

/**
  Fill the Discovery section of the NVMeOF Boot Firmware Table.

  @param[in]       Table  The ACPI table.
  @param[in, out]  Heap   The heap.

**/
VOID
NvmeOfFillDiscoverySection (
  IN     EFI_ACPI_NVMEOF_BFT_HEADER  *Table,
  IN OUT UINT8                       **Heap
  )
{
  INT8 AdapterIndex = -1;
  UINT8 Index = 0;
  EFI_ACPI_NVMEOF_BFT_CONTROL_STRUCTURE     *Control;
  EFI_ACPI_NVMEOF_BFT_DISCOVERY_DESCRIPTOR  *Discovery;
  NVMEOF_DISCOVERY_DETAILS                  *DiscoveryDetails;
  CHAR8                                     IpAddrStr[256];
  CHAR8                                     UriTransportAddr[300];
  UINT32                                    MaxDiscoveryDetailsIndex = 0;

  Control = (EFI_ACPI_NVMEOF_BFT_CONTROL_STRUCTURE *)(Table + 1);
  Discovery = (EFI_ACPI_NVMEOF_BFT_DISCOVERY_DESCRIPTOR *)((UINTN)Table + 
    (Control->SubSytemDescOffset +
    (Control->NumNamespaces * NBFT_ROUNDUP (sizeof (EFI_ACPI_NVMEOF_BFT_SUBSYSTEM_NAMESPACE_DESCRIPTOR)))
    + (Control->NumSecurityProfiles * NBFT_ROUNDUP(sizeof (EFI_ACPI_NVMEOF_BFT_SECURITY_DESCRIPTOR)))));

  // Fill the offset of discovery structure in control structure
  Control->DiscoDescOffset = (UINT16)((UINTN)Discovery - (UINTN)Table);

  // Find the number of discovery controllers in the list
  for (Index = 0; Index < gNvmeOfNbftListIndex; Index++) {
    if (gNvmeOfNbftList[Index].DeviceAdapterIndex != AdapterIndex &&
      gNvmeOfNbftList[Index].IsDiscoveryNqn) {
      MaxDiscoveryDetailsIndex++;
      AdapterIndex = gNvmeOfNbftList[Index].DeviceAdapterIndex;
    }
  }

  if (MaxDiscoveryDetailsIndex == 0)
  {
    //No need to fill this structure as we have no discovery controller
    Control->NumDiscoveryEntires = 0;
    Discovery->DiscoveryCtrlrAddrLen = 0;
    Discovery->DiscoveryCtrlrNqnLen = 0;
    return;
  }

  DiscoveryDetails = 
    (NVMEOF_DISCOVERY_DETAILS *)AllocateZeroPool (sizeof (NVMEOF_DISCOVERY_DETAILS) * MaxDiscoveryDetailsIndex);
  if (DiscoveryDetails == NULL) {
    return;
  }

  Discovery->Index = 0;
  AdapterIndex = -1;
  UINT8 NumRecs = 0;
  // Find the number of discovery records
  for (Index = 0; Index < gNvmeOfNbftListIndex; Index++) {
    if (gNvmeOfNbftList[Index].DeviceAdapterIndex != AdapterIndex &&
      gNvmeOfNbftList[Index].IsDiscoveryNqn) {
      AdapterIndex = DiscoveryDetails[NumRecs].AdapterIndex =
        gNvmeOfNbftList[Index].DeviceAdapterIndex;
      DiscoveryDetails[NumRecs].SecurityProfileIndex = 0;
      DiscoveryDetails[NumRecs].Port = 
        gNvmeOfNbftList[Index].AttemptData->SubsysConfigData.NvmeofSubsysPortId;
      CopyMem (&DiscoveryDetails[NumRecs].TransportAddress,
        &gNvmeOfNbftList[Index].AttemptData->SubsysConfigData.NvmeofSubSystemIp,
        sizeof (DiscoveryDetails[NumRecs].TransportAddress));
      if ((gNvmeOfNbftList[Index].AttemptData->SubsysConfigData.NvmeofIpMode == IP_MODE_IP4) ||
        (gNvmeOfNbftList[Index].AttemptData->AutoConfigureMode == IP_MODE_AUTOCONFIG_IP4)) {
        DiscoveryDetails[NumRecs].Ipv6Flag = FALSE;
      } else {
        DiscoveryDetails[NumRecs].Ipv6Flag = TRUE;
      }
      CopyMem(DiscoveryDetails[NumRecs].Nqn,
        gNvmeOfNbftList[Index].AttemptData->SubsysConfigData.NvmeofSubsysNqn,
        sizeof (DiscoveryDetails[NumRecs].Nqn));
      NumRecs++;
    }
  }

  for (Index = 0; Index < NumRecs; Index++) {
    Discovery->StructureId = EFI_ACPI_NVMEOF_BFT_DISCOVERY_DESCRIPTOR_ID;
    Control->DiscoDescVersion         = EFI_ACPI_NVMEOF_BFT_DISCOVERY_DESCRIPTOR_VERSION;
    Control->DiscoDescLengh    = (UINT16) sizeof (EFI_ACPI_NVMEOF_BFT_DISCOVERY_DESCRIPTOR);
    Discovery->Flags       = EFI_ACPI_NVMEOF_BFT_DISCOVERY_DESCRIPTOR_FLAG_BLOCK_VALID;

    Discovery->HfiDescriptorIndex = DiscoveryDetails[Index].AdapterIndex;
    Discovery->SecurityProfileDescriptorIndex =
      DiscoveryDetails[Index].SecurityProfileIndex;
    NvmeOfIpToStr (DiscoveryDetails[Index].TransportAddress, IpAddrStr,
      DiscoveryDetails[Index].Ipv6Flag);

    if (DiscoveryDetails[Index].Ipv6Flag) {
      sprintf (UriTransportAddr, "nvme+tcp://[%a]:%d/",
        IpAddrStr, DiscoveryDetails[Index].Port);
    } else {
      sprintf (UriTransportAddr, "nvme+tcp://%a:%d/", IpAddrStr,
        DiscoveryDetails[Index].Port);
    }

    // Discovery controller address in heap
    UINT16 Len = (UINT16)AsciiStrLen (UriTransportAddr);
    NvmeOfAddHeapItem (Heap, UriTransportAddr, Len);
    Discovery->DiscoveryCtrlrAddrLen = Len;

    // Discovery controller NQN in heap
    Len = (UINT16)AsciiStrLen (DiscoveryDetails[Index].Nqn);
    NvmeOfAddHeapItem (Heap, DiscoveryDetails[Index].Nqn, Len);
    Discovery->DiscoveryCtrlrNqnLen = Len;
    Discovery->Index = Index + 1;
    Discovery = (EFI_ACPI_NVMEOF_BFT_DISCOVERY_DESCRIPTOR *)((UINT8 *)Discovery +
      sizeof (EFI_ACPI_NVMEOF_BFT_DISCOVERY_DESCRIPTOR));
  }

  Control->NumDiscoveryEntires = NumRecs;
  FreePool (DiscoveryDetails);
}

/**
  Publish and remove the NVMeOF Boot Firmware Table.
**/
VOID
NvmeOfPublishNbft (
  IN BOOLEAN HostStructureOnly
  )
{
  EFI_STATUS                                    Status;
  EFI_ACPI_TABLE_PROTOCOL                       *AcpiTableProtocol;
  EFI_ACPI_NVMEOF_BFT_HEADER                    *Table = NULL;
  EFI_ACPI_3_0_ROOT_SYSTEM_DESCRIPTION_POINTER  *Rsdp  = NULL;
  EFI_ACPI_DESCRIPTION_HEADER                   *Rsdt = NULL;
  EFI_ACPI_DESCRIPTION_HEADER                   *Xsdt = NULL;
  UINT8                                         Checksum;
  UINT8                                         *Heap = NULL;
  NVMEOF_PROCESSED_MAC                          *ProcessedAdapter = NULL;
  NVMEOF_PROCESSED_NAMESPACE                    *ProcessedNamespace = NULL;
  NVMEOF_PROCESSED_IP_ADDR                      *ProcessedIpConfig = NULL;
  LIST_ENTRY                                    *ProcessedEntry = NULL;
  LIST_ENTRY                                    *NextProcessedEntry = NULL;

  Rsdt = NULL;
  Xsdt = NULL;

  InitializeListHead (&gAddedAdaptersList);
  InitializeListHead (&gProcessedNamespaceList.Link);
  InitializeListHead (&gProcessedIpConfigList.Link);

  Status = gBS->LocateProtocol (&gEfiAcpiTableProtocolGuid, NULL, (VOID **) &AcpiTableProtocol);
  if (EFI_ERROR (Status)) {
    return ;
  }

  //
  // Find ACPI table RSD_PTR from the system table.
  //
  Status = EfiGetSystemConfigurationTable (&gEfiAcpiTableGuid, (VOID **) &Rsdp);
  if (EFI_ERROR (Status)) {
    Status = EfiGetSystemConfigurationTable (&gEfiAcpi10TableGuid, (VOID **) &Rsdp);
  }

  if (EFI_ERROR (Status) || (Rsdp == NULL)) {
    return ;
  } else if (Rsdp->Revision >= EFI_ACPI_2_0_ROOT_SYSTEM_DESCRIPTION_POINTER_REVISION && Rsdp->XsdtAddress != 0) {
    Xsdt = (EFI_ACPI_DESCRIPTION_HEADER *) (UINTN) Rsdp->XsdtAddress;
  } else if (Rsdp->RsdtAddress != 0) {
    Rsdt = (EFI_ACPI_DESCRIPTION_HEADER *) (UINTN) Rsdp->RsdtAddress;
  }

  if ((Xsdt == NULL) && (Rsdt == NULL)) {
    return;
  }

  if (gNbftInstalled) {
    Status = AcpiTableProtocol->UninstallAcpiTable (
                                  AcpiTableProtocol,
                                  gTableKey
                                  );
    if (EFI_ERROR (Status)) {
      return ;
    }
    gNbftInstalled = FALSE;
  }

  //
  // Allocate memory to hold the ACPI table.
  //
  Table = AllocateZeroPool (NBFT_MAX_SIZE);
  if (Table == NULL) {
    return ;
  }

  //
  // Allocate memory to hold the heap contents.
  //
  gNbftHeap.Heap = NULL;
  gNbftHeap.Length = 0;
  Heap = gNbftHeap.Heap = AllocateZeroPool (NBFT_HEAP_SIZE);
  if (gNbftHeap.Heap == NULL) {
    goto Error;
  }

  //
  // Fill in the various section of the NVMEOF Boot Firmware Table.
  //
  if (HostStructureOnly) {
    if (Rsdp->Revision >= EFI_ACPI_2_0_ROOT_SYSTEM_DESCRIPTION_POINTER_REVISION) {
      NvmeOfInitIbfTableHeader (Table, Xsdt->OemId, &Xsdt->OemTableId, &Heap);
    } else {
      NvmeOfInitIbfTableHeader (Table, Rsdt->OemId, &Rsdt->OemTableId, &Heap);
    }
    NvmeOfInitControlSection (Table);
    NvmeOfFillHostSection (Table, &Heap);
  } else {
    if (Rsdp->Revision >= EFI_ACPI_2_0_ROOT_SYSTEM_DESCRIPTION_POINTER_REVISION) {
      NvmeOfInitIbfTableHeader (Table, Xsdt->OemId, &Xsdt->OemTableId, &Heap);
    } else {
      NvmeOfInitIbfTableHeader (Table, Rsdt->OemId, &Rsdt->OemTableId, &Heap);
    }
    NvmeOfInitControlSection (Table);
    NvmeOfFillHostSection (Table, &Heap);
    NvmeOfFillHostFabricInterfaceSections (Table, &Heap);
    NvmeOfFillSubsystemNamespaceSection (Table, &Heap);
    NvmeOfFillDiscoverySection (Table, &Heap);
  }

  NvmeOfFillHeapOffsets (Table, HostStructureOnly);

  Checksum = CalculateCheckSum8 ((UINT8 *)Table, Table->Length);
  Table->Checksum = Checksum;

  //
  // Install or update the nBFT table.
  //
  Status = AcpiTableProtocol->InstallAcpiTable (
                                AcpiTableProtocol,
                                Table,
                                Table->Length,
                                &gTableKey
                                );
  if (EFI_ERROR (Status)) {
    goto Error;
  }

  gNbftInstalled = TRUE;

Error:
  // Remove entries from the processed adapter list
  NET_LIST_FOR_EACH_SAFE (ProcessedEntry, NextProcessedEntry, &gAddedAdaptersList) {
    ProcessedAdapter = NET_LIST_USER_STRUCT (ProcessedEntry, NVMEOF_PROCESSED_MAC, Link);
    if (ProcessedAdapter != NULL) {
      RemoveEntryList (&ProcessedAdapter->Link);
      FreePool (ProcessedAdapter);
    }
  }
  // Remove entries from the processed namespace list
  NET_LIST_FOR_EACH_SAFE (ProcessedEntry, NextProcessedEntry, &gProcessedNamespaceList.Link) {
    ProcessedNamespace = NET_LIST_USER_STRUCT (ProcessedEntry, NVMEOF_PROCESSED_NAMESPACE, Link);
    if (ProcessedNamespace != NULL) {
      RemoveEntryList (&ProcessedNamespace->Link);
      FreePool (ProcessedNamespace);
    }
  }
  // Remove entries from the processed IP config list
  NET_LIST_FOR_EACH_SAFE(ProcessedEntry, NextProcessedEntry, &gProcessedIpConfigList.Link) {
    ProcessedIpConfig = NET_LIST_USER_STRUCT(ProcessedEntry, NVMEOF_PROCESSED_IP_ADDR, Link);
    if (ProcessedIpConfig != NULL) {
       RemoveEntryList(&ProcessedIpConfig->Link);
       FreePool(ProcessedIpConfig);
    }
  }
  // Free the allocated resources for NBFT
  if (gNvmeOfRootPath != NULL) {
    FreePool (gNvmeOfRootPath);
    gNvmeOfRootPath = NULL;
  }
  if (gNbftHeap.Heap != NULL) {
    FreePool (gNbftHeap.Heap);
  }
  if (Table != NULL) {
    FreePool (Table);
  }
}

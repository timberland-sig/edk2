/** @file
  Functions to invoke SPDK API's for NVMeOF driver.

  Copyright (c) 2020, Dell EMC All rights reserved
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "NvmeOfSpdk.h"
#include "NvmeOfDriver.h"
#include "NvmeOfBlockIo.h"
#include "NvmeOfDeviceInfo.h"
#include "spdk/nvme.h"
#include "spdk/uuid.h"
#include "spdk_internal/sock.h"
#include "nvme_internal.h"
#include "edk_sock.h"
#include "NvmeOfCliInterface.h"

NVMEOF_NQN_NID gNvmeOfNqnNidMap[MAX_SUBSYSTEMS_SUPPORTED];
NVMEOF_NBFT gNvmeOfNbftList[NID_MAX];
UINT8 NqnNidMapINdex = 0;
UINT8 gNvmeOfNbftListIndex = 0;
extern const struct spdk_nvme_transport_ops tcp_ops;
extern struct spdk_net_impl g_edksock_net_impl;
NVMEOF_CLI_CTRL_MAPPING  *CtrlrInfo = NULL;
STATIC struct spdk_nvmf_discovery_log_page *gDiscoveryPage;
STATIC UINT32 gDiscoveryPageSize;
STATIC UINT64 gDiscoveryPage_numrec;
STATIC UINT32 gOutstandingCmds;


STATIC
BOOLEAN
NvmeOfProbeCallback (
  IN VOID                                *CallbackCtx,
  IN const struct spdk_nvme_transport_id *Trid,
  IN struct spdk_nvme_ctrlr_opts         *Opts
  )
{
  NVMEOF_GLOBAL_DATA            *NvmeOfData;
  UINTN                         NvmeOfDataSize = 0;
  NVMEOF_DRIVER_DATA            *Private;
  struct spdk_edk_sock_ctx      *Context;
  NVMEOF_ATTEMPT_CONFIG_NVDATA  *AttemptData;

  NvmeOfData = NvmeOfGetVariableAndSize (
                 L"NvmeofGlobalData",
                 &gNvmeOfConfigGuid,
                 &NvmeOfDataSize
                 );
  if (NvmeOfData == NULL || NvmeOfDataSize == 0) {
    DEBUG ((EFI_D_ERROR, "NvmeOfProbeCallback: NvmeOfData Read Failed\n"));
    return TRUE;
  }

  // Host identifier in UUID format
  CopyMem (Opts->extended_host_id, NvmeOfData->NvmeofHostId, sizeof (Opts->extended_host_id));
  CopyMem (Opts->hostnqn, NvmeOfData->NvmeofHostNqn, sizeof (NvmeOfData->NvmeofHostNqn));
  
  // Set Kato timeout
  Opts->keep_alive_timeout_ms = NVMEOF_KATO_TIMOUT;

  // Fill socket context
  Private     = (NVMEOF_DRIVER_DATA*)CallbackCtx;
  Context     = &Private->Attempt->SocketContext;
  AttemptData = &Private->Attempt->Data;

  Context->Controller = Private->Controller;
  Context->IsIp6      = AttemptData->SubsysConfigData.NvmeofIpMode == IP_MODE_IP6;

  if (!Context->IsIp6) {
    CopyMem (
      &Context->StationIp.v4,
      &AttemptData->SubsysConfigData.NvmeofSubsysHostIP,
      sizeof (EFI_IPv4_ADDRESS)
      );

    CopyMem (
      &Context->SubnetMask.v4,
      &AttemptData->SubsysConfigData.NvmeofSubsysHostSubnetMask,
      sizeof (EFI_IPv4_ADDRESS)
      );

    CopyMem (
      &Context->GatewayIp.v4,
      &AttemptData->SubsysConfigData.NvmeofSubsysHostGateway.v4,
      sizeof (EFI_IPv4_ADDRESS)
      );
  }

  Opts->sock_ctx = Context;

  DEBUG ((DEBUG_INFO, "Attaching to %a\n", Trid->traddr));
  FreePool (NvmeOfData);
  return TRUE;
}

STATIC BOOLEAN IsCtrlPresent (IN struct spdk_nvme_ctrlr *Ctrlr)
 {
  NVMEOF_CONTROLLER_DATA * CtrlrData;
  LIST_ENTRY * Entry;

  //search for controller data
  NET_LIST_FOR_EACH (Entry, &gNvmeOfControllerList) {
    CtrlrData = NET_LIST_USER_STRUCT (Entry, NVMEOF_CONTROLLER_DATA, Link);
    if (CtrlrData->ctrlr == Ctrlr) {
      return TRUE;
    }
  }
  return FALSE;
}

/**
  SPDK Probe Callback function

  @param  CallbackCtx                 Driver Private context.
  @param  spdk_nvme_transport_id      SPDK Transport Id structure.
  @param  spdk_nvme_ctrlr             SPDK NVMe Control structure.
  @param  Opts                        SPDK Controller options

  @retval None
**/
STATIC
VOID
NvmeOfAttachCallback (
  IN VOID                                 *CallbackCtx,
  IN const struct spdk_nvme_transport_id  *Trid,
  IN struct spdk_nvme_ctrlr               *Ctrlr,
  IN const struct spdk_nvme_ctrlr_opts    *Opts
)
{
  UINT32                            NumOfNamespaces = 0;
  UINT32                            Nsid = 0;
  struct spdk_nvme_ns               *Namespace;
  NVMEOF_DEVICE_PRIVATE_DATA        *Device = NULL;
  NVMEOF_DRIVER_DATA                *Private;
  CHAR8                             UuidStr[SPDK_UUID_STRING_LEN];
  NVMEOF_ATTEMPT_CONFIG_NVDATA      *AttemptData = NULL;
  EFI_STATUS                        Status;
  EFI_DEVICE_PATH_PROTOCOL          *DevicePathNode = NULL;
  const struct spdk_nvme_ctrlr_data *ControllerData;
  BOOLEAN                           SecuritySendRecvSupported = FALSE;
  UINT8                             SerialNum[21];
  UINT8                             ModelNum[41];
  NVMEOF_CONTROLLER_DATA            *NvmeofCtrlr = NULL;
  NVMEOF_NQN_NID                    NqnNidMap;
  NVMEOF_NQN_NID                    *FilteredNqnNidMap = NULL;
  BOOLEAN                           IsFiltered = TRUE;
  UINT8                             Index = 0;
  UINTN                             ActiveNs = 1;
  UINT16                            Cntliduser = 0;
  CHAR8                             Key[10] = { 0 };
  NVMEOF_CLI_CTRL_MAPPING           *MappingData = NULL;
  BOOLEAN                           NidToInstall;

  Private = (NVMEOF_DRIVER_DATA *)CallbackCtx;

  // Get the controller data
  ControllerData = spdk_nvme_ctrlr_get_data (Ctrlr);
  SetMem (&NqnNidMap, sizeof (NqnNidMap), 0);

  // Create the NQN and NID map
  NumOfNamespaces = NvmeOfCreateNqnNamespaceMap (Ctrlr, &NqnNidMap);
  if (!NumOfNamespaces) {
    DEBUG ((DEBUG_ERROR, "Create NQN-NID map failed\n"));
  }

  // Save the un-filtered map in array
  if (Private->IsDiscoveryNqn) {
    CopyMem ((gNvmeOfNqnNidMap + NqnNidMapINdex), &NqnNidMap, sizeof (NqnNidMap));
    NqnNidMapINdex++;
  }

  // Filter the namespaces to be used.
  FilteredNqnNidMap =  NvmeOfFilterNamespaces (&NqnNidMap);
  if (FilteredNqnNidMap == NULL) {
    DEBUG ((DEBUG_ERROR, "Filter NQN-NID map failed.\n"));
    return;
  }

  // Get the number of namespaces managed by the controller
  NumOfNamespaces = spdk_nvme_ctrlr_get_num_ns (Ctrlr);

  Cntliduser++;

  // Namespace IDs start at 1, not 0.
  for (Nsid = 1; Nsid <= NumOfNamespaces; Nsid++) {

    // Check if namespace is filtered
    for (Index = 0; Index < FilteredNqnNidMap->NamespaceCount; Index++) {
      if (FilteredNqnNidMap->Nsid[Index] == Nsid) {
        IsFiltered = FALSE;
        break;
      }
    }

    Namespace = spdk_nvme_ctrlr_get_ns (Ctrlr, Nsid);
    if (Namespace == NULL) {
      continue;
    }

    if (!spdk_nvme_ns_is_active (Namespace)) {
      continue;
    }

    // Allocate device private data for each discovered namespace
    Device = AllocateZeroPool (sizeof (NVMEOF_DEVICE_PRIVATE_DATA));
    if (Device == NULL) {
      DEBUG ((DEBUG_ERROR, "Memory allocation to device failed\n"));
      continue;
    }

    Device->Signature = NVMEOF_DEVICE_PRIVATE_DATA_SIGNATURE;
    Device->NameSpace = Namespace;
    Device->NamespaceId = spdk_nvme_ns_get_id (Namespace);
    Device->NamespaceUuid = spdk_nvme_ns_get_uuid (Namespace);

    if (Device->NamespaceUuid == NULL) {
        Device->NamespaceUuid = (struct spdk_uuid *) spdk_nvme_ns_get_nguid (Namespace);
    }

    Device->NamespaceIdType = NvmeOfFindNidType (Namespace, Device->NamespaceUuid);
    Device->Controller = Private;

    Device->qpair = spdk_nvme_ctrlr_alloc_io_qpair (Ctrlr, NULL, 0);
    if (Device->qpair == NULL) {
      DEBUG ((DEBUG_ERROR, "spdk_nvme_ctrlr_alloc_io_qpair() failed\n"));
      FreePool (Device);
      continue;
    }
    
    Private->TcpIo = Private->Attempt->SocketContext.TcpIo;
    ASSERT (Private->TcpIo != NULL);
    Device->TcpIo = Private->TcpIo;

    //For CLI ListConnected command
    MappingData = AllocateZeroPool (sizeof (NVMEOF_CLI_CTRL_MAPPING));
    if (MappingData == NULL) {
      DEBUG ((DEBUG_ERROR, "Memory allocation to MapingData failed\n"));
      FreePool (Device);
      continue;
    }

    MappingData->Ctrlr = Ctrlr;
    sprintf (Key, "nvme%dn%d", Cntliduser, ActiveNs);
    strcpy (MappingData->Key, Key);
    MappingData->Ioqpair = Device->qpair;
    MappingData->Nsid = Device->NamespaceId;
    strcpy (MappingData->Traddr, Trid->traddr);
    strcpy (MappingData->Subnqn, Trid->subnqn);
    MappingData->Cntliduser = Cntliduser;
    ActiveNs++;

    DEBUG ((DEBUG_INFO, "NumOfNamespaces : %d \n", NumOfNamespaces));
    DEBUG ((DEBUG_INFO, "NameSpace Id : %d \n", spdk_nvme_ns_get_id (Namespace)));
    DEBUG ((DEBUG_INFO, "NameSpace flags : %d \n", spdk_nvme_ns_get_flags (Namespace)));
    DEBUG ((DEBUG_INFO, "NameSpace sector_size: %d \n", spdk_nvme_ns_get_sector_size (Namespace)));
    spdk_uuid_fmt_lower (UuidStr, SPDK_UUID_STRING_LEN, Device->NamespaceUuid);
    DEBUG ((DEBUG_INFO, "NameSpace UUID: %a\n", UuidStr));
    DEBUG ((DEBUG_INFO, "NameSpace extended_sector_size : %d \n", spdk_nvme_ns_get_extended_sector_size (Namespace)));
    DEBUG ((DEBUG_INFO, "NameSpace num_sectors: %d\n", spdk_nvme_ns_get_num_sectors (Namespace)));

    // Build BlockIo media structure
    Device->Media.MediaId = 0;
    Device->Media.RemovableMedia = FALSE;
    Device->Media.MediaPresent = TRUE;
    Device->Media.LogicalPartition = FALSE;
    Device->Media.ReadOnly = FALSE;
    Device->Media.WriteCaching = FALSE;
    Device->Media.IoAlign = sizeof (UINTN);;
    Device->Media.BlockSize = spdk_nvme_ns_get_sector_size (Namespace);
    Device->Media.LastBlock = spdk_nvme_ns_get_num_sectors (Namespace) - 1;
    Device->Media.LogicalBlocksPerPhysicalBlock = 1;
    Device->Media.LowestAlignedLba = 1;

    // Create BlockIo Protocol instance
    Device->BlockIo.Revision = EFI_BLOCK_IO_PROTOCOL_REVISION2;
    Device->BlockIo.Media = &Device->Media;
    Device->BlockIo.Reset = NvmeOfBlockIoReset;
    Device->BlockIo.ReadBlocks = NvmeOfBlockIoReadBlocks;
    Device->BlockIo.WriteBlocks = NvmeOfBlockIoWriteBlocks;
    Device->BlockIo.FlushBlocks = NvmeOfBlockIoFlushBlocks;

    // Create BlockIo2 Protocol instance
    Device->BlockIo2.Media = &Device->Media;
    Device->BlockIo2.Reset = NvmeOfBlockIoResetEx;
    Device->BlockIo2.ReadBlocksEx = NvmeOfBlockIoReadBlocksEx;
    Device->BlockIo2.WriteBlocksEx = NvmeOfBlockIoWriteBlocksEx;
    Device->BlockIo2.FlushBlocksEx = NvmeOfBlockIoFlushBlocksEx;

    InitializeListHead (&Device->AsyncQueue);

    // Create StorageSecurityProtocol Instance
    Device->StorageSecurity.ReceiveData = NvmeOfStorageSecurityReceiveData;
    Device->StorageSecurity.SendData = NvmeOfStorageSecuritySendData;

    // Create DiskInfo Protocol instance
    InitializeDiskInfo (Device);

    // Create DevicePath Protocol instance
    AttemptData = &Private->Attempt->Data;

    Status = NvmeOfBuildDevicePath (Private, Device, &DevicePathNode, AttemptData);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "Create device path failed\n"));
      FreePool (MappingData);
      FreePool (Device);
      continue;
    }
    Device->DevicePath = DevicePathNode;

    // Install the device protocols
    if (ControllerData->oacs.security) {
       SecuritySendRecvSupported = TRUE;
    }

    // Build controller name for Component Name (2) protocol.
    CopyMem (SerialNum, ControllerData->sn, sizeof (ControllerData->sn));
    SerialNum[20] = 0;
    CopyMem (ModelNum, ControllerData->mn, sizeof (ControllerData->mn));
    ModelNum[40] = 0;
    UnicodeSPrintAsciiFormat (Device->ModelName, sizeof (Device->ModelName), "%a-%a-%a",
      ModelNum, SerialNum, UuidStr);

    // If a valid NID input provided in attempt, mount only the said NID
    // and ignore all others.
    if (IsUuidValid (AttemptData->SubsysConfigData.NvmeofSubsysNid)) {
      if (spdk_nvme_ns_get_uuid (Namespace) == NULL) {
            spdk_uuid_fmt_lower(UuidStr, SPDK_UUID_STRING_LEN, (struct spdk_uuid *) spdk_nvme_ns_get_nguid (Namespace));
      } else {
            spdk_uuid_fmt_lower (UuidStr, SPDK_UUID_STRING_LEN, spdk_nvme_ns_get_uuid (Namespace));
      } 
      if (AsciiStriCmp (AttemptData->SubsysConfigData.NvmeofSubsysNid, UuidStr) != 0) {
        NidToInstall = FALSE;
      } else {
        NidToInstall = TRUE;
      }
    } else {
      NidToInstall = TRUE;
    }

    if (!IsFiltered && NidToInstall) {
      Status = NvmeOfInstallDeviceProtocols (Device, SecuritySendRecvSupported);
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "Device protocol installation failed\n"));
        FreePool (MappingData);
        FreePool (Device);
        continue;
      } else {
        InsertTailList (&CtrlrInfo->CliCtrlrList, &MappingData->CliCtrlrList);
      }
    } 

    // Create a list pointers to be referenced for nBFT
    if (gNvmeOfNbftListIndex < NID_MAX) {
      gNvmeOfNbftList[gNvmeOfNbftListIndex].Device = Device;
      gNvmeOfNbftList[gNvmeOfNbftListIndex].AttemptData = AttemptData;
      gNvmeOfNbftListIndex++;
    }
  }
  if (IsCtrlPresent (Ctrlr) == FALSE) {
    NvmeofCtrlr = AllocateZeroPool (sizeof (NVMEOF_CONTROLLER_DATA));
    if (NvmeofCtrlr == NULL) {
      FreePool (Device);
      return;
    }
    NvmeofCtrlr->ctrlr = Ctrlr;
    NvmeofCtrlr->NicHandle = Private->Controller;
    NvmeofCtrlr->KeepAliveErrorCounter = 0;
    InsertTailList (&gNvmeOfControllerList, &NvmeofCtrlr->Link);
  }
  return;
}

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
  NVMEOF_DRIVER_DATA               *Private,
  IN NVMEOF_ATTEMPT_CONFIG_NVDATA  *AttemptConfigData,
  IN UINT8                         IpVersion
  )
{
  UINT16                        TargetPort = 0;
  CHAR8                         Port[PORT_STRING_LEN];
  CHAR8                         Ipv4Addr[IPV4_STRING_SIZE];
  CHAR16                        Ipv6Addr[IPV6_STRING_SIZE];
  struct spdk_nvme_transport_id *Trid;
  struct spdk_nvme_ctrlr        *Ctrlr  = NULL;
  struct spdk_nvme_ctrlr_opts   Opts = {0, };

  Trid = AllocateZeroPool (sizeof (struct spdk_nvme_transport_id));
  if (Trid == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  Trid->trtype = SPDK_NVME_TRANSPORT_TCP;
  CopyMem (Trid->trstring, SPDK_NVME_TRANSPORT_NAME_TCP, SPDK_NVMF_TRSTRING_MAX_LEN);
  CopyMem (Trid->subnqn, AttemptConfigData->SubsysConfigData.NvmeofSubsysNqn,
            sizeof (AttemptConfigData->SubsysConfigData.NvmeofSubsysNqn));
  if (AsciiStriCmp (AttemptConfigData->SubsysConfigData.NvmeofSubsysNqn, NVMEOF_DISCOVERY_NQN) == 0) {
    Private->IsDiscoveryNqn = TRUE;
  } else {
    Private->IsDiscoveryNqn = FALSE;
  }
  TargetPort = AttemptConfigData->SubsysConfigData.NvmeofSubsysPortId;
  sprintf (Port, "%d", TargetPort);
  CopyMem (Trid->trsvcid, Port, sizeof (Port));
  if (nvme_get_transport (Trid->trstring) == NULL) {
    spdk_nvme_transport_register (&tcp_ops);
    spdk_net_impl_register(&g_edksock_net_impl, DEFAULT_SOCK_PRIORITY);
  }

  if ((IpVersion == IP_VERSION_4) &&
    (AttemptConfigData->SubsysConfigData.NvmeofIpMode == IP_MODE_IP4 ||
      AttemptConfigData->AutoConfigureMode == IP_MODE_AUTOCONFIG_IP4)) {
    Trid->adrfam = SPDK_NVMF_ADRFAM_IPV4;
    sprintf (Ipv4Addr, "%d.%d.%d.%d",
      AttemptConfigData->SubsysConfigData.NvmeofSubSystemIp.v4.Addr[0],
      AttemptConfigData->SubsysConfigData.NvmeofSubSystemIp.v4.Addr[1],
      AttemptConfigData->SubsysConfigData.NvmeofSubSystemIp.v4.Addr[2],
      AttemptConfigData->SubsysConfigData.NvmeofSubSystemIp.v4.Addr[3]);
    CopyMem (Trid->traddr, Ipv4Addr, sizeof (Ipv4Addr));
  }
  else if ((IpVersion == IP_VERSION_6) &&
    (AttemptConfigData->SubsysConfigData.NvmeofIpMode == IP_MODE_IP6 ||
      AttemptConfigData->AutoConfigureMode == IP_MODE_AUTOCONFIG_IP6)) {
    Trid->adrfam = SPDK_NVMF_ADRFAM_IPV6;
    NetLibIp6ToStr (&AttemptConfigData->SubsysConfigData.NvmeofSubSystemIp.v6, Ipv6Addr, sizeof (Ipv6Addr));
    UnicodeStrToAsciiStrS (Ipv6Addr, Trid->traddr, sizeof (Ipv6Addr));
  } else {
    FreePool (Trid);
    return EFI_NOT_FOUND;
  }

  DEBUG ((DEBUG_INFO, "Probe/Connect NQN: %a\n",  AttemptConfigData->SubsysConfigData.NvmeofSubsysNqn));
  if (spdk_nvme_probe (Trid, Private, NvmeOfProbeCallback,
                         NvmeOfAttachCallback, NULL) != 0) {
    DEBUG ((EFI_D_ERROR, "spdk_nvme_probe() failed for  %a\n", Trid->traddr));
    FreePool (Trid);
    return EFI_NOT_FOUND;
  } else {
    DEBUG ((DEBUG_INFO, "Probe Success\n"));
    if (Private->IsDiscoveryNqn) {
      EFI_STATUS Status = NvmeOfSetDiscoveryInfo ();
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "Unable to set dynamic data to UEFI variable.\n"));
        FreePool (Trid);
        return EFI_NOT_FOUND;
      }
      spdk_nvme_ctrlr_get_default_ctrlr_opts (&Opts, sizeof(Opts));
      NvmeOfProbeCallback (NULL, Trid, &Opts);
      Ctrlr = nvme_transport_ctrlr_construct (Trid, (const struct spdk_nvme_ctrlr_opts*)&Opts, NULL);
      if (Ctrlr) {
        NVMeOfGetAsqz (Ctrlr);
        nvme_transport_ctrlr_destruct (Ctrlr);
      }
    }
  }
  FreePool (Trid);
  return EFI_SUCCESS;
}

STATIC
VOID
NvmeOfGetLogPageCompletion (
  IN  VOID                       *CbArg,
  IN  const struct spdk_nvme_cpl *Cpl
  )
{
  gOutstandingCmds--;
}

/**
  Helper Function to fetch Discovery logheader completion function

  @param[in]  CbArg                  Callback args.
  @param[in]  struct spdk_nvme_cpl   spdk_nvme_cpl completion.

  @retval NONE                        VOID
**/
STATIC
VOID
NvmeOfGetDiscoveryLogHeaderCompletion ( 
  IN VOID                       *CbArg,
  IN const struct spdk_nvme_cpl *Cpl
  )
{
  struct spdk_nvmf_discovery_log_page *NewDiscoveryPage;
  struct spdk_nvme_ctrlr *ctrlr = CbArg;
  UINT16 RecFmt;
  UINT64 Remaining;
  UINT64 Offset;
  INT64  Ret;

  gOutstandingCmds--;
  if (spdk_nvme_cpl_is_error (Cpl)) {
    // Non discovery controller
    free (gDiscoveryPage);
    gDiscoveryPage = NULL;
    return;
  }

  // First 4K of the discovery log page
  RecFmt = gDiscoveryPage->recfmt;
  if (RecFmt != 0) {
    return;
  }

  gDiscoveryPage_numrec = gDiscoveryPage->numrec;

  if (gDiscoveryPage_numrec > MAX_DISCOVERY_LOG_ENTRIES) {
    gDiscoveryPage_numrec = MAX_DISCOVERY_LOG_ENTRIES;
  }

   // Allocate full based on entries
  gDiscoveryPageSize += gDiscoveryPage_numrec * sizeof (struct
    spdk_nvmf_discovery_log_page_entry);
  NewDiscoveryPage = ReallocatePool (sizeof (*gDiscoveryPage), gDiscoveryPageSize, gDiscoveryPage);
  if (NewDiscoveryPage == NULL) {
    free (gDiscoveryPage);
    return;
  }

  gDiscoveryPage = NewDiscoveryPage;

  // Retrieve the rest of the discovery log page
  Offset = offsetof (struct spdk_nvmf_discovery_log_page, entries);
  Remaining = gDiscoveryPageSize - Offset;
  while (Remaining) {
    uint32_t Size;

    Size = spdk_min (Remaining, 4096);
    Ret = spdk_nvme_ctrlr_cmd_get_log_page (
            ctrlr,
            SPDK_NVME_LOG_DISCOVERY,
            0,
            (char *)gDiscoveryPage + Offset,
            Size,
            Offset,
            NvmeOfGetLogPageCompletion,
            NULL
            );
    if (Ret) {
      return;
    }

    Offset += Size;
    Remaining -= Size;
    gOutstandingCmds++;
  }
}

/**
  Helper Function to get discovery log page

  @param[in]  Controller           The handle of the controller.

  @retval NONE                     VOID
**/
STATIC
VOID
NvmeOfGetDiscoveryLogPage (
  IN struct spdk_nvme_ctrlr *Ctrlr
  )
{
  INT64 Ret;

  gDiscoveryPageSize = sizeof (*gDiscoveryPage);
  gDiscoveryPage = calloc (1, gDiscoveryPageSize);
  if (gDiscoveryPage == NULL) {
    DEBUG((EFI_D_INFO, "NvmeOfGetDiscoveryLogPage calloc failed\n"));
    return;
  }

  Ret = spdk_nvme_ctrlr_cmd_get_log_page (
          Ctrlr,
          SPDK_NVME_LOG_DISCOVERY,
          0,
          gDiscoveryPage,
          gDiscoveryPageSize,
          0,
          NvmeOfGetDiscoveryLogHeaderCompletion,
          Ctrlr
          );
  if (Ret) {
    return;
  }

  gOutstandingCmds++;
  while (gOutstandingCmds) {
    spdk_nvme_ctrlr_process_admin_completions (Ctrlr);
  }
}

/**
  Function to fetch ASQSZ value required for nBFT

  @param[in]  Controller  The handle of the controller.

  @retval NONE            VOID
**/
VOID
NVMeOfGetAsqz (
  IN  struct spdk_nvme_ctrlr  *Ctrlr
  )
{
  INT32      DCntr, NCntr;
  BOOLEAN    IsDiscovery;
  CHAR8      *TrAddress;

  NvmeOfGetDiscoveryLogPage (Ctrlr);

  for (DCntr = 0; DCntr < gDiscoveryPage_numrec; DCntr++) {
    struct spdk_nvmf_discovery_log_page_entry *Entry = &gDiscoveryPage->entries[DCntr];
     
    for (NCntr = 0; NCntr < gNvmeOfNbftListIndex; NCntr++) {
      IsDiscovery = gNvmeOfNbftList[NCntr].Device->Controller->IsDiscoveryNqn;
      TrAddress = gNvmeOfNbftList[NCntr].Device->NameSpace->ctrlr->trid.traddr;

      if ((IsDiscovery == true) && AsciiStrCmp (TrAddress, (CHAR8*)Entry->traddr) == 0) {
        gNvmeOfNbftList[NCntr].Device->Asqsz = Entry->asqsz;
        break;
      }
    }
  }
}

/** @file
  Interface between Cli Application and Driver
  Copyright (c) 2020, Dell EMC All rights reserved
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Uefi.h>
#include <Library/UefiLib.h>
#include "NvmeOfImpl.h"
#include "NvmeOfDriver.h"
#include "NvmeOfSpdk.h"
#include "NvmeOfCliInterface.h"
#include "NvmeOfImpl.h"
#include "NvmeOfDeviceInfo.h"
#include "spdk/nvme.h" 
#include "spdk/uuid.h"
#include "spdk_internal/sock.h"
#include "nvme_internal.h"
#include "NvmeOfBlockIo.h"
#include <Library/UefiLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include "NvmeOfCliInterface.h"
#include <Protocol/NvmeOFPassthru.h>

#define CONNECT_TIMEOUT      5000

EFI_HANDLE                        mImageHandler;
NVMEOF_CLI_CTRL_MAPPING           *gCliCtrlMap = NULL;
NVMEOF_CONNECT_COMMAND            *ProbeconnectData = NULL;
extern EFI_GUID                   gNvmeofPassThroughProtocolGuid;
extern NVMEOF_CLI_CTRL_MAPPING    *CtrlrInfo;

extern NVMEOF_NBFT gNvmeOfNbftList[NID_MAX];
extern UINT8 gNvmeOfNbftListIndex;

extern const struct spdk_nvme_transport_ops tcp_ops;
extern struct spdk_net_impl g_edksock_net_impl;

EFI_STATUS
EFIAPI 
NvmeOfGetBootDesc (
  EFI_HANDLE Handle,
  CHAR16     *Description
)
{
  UINTN Index;
  BOOLEAN Found = FALSE;

  for (Index = 0; Index < gNvmeOfNbftListIndex; Index++) {
    if (gNvmeOfNbftList[Index].Device->DeviceHandle == Handle) {
      Found = TRUE;
      break;
    }
  }

  if (Found) {
    CopyMem (Description, gNvmeOfNbftList[Index].Device->ModelName,
      sizeof (gNvmeOfNbftList[Index].Device->ModelName));
  } else {
    return EFI_NOT_FOUND;
  }
  
  return EFI_SUCCESS;
}

/**
  Delete Map Enteies for Cli

  @param  Ctrlr     controller data.

  @retval EFI_STATUS
**/
VOID
EFIAPI
NvmeOfCliDeleteMapEntries (
  struct spdk_nvme_ctrlr *Ctrlr
  )
{
  LIST_ENTRY               *Entry;
  NVMEOF_CLI_CTRL_MAPPING  *MappingList;
  BOOLEAN                  Flag = FALSE;

  NET_LIST_FOR_EACH (Entry, &gCliCtrlMap->CliCtrlrList) {
    MappingList = NET_LIST_USER_STRUCT (Entry, NVMEOF_CLI_CTRL_MAPPING, CliCtrlrList);
    if (MappingList->Ctrlr == Ctrlr) {
      RemoveEntryList (&MappingList->CliCtrlrList);
      Flag = TRUE;
    }
  }
  if (Flag == FALSE) {
    DEBUG ((EFI_D_ERROR, "NvmeOfCliDeleteMapKey: Error in removing MapKey\n"));
    Print (L"Error in removing device key\n");
  }  
}

/**
  Cleanup function for Cli
  @retval None
**/
VOID
EFIAPI
NvmeOfCliCleanup ()
{
  LIST_ENTRY                *Entry = NULL;
  LIST_ENTRY                *NextEntryProcessed = NULL;
  NVMEOF_CLI_CTRL_MAPPING   *MappingList = NULL;
  UINT16                    PrevCntliduser = 0;

  NET_LIST_FOR_EACH_SAFE (Entry, NextEntryProcessed, &gCliCtrlMap->CliCtrlrList) {
    MappingList = NET_LIST_USER_STRUCT (Entry, NVMEOF_CLI_CTRL_MAPPING, CliCtrlrList);
    if (PrevCntliduser != MappingList->Cntliduser) {
      spdk_nvme_detach (MappingList->Ctrlr);
      PrevCntliduser = MappingList->Cntliduser;
    }
    RemoveEntryList (&MappingList->CliCtrlrList);
    FreePool (MappingList);
  }
}

/**
  Cli Connect function

  @param  ConnectData

  @retval EFI_SUCCESS            Connected successfully.
  @retval EFI_OUT_OF_RESOURCES   Out of Resources
  @retval EFI_NOT_FOUND          Resource Not Found
**/
EFI_STATUS
EFIAPI
NvmeOfCliConnect (
  NVMEOF_CONNECT_COMMAND ConnectData
  )
{
  EFI_STATUS Status = EFI_SUCCESS;

  Status = InstallControllerHandler (&ConnectData);
  if (EFI_ERROR (Status)) {
    Print(L"\nError connecting get controller\n");
  } 
   return Status;
}

/**
  IO completion callback.

  @param  arg                 Variable to update completion.
  @param  spdk_nvme_cpl       Completion instance from SPDK.
**/
VOID 
NvmeOfCliIoComplete (
  VOID                       *arg,
  const struct spdk_nvme_cpl *completion
)
{
  UINT8 *IsCompleted = (UINT8*)arg;
  /* See if an error occurred. If so, display information
   * about it, and set completion value so that I/O
   * caller is aware that an error occurred.
  */
  if (spdk_nvme_cpl_is_error (completion)) {
    DEBUG ((DEBUG_ERROR, "\n IoComplete: I/O error status: %s\n", spdk_nvme_cpl_get_status_string (&completion->status)));
    *IsCompleted = ERROR_IN_COMPLETION;
    return;
  }
  *IsCompleted = IS_COMPLETED;
}

/**
  Read data from the cli cmd.

  @param  ReadData               The read data for cli cmd.

  @retval EFI_SUCCESS            Data successfully read from the device.
  @retval EFI_INVALID_PARAMETER  invalid parameter.
**/
EFI_STATUS
EFIAPI
NvmeOfCliRead (NVMEOF_READ_WRITE_DATA ReadData)
{
  EFI_STATUS Status = EFI_SUCCESS;
  UINT8      Is_Completed = 0;
  int        rc = 0;

  /*ctrl and NsId getting from global structure of type  (struct spdk_nvme_dev)*/
  struct spdk_nvme_ns *ns = spdk_nvme_ctrlr_get_ns (ReadData.Ctrlr, ReadData.Nsid);

  Status = spdk_nvme_ns_cmd_read (ns,
             ReadData.Ioqpair, 
             ReadData.Payload,
             ReadData.Startblock,
             1, 
             NvmeOfCliIoComplete, 
             &Is_Completed, 
             0);
  if (Status != 0) {
    DEBUG ((DEBUG_ERROR, "NvmeOfCliRead: Error in spdk read\n"));
    Status = EFI_INVALID_PARAMETER;
    return Status;    
  }
  while (!Is_Completed) {
    rc = spdk_nvme_qpair_process_completions (ReadData.Ioqpair, 0);
    if (rc < 0) {
        Status = EFI_DEVICE_ERROR;
        break;
    }
  }

  if (Is_Completed == ERROR_IN_COMPLETION) {
    DEBUG ((DEBUG_ERROR, "NvmeOfCliRead: Error In Process Read Data\n"));
    Status = EFI_INVALID_PARAMETER;    
  }
  return Status;  
}


/**
  Check if controller use block I/O path.

  @param   Key

  @retval  FALSE   if we find the same connection in block driver
  @retval  TRUE    if we do not find the connection
**/
BOOLEAN
EFIAPI
NvmeOfCliIsBlockCtrlr (CHAR16  **Key)
{
  LIST_ENTRY                *Entry = NULL;
  NVMEOF_CLI_CTRL_MAPPING   *MappingList = NULL;
  CHAR8                     MapKey8[KEY_SIZE] = { 0 };
  CHAR8                     TraddrCli[ADDR_SIZE] = { 0 };
  BOOLEAN                   Flag = TRUE;
  CHAR8                     SubnqnCli[ADDR_SIZE] = { 0 };
  UINTN                     NsidCli = 0;

  UnicodeStrToAsciiStrS (Key[2], MapKey8, KEY_SIZE);
  NET_LIST_FOR_EACH (Entry, &gCliCtrlMap->CliCtrlrList) {
    MappingList = NET_LIST_USER_STRUCT (Entry, NVMEOF_CLI_CTRL_MAPPING, CliCtrlrList);
    if (!AsciiStriCmp (MapKey8, MappingList->Key)) {
      CopyMem (TraddrCli, MappingList->Traddr, ADDR_SIZE);
      CopyMem (SubnqnCli, MappingList->Subnqn, ADDR_SIZE);
      NsidCli = MappingList->Nsid;
    }
  }
  NET_LIST_FOR_EACH (Entry, &CtrlrInfo->CliCtrlrList) {
    MappingList = NET_LIST_USER_STRUCT (Entry, NVMEOF_CLI_CTRL_MAPPING, CliCtrlrList);
    if ((!AsciiStriCmp (TraddrCli, MappingList->Traddr)) && (!AsciiStriCmp (SubnqnCli, MappingList->Subnqn))
      && NsidCli == MappingList->Nsid) {
      Flag = FALSE;
      break;
    }
  }

  return Flag;
}

/**
  Reset the Device.

  @param  Ctrlr                Controller information

  @retval EFI_SUCCESS          The device was reset.
  @retval EFI_DEVICE_ERROR     The device is not functioning properly and could
                               not be reset.
**/

UINT8
EFIAPI
NvmeOfCliReset (struct spdk_nvme_ctrlr *Ctrlr, CHAR16 **Key)
{
  UINT8                          Status = 0;
  LIST_ENTRY                     *Entry = NULL;
  NVMEOF_CLI_CTRL_MAPPING        *MappingList = NULL; 
  struct spdk_nvme_qpair         *Io_qpair;
  BOOLEAN Flag = FALSE;

  Flag = NvmeOfCliIsBlockCtrlr (Key);
  if (Flag) {
    Status = spdk_nvme_ctrlr_reset (Ctrlr);
    if (Status != 0) {
      Print (L"Reset Error\n");
      return Status;
    }

    Io_qpair = spdk_nvme_ctrlr_alloc_io_qpair (Ctrlr, NULL, 0);
    if (Io_qpair == NULL) {
      DEBUG ((DEBUG_ERROR, "spdk_nvme_ctrlr_alloc_io_qpair() failed\n"));
      Status = 1;
      return Status;
    }
    // Saved the new Io_qpair
    NET_LIST_FOR_EACH (Entry, &gCliCtrlMap->CliCtrlrList) {
      MappingList = NET_LIST_USER_STRUCT (Entry, NVMEOF_CLI_CTRL_MAPPING, CliCtrlrList);
      if (Ctrlr == MappingList->Ctrlr) {
        MappingList->Ioqpair = Io_qpair;
      }
    }
  } else {
    Print (L"Reset can't be done as there is an active Block I/O device from same target and port\n");
    Status = 1;
  }
  return Status;
}

/**
 NVMEOF Driver version
 @retval Driver version
**/

UINTN
EFIAPI
NvmeOfCliVersion ()
{
  return NVMEOF_DRIVER_VERSION;
}

/**
  Nvmeof Cli Disconnect.

  @param  DisconnectData data

  @retval None
**/

VOID
EFIAPI
NvmeOfCliDisconnect (
  NVMEOF_CLI_DISCONNECT DisconnectData,
  CHAR16 **Key
  )
{
  spdk_nvme_detach (DisconnectData.Ctrlr);
  NvmeOfCliDeleteMapEntries (DisconnectData.Ctrlr); 
  Print (L"Disconnected Successfully\n");
}

/*
  Get the serial number and model number from buffer
  @param  String
  @retval None
*/
UINT8 ReturnStr[21];
static 
VOID
GetSerialModelStr (
  const UINT8 *sn
  )
{
  UINT8 i = 0;

  while (sn[i] != ' ') {
    ReturnStr[i] = sn[i];
    i++;
    if (i >= (sizeof (ReturnStr)-1)) {
      break;
    }
  }
  ReturnStr[i] = '\0';  
}


/**
  Nvmeof Cli List.

  @retval None
**/
VOID
EFIAPI
NvmeOfCliList ()
{
  LIST_ENTRY                         *Entry = NULL;
  NVMEOF_CLI_CTRL_MAPPING            *MappingList; 
  const struct spdk_nvme_ctrlr_data  *cdata;
  struct spdk_nvme_ns                *ns;
  const struct spdk_nvme_ns_data     *nsdata;
  UINT64                             sector_size;
  const struct spdk_uuid             *uuid;
  char                               uuid_str[SPDK_UUID_STRING_LEN];

  NET_LIST_FOR_EACH (Entry, &gCliCtrlMap->CliCtrlrList) {
    MappingList = NET_LIST_USER_STRUCT (Entry, NVMEOF_CLI_CTRL_MAPPING, CliCtrlrList);
    cdata = spdk_nvme_ctrlr_get_data (MappingList->Ctrlr);
    ns = spdk_nvme_ctrlr_get_ns (MappingList->Ctrlr, MappingList->Nsid);
    sector_size = spdk_nvme_ns_get_sector_size (ns);
    nsdata = spdk_nvme_ns_get_data (ns);    
    uuid = spdk_nvme_ns_get_uuid (ns);
    if (uuid == NULL) {
       uuid = (struct spdk_uuid *)spdk_nvme_ns_get_nguid (ns);
    }

    Print(L"-----------\n");
    Print(L"Node      : %a \n", MappingList->Key);
    if (uuid) {
      spdk_uuid_fmt_lower (uuid_str, sizeof (uuid_str), uuid);
      Print (L"NID       : %a\n", uuid_str);
    }
    GetSerialModelStr ((const UINT8 *)cdata->sn);
    Print (L"SN        : %a \n", ReturnStr);
    GetSerialModelStr ((const UINT8 *)cdata->mn);
    Print (L"Model     : %a \n", ReturnStr);
    Print (L"NSID      : %d \n", MappingList->Nsid);
    if (((long long)nsdata->nuse * sector_size / (1024 * 1024 * 1024)) == 0)
      Print (L"Usage     : %lld MB \n", (long long)nsdata->nuse * sector_size / (1024 * 1024));
    else
      Print (L"Usage     : %lld GiB \n", (long long)nsdata->nuse * sector_size / (1024 * 1024 * 1024));
    Print (L"Format    : %d \n", sector_size);
    Print (L"FW Rev    : %a \n", cdata->fr);  
  }
}

/**
  Nvmeof Cli List Connect.

  @retval None
**/

VOID
EFIAPI
NvmeOfCliListConnect ()
{
  LIST_ENTRY                         *Entry = NULL;
  NVMEOF_CLI_CTRL_MAPPING            *MappingList;
  UINT64                             sector_size = 0;
  const struct spdk_nvme_ctrlr_data  *cdata;
  struct spdk_nvme_ns                *ns;
  const struct spdk_nvme_ns_data     *nsdata;
  const struct spdk_uuid             *uuid;
  char                               uuid_str[SPDK_UUID_STRING_LEN];

  NET_LIST_FOR_EACH (Entry, &CtrlrInfo->CliCtrlrList) {
    MappingList = NET_LIST_USER_STRUCT (Entry, NVMEOF_CLI_CTRL_MAPPING, CliCtrlrList);
    cdata = spdk_nvme_ctrlr_get_data (MappingList->Ctrlr);
    ns = spdk_nvme_ctrlr_get_ns (MappingList->Ctrlr, MappingList->Nsid);
    sector_size = spdk_nvme_ns_get_sector_size (ns);
    nsdata = spdk_nvme_ns_get_data (ns);
    uuid = spdk_nvme_ns_get_uuid (ns);
    if (uuid == NULL) {
        uuid = (struct spdk_uuid *) spdk_nvme_ns_get_nguid (ns);
    }
    Print (L"-----------\n");   
    GetSerialModelStr ((const UINT8 *)cdata->sn);
    Print (L"SN        : %a \n", ReturnStr);
    if (uuid) {
      spdk_uuid_fmt_lower (uuid_str, sizeof (uuid_str), uuid);
      Print (L"NID       : %a\n", uuid_str);
    }
    GetSerialModelStr ((const UINT8 *)cdata->mn);
    Print (L"Model     : %a \n", ReturnStr);
    Print (L"NSID      : %d \n", MappingList->Nsid);
    if (((long long)nsdata->nuse * sector_size / (1024 * 1024 * 1024)) == 0)
      Print (L"Usage     : %lld MB \n", (long long)nsdata->nuse * sector_size / (1024 * 1024));
    else
      Print (L"Usage     : %lld GiB \n", (long long)nsdata->nuse * sector_size / (1024 * 1024 * 1024));
    Print (L"Format    : %d \n", sector_size);
    Print (L"FW Rev    : %a \n", cdata->fr);
  }
}

/**
  Write some blocks to the device.

  @param  WriteData              write data
  @retval EFI_SUCCESS            Data are written into the buffer.
  @retval EFI_DEVICE_ERROR       Fail to write all the data.

**/

EFI_STATUS
EFIAPI
NvmeOfCliWrite (
  NVMEOF_READ_WRITE_DATA WriteData
  )
{
  EFI_STATUS Status           = EFI_SUCCESS;
  UINT8      Is_Completed     = 0;  
  struct     spdk_nvme_ns *ns = NULL;  
  int        rc               = 0;

  /*ctrl and NsId getting from global structure of type  (struct spdk_nvme_dev)*/
  ns = spdk_nvme_ctrlr_get_ns (WriteData.Ctrlr, WriteData.Nsid);

  //redirecting to SPDK lib write function
  Status = spdk_nvme_ns_cmd_write (ns,
             WriteData.Ioqpair,
             WriteData.Payload,
             WriteData.Startblock,
             1,
             NvmeOfCliIoComplete,
             &Is_Completed,
             0);
  while (!Is_Completed) {
    rc = spdk_nvme_qpair_process_completions (WriteData.Ioqpair, 0);
    if (rc < 0) {
      Status = EFI_DEVICE_ERROR;
      break;
    }
  }

  if (Is_Completed == ERROR_IN_COMPLETION) {
    DEBUG ((DEBUG_ERROR, "NvmeOfCliWrite: Error In Process Write Data\n"));
    Status = EFI_DEVICE_ERROR;
  }
  return Status;
}

/*
 Print the identfy command data for given name space
*/
static 
VOID
print_namespace (struct spdk_nvme_ns *ns)
{
  const struct spdk_nvme_ns_data   *nsdata;
  const struct spdk_uuid           *uuid;
  uint32_t                         i;
  uint32_t                         flags;
  char                             uuid_str[SPDK_UUID_STRING_LEN];
  uint32_t                         blocksize;

  nsdata = spdk_nvme_ns_get_data (ns);
  flags = spdk_nvme_ns_get_flags (ns);

  Print (L"Namespace ID:%d\n", spdk_nvme_ns_get_id (ns));

  /* This function is only called for active namespaces. */
  assert (spdk_nvme_ns_is_active (ns));

  Print (L"Deallocate:                            %a\n",
    (flags & SPDK_NVME_NS_DEALLOCATE_SUPPORTED) ? "Supported" : "Not Supported");
  Print (L"Deallocated/Unwritten Error:           %a\n",
    nsdata->nsfeat.dealloc_or_unwritten_error ? "Supported" : "Not Supported");
  Print (L"Deallocated Read Value:                %a\n",
    nsdata->dlfeat.bits.read_value == SPDK_NVME_DEALLOC_READ_00 ? "All 0x00" :
    nsdata->dlfeat.bits.read_value == SPDK_NVME_DEALLOC_READ_FF ? "All 0xFF" :
    "Unknown");
  Print (L"Deallocate in Write Zeroes:            %a\n",
    nsdata->dlfeat.bits.write_zero_deallocate ? "Supported" : "Not Supported");
  Print (L"Deallocated Guard Field:               %a\n",
    nsdata->dlfeat.bits.guard_value ? "CRC for Read Value" : "0xFFFF");
  Print (L"Flush:                                 %a\n",
    (flags & SPDK_NVME_NS_FLUSH_SUPPORTED) ? "Supported" : "Not Supported");
  Print (L"Reservation:                           %a\n",
    (flags & SPDK_NVME_NS_RESERVATION_SUPPORTED) ? "Supported" : "Not Supported");
  if (flags & SPDK_NVME_NS_DPS_PI_SUPPORTED) {
    Print (L"End-to-End Data Protection:            Supported\n");
    Print (L"Protection Type:                       Type%d\n", nsdata->dps.pit);
    Print (L"Protection Information Transferred as: %a\n",
      nsdata->dps.md_start ? "First 8 Bytes" : "Last 8 Bytes");
  }
  if (nsdata->lbaf[nsdata->flbas.format].ms > 0) {
    Print (L"Metadata Transferred as:               %a\n",
      nsdata->flbas.extended ? "Extended Data LBA" : "Separate Metadata Buffer");
  }
  Print (L"Namespace Sharing Capabilities:        %a\n",
    nsdata->nmic.can_share ? "Multiple Controllers" : "Private");
  blocksize = 1 << nsdata->lbaf[nsdata->flbas.format].lbads;
  Print (L"Size (in LBAs):                        %lld (%lldGiB)\n",
    (long long)nsdata->nsze,
    (long long)nsdata->nsze * blocksize / 1024 / 1024 / 1024);
  Print (L"Capacity (in LBAs):                    %lld (%lldGiB)\n",
    (long long)nsdata->ncap,
    (long long)nsdata->ncap * blocksize / 1024 / 1024 / 1024);
  Print (L"Utilization (in LBAs):                 %lld (%lldGiB)\n",
    (long long)nsdata->nuse,
    (long long)nsdata->nuse * blocksize / 1024 / 1024 / 1024);
  if (nsdata->noiob) {
    Print (L"Optimal I/O Boundary:                  %d blocks\n", nsdata->noiob);
  }
  uuid = spdk_nvme_ns_get_uuid (ns);
  if (uuid == NULL) {
      uuid = (struct spdk_uuid *) spdk_nvme_ns_get_nguid (ns);
  }
  if (uuid) {
    spdk_uuid_fmt_lower (uuid_str, sizeof (uuid_str), uuid);
    Print (L"UUID:                                  %a\n", uuid_str);
  }
  Print (L"Thin Provisioning:                     %a\n",
    nsdata->nsfeat.thin_prov ? "Supported" : "Not Supported");
  Print (L"Per-NS Atomic Units:                   %a\n",
    nsdata->nsfeat.ns_atomic_write_unit ? "Yes" : "No");
  if (nsdata->nsfeat.ns_atomic_write_unit) {
    if (nsdata->nawun) {
      Print (L"  Atomic Write Unit (Normal):          %d\n", nsdata->nawun + 1);
    }

    if (nsdata->nawupf) {
      Print (L"  Atomic Write Unit (PFail):           %d\n", nsdata->nawupf + 1);
    }

    if (nsdata->nacwu) {
      Print (L"  Atomic Compare & Write Unit:         %d\n", nsdata->nacwu + 1);
    }

    Print (L"  Atomic Boundary Size (Normal):       %d\n", nsdata->nabsn);
    Print (L"  Atomic Boundary Size (PFail):        %d\n", nsdata->nabspf);
    Print (L"  Atomic Boundary Offset:              %d\n", nsdata->nabo);
  }

  Print (L"NGUID/EUI64 Never Reused:              %a\n",
    nsdata->nsfeat.guid_never_reused ? "Yes" : "No");
  Print (L"Number of LBA Formats:                 %d\n", nsdata->nlbaf + 1);
  Print (L"Current LBA Format:                    LBA Format #%02d\n",
    nsdata->flbas.format);
  for (i = 0; i <= nsdata->nlbaf; i++)
    Print (L"LBA Format #%02d: Data Size: %5d  Metadata Size: %5d\n",
      i, 1 << nsdata->lbaf[i].lbads, nsdata->lbaf[i].ms);
  Print (L"\n");
}

/*
 Print the identify controller data
*/

static 
NVMEOF_CLI_IDENTIFY
PrintIdentifyControllerData (NVMEOF_CLI_IDENTIFY IdentifyData)
{  
  NVMEOF_CLI_IDENTIFY ReturnData       = { 0 };
  union spdk_nvme_cap_register cap     = spdk_nvme_ctrlr_get_regs_cap (IdentifyData.Ctrlr);
  union spdk_nvme_vs_register vs       = spdk_nvme_ctrlr_get_regs_vs (IdentifyData.Ctrlr);
  union spdk_nvme_cmbsz_register cmbsz = spdk_nvme_ctrlr_get_regs_cmbsz (IdentifyData.Ctrlr);
  ReturnData.Cdata                     = spdk_nvme_ctrlr_get_data (IdentifyData.Ctrlr);

  Print (L"=====================================================\n");
  Print (L"Controller Capabilities/Features\n");
  Print (L"================================\n");
  Print (L"Vendor ID:                             %04d\n", ReturnData.Cdata->vid);
  Print (L"Subsystem Vendor ID:                   %04d\n", ReturnData.Cdata->ssvid);
  Print (L"Serial Number:                         ");
  GetSerialModelStr ((const UINT8 *)ReturnData.Cdata->sn);
  Print (L"%a", ReturnStr);
  Print (L"\n");
  Print (L"Model Number:                          ");
  GetSerialModelStr ((const UINT8 *)ReturnData.Cdata->mn);
  Print (L"%a", ReturnStr);
  Print (L"\n");
  Print (L"Firmware Version:                      ");
  Print (L"%a", ReturnData.Cdata->fr);
  Print (L"\n");
  Print (L"Recommended Arb Burst:                 %d\n", ReturnData.Cdata->rab);
  Print (L"IEEE OUI Identifier:                   %02d %02d %02d\n",
    ReturnData.Cdata->ieee[0], ReturnData.Cdata->ieee[1], ReturnData.Cdata->ieee[2]);
  Print (L"Multi-path I/O\n");
  Print (L"May have multiple subsystem ports:     %a\n", ReturnData.Cdata->cmic.multi_port ? "Yes" : "No");
  Print (L"May be connected to multiple hosts:    %a\n", ReturnData.Cdata->cmic.multi_host ? "Yes" : "No");
  Print (L"Associated with SR-IOV VF:             %a\n", ReturnData.Cdata->cmic.sr_iov ? "Yes" : "No");
  Print (L"Max Data Transfer Size:                ");
  if (ReturnData.Cdata->mdts == 0) {
    Print (L"Unlimited\n");
  } else {
    Print (L"%" PRIu64 "\n", (uint64_t)1 << (12 + cap.bits.mpsmin + ReturnData.Cdata->mdts));
  }
  Print (L"Max Number of Namespaces:              %d\n", ReturnData.Cdata->nn);
  Print (L"NVMe Specification Version (VS):       %d.%d", vs.bits.mjr, vs.bits.mnr);
  if (vs.bits.ter) {
    Print (L".%d", vs.bits.ter);
  }
  Print (L"\n");
  if (ReturnData.Cdata->ver.raw != 0) {
    Print (L"NVMe Specification Version (Identify): %d.%d", ReturnData.Cdata->ver.bits.mjr, ReturnData.Cdata->ver.bits.mnr);
    if (ReturnData.Cdata->ver.bits.ter) {
      Print (L".%d", ReturnData.Cdata->ver.bits.ter);
    }
    Print (L"\n");
  }

  Print (L"Maximum Queue Entries:                 %d\n", cap.bits.mqes + 1);
  Print (L"Contiguous Queues Required:            %a\n", cap.bits.cqr ? "Yes" : "No");
  Print (L"Arbitration Mechanisms Supported\n");
  Print (L"  Weighted Round Robin:                %a\n",
    cap.bits.ams & SPDK_NVME_CAP_AMS_WRR ? "Supported" : "Not Supported");
  Print (L"  Vendor Specific:                     %a\n",
    cap.bits.ams & SPDK_NVME_CAP_AMS_VS ? "Supported" : "Not Supported");
  Print (L"Reset Timeout:                         %" PRIu64 " ms\n", (uint64_t)500 * cap.bits.to);
  Print (L"Doorbell Stride:                       %" PRIu64 " bytes\n",
    (uint64_t)1 << (2 + cap.bits.dstrd));
  Print (L"NVM Subsystem Reset:                   %a\n",
    cap.bits.nssrs ? "Supported" : "Not Supported");
  Print (L"Command Sets Supported\n");
  Print (L"  NVM Command Set:                     %a\n",
    cap.bits.css & SPDK_NVME_CAP_CSS_NVM ? "Supported" : "Not Supported");
  Print (L"Boot Partition:                        %a\n",
    cap.bits.bps ? "Supported" : "Not Supported");
  Print (L"Memory Page Size Minimum:              %" PRIu64 " bytes\n",
    (uint64_t)1 << (12 + cap.bits.mpsmin));
  Print (L"Memory Page Size Maximum:              %" PRIu64 " bytes\n",
    (uint64_t)1 << (12 + cap.bits.mpsmax));
  Print (L"Optional Asynchronous Events Supported\n");
  Print (L"  Namespace Attribute Notices:         %a\n",
    ReturnData.Cdata->oaes.ns_attribute_notices ? "Supported" : "Not Supported");
  Print (L"  Firmware Activation Notices:         %a\n",
    ReturnData.Cdata->oaes.fw_activation_notices ? "Supported" : "Not Supported");

  Print (L"128-bit Host Identifier:               %a\n",
    ReturnData.Cdata->ctratt.host_id_exhid_supported ? "Supported" : "Not Supported");
  Print (L"\n");

  Print (L"Controller Memory Buffer Support\n");
  Print (L"================================\n");
  if (cmbsz.raw != 0) {
    uint64_t size = cmbsz.bits.sz;

    /* Convert the size to bytes by multiplying by the granularity.
       By spec, szu is at most 6 and sz is 20 bits, so size requires
       at most 56 bits. */
    size *= (0x1000 << (cmbsz.bits.szu * 4));

    Print (L"Supported:                             Yes\n");
    Print (L"Total Size:                            %llu bytes\n", size);
    Print (L"Submission Queues in CMB:              %a\n",
      cmbsz.bits.sqs ? "Supported" : "Not Supported");
    Print (L"Completion Queues in CMB:              %a\n",
      cmbsz.bits.cqs ? "Supported" : "Not Supported");
    Print (L"Read data and metadata in CMB          %a\n",
      cmbsz.bits.rds ? "Supported" : "Not Supported");
    Print (L"Write data and metadata in CMB:        %a\n",
      cmbsz.bits.wds ? "Supported" : "Not Supported");
  } else {
    Print (L"Supported:                             No\n");
  }
  Print (L"\n");
  Print (L"Admin Command Set Attributes\n");
  Print (L"============================\n");
  Print (L"Security Send/Receive:                 %a\n",
    ReturnData.Cdata->oacs.security ? "Supported" : "Not Supported");
  Print (L"Format NVM:                            %a\n",
    ReturnData.Cdata->oacs.format ? "Supported" : "Not Supported");
  Print (L"Firmware Activate/Download:            %a\n",
    ReturnData.Cdata->oacs.firmware ? "Supported" : "Not Supported");
  Print (L"Namespace Management:                  %a\n",
    ReturnData.Cdata->oacs.ns_manage ? "Supported" : "Not Supported");
  Print (L"Device Self-Test:                      %a\n",
    ReturnData.Cdata->oacs.device_self_test ? "Supported" : "Not Supported");
  Print (L"Directives:                            %a\n",
    ReturnData.Cdata->oacs.directives ? "Supported" : "Not Supported");
  Print (L"NVMe-MI:                               %a\n",
    ReturnData.Cdata->oacs.nvme_mi ? "Supported" : "Not Supported");
  Print (L"Virtualization Management:             %a\n",
    ReturnData.Cdata->oacs.virtualization_management ? "Supported" : "Not Supported");
  Print (L"Doorbell Buffer Config:                %a\n",
    ReturnData.Cdata->oacs.doorbell_buffer_config ? "Supported" : "Not Supported");
  Print (L"Abort Command Limit:                   %d\n", ReturnData.Cdata->acl + 1);
  Print (L"Async Event Request Limit:             %d\n", ReturnData.Cdata->aerl + 1);
  Print (L"Number of Firmware Slots:              ");
  if (ReturnData.Cdata->oacs.firmware != 0) {
    Print (L"%d\n", ReturnData.Cdata->frmw.num_slots);
  } else {
    Print (L"N/A\n");
  }
  Print (L"Firmware Slot 1 Read-Only:             ");
  if (ReturnData.Cdata->oacs.firmware != 0) {
    Print (L"%a\n", ReturnData.Cdata->frmw.slot1_ro ? "Yes" : "No");
  } else {
    Print (L"N/A\n");
  }
  if (ReturnData.Cdata->fwug == 0x00) {
    Print (L"Firmware Update Granularity:           No Information Provided\n");
  } else if (ReturnData.Cdata->fwug == 0xFF) {
    Print (L"Firmware Update Granularity:           No Restriction\n");
  } else {
    Print (L"Firmware Update Granularity:           %u KiB\n",
      ReturnData.Cdata->fwug * 4);
  }
  Print (L"Per-Namespace SMART Log:               %a\n",
    ReturnData.Cdata->lpa.ns_smart ? "Yes" : "No");
  Print (L"Command Effects Log Page:              %a\n",
    ReturnData.Cdata->lpa.celp ? "Supported" : "Not Supported");
  Print (L"Get Log Page Extended Data:            %a\n",
    ReturnData.Cdata->lpa.edlp ? "Supported" : "Not Supported");
  Print (L"Telemetry Log Pages:                   %a\n",
    ReturnData.Cdata->lpa.telemetry ? "Supported" : "Not Supported");
  Print (L"Error Log Page Entries Supported:      %d\n", ReturnData.Cdata->elpe + 1);
  if (ReturnData.Cdata->kas == 0) {
    Print (L"Keep Alive:                            Not Supported\n");
  } else {
    Print (L"Keep Alive:                            Supported\n");
    Print (L"Keep Alive Granularity:                %u ms\n",
      ReturnData.Cdata->kas * 100);
  }
  Print (L"\n");

  Print (L"NVM Command Set Attributes\n");
  Print (L"==========================\n");
  Print (L"Submission Queue Entry Size\n");
  Print (L"  Max:                       %d\n", 1 << ReturnData.Cdata->sqes.max);
  Print (L"  Min:                       %d\n", 1 << ReturnData.Cdata->sqes.min);
  Print (L"Completion Queue Entry Size\n");
  Print (L"  Max:                       %d\n", 1 << ReturnData.Cdata->cqes.max);
  Print (L"  Min:                       %d\n", 1 << ReturnData.Cdata->cqes.min);
  Print (L"Number of Namespaces:        %d\n", ReturnData.Cdata->nn);
  Print (L"Compare Command:             %a\n",
    ReturnData.Cdata->oncs.compare ? "Supported" : "Not Supported");
  Print (L"Write Uncorrectable Command: %a\n",
    ReturnData.Cdata->oncs.write_unc ? "Supported" : "Not Supported");
  Print (L"Dataset Management Command:  %a\n",
    ReturnData.Cdata->oncs.dsm ? "Supported" : "Not Supported");
  Print (L"Write Zeroes Command:        %a\n",
    ReturnData.Cdata->oncs.write_zeroes ? "Supported" : "Not Supported");
  Print (L"Set Features Save Field:     %a\n",
    ReturnData.Cdata->oncs.set_features_save ? "Supported" : "Not Supported");
  Print (L"Reservations:                %a\n",
    ReturnData.Cdata->oncs.reservations ? "Supported" : "Not Supported");
  Print (L"Timestamp:                   %a\n",
    ReturnData.Cdata->oncs.timestamp ? "Supported" : "Not Supported");
  Print (L"Volatile Write Cache:        %a\n",
    ReturnData.Cdata->vwc.present ? "Present" : "Not Present");
  Print (L"Atomic Write Unit (Normal):  %d\n", ReturnData.Cdata->awun + 1);
  Print (L"Atomic Write Unit (PFail):   %d\n", ReturnData.Cdata->awupf + 1);
  Print (L"Atomic Compare & Write Unit: %d\n", ReturnData.Cdata->acwu + 1);
  Print (L"Fused Compare & Write:       %a\n",
    ReturnData.Cdata->fuses.compare_and_write ? "Supported" : "Not Supported");
  Print (L"Scatter-Gather List\n");
  Print (L"  SGL Command Set:           %a\n",
    ReturnData.Cdata->sgls.supported == SPDK_NVME_SGLS_SUPPORTED ? "Supported" :
    ReturnData.Cdata->sgls.supported == SPDK_NVME_SGLS_SUPPORTED_DWORD_ALIGNED ? "Supported (Dword aligned)" :
    "Not Supported");
  Print(L"  SGL Keyed:                 %a\n",
    ReturnData.Cdata->sgls.keyed_sgl ? "Supported" : "Not Supported");
  Print (L"  SGL Bit Bucket Descriptor: %a\n",
    ReturnData.Cdata->sgls.bit_bucket_descriptor ? "Supported" : "Not Supported");
  Print (L"  SGL Metadata Pointer:      %a\n",
    ReturnData.Cdata->sgls.metadata_pointer ? "Supported" : "Not Supported");
  Print (L"  Oversized SGL:             %a\n",
    ReturnData.Cdata->sgls.oversized_sgl ? "Supported" : "Not Supported");
  Print (L"  SGL Metadata Address:      %a\n",
    ReturnData.Cdata->sgls.metadata_address ? "Supported" : "Not Supported");
  Print (L"  SGL Offset:                %a\n",
    ReturnData.Cdata->sgls.sgl_offset ? "Supported" : "Not Supported");
  Print (L"  Transport SGL Data Block:  %a\n",
    ReturnData.Cdata->sgls.transport_sgl ? "Supported" : "Not Supported");
  Print (L"Replay Protected Memory Block:");
  if (ReturnData.Cdata->rpmbs.num_rpmb_units > 0) {
    Print (L"  Supported\n");
    Print (L"  Number of RPMB Units:  %d\n", ReturnData.Cdata->rpmbs.num_rpmb_units);
    Print (L"  Authentication Method: %a\n", ReturnData.Cdata->rpmbs.auth_method == 0 ? "HMAC SHA-256" : "Unknown");
    Print (L"  Total Size (in 128KB units) = %d\n", ReturnData.Cdata->rpmbs.total_size + 1);
    Print (L"  Access Size (in 512B units) = %d\n", ReturnData.Cdata->rpmbs.access_size + 1);
  } else {
    Print (L"  Not Supported\n");
  }
  Print (L"\n");

  if (ReturnData.Cdata->hctma.bits.supported) {
    Print (L"Host Controlled Thermal Management\n");
    Print (L"==================================\n");
    Print (L"Minimum Thermal Management Temperature:  ");
    if (ReturnData.Cdata->mntmt) {
      Print (L"%u Kelvin (%d Celsius)\n", ReturnData.Cdata->mntmt, (int)ReturnData.Cdata->mntmt - 273);
    } else {
      Print (L"Not Reported\n");
    }
    Print (L"Maximum Thermal Managment Temperature:   ");
    if (ReturnData.Cdata->mxtmt) {
      Print (L"%u Kelvin (%d Celsius)\n", ReturnData.Cdata->mxtmt, (int)ReturnData.Cdata->mxtmt - 273);
    } else {
      Print (L"Not Reported\n");
    }
    Print (L"\n");
  }

  Print (L"Active Namespace\n");
  Print (L"=================\n");
  print_namespace (spdk_nvme_ctrlr_get_ns (IdentifyData.Ctrlr, IdentifyData.NsId));
  return ReturnData;
}

/**
  the identify command data for given name space

  @param  IdentifyData           Identify Data

  @retval EFI_SUCCESS            .

**/
NVMEOF_CLI_IDENTIFY
EFIAPI
NvmeOfCliIdentify (NVMEOF_CLI_IDENTIFY IdentifyData)
{
  NVMEOF_CLI_IDENTIFY    ReturnData;

  ReturnData = PrintIdentifyControllerData (IdentifyData);
  return ReturnData;
}

/*
  EFI Protocol call back function
*/
NVMEOF_PASSTHROUGH_PROTOCOL gNvmeofPassThroughInstance = {
                              NvmeOfCliConnect,
                              NvmeOfCliRead,
                              NvmeOfCliWrite,
                              NvmeOfCliIdentify,
                              NvmeOfCliDisconnect,
                              NvmeOfCliReset,
                              NvmeOfCliList,
                              NvmeOfCliListConnect,
                              NvmeOfCliVersion,
                              &gCliCtrlMap,
                              NvmeOfGetBootDesc
                              };

STATIC
BOOLEAN
NvmeOfCliProbeCallback (
  IN VOID *CallbackCtx,
  IN const struct spdk_nvme_transport_id *Trid,
  IN struct spdk_nvme_ctrlr_opts *Opts
  )
{
  EFI_GUID  NvmeOfCliUuid = NVMEOF_CLI_UUID;
  DEBUG ((DEBUG_INFO, "Passthrough, Attaching to %a\n", Trid->traddr));
  //For CLI use Zero KATO
  Opts->keep_alive_timeout_ms = 0;
  CopyMem (Opts->extended_host_id, &NvmeOfCliUuid, sizeof (Opts->extended_host_id));
  if (ProbeconnectData != NULL) {
    if (ProbeconnectData->UseHostNqn == 1) {
      CopyMem (Opts->hostnqn, ProbeconnectData->Hostnqn, sizeof (ProbeconnectData->Hostnqn));
    }
  }
  return TRUE;
}

/**
  SPDK Probe Cli Callback function

  @param  CallbackCtx                 Driver Private context.
  @param  spdk_nvme_transport_id      SPDK Transport Id structure.
  @param  spdk_nvme_ctrlr             SPDK NVMe Control structure.
  @param  Opts                        SPDK Controller options

  @retval None
**/

STATIC
VOID
NvmeOfAttachCliCallback (
  IN VOID *CallbackCtx,
  IN const struct spdk_nvme_transport_id *Trid,
  IN struct spdk_nvme_ctrlr *Ctrlr,
  IN const struct spdk_nvme_ctrlr_opts *Opts
  )
{
  UINT32                            NumOfNamespaces = 0;
  UINT32                            Nsid            = 0;
  struct spdk_nvme_ns               *Namespace;
  NVMEOF_DEVICE_PRIVATE_DATA        *Device         = NULL;
  NVMEOF_DRIVER_DATA                *Private;
  UINT8                             uuidstr[SPDK_UUID_STRING_LEN];
  UINTN                             ActiveNs        = 1;
  UINT16                            Cntliduser      = 0;
  CHAR8                             Key[10]         = { 0 };
  NVMEOF_CLI_CTRL_MAPPING           *MappingData;
  NVMEOF_CLI_CTRL_MAPPING           *MappingList    = NULL;

  Private = (NVMEOF_DRIVER_DATA *)CallbackCtx;
  // Get the number of namespaces managed by the controller
  NumOfNamespaces = spdk_nvme_ctrlr_get_num_ns (Ctrlr);
  // Iterate through List to get user controller id
  if (!IsListEmpty (&gCliCtrlMap->CliCtrlrList)) {
    MappingList = NET_LIST_TAIL (&gCliCtrlMap->CliCtrlrList, NVMEOF_CLI_CTRL_MAPPING, CliCtrlrList);
    Cntliduser = MappingList->Cntliduser;
  }
  Cntliduser++;

  // Namespace IDs start at 1, not 0.
  for (Nsid = 1; Nsid <= NumOfNamespaces; Nsid++) {
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
      goto Exit;
    }

    Device->Signature = NVMEOF_DEVICE_PRIVATE_DATA_SIGNATURE;
    Device->NameSpace = Namespace;
    Device->NamespaceId = spdk_nvme_ns_get_id (Namespace);
    Device->NamespaceUuid = spdk_nvme_ns_get_uuid (Namespace);
    if (Device->NamespaceUuid == NULL) {
        Device->NamespaceUuid = (struct spdk_uuid *) spdk_nvme_ns_get_nguid (Namespace);
    }

    Device->Controller = Private;
    Device->qpair = spdk_nvme_ctrlr_alloc_io_qpair (Ctrlr, NULL, 0);
    if (Device->qpair == NULL) {
      DEBUG ((DEBUG_ERROR, "spdk_nvme_ctrlr_alloc_io_qpair() failed\n"));
      goto Exit;
    }

    MappingData = AllocateZeroPool (sizeof (NVMEOF_CLI_CTRL_MAPPING));
    if (MappingData == NULL) {
      Print(L"Error allocating MaapingData\n");
      return;
    }
    MappingData->Ctrlr = Ctrlr;
    sprintf (Key, "nvme%dn%d", Cntliduser, ActiveNs);// Node : Eg nvme0n1,nvme0n2 
    strcpy (MappingData->Key, Key);
    MappingData->Ioqpair = Device->qpair;
    MappingData->Nsid = Device->NamespaceId;
    strcpy (MappingData->Traddr, Trid->traddr);
    strcpy (MappingData->Subnqn, Trid->subnqn);
    MappingData->Cntliduser = Cntliduser;    
    ActiveNs++;
    InsertTailList (&gCliCtrlMap->CliCtrlrList, &MappingData->CliCtrlrList);

    DEBUG ((DEBUG_INFO, "NumOfNamespaces : %d \n", NumOfNamespaces));
    DEBUG ((DEBUG_INFO, "NameSpace Id : %d \n", spdk_nvme_ns_get_id (Namespace)));
    DEBUG ((DEBUG_INFO, "NameSpace flags : %d \n", spdk_nvme_ns_get_flags (Namespace)));
    DEBUG ((DEBUG_INFO, "NameSpace sector_size: %d \n", spdk_nvme_ns_get_sector_size (Namespace)));
    spdk_uuid_fmt_lower ((char *)uuidstr, SPDK_UUID_STRING_LEN, Device->NamespaceUuid);
    DEBUG ((DEBUG_INFO, "NameSpace UUID: %a\n", uuidstr));
    DEBUG ((DEBUG_INFO, "NameSpace extended_sector_size : %d \n", spdk_nvme_ns_get_extended_sector_size (Namespace)));
    DEBUG ((DEBUG_INFO, "NameSpace num_sectors: %d\n", spdk_nvme_ns_get_num_sectors (Namespace)));
  }
  return;

  Exit:
  if (Device != NULL) {
   FreePool (Device);
  }
  return;
}

 /**
  Populates transport data and calls SPDK probe

  @param  NVMEOF_DRIVER_DATA             Driver Private context.
  @param  NVMEOF_ATTEMPT_CONFIG_NVDATA   Attempt data
  @param  IpVersion                      IP version

  @retval EFI_SUCCESS                    All OK
  @retval EFI_OUT_OF_RESOURCES           Out of Resources
  @retval EFI_NOT_FOUND                  Resource Not Found
**/

EFI_STATUS
NvmeOfCliProbeControllers (
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

  Trid = AllocateZeroPool (sizeof (struct spdk_nvme_transport_id));
  if (Trid == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  Trid->trtype = SPDK_NVME_TRANSPORT_TCP;
  CopyMem(Trid->trstring, SPDK_NVME_TRANSPORT_NAME_TCP, SPDK_NVMF_TRSTRING_MAX_LEN);
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
    CopyMem (Trid->traddr, Ipv4Addr, sizeof(Ipv4Addr));
  } else if ((IpVersion == IP_VERSION_6) &&
    (AttemptConfigData->SubsysConfigData.NvmeofIpMode == IP_MODE_IP6 ||
      AttemptConfigData->AutoConfigureMode == IP_MODE_AUTOCONFIG_IP6)) {
    Trid->adrfam = SPDK_NVMF_ADRFAM_IPV6;
    NetLibIp6ToStr (&AttemptConfigData->SubsysConfigData.NvmeofSubSystemIp.v6, Ipv6Addr, sizeof (Ipv6Addr));
    UnicodeStrToAsciiStrS (Ipv6Addr, Trid->traddr, sizeof (Ipv6Addr));
  } else { //free trid
    return EFI_NOT_FOUND;
  }

  if (spdk_nvme_probe (Trid, Private, NvmeOfCliProbeCallback,
    NvmeOfAttachCliCallback, NULL) != 0) {
    DEBUG ((EFI_D_ERROR, "spdk_nvme_probe() failed for  %a\n", Trid->traddr));
    Print (L"Unable to connect to target\n");
    FreePool (Trid);
    return EFI_NOT_FOUND;
  } else {
    DEBUG ((DEBUG_INFO, "Probe Success\n"));
    Print (L"Connected Successfully\n");
  }
  FreePool (Trid);
  return EFI_SUCCESS;
}
  /**
  Install Controller Handler

  @param  NVMEOF_CONNECT_COMMAND     connect data

  @retval EFI_SUCCESS                    All OK
  @retval EFI_OUT_OF_RESOURCES           Out of Resources
  @retval EFI_NOT_FOUND                  Resource Not Found
**/

EFI_STATUS
EFIAPI
InstallControllerHandler (
  IN NVMEOF_CONNECT_COMMAND *ConnectData
  )
{
  UINTN                             DeviceHandleCount;
  EFI_HANDLE                        *DeviceHandleBuffer;
  UINTN                             Index;
  NVMEOF_DRIVER_DATA                *Private = NULL;
  UINT8                             IpVersion;
  EFI_STATUS                        Status = EFI_SUCCESS;
  EFI_MAC_ADDRESS                   MacAddr;
  UINTN                             HwAddressSize = 0;
  UINT16                            VlanId = 0;
  CHAR16                            MacString[NVMEOF_MAX_MAC_STRING_LEN];
  CHAR16                            AttemptMacString[NVMEOF_MAX_MAC_STRING_LEN];
  BOOLEAN                           AttemptFound = FALSE;
  EFI_GUID                          *TcpServiceBindingGuid;
  NVMEOF_ATTEMPT_CONFIG_NVDATA      AttemptData;
  CHAR8                             *EndPointer = NULL;

  //Set Attempt data
  CopyMem (AttemptData.AttemptName, "Attempt 1", sizeof(AttemptData.AttemptName));
  snprintf (AttemptData.MacString, NVMEOF_MAX_MAC_STRING_LEN, "%s", ConnectData->Mac);
  CopyMem (AttemptData.SubsysConfigData.NvmeofSubsysNqn, ConnectData->Nqn, NVMEOF_CLI_MAX_SIZE);
  AttemptData.SubsysConfigData.NvmeofIpMode = ConnectData->IpMode;
  AttemptData.SubsysConfigData.NvmeofDnsMode = FALSE;
  if (ConnectData->IpMode == 0) {
    AsciiStrToIpv4Address (ConnectData->LocalIp, &EndPointer, &AttemptData.SubsysConfigData.NvmeofSubsysHostIP.v4, NULL);
    AsciiStrToIpv4Address (ConnectData->Traddr, &EndPointer, &AttemptData.SubsysConfigData.NvmeofSubSystemIp.v4, NULL);
    AsciiStrToIpv4Address (ConnectData->SubnetMask, &EndPointer, &AttemptData.SubsysConfigData.NvmeofSubsysHostSubnetMask.v4, NULL);
    AsciiStrToIpv4Address (ConnectData->Gateway, &EndPointer, &AttemptData.SubsysConfigData.NvmeofSubsysHostGateway.v4, NULL);
    IpVersion = IP_VERSION_4;
  } else if (ConnectData->IpMode == 1) {
    AsciiStrToIpv6Address (ConnectData->LocalIp, &EndPointer, &AttemptData.SubsysConfigData.NvmeofSubsysHostIP.v6, NULL);
    AsciiStrToIpv6Address (ConnectData->Traddr, &EndPointer, &AttemptData.SubsysConfigData.NvmeofSubSystemIp.v6, NULL);
    IpVersion = IP_VERSION_6;
  } else {
    Status = EFI_INVALID_PARAMETER;
    Print (L"Invalid IpMode\n");
    return Status;
  }

  ProbeconnectData = ConnectData;
  AttemptData.SubsysConfigData.NvmeofSubsysPortId = ConnectData->Trsvcid;
  AttemptData.SubsysConfigData.NvmeofTimeout = CONNECT_TIMEOUT;
  DeviceHandleCount=0;
  DeviceHandleBuffer=NULL;
  if (IpVersion == IP_VERSION_4) {
    TcpServiceBindingGuid = &gEfiTcp4ServiceBindingProtocolGuid;
  } else if (IpVersion == IP_VERSION_6) {
    TcpServiceBindingGuid = &gEfiTcp6ServiceBindingProtocolGuid;
  }
  //
  // Try to disconnect the driver from the devices it's controlling.
  //
  Status = gBS->LocateHandleBuffer (
                  ByProtocol,
                  TcpServiceBindingGuid,
                  NULL,
                  &DeviceHandleCount,
                  &DeviceHandleBuffer
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((EFI_D_ERROR, "Exit from protocol\n"));
    Print (L"Unable to locate TcpServiceBindingGuid protocol\n");
    return Status;
  }
  //
  // IPv4 based mac controllers
  //
  for (Index = 0; Index < DeviceHandleCount; Index++) {
    //
    // Get MAC address of this network device.
    //
    Status = NetLibGetMacAddress (DeviceHandleBuffer[Index], &MacAddr, &HwAddressSize);
    if (EFI_ERROR (Status)) {
      Print (L"Unable to get Mac address.\n");
      return Status;
    }
    //
    // Get VLAN ID of this network device.
    //
    VlanId = NetLibGetVlanId (DeviceHandleBuffer[Index]);

    NvmeOfMacAddrToStr (&MacAddr, HwAddressSize,
      VlanId, MacString);

    AsciiStrToUnicodeStrS (AttemptData.MacString, AttemptMacString,
      sizeof (AttemptMacString) / sizeof (AttemptMacString[0]));
    if (StrCmp (MacString, AttemptMacString) != 0) {
      continue;
    } else {
      AttemptFound = TRUE;
      break;
    }
  }
  if (AttemptFound == TRUE) {
    Private = NvmeOfCreateDriverData (mImageHandler, DeviceHandleBuffer[Index]);
    if (Private == NULL) {
      DEBUG ((EFI_D_ERROR, "Error allocating driver private structure .\n"));
      return EFI_OUT_OF_RESOURCES;
    }

    NvmeOfCliProbeControllers (Private, &AttemptData, IpVersion);
  } else {
    Status = EFI_INVALID_PARAMETER;
    Print (L"The specified MacId is not from this system\n");
  }
  return Status;
}

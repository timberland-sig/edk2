/** @file
  NvmeOf driver is used to manage non-volatile memory subsystem which follows
  NVM Express specification.

  Copyright (c) 2020, Dell EMC All rights reserved
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Uefi.h>
#include <Library/MemoryAllocationLib.h>    
#include <Protocol/DriverBinding.h>
#include <Protocol/ComponentName2.h>
#include <Protocol/ComponentName.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiDriverEntryPoint.h>
#include <Library/UefiLib.h>
#include <Library/DebugLib.h>
#include <Library/NetLib.h>
#include <Library/TcpIoLib.h>
#include <Protocol/ServiceBinding.h>
#include <Protocol/NvmeOFPassthru.h>
#include "NvmeOfImpl.h"
#include "NvmeOfDns.h"
#include "NvmeOfSpdk.h"
#include "NvmeOfBlockIo.h"
#include "NvmeOfCliInterface.h"
#include "NvmeOfNbft.h"

EFI_EVENT                        KatoEvent = NULL;
EFI_EVENT                        gBeforeEBSEvent = NULL;
NVMEOF_PRIVATE_PROTOCOL          NVMEOF_Identifier;
NVMEOF_NIC_PRIVATE_DATA          *mNicPrivate = NULL;
LIST_ENTRY                       gNvmeOfControllerList;
extern NVMEOF_CLI_CTRL_MAPPING   *gCliCtrlMap;
extern NVMEOF_CLI_CTRL_MAPPING   *CtrlrInfo;
CHAR8                            *gNvmeOfRootPath = NULL;
BOOLEAN                          gAttemtsAlreadyRead = FALSE;
CHAR8                            *gNvmeOfImagePath = NULL;
BOOLEAN                          gDriverInRuntime = FALSE;

EFI_GUID  gNvmeOfV4PrivateGuid = NVMEOF_V4_PRIVATE_GUID;
EFI_GUID  gNvmeOfV6PrivateGuid = NVMEOF_V6_PRIVATE_GUID;

extern EFI_GUID gNvmeofPassThroughProtocolGuid;
extern NVMEOF_PASSTHROUGH_PROTOCOL gNvmeofPassThroughInstance;
extern EFI_HANDLE  mImageHandler;
extern GLOBAL_REMOVE_IF_UNREFERENCED EFI_UNICODE_STRING_TABLE  *gNvmeOfControllerNameTable;
extern VOID EFIAPI NvmeOfCliCleanup ();
EFI_HANDLE     gPassThroughProtocolHandle;


EFI_DRIVER_BINDING_PROTOCOL gNvmeOfIp4DriverBinding = {
  NvmeOfIp4DriverBindingSupported,
  NvmeOfIp4DriverBindingStart,
  NvmeOfIp4DriverBindingStop,
  0xa,
  NULL,
  NULL
};

EFI_DRIVER_BINDING_PROTOCOL gNvmeOfIp6DriverBinding = {
  NvmeOfIp6DriverBindingSupported,
  NvmeOfIp6DriverBindingStart,
  NvmeOfIp6DriverBindingStop,
  0xa,
  NULL,
  NULL
};

/**
  Install the device specific protocols like block device,
  device path and device info

  @param[in]  Device                     The handle of the device.
  @param[in]  SecuritySendRecvSupported  Security suupported flag.

  @retval     EFI_SUCCESS          The device protocols installed successfully.
  @retval     EFI_DEVICE_ERROR     An unexpected error occurred.

**/
EFI_STATUS
NvmeOfInstallDeviceProtocols (
  NVMEOF_DEVICE_PRIVATE_DATA *Device,
  BOOLEAN                    SecuritySendRecvSupported
  )
{
  EFI_STATUS          Status;
  VOID                *DummyInterface;
  NVMEOF_DRIVER_DATA  *Private;
  EFI_GUID            *ProtocolGuid;

  if (!mNicPrivate->Ipv6Flag) {
    ProtocolGuid = &gEfiTcp4ProtocolGuid;
  } else {
    ProtocolGuid = &gEfiTcp6ProtocolGuid;
  }

  // Make sure the handle is NULL so we create a new handle
  Device->DeviceHandle = NULL;
  Private = Device->Controller;

  Status = gBS->InstallMultipleProtocolInterfaces (
                  &Device->DeviceHandle,
                  &gEfiDevicePathProtocolGuid,
                  Device->DevicePath,
                  &gEfiBlockIoProtocolGuid,
                  &Device->BlockIo,
                  &gEfiBlockIo2ProtocolGuid,
                  &Device->BlockIo2,
                  &gEfiDiskInfoProtocolGuid,
                  &Device->DiskInfo,
                  NULL
                  );
  if (EFI_ERROR (Status)) {
    goto Exit;
  }

  //
  // Check if the NVMe controller supports the Security Send and Security Receive commands
  //
  if (SecuritySendRecvSupported) {
    Status = gBS->InstallProtocolInterface (
                    &Device->DeviceHandle,
                    &gEfiStorageSecurityCommandProtocolGuid,
                    EFI_NATIVE_INTERFACE,
                    &Device->StorageSecurity
                    );
    if (EFI_ERROR(Status)) {
      gBS->UninstallMultipleProtocolInterfaces (
             Device->DeviceHandle,
             &gEfiDevicePathProtocolGuid,
             Device->DevicePath,
             &gEfiBlockIoProtocolGuid,
             &Device->BlockIo,
             &gEfiBlockIo2ProtocolGuid,
             &Device->BlockIo2,
             &gEfiDiskInfoProtocolGuid,
             &Device->DiskInfo,
             NULL
             );
      goto Exit;
    }
  }

  Status = gBS->OpenProtocol (
                  Private->ChildHandle,
                  ProtocolGuid,
                  (VOID **)&DummyInterface,
                  Private->Image,
                  Device->DeviceHandle,
                  EFI_OPEN_PROTOCOL_BY_CHILD_CONTROLLER
                  );
  if (EFI_ERROR(Status)) {
    gBS->UninstallMultipleProtocolInterfaces (
           Device->DeviceHandle,
           &gEfiDevicePathProtocolGuid,
           Device->DevicePath,
           &gEfiBlockIoProtocolGuid,
           &Device->BlockIo,
           &gEfiBlockIo2ProtocolGuid,
           &Device->BlockIo2,
           &gEfiDiskInfoProtocolGuid,
           &Device->DiskInfo,
           NULL
           );
    goto Exit;
  }

  AddUnicodeString2 (
    "eng",
    gNvmeOfComponentName.SupportedLanguages,
    &Device->ControllerNameTable,
    Device->ModelName,
    TRUE
    );

  AddUnicodeString2 (
    "en",
    gNvmeOfComponentName2.SupportedLanguages,
    &Device->ControllerNameTable,
    Device->ModelName,
    FALSE
    );

  return EFI_SUCCESS;

Exit:
  DEBUG ((DEBUG_ERROR, "NvmeOfInstallDeviceProtocols:Device protocol installation failed\n"));
  return EFI_DEVICE_ERROR;
}

/**
  Tests to see if this driver supports a given controller. This is the worker function for
  NvmeOfIp4(6)DriverBindingSupported.

  @param[in]  This                 A pointer to the EFI_DRIVER_BINDING_PROTOCOL instance.
  @param[in]  ControllerHandle     The handle of the controller to test. This handle
                                   must support a protocol interface that supplies
                                   an I/O abstraction to the driver.
  @param[in]  RemainingDevicePath  A pointer to the remaining portion of a device path.  This
                                   parameter is ignored by device drivers, and is optional for bus
                                   drivers. For bus drivers, if this parameter is not NULL, then
                                   the bus driver must determine if the bus controller specified
                                   by ControllerHandle and the child controller specified
                                   by RemainingDevicePath are both supported by this
                                   bus driver.
  @param[in]  IpVersion            IP_VERSION_4 or IP_VERSION_6.

  @retval EFI_SUCCESS              The device specified by ControllerHandle and
                                   RemainingDevicePath is supported by the driver specified by This.
  @retval EFI_ALREADY_STARTED      The device specified by ControllerHandle and
                                   RemainingDevicePath is already being managed by the driver
                                   specified by This.
  @retval EFI_UNSUPPORTED          The device specified by ControllerHandle and
                                   RemainingDevicePath is not supported by the driver specified by This.
**/
EFI_STATUS
EFIAPI
NvmeOfSupported (
  IN EFI_DRIVER_BINDING_PROTOCOL  *This,
  IN EFI_HANDLE                   ControllerHandle,
  IN EFI_DEVICE_PATH_PROTOCOL     *RemainingDevicePath OPTIONAL,
  IN UINT8                        IpVersion
  )
{
  EFI_STATUS                    Status;
  EFI_GUID                      *TcpServiceBindingGuid;
  EFI_GUID                      *NvmeOfServiceBindingGuid;
  EFI_GUID                      *DhcpServiceBindingGuid;
  EFI_GUID                      *DnsServiceBindingGuid;
  UINT8                         TargetCount = 0;
  NVMEOF_ATTEMPT_CONFIG_NVDATA  *AttemptConfigData = NULL;

  // Read the attempt and check for version and target count
  Status = NvmeOfReadAttemptVariables ((VOID *)&AttemptConfigData, &TargetCount);
  if (EFI_ERROR (Status)) {
    return EFI_UNSUPPORTED;
  }

  if (IpVersion == IP_VERSION_4) {
    NvmeOfServiceBindingGuid = &gNvmeOfV4PrivateGuid;
    TcpServiceBindingGuid    = &gEfiTcp4ServiceBindingProtocolGuid;
    DhcpServiceBindingGuid   = &gEfiDhcp4ServiceBindingProtocolGuid;
    DnsServiceBindingGuid    = &gEfiDns4ServiceBindingProtocolGuid;
  } else {
    NvmeOfServiceBindingGuid = &gNvmeOfV6PrivateGuid;
    TcpServiceBindingGuid    = &gEfiTcp6ServiceBindingProtocolGuid;
    DhcpServiceBindingGuid   = &gEfiDhcp6ServiceBindingProtocolGuid;
    DnsServiceBindingGuid    = &gEfiDns6ServiceBindingProtocolGuid;
  }

  Status = gBS->OpenProtocol (
                  ControllerHandle,
                  NvmeOfServiceBindingGuid,
                  NULL,
                  This->DriverBindingHandle,
                  ControllerHandle,
                  EFI_OPEN_PROTOCOL_TEST_PROTOCOL
                  );
  if (!EFI_ERROR (Status)) {
    FreePool (AttemptConfigData);
    return EFI_ALREADY_STARTED;
  }

  Status = gBS->OpenProtocol (
                  ControllerHandle,
                  TcpServiceBindingGuid,
                  NULL,
                  This->DriverBindingHandle,
                  ControllerHandle,
                  EFI_OPEN_PROTOCOL_TEST_PROTOCOL
                  );
  if (EFI_ERROR (Status)) {
    FreePool (AttemptConfigData);
    return EFI_UNSUPPORTED;
  }

  // Check if attempt configured for this NIC
  if (!NvmeOfIsAttemptPresentForMac (ControllerHandle, AttemptConfigData, TargetCount)) {
    FreePool (AttemptConfigData);
    return EFI_UNSUPPORTED;
  }

  // Check if DHCP configured
  if (NvmeOfDhcpIsConfigured (ControllerHandle, IpVersion, AttemptConfigData, TargetCount)) {
    Status = gBS->OpenProtocol (
                    ControllerHandle,
                    DhcpServiceBindingGuid,
                    NULL,
                    This->DriverBindingHandle,
                    ControllerHandle,
                    EFI_OPEN_PROTOCOL_TEST_PROTOCOL
                    );
    if (EFI_ERROR (Status)) {
      FreePool (AttemptConfigData);
      return EFI_UNSUPPORTED;
    }
  }

  // Check if DNS configured
  if (NvmeOfDnsIsConfigured (ControllerHandle, AttemptConfigData, TargetCount)) {
    Status = gBS->OpenProtocol (
                    ControllerHandle,
                    DnsServiceBindingGuid,
                    NULL,
                    This->DriverBindingHandle,
                    ControllerHandle,
                    EFI_OPEN_PROTOCOL_TEST_PROTOCOL
                    );
    if (EFI_ERROR (Status)) {
      FreePool (AttemptConfigData);
      return EFI_UNSUPPORTED;
    }
  }

  FreePool (AttemptConfigData);
  return EFI_SUCCESS;
}


void Io2Complete (void *arg, const struct spdk_nvme_cpl *completion)
{
  NVMEOF_BLKIO2_SUBTASK                *Subtask;

  Subtask = (NVMEOF_BLKIO2_SUBTASK*)arg;
  Subtask->NvmeOfAsyncData->IsCompleted = IS_COMPLETED;
  /* See if an error occurred. If so, display information
   * about it, and set completion value so that I/O
  * caller is aware that an error occurred.
  */
  if (spdk_nvme_cpl_is_error (completion)) {
      DEBUG((DEBUG_ERROR, "\n I/O error status: %s\n", spdk_nvme_cpl_get_status_string(&completion->status)));
      Subtask->NvmeOfAsyncData->IsCompleted = ERROR_IN_COMPLETION;
      return;
  }
  return;
}

/**
  Callback function for KATO timer

  @param[in]  Event     The Event this notify function registered to.
  @param[in]  Context   Pointer to the context data registered to the
                        Event.
**/
VOID
EFIAPI
ProcessKatoTimer (
  IN EFI_EVENT                    Event,
  IN VOID*                        Context
  )
{
  NVMEOF_CONTROLLER_DATA *CtrlrData;
  LIST_ENTRY             *Entry;
  UINT8                  ret = 0;

  //search for controller data
  NET_LIST_FOR_EACH (Entry, &gNvmeOfControllerList) {
    CtrlrData = NET_LIST_USER_STRUCT (Entry, NVMEOF_CONTROLLER_DATA, Link);
    if(CtrlrData->KeepAliveErrorCounter >= SKIP_KEEP_AIVE_COUNTER) {
      continue;
    }
    CtrlrData->ctrlr->next_keep_alive_tick = 0;
    ret = nvme_ctrlr_keep_alive (CtrlrData->ctrlr);
    if (ret != 0) {
      CtrlrData->KeepAliveErrorCounter++;
      DEBUG ((DEBUG_INFO, "\nError: send keepalive to %a target.\n", CtrlrData->ctrlr->trid.traddr));
      if(CtrlrData->KeepAliveErrorCounter >= SKIP_KEEP_AIVE_COUNTER) {
        DEBUG ((DEBUG_INFO, "The controller is no longer rechable and keepalive will be stopped.\n"));
      }
    } else {
      CtrlrData->KeepAliveErrorCounter = 0;
    }
    spdk_nvme_qpair_process_completions (CtrlrData->ctrlr->adminq, 0);
  } 
}

/**
  Callback function for Async IO tasks timer

  @param[in]  Event     The Event this notify function registered to.
  @param[in]  Context   Pointer to the context data registered to the
                        Event.

**/
VOID
EFIAPI
ProcessAsyncTaskList (
  IN EFI_EVENT                    Event,
  IN VOID*                        Context
  )
{
  LIST_ENTRY                           *Link;
  LIST_ENTRY                           *NextLink;
  NVMEOF_BLKIO2_SUBTASK                *Subtask;
  NVMEOF_BLKIO2_REQUEST                *BlkIo2Request;
  EFI_BLOCK_IO2_TOKEN                  *Token;
  EFI_STATUS                           Status;
  NVMEOF_DEVICE_PRIVATE_DATA           *Device;
  NVMEOF_DRIVER_DATA                   *Private;
  int                                  rc = 0;

  Private = (NVMEOF_DRIVER_DATA*)Context;
  // Submit asynchronous subtasks to the NVMe Submission Queue 
  for (Link = GetFirstNode (&Private->UnsubmittedSubtasks);
    !IsNull(&Private->UnsubmittedSubtasks, Link);
    Link = NextLink) {

    NextLink = GetNextNode (&Private->UnsubmittedSubtasks, Link);
    Subtask = NVMEOF_BLKIO2_SUBTASK_FROM_LINK (Link);
    BlkIo2Request = Subtask->BlockIo2Request;
    Token = BlkIo2Request->Token;
    RemoveEntryList (Link);
    BlkIo2Request->UnsubmittedSubtaskNum--;

    // If any previous subtask fails, do not process subsequent ones.    
    if (Token != NULL) {
      if (Token->TransactionStatus != EFI_SUCCESS) {
        if (IsListEmpty (&BlkIo2Request->SubtasksQueue) &&
            BlkIo2Request->LastSubtaskSubmitted &&
            (BlkIo2Request->UnsubmittedSubtaskNum == 0)) {

            // Remove the BlockIo2 request from the device asynchronous queue.
            RemoveEntryList (&BlkIo2Request->Link);
            FreePool (BlkIo2Request);
            gBS->SignalEvent (Token->Event);
        }
        FreePool (Subtask->NvmeOfAsyncData);
        FreePool (Subtask);
        continue;
      }
    }

    Subtask->NvmeOfAsyncData->IsCompleted = 0;
    Device = Subtask->NvmeOfAsyncData->Device;

    if (Subtask->NvmeOfAsyncData->IsRead) {
        Status = spdk_nvme_ns_cmd_read (Device->NameSpace,
                   Device->qpair,
                   (void*)Subtask->NvmeOfAsyncData->Buffer,
                   Subtask->NvmeOfAsyncData->Lba,
                   Subtask->NvmeOfAsyncData->Blocks,
                   Io2Complete,
                   Subtask,
                   0);
    } else {
        Status = spdk_nvme_ns_cmd_write (Device->NameSpace,
                   Device->qpair,
                   (void*)Subtask->NvmeOfAsyncData->Buffer,
                   Subtask->NvmeOfAsyncData->Lba,
                   Subtask->NvmeOfAsyncData->Blocks,
                   Io2Complete,
                   Subtask,
                   0);
    }

    if (Status == ESUCCESS) {
      while (!Subtask->NvmeOfAsyncData->IsCompleted) {
          rc = spdk_nvme_qpair_process_completions (Device->qpair, 0);
          if (rc < 0) {
            Subtask->NvmeOfAsyncData->IsCompleted = ERROR_IN_COMPLETION;
            break;
          }
      }
      if(Subtask->NvmeOfAsyncData->IsCompleted == ERROR_IN_COMPLETION)
      {
        Status = EFI_DEVICE_ERROR;
      }
    }
    if (Status != ESUCCESS) {
      if (Token != NULL) {
          Token->TransactionStatus = EFI_DEVICE_ERROR;
      }

      if (IsListEmpty (&BlkIo2Request->SubtasksQueue) &&
        Subtask->IsLast) {
        // Remove the BlockIo2 request from the device asynchronous queue.
        RemoveEntryList (&BlkIo2Request->Link);
        FreePool (BlkIo2Request);
        if (Token != NULL) {
            gBS->SignalEvent (Token->Event);
        }
      }

      FreePool (Subtask->NvmeOfAsyncData);
      FreePool (Subtask);
    } else {
      InsertTailList (&BlkIo2Request->SubtasksQueue, Link);
      if (Subtask->IsLast) {
          BlkIo2Request->LastSubtaskSubmitted = TRUE;
      }
      gBS->SignalEvent (Subtask->Event);
    }
  }
}

/**
  Checks if an attempt needs to be skipped or not

  @param[in]  Image                Handle of the image.
  @param[in]  ControllerHandle     Handle of the controller.
  @param[in]  IpVersion            IP_VERSION_4 or IP_VERSION_6.
  @param[in]  AttemptConfigData    Attempt config data
  @param[in]  AttemptMacString[]   Attempt mac string
  @param[out] SkipAttempt          skip attempt

**/
VOID
NvmeOfCheckIfSkipAttempt (
  IN  EFI_HANDLE                    Image,
  IN  EFI_HANDLE                    ControllerHandle,
  IN  UINT8                         IpVersion,
  IN  NVMEOF_ATTEMPT_CONFIG_NVDATA  *AttemptConfigData,
  IN  CHAR16                        AttemptMacString[],
  OUT BOOLEAN                       *SkipAttempt
  )
{
  BOOLEAN                       CheckForOtherFields = FALSE;
  BOOLEAN                       Nid1Valid = FALSE;
  BOOLEAN                       Nid2Valid = FALSE;
  LIST_ENTRY                    *EntryProcessed = NULL;
  LIST_ENTRY                    *NextEntryProcessed = NULL;
  CHAR16                        ProcessedMacString[NVMEOF_MAX_MAC_STRING_LEN] = { 0 };
  NVMEOF_ATTEMPT_ENTRY          *AttemptEntry;
  NVMEOF_ATTEMPT_CONFIG_NVDATA  *ProcessedAttempt = NULL;

  NET_LIST_FOR_EACH_SAFE (EntryProcessed, NextEntryProcessed, &mNicPrivate->ProcessedAttempts) {
    BOOLEAN IsIpSame = FALSE;
    AttemptEntry = NET_LIST_USER_STRUCT (EntryProcessed, NVMEOF_ATTEMPT_ENTRY, Link);
    ProcessedAttempt = &AttemptEntry->Data;

    AsciiStrToUnicodeStrS (ProcessedAttempt->MacString, ProcessedMacString,
      sizeof (ProcessedMacString) / sizeof (ProcessedMacString[0]));

    if (IpVersion == IP_VERSION_4) {
      IsIpSame = EFI_IP4_EQUAL (&AttemptConfigData->SubsysConfigData.NvmeofSubSystemIp, &ProcessedAttempt->SubsysConfigData.NvmeofSubSystemIp);
    } else {
      IsIpSame = EFI_IP6_EQUAL (&AttemptConfigData->SubsysConfigData.NvmeofSubSystemIp, &ProcessedAttempt->SubsysConfigData.NvmeofSubSystemIp);
    }

    // Check if valid NID input in both attempts.
    Nid1Valid = IsUuidValid (AttemptConfigData->SubsysConfigData.NvmeofSubsysNid);
    Nid2Valid = IsUuidValid (ProcessedAttempt->SubsysConfigData.NvmeofSubsysNid);

    // If both are valid
    if (Nid1Valid && Nid2Valid) {
      // Match the NIDs
      if (NvmeOfMatchNid (AttemptConfigData->SubsysConfigData.NvmeofSubsysNid,
        ProcessedAttempt->SubsysConfigData.NvmeofSubsysNid)) {
        // If NID matches, match other parameters
        CheckForOtherFields = TRUE;
      } else {
        // If NID match fails, no need to check other parameters
        CheckForOtherFields = FALSE;
      }
    } else if ((Nid1Valid && !Nid2Valid) || (!Nid1Valid && Nid2Valid)) {
      // If either one is invalid
      CheckForOtherFields = FALSE;
    } else {
      // If both are invalid
      CheckForOtherFields = TRUE;
    }

    if (CheckForOtherFields) {
      if ((StrCmp (ProcessedMacString, AttemptMacString) == 0) && (IsIpSame) &&
        (AttemptConfigData->SubsysConfigData.NvmeofSubsysPortId == ProcessedAttempt->SubsysConfigData.NvmeofSubsysPortId) &&
        (AsciiStrCmp (AttemptConfigData->SubsysConfigData.NvmeofSubsysNqn, ProcessedAttempt->SubsysConfigData.NvmeofSubsysNqn) == 0)) {
        *SkipAttempt = TRUE;
        break;
      }
    }
  }
}

/**
  Get controller guid of the controller

  @param[in]  IpVersion                 IP_VERSION_4 or IP_VERSION_6.
  @param[in]  NvmeOfServiceBindingGuid  NvmeOf service binding guid of the controller.
  @param[in]  ProtocolGuid              Protocol guid of the controller.
  @param[in]  TcpServiceBindingGuid     tcp servicebinding guid of the controller.

  @retval EFI_SUCCESS           This driver was started.
  @retval EFI_INVALID_PARAMETER Any input parameter is invalid.

**/
EFI_STATUS
NvmeOfGetGuid (
  IN  UINT8                        IpVersion,
  OUT EFI_GUID                     **NvmeOfServiceBindingGuid,
  OUT EFI_GUID                     **ProtocolGuid,
  OUT EFI_GUID                     **TcpServiceBindingGuid
  )
{
  if (IpVersion == IP_VERSION_4) {
    *NvmeOfServiceBindingGuid = &gNvmeOfV4PrivateGuid;
    *TcpServiceBindingGuid = &gEfiTcp4ServiceBindingProtocolGuid;
    *ProtocolGuid = &gEfiTcp4ProtocolGuid;
    mNicPrivate->Ipv6Flag = FALSE;
  } else if (IpVersion == IP_VERSION_6) {
    *NvmeOfServiceBindingGuid = &gNvmeOfV6PrivateGuid;
    *TcpServiceBindingGuid = &gEfiTcp6ServiceBindingProtocolGuid;
    *ProtocolGuid = &gEfiTcp6ProtocolGuid;
    mNicPrivate->Ipv6Flag = TRUE;
  } else {
    DEBUG ((EFI_D_ERROR, "NvmeOFDriverBindingStart: Invalid IP version parmeter passed\n"));
    return EFI_INVALID_PARAMETER;
  }
  return EFI_SUCCESS;
}

/**
  Check for DNS Mode 

  @param[in]  Image                Handle of the image.
  @param[in]  ControllerHandle     Handle of the controller.
  @param[in]  AttemptConfigData     

  @retval EFI_SUCCESS           This driver was started.
  @retval EFI_INVALID_PARAMETER Any input parameter is invalid.
  @retval EFI_DEVICE_ERROR      Failed to get TCP connection device path.

**/
EFI_STATUS
NvmeOfDnsMode (
  IN EFI_HANDLE                    Image,
  IN EFI_HANDLE                    ControllerHandle,
  IN NVMEOF_ATTEMPT_CONFIG_NVDATA  **AttemptConfigData
  )
{
  EFI_STATUS  Status = EFI_SUCCESS;

  if ((*AttemptConfigData)->SubsysConfigData.NvmeofDnsMode) {
    //
    // perform dns process if target address expressed by domain name.
    //
    if (!mNicPrivate->Ipv6Flag) {
      Status = NvmeOfDns4 (Image, ControllerHandle, &(*AttemptConfigData)->SubsysConfigData);
    } else {
      Status = NvmeOfDns6 (Image, ControllerHandle, &(*AttemptConfigData)->SubsysConfigData);
    }
  }
  return Status;
}

/**
  Creates Events or Timers required for various NVMeOF specific work as follows:
  Kato timer
  Timer to process async task

  @param[in]  Private           Private data struture

  @retval EFI_SUCCESS           This driver was started.
  @retval EFI_INVALID_PARAMETER Any input parameter is invalid.
  @retval EFI_NOT_FOUND         There is no sufficient information to establish
                                the NvmeOf session.
  @retval EFI_OUT_OF_RESOURCES  Failed to allocate memory.
  @retval EFI_DEVICE_ERROR      Failed to get TCP connection device path.

**/
EFI_STATUS
NvmeOfCreatEvents (
  IN NVMEOF_DRIVER_DATA         *Private
  )
{
  EFI_STATUS                    Status = EFI_SUCCESS;

  //
  // Start the asynchronous I/O completion monitor
  //
  Status = gBS->CreateEvent (
                  EVT_TIMER | EVT_NOTIFY_SIGNAL,
                  TPL_CALLBACK,
                  ProcessAsyncTaskList,
                  Private,
                  &Private->TimerEvent
                  );
  if (EFI_ERROR (Status)) {
    goto ON_ERROR;
  }

  Status = gBS->SetTimer (
                  Private->TimerEvent,
                  TimerPeriodic,
                  NVMEOF_HC_ASYNC_TIMER
                  );
  if (EFI_ERROR (Status)) {
    goto ON_ERROR;
  }
  //
  // event for kato timer
  //
  if (KatoEvent == NULL) {
    Status = gBS->CreateEvent (
                    EVT_TIMER | EVT_NOTIFY_SIGNAL,
                    TPL_CALLBACK,
                    ProcessKatoTimer,
                    NULL,
                    &KatoEvent
                    );
    if (EFI_ERROR (Status)) {
      goto ON_ERROR;
    }

    Status = gBS->SetTimer (
                    KatoEvent,
                    TimerPeriodic,
                    NVMEOF_KATO_TIMER
                    );
  }
  //
  // event for exit boot service
  //
  Status = gBS->CreateEventEx (
                  EVT_NOTIFY_SIGNAL,
                  TPL_CALLBACK,
                  NvmeOfOnExitBootService,
                  Private,
                  &gEfiEventExitBootServicesGuid,
                  &Private->ExitBootServiceEvent
                  );
  if (EFI_ERROR (Status)) {
    goto ON_ERROR;
  }

  return Status;

ON_ERROR:
  if (Private->TimerEvent != NULL) {
    gBS->CloseEvent (Private->TimerEvent);
    Private->TimerEvent = (EFI_EVENT)NULL;
  }
  if (KatoEvent != NULL) {
    gBS->CloseEvent (KatoEvent);
    KatoEvent = (EFI_EVENT)NULL;
  }
  if (Private->ExitBootServiceEvent != NULL) {
    gBS->CloseEvent (Private->ExitBootServiceEvent);
    Private->ExitBootServiceEvent = (EFI_EVENT)NULL;
  }
  return EFI_UNSUPPORTED;
}

/**
  Uninstall Protocol Interface.

  @param[in]  Image                     Handle of the image.
  @param[in]  ControllerHandle          Handle of the controller.
  @param[in]  ProtocolGuid              Protocol Guid
  @param[in]  NvmeOfServiceBindingGuid  Service binding guid
  @param[in]  Private                   private 
**/
VOID
NvmeOfUninstallProtocolInterface (
  IN EFI_HANDLE                   Image,
  IN EFI_HANDLE                   ControllerHandle,
  IN EFI_GUID                     *ProtocolGuid,
  IN EFI_GUID                     *NvmeOfServiceBindingGuid,
  IN NVMEOF_DRIVER_DATA           *Private
  )
{
  if (Private != NULL && Private->ChildHandle != NULL) {
    gBS->CloseProtocol (
           Private->ChildHandle,
           ProtocolGuid,
           Image,
           ControllerHandle
           );

    NetLibDestroyServiceChild (
      ControllerHandle,
      Image,
      ProtocolGuid,
      Private->ChildHandle
      );
  }
  gBS->UninstallProtocolInterface (
         ControllerHandle,
         ProtocolGuid,
         &Private->NvmeOfIdentifier
         );

}

/**
  Start to manage the controller. This is the worker function for
  NvmeOfIp4(6)DriverBindingStart.

  @param[in]  Image                Handle of the image.
  @param[in]  ControllerHandle     Handle of the controller.
  @param[in]  IpVersion            IP_VERSION_4 or IP_VERSION_6.

  @retval EFI_SUCCESS           This driver was started.
  @retval EFI_ALREADY_STARTED   This driver is already running on this device.
  @retval EFI_INVALID_PARAMETER Any input parameter is invalid.
  @retval EFI_NOT_FOUND         There is no sufficient information to establish
                                the NvmeOf session.
  @retval EFI_OUT_OF_RESOURCES  Failed to allocate memory.
  @retval EFI_DEVICE_ERROR      Failed to get TCP connection device path.
  @retval EFI_ACCESS_DENIED     The protocol could not be removed from the Handle
                                because its interfaces are being used.
**/
EFI_STATUS
NvmeOfStart (
  IN EFI_HANDLE                   Image,
  IN EFI_HANDLE                   ControllerHandle,
  IN UINT8                        IpVersion
  )
{
  EFI_STATUS                    Status = EFI_SUCCESS;
  EFI_GUID                      *NvmeOfServiceBindingGuid = NULL;
  EFI_GUID                      *TcpServiceBindingGuid = NULL;
  EFI_GUID                      *ProtocolGuid = NULL;
  EFI_MAC_ADDRESS               MacAddrCon = { 0 };
  UINTN                         HwAddressSize = 0;
  NVMEOF_ATTEMPT_ENTRY          *AttemptTmp = NULL;
  NVMEOF_ATTEMPT_ENTRY          *AttemptEntry;
  NVMEOF_ATTEMPT_CONFIG_NVDATA  *AttemptConfigData = NULL;
  NVMEOF_NIC_INFO               *NicInfo = NULL;
  CHAR16                        MacString[NVMEOF_MAX_MAC_STRING_LEN] = { 0 };
  CHAR16                        AttemptMacString[NVMEOF_MAX_MAC_STRING_LEN] = { 0 };
  BOOLEAN                       AttemptFound = FALSE;
  BOOLEAN                       SkipAttempt = FALSE;
  LIST_ENTRY                    *Entry = NULL;
  LIST_ENTRY                    *NextEntry = NULL;
  NVMEOF_DRIVER_DATA            *Private = NULL;
  CHAR16                        AttemptNqn[NVMEOF_NAME_MAX_SIZE] = { 0 };
  VOID                          *Interface = NULL;

  mImageHandler = Image;

  Status = NvmeOfGetGuid (
             IpVersion,
             &NvmeOfServiceBindingGuid,
             &ProtocolGuid,
             &TcpServiceBindingGuid
             );
  if (EFI_ERROR (Status)) {
    return EFI_INVALID_PARAMETER;
  }

  Status = gBS->OpenProtocol (
                  ControllerHandle,
                  NvmeOfServiceBindingGuid,
                  NULL,
                  Image,
                  ControllerHandle,
                  EFI_OPEN_PROTOCOL_TEST_PROTOCOL
                  );
  if (!EFI_ERROR (Status)) {
    return EFI_ALREADY_STARTED;
  }

  if (!gAttemtsAlreadyRead) {
    // Read the attempt configuration data
    Status = NvmeOfReadConfigData ();
    if (EFI_ERROR (Status)) {
      DEBUG ((EFI_D_ERROR, "Reading attempts failed.\n"));
      return EFI_UNSUPPORTED;
    }
    gAttemtsAlreadyRead = TRUE;
  }

  //
  // Record the incoming NIC info.
  //
  Status = NvmeOfSaveNic (ControllerHandle, Image);
  if (EFI_ERROR (Status)) {
    DEBUG ((EFI_D_ERROR, "NvmeOFDriverBindingStart: error saving NIC info\n"));
    return Status;
  }

  //
  // Get MAC address of this network device.
  //
  Status = NetLibGetMacAddress (ControllerHandle, &MacAddrCon, &HwAddressSize);
  if (EFI_ERROR (Status)) {
    return FALSE;
  }

  DEBUG ((DEBUG_INFO, "Processing NIC adapter with mac %02x:%02x:%02x:%02x:%02x:%02x\n",
    MacAddrCon.Addr[0], MacAddrCon.Addr[1], MacAddrCon.Addr[2], MacAddrCon.Addr[3],
    MacAddrCon.Addr[4], MacAddrCon.Addr[5]));

  UINTN AttemptCount = 0;
  NET_LIST_FOR_EACH_SAFE (Entry, NextEntry, &mNicPrivate->AttemptConfigs) {
    SkipAttempt   = FALSE;
    AttemptFound  = FALSE;
    AttemptEntry  = NET_LIST_USER_STRUCT (Entry, NVMEOF_ATTEMPT_ENTRY, Link);
    AttemptConfigData = &AttemptEntry->Data;

    // Check if the attempt is enabled
    if (AttemptConfigData->SubsysConfigData.Enabled == 0) {
      continue;
    }

    // Get the Nic information from the NIC list
    NicInfo = NvmeOfGetNicInfoByIndex (mNicPrivate->CurrentNic);
    ASSERT (NicInfo != NULL);
    NvmeOfMacAddrToStr (&NicInfo->PermanentAddress, NicInfo->HwAddressSize,
      NicInfo->VlanId, MacString);

    AsciiStrToUnicodeStrS (AttemptConfigData->MacString, AttemptMacString,
      sizeof (AttemptMacString) / sizeof (AttemptMacString[0]));
    if (StrCmp (MacString, AttemptMacString) != 0) {
      continue;
    } else {
        if ((mNicPrivate->Ipv6Flag == TRUE && AttemptConfigData->SubsysConfigData.NvmeofIpMode == IP_MODE_IP6) ||
         (mNicPrivate->Ipv6Flag == FALSE && AttemptConfigData->SubsysConfigData.NvmeofIpMode == IP_MODE_IP4) ||
         (AttemptConfigData->SubsysConfigData.NvmeofIpMode == IP_MODE_AUTOCONFIG)) {
          AttemptFound = TRUE;
      } else {
        continue;
      }
    }

    // Add the NIC Index to the attempt.
    AttemptConfigData->NicIndex = NicInfo->NicIndex;

    // Check if the Mac and TargetIP and TargetPort of attempt matches the already processed attempts
    // If yes, hence skip it.
    NvmeOfCheckIfSkipAttempt (
      Image,
      ControllerHandle,
      IpVersion,
      AttemptConfigData,
      AttemptMacString,
      &SkipAttempt
      );
    if (SkipAttempt) {
      continue;
    }

    // Get the attempt config data based on configuration
    Status = NvmeOfGetConfigData (Image, ControllerHandle, AttemptConfigData);
    if (EFI_ERROR (Status)) {
      DEBUG ((EFI_D_ERROR, "Error getting config data.\n"));
      goto ON_ERROR;
    }

    Status = NvmeOfDnsMode (Image, ControllerHandle, &AttemptConfigData);
      if (EFI_ERROR (Status)) {
        DEBUG ((EFI_D_ERROR, "The configuration of Target address or DNS server \
            address is invalid!\n"));
        goto ON_ERROR;
      }
    //
    // Create the instance private data.
    //
    if (Private == NULL) {
      Private = NvmeOfCreateDriverData (Image, ControllerHandle);
      if (Private == NULL) {
        DEBUG ((EFI_D_ERROR, "Error allocating driver private structure .\n"));
        goto ON_ERROR;
      }

      Private->NvmeOfIdentifier.Reserved = 0;
      //
      // Create a underlayer child instance, but no need to configure it. Just open ChildHandle
      // via BY_DRIVER. That is, establishing the relationship between ControllerHandle and ChildHandle.
      //
      Status = NetLibCreateServiceChild (
                 ControllerHandle,
                 Image,
                 TcpServiceBindingGuid,
                 &Private->ChildHandle
                 );
      if (EFI_ERROR (Status)) {
        goto ON_ERROR;
      }

      Status = gBS->OpenProtocol (
                      Private->ChildHandle, /// Default Tcp child
                      ProtocolGuid,
                      &Interface,
                      Image,
                      ControllerHandle,
                      EFI_OPEN_PROTOCOL_BY_DRIVER
                      );
      if (EFI_ERROR (Status)) {
        goto ON_ERROR;
      }
      //
      // NvmeOfDriver children should share the default Tcp child, just open the default Tcp child via BY_CHILD_CONTROLLER.
      //
      gBS->OpenProtocol (
             Private->ChildHandle, /// Default Tcp child
             ProtocolGuid,
             &Interface,
             Image,
             Private->NvmeOfProtocolHandle,
             EFI_OPEN_PROTOCOL_BY_CHILD_CONTROLLER
             );

      //
      // Always install private protocol no matter what happens later. We need to
      // keep the relationship between ControllerHandle and ChildHandle.
      //
      Status = gBS->InstallProtocolInterface (
                      &ControllerHandle,
                      NvmeOfServiceBindingGuid,
                      EFI_NATIVE_INTERFACE,
                      &Private->NvmeOfIdentifier
                      );
      if (EFI_ERROR (Status)) {
        DEBUG ((EFI_D_ERROR, "Error while Installing private guid .\n"));
        goto ON_ERROR;
      }
    }

    if (AttemptFound) {
      Private->Attempt = AttemptEntry;
      AsciiStrToUnicodeStrS (AttemptConfigData->SubsysConfigData.NvmeofSubsysNqn, AttemptNqn,
        sizeof (AttemptNqn) / sizeof (AttemptNqn[0]));

      if (!SkipAttempt) {
        Status = NvmeOfProbeControllers (Private, AttemptConfigData, IpVersion);
        if (EFI_ERROR (Status)) {
          continue;
        } else {
          // Get IPv6 info to populate in nBFT
          if (mNicPrivate->Ipv6Flag) {
            NvmeOfGetIp6NicInfo (&AttemptConfigData->SubsysConfigData, Private->TcpIo);
          }

          NvmeOfPublishNbft (FALSE);
        }
      }

      // Attempt processing completed, queue to processed attempts list.
      AttemptTmp = AllocateZeroPool (sizeof (NVMEOF_ATTEMPT_ENTRY));
      CopyMem (AttemptTmp, AttemptEntry, sizeof (NVMEOF_ATTEMPT_ENTRY));
      InsertTailList (&mNicPrivate->ProcessedAttempts, &AttemptTmp->Link);

      mNicPrivate->ProcessedAttemptCount++;
      AttemptCount++;
      continue;
    ON_ERROR:
      //Destroy TCP Child Handle and Free the Private,
      //when error occurs while opening protocol on TCP Child Handle and
      //when error occurs while NvmeOfServiceBindingGuid protocol installation.
      NvmeOfUninstallProtocolInterface (
        Image,
        ControllerHandle,
        ProtocolGuid,
        NvmeOfServiceBindingGuid,
        Private
        );

      if (Private != NULL) {
        FreePool (Private);
        Private = NULL;
      }
      continue;
    }//End of Attempt matched
  }//End Of Attempt

  //Destroy TCP Child Handle and Free the Private, when Attempt == 0,
  //else other successful probe will share the private.
  if (AttemptCount == 0) {
    NvmeOfUninstallProtocolInterface (
      Image,
      ControllerHandle,
      ProtocolGuid,
      NvmeOfServiceBindingGuid,
      Private
      );

    if (Private != NULL) {
      FreePool (Private);
      Private = NULL;
    }
  }

  if (mNicPrivate->ProcessedAttemptCount == 0) {
    // Clear the Attempt Config List
    NET_LIST_FOR_EACH_SAFE (Entry, NextEntry, &mNicPrivate->AttemptConfigs) {
      AttemptEntry = NET_LIST_USER_STRUCT (Entry, NVMEOF_ATTEMPT_ENTRY, Link);        
      RemoveEntryList (&AttemptEntry->Link);
      mNicPrivate->AttemptCount--;
      FreePool (AttemptEntry);
    }
    gAttemtsAlreadyRead = FALSE;
  }

  if (Private == NULL) {
    return EFI_UNSUPPORTED;
  } else {
    Private->NvmeOfIdentifier.Reserved = 0;
    NvmeOfCreatEvents (Private);
  }
  return EFI_SUCCESS;
}

/**
  Unregisters a NVMeOF device namespace.

  This function removes the protocols installed on the controller handle and
  frees the resources allocated for the namespace.

  @param  This                  The pointer to EFI_DRIVER_BINDING_PROTOCOL instance.
  @param  Controller            The controller handle of the namespace.
  @param  Handle                The child handle.

  @retval EFI_SUCCESS           The namespace is successfully unregistered.
  @return Others                Some error occurs when unregistering the namespace.

**/
EFI_STATUS
UnregisterNvmeOfNamespace (
  IN  EFI_DRIVER_BINDING_PROTOCOL    *This,
  IN  EFI_HANDLE                     Controller,
  IN  EFI_HANDLE                     Handle
  )
{
  EFI_STATUS                               Status;
  EFI_BLOCK_IO_PROTOCOL                    *BlockIo;
  NVMEOF_DEVICE_PRIVATE_DATA               *Device;
  EFI_STORAGE_SECURITY_COMMAND_PROTOCOL    *StorageSecurity;
  BOOLEAN                                  IsEmpty;
  EFI_TPL                                  OldTpl;
  EFI_GUID                                 *ProtocolGuid;
  NVMEOF_DRIVER_DATA                       *Private;

  if (!mNicPrivate->Ipv6Flag) {
    ProtocolGuid = &gEfiTcp4ProtocolGuid;
  } else {
    ProtocolGuid = &gEfiTcp6ProtocolGuid;
  }

  BlockIo = NULL;

  Status = gBS->OpenProtocol (
                  Handle,
                  &gEfiBlockIoProtocolGuid,
                  (VOID **)&BlockIo,
                  This->DriverBindingHandle,
                  Controller,
                  EFI_OPEN_PROTOCOL_GET_PROTOCOL
                  );
  if (EFI_ERROR (Status)) {
    // Not a block device handle. It may come here for TCP child as well.
    // Treat it as soft error.
    Status = EFI_SUCCESS;
    return Status;
  }

  Device = NVMEOF_DEVICE_PRIVATE_DATA_FROM_BLOCK_IO (BlockIo);
  Private = Device->Controller;
  ASSERT (Private != NULL);

  //
  // Wait for the device's asynchronous I/O queue to become empty.
  //
  while (TRUE) {
    OldTpl = gBS->RaiseTPL (TPL_CALLBACK);
    IsEmpty = IsListEmpty (&Device->AsyncQueue);
    gBS->RestoreTPL (OldTpl);

    if (IsEmpty) {
      break;
    }

    gBS->Stall (DELAY);
  }

  Status = gBS->CloseProtocol (
                  Private->ChildHandle,
                  ProtocolGuid,
                  Private->Image,
                  Device->DeviceHandle
                  );
  if (EFI_ERROR (Status)) {
    return Status;
  }
  //
  // The NVMeOF driver installs the DevicePath, BlockIo, BlockIo2 and DiskInfo
  // in the DriverBindingStart(). Uninstall all of them.
  //

  Status = gBS->UninstallMultipleProtocolInterfaces (
                  Device->DeviceHandle,
                  &gEfiDevicePathProtocolGuid,
                  Device->DevicePath,
                  &gEfiBlockIoProtocolGuid,
                  &Device->BlockIo,
                  &gEfiBlockIo2ProtocolGuid,
                  &Device->BlockIo2,
                  &gEfiDiskInfoProtocolGuid,
                  &Device->DiskInfo,
                  NULL
                  );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  //
  // If Storage Security Command Protocol is installed, then uninstall
  // this protocol.
  //
  Status = gBS->OpenProtocol (
                  Handle,
                  &gEfiStorageSecurityCommandProtocolGuid,
                  (VOID **)&StorageSecurity,
                  This->DriverBindingHandle,
                  Controller,
                  EFI_OPEN_PROTOCOL_GET_PROTOCOL
                  );

  if (!EFI_ERROR (Status)) {
    Status = gBS->UninstallProtocolInterface (
                    Handle,
                    &gEfiStorageSecurityCommandProtocolGuid,
                    &Device->StorageSecurity
                    );
    if (EFI_ERROR(Status)) {
      return Status;
    }
  }

  if (Device->DevicePath != NULL) {
    FreePool (Device->DevicePath);
  }

  if (Device->ControllerNameTable != NULL) {
    FreeUnicodeStringTable (Device->ControllerNameTable);
  }

  return EFI_SUCCESS;
}

/**
  Stops a device controller or a bus controller. This is the worker function for
  NvmeOfIp4(6)DriverBindingStop.

  @param[in]  This              A pointer to the EFI_DRIVER_BINDING_PROTOCOL instance.
  @param[in]  ControllerHandle  A handle to the device being stopped. The handle must
                                support a bus specific I/O protocol for the driver
                                to use to stop the device.
  @param[in]  NumberOfChildren  The number of child device handles in ChildHandleBuffer.
  @param[in]  ChildHandleBuffer An array of child handles to be freed. May be NULL
                                if NumberOfChildren is 0.
  @param[in]  IpVersion         IP_VERSION_4 or IP_VERSION_6.

  @retval EFI_SUCCESS           The device was stopped.
  @retval EFI_DEVICE_ERROR      The device could not be stopped due to a device error.
  @retval EFI_INVALID_PARAMETER Child handle is NULL.
  @retval EFI_ACCESS_DENIED     The protocol could not be removed from the Handle
                                because its interfaces are being used.

**/
EFI_STATUS
EFIAPI
NvmeOfStop (
  IN EFI_DRIVER_BINDING_PROTOCOL  *This,
  IN EFI_HANDLE                   ControllerHandle,
  IN UINTN                        NumberOfChildren,
  IN EFI_HANDLE                   *ChildHandleBuffer OPTIONAL,
  IN UINT8                        IpVersion
  )
{
  /* This will be populated once child handlers are created in Start function */
  EFI_GUID                            *ProtocolGuid = NULL;
  EFI_GUID                            *TcpServiceBindingGuid = NULL;
  EFI_GUID                            *TcpProtocolGuid = NULL;
  EFI_HANDLE                          NvmeOfController = NULL;
  NVMEOF_PRIVATE_PROTOCOL             *NvmeOfIdentifier = NULL;
  NVMEOF_DRIVER_DATA                  *Private = NULL;
  EFI_STATUS                          Status = EFI_SUCCESS;
  EFI_TPL                             OldTpl;
  BOOLEAN                             IsEmpty;
  UINT32                              Index = 0;
  UINT8                               counter = 0;
  NVMEOF_CONTROLLER_DATA              *CtrlrData = NULL;
  LIST_ENTRY                          *Entry = NULL;
  NVMEOF_ATTEMPT_ENTRY                *AttemptEntry;
  LIST_ENTRY                          *NextEntryProcessed = NULL;
  EFI_OPEN_PROTOCOL_INFORMATION_ENTRY *OpenInfoBuffer = NULL;
  UINTN                               EntryCount = 0;
  NVMEOF_CLI_CTRL_MAPPING             *CtrlrInfoData = NULL;

  gAttemtsAlreadyRead = FALSE;
  NqnNidMapINdex = 0;

  if (gNvmeOfNbftListIndex > 0) {
    SetMem (gNvmeOfNbftList, (NID_MAX * sizeof (NVMEOF_NBFT)), 0);
    gNvmeOfNbftListIndex = 0;
  }

  if (IpVersion == IP_VERSION_4) {
    ProtocolGuid = &gNvmeOfV4PrivateGuid;
    TcpProtocolGuid = &gEfiTcp4ProtocolGuid;
    TcpServiceBindingGuid = &gEfiTcp4ServiceBindingProtocolGuid;
    mNicPrivate->Ipv6Flag = FALSE;
  } else if (IpVersion == IP_VERSION_6) {
    ProtocolGuid = &gNvmeOfV6PrivateGuid;
    TcpProtocolGuid = &gEfiTcp6ProtocolGuid;
    TcpServiceBindingGuid = &gEfiTcp6ServiceBindingProtocolGuid;
    mNicPrivate->Ipv6Flag = TRUE;
  } else {
    return EFI_DEVICE_ERROR;
  }

  // Disconnect CLI contollers
  if (gCliCtrlMap) {
    NvmeOfCliCleanup ();
  }

  NvmeOfController = NetLibGetNicHandle (ControllerHandle, TcpProtocolGuid);
  if (NvmeOfController == NULL) {
    return EFI_SUCCESS;
  }

  Status = gBS->OpenProtocol (
                  NvmeOfController,
                  ProtocolGuid,
                  (VOID **)&NvmeOfIdentifier,
                  This->DriverBindingHandle,
                  ControllerHandle,
                  EFI_OPEN_PROTOCOL_GET_PROTOCOL
                  );
  if (EFI_ERROR (Status)) {
    return EFI_DEVICE_ERROR;
  }
  Private = NVMEOF_DRIVER_DATA_FROM_IDENTIFIER (NvmeOfIdentifier);
  ASSERT (Private != NULL);

  // Controller is stopping now. Gate external APIs.
  Private->IsStopping = TRUE;
  
  // Wait for the asynchronous PassThru queue to become empty.
  while (TRUE) {
    OldTpl = gBS->RaiseTPL (TPL_NOTIFY);
    if (!((&Private->UnsubmittedSubtasks.ForwardLink != NULL) && (&Private->UnsubmittedSubtasks.BackLink != NULL))) {
      IsEmpty = IsListEmpty (&Private->UnsubmittedSubtasks);
    } else {
      IsEmpty = TRUE;
    }
    gBS->RestoreTPL (OldTpl);

    counter++;
    if (counter == 10) {
      DEBUG ((DEBUG_INFO, "Exiting since delay timeout\n"));
      break;
    }

    if (IsEmpty) {
      break;
    }

    gBS->Stall (DELAY);
  }

  if (Private->ChildHandle != NULL) {
    gBS->OpenProtocolInformation (
            Private->ChildHandle,
            TcpProtocolGuid,
            &OpenInfoBuffer,
            &EntryCount
            );

    for (Index = 0; Index < EntryCount; Index++) {
      if ((OpenInfoBuffer[Index].Attributes & EFI_OPEN_PROTOCOL_BY_CHILD_CONTROLLER) != 0) {
        UnregisterNvmeOfNamespace (This, ControllerHandle, OpenInfoBuffer[Index].ControllerHandle);
      }
    }
  }

  if (Private->TimerEvent != NULL) {
    gBS->CloseEvent (Private->TimerEvent);
  }

  if (Private->ChildHandle != NULL) {
    Status = gBS->CloseProtocol (
                    Private->ChildHandle,
                    TcpProtocolGuid,
                    This->DriverBindingHandle,
                    NvmeOfController
                    );
    ASSERT (!EFI_ERROR (Status));
    Status = NetLibDestroyServiceChild (
               NvmeOfController,
               This->DriverBindingHandle,
               TcpServiceBindingGuid,
               Private->ChildHandle
               );
    ASSERT (!EFI_ERROR (Status));
  }
  Status = gBS->UninstallProtocolInterface (
                  NvmeOfController,
                  ProtocolGuid,
                  &Private->NvmeOfIdentifier
                  );
  if (EFI_ERROR (Status)) {
    goto StopError1;
  }

  // Remove NIC to be done once only. Comes back for IPv6 binding, hence error ignored.
  NvmeOfRemoveNic (NvmeOfController);

  //
  // Uninstall the NBFT from ACPI tables, but only during DXE.
  // This is effectively a workaround for iPXE driver, which
  // closes its UNDI/SNP protocols during ExitBootServices().
  //
  if (!gDriverInRuntime) {
    NvmeOfPublishNbft (FALSE);
  }

  NET_LIST_FOR_EACH_SAFE (Entry, NextEntryProcessed, &CtrlrInfo->CliCtrlrList) {
    CtrlrInfoData =
      NET_LIST_USER_STRUCT (Entry, NVMEOF_CLI_CTRL_MAPPING, CliCtrlrList);
    RemoveEntryList (&CtrlrInfoData->CliCtrlrList);
    FreePool (CtrlrInfoData);
  }

  NET_LIST_FOR_EACH_SAFE (Entry, NextEntryProcessed, &mNicPrivate->AttemptConfigs) {
    AttemptEntry = NET_LIST_USER_STRUCT (Entry, NVMEOF_ATTEMPT_ENTRY, Link);
    RemoveEntryList (&AttemptEntry->Link);
    mNicPrivate->AttemptCount--;
    FreePool (AttemptEntry);
  }

  NET_LIST_FOR_EACH_SAFE (Entry, NextEntryProcessed, &mNicPrivate->ProcessedAttempts) {
    AttemptEntry = NET_LIST_USER_STRUCT (Entry, NVMEOF_ATTEMPT_ENTRY, Link);
    RemoveEntryList (&AttemptEntry->Link);
    mNicPrivate->ProcessedAttemptCount--;
    FreePool (AttemptEntry);
  }

  if (KatoEvent != NULL) {
    gBS->CloseEvent (KatoEvent);
    KatoEvent = (EFI_EVENT)NULL;
  }

  if (Private->ExitBootServiceEvent != NULL) {
    gBS->CloseEvent (Private->ExitBootServiceEvent);
    Private->ExitBootServiceEvent = (EFI_EVENT)NULL;
  }

  // Search for controller data
  NET_LIST_FOR_EACH_SAFE (Entry, NextEntryProcessed, &gNvmeOfControllerList) {
    CtrlrData = NET_LIST_USER_STRUCT (Entry, NVMEOF_CONTROLLER_DATA, Link);
    if (CtrlrData != NULL) {
      nvme_ctrlr_keep_alive(CtrlrData->ctrlr);
      spdk_nvme_qpair_process_completions(CtrlrData->ctrlr->adminq, 0);
      if (Private->Controller == CtrlrData->NicHandle) {
        spdk_nvme_detach(CtrlrData->ctrlr);
        if (CtrlrData->Link.ForwardLink != NULL
          && CtrlrData->Link.BackLink != NULL) {
          RemoveEntryList(&CtrlrData->Link);
          FreePool(CtrlrData);
        }
      }
    }
  }


  FreePool (Private);
  return Status;

StopError1:
  return EFI_DEVICE_ERROR;
}

/**
  Tests to see if this driver supports a given controller. If a child device is provided,
  it further tests to see if this driver supports creating a handle for the specified child device.

  This function checks to see if the driver specified by This supports the device specified by
  ControllerHandle. Drivers will typically use the device path attached to
  ControllerHandle and/or the services from the bus I/O abstraction attached to
  ControllerHandle to determine if the driver supports ControllerHandle. This function
  may be called many times during platform initialization. In order to reduce boot times, the tests
  performed by this function must be very small, and take as little time as possible to execute. This
  function must not change the state of any hardware devices, and this function must be aware that the
  device specified by ControllerHandle may already be managed by the same driver or a
  different driver. This function must match its calls to AllocatePages() with FreePages(),
  AllocatePool() with FreePool(), and OpenProtocol() with CloseProtocol().
  Since ControllerHandle may have been previously started by the same driver, if a protocol is
  already in the opened state, then it must not be closed with CloseProtocol(). This is required
  to guarantee the state of ControllerHandle is not modified by this function.

  @param[in]  This                 A pointer to the EFI_DRIVER_BINDING_PROTOCOL instance.
  @param[in]  ControllerHandle     The handle of the controller to test. This handle
                                   must support a protocol interface that supplies
                                   an I/O abstraction to the driver.
  @param[in]  RemainingDevicePath  A pointer to the remaining portion of a device path.  This
                                   parameter is ignored by device drivers, and is optional for bus
                                   drivers. For bus drivers, if this parameter is not NULL, then
                                   the bus driver must determine if the bus controller specified
                                   by ControllerHandle and the child controller specified
                                   by RemainingDevicePath are both supported by this
                                   bus driver.

  @retval EFI_SUCCESS              The device specified by ControllerHandle and
                                   RemainingDevicePath is supported by the driver specified by This.
  @retval EFI_ALREADY_STARTED      The device specified by ControllerHandle and
                                   RemainingDevicePath is already being managed by the driver
                                   specified by This.
  @retval EFI_ACCESS_DENIED        The device specified by ControllerHandle and
                                   RemainingDevicePath is already being managed by a different
                                   driver or an application that requires exclusive access.
                                   Currently not implemented.
  @retval EFI_UNSUPPORTED          The device specified by ControllerHandle and
                                   RemainingDevicePath is not supported by the driver specified by This.
**/
EFI_STATUS
EFIAPI
NvmeOfIp4DriverBindingSupported (
  IN EFI_DRIVER_BINDING_PROTOCOL  *This,
  IN EFI_HANDLE                   ControllerHandle,
  IN EFI_DEVICE_PATH_PROTOCOL     *RemainingDevicePath
  )
{
  return NvmeOfSupported (
           This,
           ControllerHandle,
           RemainingDevicePath,
           IP_VERSION_4
           );
}

EFI_STATUS
EFIAPI
NvmeOfIp6DriverBindingSupported (
  IN EFI_DRIVER_BINDING_PROTOCOL  *This,
  IN EFI_HANDLE                   ControllerHandle,
  IN EFI_DEVICE_PATH_PROTOCOL     *RemainingDevicePath
  )
{
  return NvmeOfSupported (
           This,
           ControllerHandle,
           RemainingDevicePath,
           IP_VERSION_6
           );
}

/**
  Starts a device controller or a bus controller.

  The Start() function is designed to be invoked from the EFI boot service ConnectController().
  As a result, much of the error checking on the parameters to Start() has been moved into this
  common boot service. It is legal to call Start() from other locations,
  but the following calling restrictions must be followed or the system behavior will not be deterministic.
  1. ControllerHandle must be a valid EFI_HANDLE.
  2. If RemainingDevicePath is not NULL, then it must be a pointer to a naturally aligned
     EFI_DEVICE_PATH_PROTOCOL.
  3. Prior to calling Start(), the Supported() function for the driver specified by This must
     have been called with the same calling parameters, and Supported() must have returned EFI_SUCCESS.

  @param[in]  This                 A pointer to the EFI_DRIVER_BINDING_PROTOCOL instance.
  @param[in]  ControllerHandle     The handle of the controller to start. This handle
                                   must support a protocol interface that supplies
                                   an I/O abstraction to the driver.
  @param[in]  RemainingDevicePath  A pointer to the remaining portion of a device path.  This
                                   parameter is ignored by device drivers, and is optional for bus
                                   drivers. For a bus driver, if this parameter is NULL, then handles
                                   for all the children of Controller are created by this driver.
                                   If this parameter is not NULL and the first Device Path Node is
                                   not the End of Device Path Node, then only the handle for the
                                   child device specified by the first Device Path Node of
                                   RemainingDevicePath is created by this driver.
                                   If the first Device Path Node of RemainingDevicePath is
                                   the End of Device Path Node, no child handle is created by this
                                   driver.

  @retval EFI_SUCCESS              The device was started.
  @retval EFI_DEVICE_ERROR         The device could not be started due to a device error.Currently not implemented.
  @retval EFI_OUT_OF_RESOURCES     The request could not be completed due to a lack of resources.
  @retval Others                   The driver failded to start the device.

**/
EFI_STATUS
EFIAPI
NvmeOfIp4DriverBindingStart (
  IN EFI_DRIVER_BINDING_PROTOCOL  *This,
  IN EFI_HANDLE                   ControllerHandle,
  IN EFI_DEVICE_PATH_PROTOCOL     *RemainingDevicePath
  )
{
  EFI_STATUS        Status;

  DEBUG ((EFI_D_INFO, "NVMEOF_DRIVER_VERSION is %d\n", NVMEOF_DRIVER_VERSION));
  Status = NvmeOfStart (This->DriverBindingHandle, ControllerHandle, IP_VERSION_4);
  if (Status == EFI_ALREADY_STARTED) {
    Status = EFI_SUCCESS;
  }

  return Status;
}

EFI_STATUS
EFIAPI
NvmeOfIp6DriverBindingStart (
  IN EFI_DRIVER_BINDING_PROTOCOL  *This,
  IN EFI_HANDLE                   ControllerHandle,
  IN EFI_DEVICE_PATH_PROTOCOL     *RemainingDevicePath
  )
{
  EFI_STATUS        Status;

  Status = NvmeOfStart (This->DriverBindingHandle, ControllerHandle, IP_VERSION_6);
  if (Status == EFI_ALREADY_STARTED) {
    Status = EFI_SUCCESS;
  }

  return Status;
}


/**
  Stops a device controller or a bus controller.

  The Stop() function is designed to be invoked from the EFI boot service DisconnectController().
  As a result, much of the error checking on the parameters to Stop() has been moved
  into this common boot service. It is legal to call Stop() from other locations,
  but the following calling restrictions must be followed or the system behavior will not be deterministic.
  1. ControllerHandle must be a valid EFI_HANDLE that was used on a previous call to this
     same driver's Start() function.
  2. The first NumberOfChildren handles of ChildHandleBuffer must all be a valid
     EFI_HANDLE. In addition, all of these handles must have been created in this driver's
     Start() function, and the Start() function must have called OpenProtocol() on
     ControllerHandle with an Attribute of EFI_OPEN_PROTOCOL_BY_CHILD_CONTROLLER.

  @param[in]  This              A pointer to the EFI_DRIVER_BINDING_PROTOCOL instance.
  @param[in]  ControllerHandle  A handle to the device being stopped. The handle must
                                support a bus specific I/O protocol for the driver
                                to use to stop the device.
  @param[in]  NumberOfChildren  The number of child device handles in ChildHandleBuffer.
  @param[in]  ChildHandleBuffer An array of child handles to be freed. May be NULL
                                if NumberOfChildren is 0.

  @retval EFI_SUCCESS           The device was stopped.
  @retval EFI_DEVICE_ERROR      The device could not be stopped due to a device error.

**/
EFI_STATUS
EFIAPI
NvmeOfIp4DriverBindingStop (
  IN  EFI_DRIVER_BINDING_PROTOCOL     *This,
  IN  EFI_HANDLE                      ControllerHandle,
  IN  UINTN                           NumberOfChildren,
  IN  EFI_HANDLE                      *ChildHandleBuffer
  )
{
  return NvmeOfStop (
           This,
           ControllerHandle,
           NumberOfChildren,
           ChildHandleBuffer,
           IP_VERSION_4
           );
}

EFI_STATUS
EFIAPI
NvmeOfIp6DriverBindingStop (
  IN  EFI_DRIVER_BINDING_PROTOCOL     *This,
  IN  EFI_HANDLE                      ControllerHandle,
  IN  UINTN                           NumberOfChildren,
  IN  EFI_HANDLE                      *ChildHandleBuffer
  )
{
  return NvmeOfStop (
           This,
           ControllerHandle,
           NumberOfChildren,
           ChildHandleBuffer,
           IP_VERSION_6
           );
}

/**
  Unload the NVMeOF driver.

  @param[in]  ImageHandle          The handle of the driver image.

  @retval     EFI_SUCCESS          The driver is unloaded.
  @retval     EFI_DEVICE_ERROR     An unexpected error occurred.

**/
EFI_STATUS
EFIAPI
NvmeOfDriverUnload (
  IN EFI_HANDLE  ImageHandle
  )
{
  EFI_STATUS                        Status;
  EFI_COMPONENT_NAME_PROTOCOL       *ComponentName;
  EFI_COMPONENT_NAME2_PROTOCOL      *ComponentName2;
  UINTN                             DeviceHandleCount;
  EFI_HANDLE                        *DeviceHandleBuffer;
  UINTN                             Index;

  //
  // Close "before ExitBootServices" event
  //
  gBS->CloseEvent (gBeforeEBSEvent);
   
  //
  // Disconnect CLI contollers
  //
  if (gCliCtrlMap) {
    NvmeOfCliCleanup ();
  }

  //
  // Try to disconnect the driver from the devices it's controlling.
  //
  DeviceHandleBuffer = NULL;
  Status = gBS->LocateHandleBuffer (
                  AllHandles,
                  NULL,
                  NULL,
                  &DeviceHandleCount,
                  &DeviceHandleBuffer
                  );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  //
  // Disconnect the NvmeOf IPV4 driver from the controlled device.
  //
  for (Index = 0; Index < DeviceHandleCount; Index++) {
    Status = NvmeOfTestManagedDevice (
               DeviceHandleBuffer[Index],
               gNvmeOfIp4DriverBinding.DriverBindingHandle,
               &gEfiTcp4ProtocolGuid
               );
    if (EFI_ERROR (Status)) {
      continue;
    }
    Status = gBS->DisconnectController (
                    DeviceHandleBuffer[Index],
                    gNvmeOfIp4DriverBinding.DriverBindingHandle,
                    NULL
                    );
    if (EFI_ERROR (Status)) {
      goto ON_EXIT;
    }
  }

  //
  // Disconnect the NvmeOf IPV6 driver from the controlled device.
  //
  for (Index = 0; Index < DeviceHandleCount; Index++) {
    Status = NvmeOfTestManagedDevice (
               DeviceHandleBuffer[Index],
               gNvmeOfIp6DriverBinding.DriverBindingHandle,
               &gEfiTcp6ProtocolGuid
               );
    if (EFI_ERROR (Status)) {
      continue;
    }
    Status = gBS->DisconnectController (
                    DeviceHandleBuffer[Index],
                    gNvmeOfIp6DriverBinding.DriverBindingHandle,
                    NULL
                    );
    if (EFI_ERROR(Status)) {
      goto ON_EXIT;
    }
  }

  if (gNvmeOfControllerNameTable != NULL) {
    Status = FreeUnicodeStringTable (gNvmeOfControllerNameTable);
    if (EFI_ERROR (Status)) {
      DEBUG ((EFI_D_ERROR, "NvmeOfDriverUnload: Error freeing unicode table\n"));
      goto ON_EXIT;
    }
    gNvmeOfControllerNameTable = NULL;
  }

  //
  // Uninstall the ComponentName and ComponentName2 protocol from Nvmeof4 driver binding handle
  // if it has been installed.
  //
  Status = gBS->HandleProtocol (
                  gNvmeOfIp4DriverBinding.DriverBindingHandle,
                  &gEfiComponentNameProtocolGuid,
                  (VOID **) &ComponentName
                  );
  if (!EFI_ERROR (Status)) {
    Status = gBS->UninstallMultipleProtocolInterfaces (
                    gNvmeOfIp4DriverBinding.DriverBindingHandle,
                    &gEfiComponentNameProtocolGuid,
                    ComponentName,
                    NULL
                    );
    if (EFI_ERROR (Status)) {
      goto ON_EXIT;
    }
  }

  Status = gBS->HandleProtocol (
                  gNvmeOfIp4DriverBinding.DriverBindingHandle,
                  &gEfiComponentName2ProtocolGuid,
                  (VOID **) &ComponentName2
                  );
  if (!EFI_ERROR (Status)) {
    Status =  gBS->UninstallMultipleProtocolInterfaces (
                     gNvmeOfIp4DriverBinding.DriverBindingHandle,
                     &gEfiComponentName2ProtocolGuid,
                     ComponentName2,
                     NULL
                     );
    if (EFI_ERROR (Status)) {
      goto ON_EXIT;
    }
  }

  //
  // Uninstall the ComponentName and ComponentName2 protocol from NvmeOf6 driver binding handle
  // if it has been installed.
  //
  Status = gBS->HandleProtocol (
                  gNvmeOfIp6DriverBinding.DriverBindingHandle,
                  &gEfiComponentNameProtocolGuid,
                  (VOID **) &ComponentName
                  );
  if (!EFI_ERROR (Status)) {
    Status = gBS->UninstallMultipleProtocolInterfaces (
                    gNvmeOfIp6DriverBinding.DriverBindingHandle,
                    &gEfiComponentNameProtocolGuid,
                    ComponentName,
                    NULL
                    );
    if (EFI_ERROR (Status)) {
      goto ON_EXIT;
    }
  }

  Status = gBS->HandleProtocol (
                  gNvmeOfIp6DriverBinding.DriverBindingHandle,
                  &gEfiComponentName2ProtocolGuid,
                  (VOID **) &ComponentName2
                  );
  if (!EFI_ERROR (Status)) {
  Status =  gBS->UninstallMultipleProtocolInterfaces (
                   gNvmeOfIp6DriverBinding.DriverBindingHandle,
                   &gEfiComponentName2ProtocolGuid,
                   ComponentName2,
                   NULL
                   );
    if (EFI_ERROR (Status)) {
      goto ON_EXIT;
    }
  }

  //
  // Uninstall the driver binding protocols.
  //
  Status = gBS->UninstallMultipleProtocolInterfaces (
                  gNvmeOfIp4DriverBinding.DriverBindingHandle,
                  &gEfiDriverBindingProtocolGuid,
                  &gNvmeOfIp4DriverBinding,
                  NULL
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((EFI_D_ERROR, "Uninstall multiple protocol for ipv4 failed\n"));
    goto ON_EXIT;
  }

  Status = gBS->UninstallMultipleProtocolInterfaces (
                  gNvmeOfIp6DriverBinding.DriverBindingHandle,
                  &gEfiDriverBindingProtocolGuid,
                  &gNvmeOfIp6DriverBinding,
                  NULL
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((EFI_D_ERROR, "Uninstall multiple protocol for ipv6 failed\n"));
  }         

  Status = gBS->UninstallMultipleProtocolInterfaces (
                  gPassThroughProtocolHandle,
                  &gNvmeofPassThroughProtocolGuid,
                  &gNvmeofPassThroughInstance,
                  NULL
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((EFI_D_ERROR, "Uninstall multiple protocol for Passthrough failed\n"));
  }

  if (mNicPrivate != NULL) {
    FreePool (mNicPrivate);
  }

  ASSERT (IsListEmpty (&gNvmeOfControllerList));

  if (gCliCtrlMap != NULL) {
    FreePool (gCliCtrlMap);
    gCliCtrlMap = NULL;
  }

  if (CtrlrInfo != NULL) {
    FreePool (CtrlrInfo);
    CtrlrInfo = NULL;
  }

  if (gNvmeOfImagePath != NULL) {
    FreePool (gNvmeOfImagePath);
    gNvmeOfImagePath = NULL;
  }

ON_EXIT:
  return Status;
}


/**
  The entry point for NvmeOf driver, used to install Nvm Express driver on the ImageHandle.

  @param  ImageHandle   The firmware allocated handle for this driver image.
  @param  SystemTable   Pointer to the EFI system table.

  @retval EFI_SUCCESS   Driver loaded.
  @retval other         Driver not loaded.

 **/

EFI_STATUS
EFIAPI
NvmeOfDriverEntry (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS                         Status;
  CHAR16      *Name = NULL;
  CHAR8       Utf8Name[512] = { 0 };
  UINT16      Length;

  //Set Host NBFT Info
  mImageHandler = ImageHandle;
  // Get Driver Image path for NBFT
  if (NvmeOfGetDriverImageName (mImageHandler, &Name) == EFI_SUCCESS) {
    UnicodeStrToAsciiStrS (Name, Utf8Name, 512);
    Length = AsciiStrLen (Utf8Name);
    if (Name != NULL) {
      FreePool (Name);
    }
    gNvmeOfImagePath = AllocateZeroPool (Length+1);
    CopyMem (gNvmeOfImagePath, Utf8Name, Length);
  }
  NvmeOfPublishNbft (TRUE);

  //
  // Initialize the EFI Driver Library.
  //
  Status = EfiLibInstallDriverBindingComponentName2 (
             ImageHandle,
             SystemTable,
             &gNvmeOfIp4DriverBinding,
             ImageHandle,
             &gNvmeOfComponentName,
             &gNvmeOfComponentName2
             );
  if (EFI_ERROR (Status)) {
    DEBUG ((EFI_D_ERROR, "NvmeOfDriverEntry: EfiLibInstallDriverBindingComponentName2 \
      Ipv4 failed\n"));
    return Status;
  }
  
  Status = EfiLibInstallDriverBindingComponentName2 (
             ImageHandle,
             SystemTable,
             &gNvmeOfIp6DriverBinding,
             NULL,
             &gNvmeOfComponentName,
             &gNvmeOfComponentName2
             );
  if (EFI_ERROR (Status)) {
     DEBUG ((EFI_D_ERROR, "NvmeOfDriverEntry: EfiLibInstallDriverBindingComponentName2 \
       Ipv6 failed\n"));
     goto Error1;
  }

  mImageHandler = ImageHandle;
  Status = gBS->InstallMultipleProtocolInterfaces (
                  &gPassThroughProtocolHandle,
                  &gNvmeofPassThroughProtocolGuid,
                  &gNvmeofPassThroughInstance,
                  NULL);
  if (EFI_ERROR (Status)) {
    DEBUG ((EFI_D_ERROR, "NvmeOfDriverEntry: InstallMultipleProtocolInterfaces \
       passthrough failed\n"));
    goto Error2;
  }
   
  //
  // Create the private data structures.
  //
  mNicPrivate = AllocateZeroPool (sizeof (NVMEOF_NIC_PRIVATE_DATA));
  if (mNicPrivate == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    DEBUG ((EFI_D_ERROR, "NvmeOfDriverEntry: AllocateZeroPool failed\n"));
    goto Error2;
  }

  InitializeListHead (&gNvmeOfControllerList);

  gCliCtrlMap = AllocateZeroPool (sizeof (NVMEOF_CLI_CTRL_MAPPING));
  if (gCliCtrlMap == NULL) {
    goto Error2;;
  }

  CtrlrInfo = AllocateZeroPool (sizeof (NVMEOF_CLI_CTRL_MAPPING));
  if (CtrlrInfo == NULL) {
    goto Error2;
  }
   
  InitializeListHead (&mNicPrivate->NicInfoList);
  InitializeListHead (&mNicPrivate->AttemptConfigs);
  InitializeListHead (&mNicPrivate->ProcessedAttempts);
  InitializeListHead (&gCliCtrlMap->CliCtrlrList);
  InitializeListHead (&CtrlrInfo->CliCtrlrList);

  //
  // Create event for BeforeExitBootServices group.
  //
  Status = gBS->CreateEventEx (
                  EVT_NOTIFY_SIGNAL,
                  TPL_CALLBACK,
                  NvmeOfBeforeEBS,
                  NULL,
                  &gEfiEventBeforeExitBootServicesGuid,
                  &gBeforeEBSEvent
                  );

  if (EFI_ERROR (Status)) {
    goto Error2;
  }

  return Status;

Error2:
 EfiLibUninstallDriverBindingComponentName2 (
   &gNvmeOfIp6DriverBinding,
   &gNvmeOfComponentName,
   &gNvmeOfComponentName2
   );

   // Free the allocations in case of failure.
   if (mNicPrivate != NULL) {
     FreePool (mNicPrivate);
   }
   if (gCliCtrlMap != NULL) {
     FreePool (gCliCtrlMap);
   }
   if (CtrlrInfo != NULL) {
     FreePool (CtrlrInfo);
   }

Error1:
  EfiLibUninstallDriverBindingComponentName2 (
    &gNvmeOfIp4DriverBinding,
    &gNvmeOfComponentName,
    &gNvmeOfComponentName2
    );
   
  return Status;
}

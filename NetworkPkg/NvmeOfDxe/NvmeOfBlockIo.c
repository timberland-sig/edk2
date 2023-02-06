/** @file
  NvmeOf driver BlockIo is used to manage non-volatile memory over fabric which follows
  NVM Express specification.

  Copyright (c) 2020, Dell EMC All rights reserved
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "NvmeOfBlockIo.h"

/**
  Reset the Block Device.

  @param  This                 Indicates a pointer to the calling context.
  @param  ExtendedVerification Driver may perform diagnostics on reset.

  @retval EFI_SUCCESS          The device was reset.
  @retval EFI_DEVICE_ERROR     The device is not functioning properly and could
                               not be reset.

**/
EFI_STATUS
EFIAPI
NvmeOfBlockIoReset (
  IN  EFI_BLOCK_IO_PROTOCOL   *This,
  IN  BOOLEAN                 ExtendedVerification
  )
{
  EFI_STATUS                      Status = EFI_SUCCESS;
  EFI_TPL                         OldTpl;
  NVMEOF_DEVICE_PRIVATE_DATA      *Device;
  NVMEOF_DRIVER_DATA              *Private;
 
  DEBUG ((DEBUG_INFO, "NvmeOfBlockIoReset\n"));

  if (This == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  if (IsListEmpty (&gNvmeOfControllerList)) {
    return EFI_INVALID_PARAMETER;
  }

  //
  // For Nvm Express subsystem, reset block device means reset controller.
  //
  OldTpl = gBS->RaiseTPL (TPL_CALLBACK);
  Device = NVMEOF_DEVICE_PRIVATE_DATA_FROM_BLOCK_IO (This);
  if (Device == NULL) {
    Status = EFI_DEVICE_ERROR;
    goto ForcedExit;
  }
  if (Device->qpair == NULL) {
    Status = EFI_INVALID_PARAMETER;
    goto ForcedExit;
  }

  Private = Device->Controller;
  if (Private == NULL) {
    Status = EFI_DEVICE_ERROR;
    goto ForcedExit;
  }

  //
  // Do not perform actual reset when DriverBinding->Stop was called.
  //
  if (Private->IsStopping) {
    Status = EFI_SUCCESS;
    goto ForcedExit;
  }

  Status = spdk_nvme_ctrlr_reset (Device->NameSpace->ctrlr);
  Device->TcpIo = Device->Controller->TcpIo;
  if (EFI_ERROR (Status)) {
    Status = EFI_DEVICE_ERROR;
  }
  gBS->RestoreTPL (OldTpl);

  Device->qpair = spdk_nvme_ctrlr_alloc_io_qpair (Device->NameSpace->ctrlr, NULL, 0);
  if (Device->qpair == NULL) {
    DEBUG ((DEBUG_ERROR, "spdk_nvme_ctrlr_alloc_io_qpair() failed\n"));
    Status = EFI_DEVICE_ERROR;
  }

  return Status;

ForcedExit:
  gBS->RestoreTPL (OldTpl);
  return Status;
}

/**
  Flush the Block Device.

  @param  This              Indicates a pointer to the calling context.

  @retval EFI_SUCCESS       All outstanding data was written to the device.
  @retval EFI_DEVICE_ERROR  The device reported an error while writing back the data.
  @retval EFI_NO_MEDIA      There is no media in the device.

**/
EFI_STATUS
EFIAPI
NvmeOfBlockIoFlushBlocks (
  IN  EFI_BLOCK_IO_PROTOCOL   *This
  )
{
  NVMEOF_DEVICE_PRIVATE_DATA        *Device = NULL;
  EFI_STATUS                        Status;
  EFI_TPL                           OldTpl;
  UINT8                             IsCompleted = 0;
  int                               rc = 0;

  //
  // Check parameters.
  //
  if (This == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  if (IsListEmpty (&gNvmeOfControllerList)) {
    return EFI_INVALID_PARAMETER;
  }

  OldTpl = gBS->RaiseTPL (TPL_CALLBACK);

  Device = NVMEOF_DEVICE_PRIVATE_DATA_FROM_BLOCK_IO (This);
  if (Device == NULL) {
    gBS->RestoreTPL (OldTpl);
    return EFI_DEVICE_ERROR;
  }
  if (Device->qpair == NULL) {
    gBS->RestoreTPL (OldTpl);
    return EFI_INVALID_PARAMETER;
  }

  Status = spdk_nvme_ns_cmd_flush (Device->NameSpace, Device->qpair, IoComplete, &IsCompleted);

  while (!IsCompleted) {
    rc = spdk_nvme_qpair_process_completions (Device->qpair, 0);
    if (rc < 0) {
      Status = EFI_DEVICE_ERROR;
      break;
    }
  }
  gBS->RestoreTPL (OldTpl);

  return Status;
}

/**
  Reset the block device hardware.

  @param[in]  This                 Indicates a pointer to the calling context.
  @param[in]  ExtendedVerification Indicates that the driver may perform a more
                                   exhausive verfication operation of the
                                   device during reset.

  @retval EFI_SUCCESS          The device was reset.
  @retval EFI_DEVICE_ERROR     The device is not functioning properly and could
                               not be reset.

**/
EFI_STATUS
EFIAPI
NvmeOfBlockIoResetEx (
  IN EFI_BLOCK_IO2_PROTOCOL  *This,
  IN BOOLEAN                 ExtendedVerification
  )
{
  EFI_STATUS                      Status;
  NVMEOF_DRIVER_DATA              *Private;
  NVMEOF_DEVICE_PRIVATE_DATA      *Device;
  BOOLEAN                         IsEmpty;
  EFI_TPL                         OldTpl;
  UINT8                           Counter = 0;

  if (This == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  if (IsListEmpty (&gNvmeOfControllerList)) {
    return EFI_INVALID_PARAMETER;
  }

  Device = NVMEOF_DEVICE_PRIVATE_DATA_FROM_BLOCK_IO2 (This);
  if (Device == NULL) {
    return EFI_DEVICE_ERROR;
  }
  if (Device->qpair == NULL) {
    return EFI_INVALID_PARAMETER;
  }
  Private = Device->Controller;
  if (Private == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  //
  // Do not perform actual reset when DriverBinding->Stop was called.
  //
  if (Private->IsStopping) {
    return EFI_SUCCESS;
  }

  //
  // Wait for the asynchronous queue to become empty.
  //
  while (TRUE) {
    OldTpl = gBS->RaiseTPL (TPL_NOTIFY);
    if (!((&Private->UnsubmittedSubtasks.ForwardLink != NULL) && (&Private->UnsubmittedSubtasks.BackLink != NULL))) {
      IsEmpty = IsListEmpty (&Private->UnsubmittedSubtasks);
    } else {
      IsEmpty = TRUE;
    }
    gBS->RestoreTPL (OldTpl);

    Counter++;
    if (Counter == 10) {
      DEBUG ((DEBUG_INFO, "Exiting since delay timeout\n"));
      break;
    }

    if (IsEmpty) {
      break;
    }

    gBS->Stall (DELAY);
  }

  OldTpl = gBS->RaiseTPL (TPL_CALLBACK);

  Status = spdk_nvme_ctrlr_reset (Device->NameSpace->ctrlr);
  Device->TcpIo = Device->Controller->TcpIo;

  if (EFI_ERROR (Status)) {
    Status = EFI_DEVICE_ERROR;
  }

  gBS->RestoreTPL (OldTpl);

  Device->qpair = spdk_nvme_ctrlr_alloc_io_qpair (Device->NameSpace->ctrlr, NULL, 0);
  if (Device->qpair == NULL) {
    DEBUG ((DEBUG_ERROR, "spdk_nvme_ctrlr_alloc_io_qpair() failed\n"));
    Status = EFI_DEVICE_ERROR;
  }

  return Status;
}

/**
  Flush the Block Device.

  If EFI_DEVICE_ERROR, EFI_NO_MEDIA,_EFI_WRITE_PROTECTED or EFI_MEDIA_CHANGED
  is returned and non-blocking I/O is being used, the Event associated with
  this request will not be signaled.

  @param[in]      This     Indicates a pointer to the calling context.
  @param[in,out]  Token    A pointer to the token associated with the
                           transaction.

  @retval EFI_SUCCESS          The flush request was queued if Event is not
                               NULL.
                               All outstanding data was written correctly to
                               the device if the Event is NULL.
  @retval EFI_DEVICE_ERROR     The device reported an error while writting back
                               the data.
  @retval EFI_WRITE_PROTECTED  The device cannot be written to.
  @retval EFI_NO_MEDIA         There is no media in the device.
  @retval EFI_MEDIA_CHANGED    The MediaId is not for the current media.
  @retval EFI_OUT_OF_RESOURCES The request could not be completed due to a lack
                               of resources.

**/
EFI_STATUS
EFIAPI
NvmeOfBlockIoFlushBlocksEx (
  IN     EFI_BLOCK_IO2_PROTOCOL   *This,
  IN OUT EFI_BLOCK_IO2_TOKEN      *Token
  )
{
  NVMEOF_DEVICE_PRIVATE_DATA      *Device = NULL;
  BOOLEAN                         IsEmpty;
  EFI_TPL                         OldTpl;
  UINT8                           Counter = 0;

  // Check parameters.
  if (This == NULL) {
      return EFI_INVALID_PARAMETER;
  }

  if (IsListEmpty (&gNvmeOfControllerList)) {
    return EFI_INVALID_PARAMETER;
  }

  Device = NVMEOF_DEVICE_PRIVATE_DATA_FROM_BLOCK_IO2 (This);
  if (Device == NULL) {
    return EFI_DEVICE_ERROR;
  }
  if (Device->qpair == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  // Wait for the asynchronous I/O queue to become empty.
  while (TRUE) {
    OldTpl = gBS->RaiseTPL (TPL_NOTIFY);
    IsEmpty = IsListEmpty (&Device->AsyncQueue);
    gBS->RestoreTPL (OldTpl);

    Counter++;
    if (Counter == 10) {
      DEBUG ((DEBUG_INFO, "Exiting since delay timeout\n"));
      break;
    }

    if (IsEmpty) {
      break;
    }
      gBS->Stall (DELAY);
  }

    // Signal caller event
  if ((Token != NULL) && (Token->Event != NULL)) {
    Token->TransactionStatus = EFI_SUCCESS;
    gBS->SignalEvent (Token->Event);
  }
  return EFI_SUCCESS;
}

/**
  IO completion callback.

  @param  arg                 Variable to update completion.
  @param  spdk_nvme_cpl       Completion instance from SPDK.
**/
VOID 
IoComplete (
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
  Read some sectors from the device.

  @param  Device                 The pointer to the NVMEOF_DEVICE_PRIVATE_DATA data structure.
  @param  Buffer                 The buffer used to store the data read from the device.
  @param  Lba                    The start block number.
  @param  Blocks                 Total block number to be read.

  @retval EFI_SUCCESS            Datum are read from the device.
  @retval Others                 Fail to read all the datum.

**/
EFI_STATUS
ReadSectors (
  IN NVMEOF_DEVICE_PRIVATE_DATA         *Device,
  IN UINT64                             Buffer,
  IN UINT64                             Lba,
  IN UINT32                             Blocks
  )
{
  UINT8      IsCompleted = 0;
  EFI_STATUS Status;
  int        rc = 0;

  Status = spdk_nvme_ns_cmd_read (Device->NameSpace,
    Device->qpair,
    (void*)Buffer,
    Lba,
    Blocks,
    IoComplete,
    &IsCompleted,
    0);
  if (Status == 0) {
    while (!IsCompleted) {
      rc = spdk_nvme_qpair_process_completions (Device->qpair, 0);
      if (rc < 0) {
        IsCompleted = ERROR_IN_COMPLETION;
        break;
      }
    }
  } else {
    IsCompleted = ERROR_IN_COMPLETION;
  }

  if (IsCompleted == ERROR_IN_COMPLETION) {
    DEBUG ((DEBUG_ERROR, "ReadSectors: Error In Process Read Data\n"));
    Status = EFI_DEVICE_ERROR;
  }

  return Status;
}
/**
  Read some blocks from the device.

  @param  Device                 The pointer to the NVMEOF_DEVICE_PRIVATE_DATA data structure.
  @param  Buffer                 The buffer used to store the data read from the device.
  @param  Lba                    The start block number.
  @param  Blocks                 Total block number to be read.

  @retval EFI_SUCCESS            Datum are read from the device.
  @retval Others                 Fail to read all the datum.
**/
EFI_STATUS
NvmeOfRead (
  IN     NVMEOF_DEVICE_PRIVATE_DATA     *Device,
  OUT    VOID                           *Buffer,
  IN     UINT64                         Lba,
  IN     UINTN                          Blocks
  )
{
  EFI_STATUS           Status;
  UINT32               BlockSize;
  UINT32               MaxTransferBlocks;
  UINTN                OrginalBlocks;
  BOOLEAN              IsEmpty;
  EFI_TPL              OldTpl;
  UINT8                Counter = 0;
  Status             = EFI_SUCCESS;

  if (Buffer == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  //
  // Wait for the device's asynchronous I/O queue to become empty.
  //
  while (TRUE) {
    OldTpl  = gBS->RaiseTPL (TPL_NOTIFY);
    IsEmpty = IsListEmpty (&Device->AsyncQueue);
    gBS->RestoreTPL (OldTpl);

    Counter++;
    if (Counter == 10) {
      DEBUG ((DEBUG_INFO, "Exiting since delay timeout\n"));
      break;
    }

    if (IsEmpty) {
      break;
    }

    gBS->Stall (DELAY);
  }

  BlockSize         = spdk_nvme_ns_get_sector_size (Device->NameSpace);
  OrginalBlocks     = Blocks;
     
  MaxTransferBlocks = spdk_nvme_ctrlr_get_max_xfer_size (Device->NameSpace->ctrlr);

  while (Blocks > 0) {
    if (Blocks > MaxTransferBlocks) {
      Status  = ReadSectors (Device, (UINT64)(UINTN)Buffer, Lba, MaxTransferBlocks);
      Blocks -= MaxTransferBlocks;
      Buffer  = (VOID *)(UINTN)((UINT64)(UINTN)Buffer + MaxTransferBlocks * BlockSize);
      Lba    += MaxTransferBlocks;
    } else {
      Status = ReadSectors (Device, (UINT64)(UINTN)Buffer, Lba, (UINT32)Blocks);
      Blocks = 0;
    }
    if (EFI_ERROR (Status)) {
      break;
    }
  }

  DEBUG ((DEBUG_BLKIO, "NvmeOfRead: Lba = 0x%08Lx, Original = 0x%08Lx, "
    "Remaining = 0x%08Lx, BlockSize = 0x%x, Status = %r\n", Lba,
    (UINT64)OrginalBlocks, (UINT64)Blocks, BlockSize, Status));

  return Status;
}
/**
  Nonblocking I/O callback funtion when the event is signaled.

  @param[in]  Event     The Event this notify function registered to.
  @param[in]  Context   Pointer to the context data registered to the
                        Event.
**/
VOID
EFIAPI
AsyncIoCallback (
  IN EFI_EVENT                Event,
  IN VOID                     *Context
  )
{
  NVMEOF_BLKIO2_SUBTASK         *Subtask;
  NVMEOF_BLKIO2_REQUEST         *Request;  
  EFI_BLOCK_IO2_TOKEN           *Token;

  gBS->CloseEvent (Event);

  Subtask    = (NVMEOF_BLKIO2_SUBTASK *) Context;  
  Request    = Subtask->BlockIo2Request;
  Token      = Request->Token;  
  
  // Remove the subtask from the BlockIo2 subtasks list.
  RemoveEntryList (&Subtask->Link);

  if (IsListEmpty (&Request->SubtasksQueue) && Request->LastSubtaskSubmitted) {
    // Remove the BlockIo2 request from the device asynchronous queue.
    RemoveEntryList (&Request->Link);
    FreePool (Request);
    gBS->SignalEvent (Token->Event);
  }
  
  FreePool (Subtask->NvmeOfAsyncData);
  FreePool (Subtask);

  return;
}

/**
  Read some sectors from the device in an asynchronous manner.

  @param  Device        The pointer to the NVMEOF_DEVICE_PRIVATE_DATA data
                        structure.
  @param  Request       The pointer to the NVMEOF_BLKIO2_REQUEST data structure.
  @param  Buffer        The buffer used to store the data read from the device.
  @param  Lba           The start block number.
  @param  Blocks        Total block number to be read.
  @param  IsLast        The last subtask of an asynchronous read request.

  @retval EFI_SUCCESS   Asynchronous read request has been queued.
  @retval Others        Fail to send the asynchronous request.
**/
EFI_STATUS
AsyncReadSectors (
  IN NVMEOF_DEVICE_PRIVATE_DATA         *Device,
  IN NVMEOF_BLKIO2_REQUEST              *Request,
  IN UINT64                             Buffer,
  IN UINT64                             Lba,
  IN UINT32                             Blocks,
  IN BOOLEAN                            IsLast
  )
{
  NVMEOF_DRIVER_DATA                       *Private = NULL;
  NVMEOF_BLKIO2_SUBTASK                    *Subtask = NULL;
  NVMEOF_ASYNC_CMD_DATA                    *NvmeOfAsyncData = NULL;
  EFI_STATUS                               Status;
  EFI_TPL                                  OldTpl;

  Private = Device->Controller;
  Subtask = AllocateZeroPool (sizeof(NVMEOF_BLKIO2_SUBTASK));

  if (Subtask == NULL) {
      Status = EFI_OUT_OF_RESOURCES;
      goto ErrorExit;
  }

  Subtask->Signature = NVMEOF_BLKIO2_SUBTASK_SIGNATURE;
  Subtask->IsLast = IsLast;
  Subtask->NamespaceId = Device->NamespaceId;
  Subtask->BlockIo2Request = Request;

  NvmeOfAsyncData = AllocateZeroPool (sizeof(NVMEOF_ASYNC_CMD_DATA));
  if (NvmeOfAsyncData == NULL) {
      Status = EFI_OUT_OF_RESOURCES;
      goto ErrorExit;
  } else {
      Subtask->NvmeOfAsyncData = NvmeOfAsyncData;
  }

  NvmeOfAsyncData->Lba = Lba;
  NvmeOfAsyncData->Blocks = Blocks;
  NvmeOfAsyncData->Buffer = Buffer;
  NvmeOfAsyncData->IsRead = TRUE;
  NvmeOfAsyncData->Device = Device;
  NvmeOfAsyncData->IsCompleted = 0;

  // Create Event for Cleanup Subtask.  
  Status = gBS->CreateEvent (
                  EVT_NOTIFY_SIGNAL,
                  TPL_NOTIFY,
                  AsyncIoCallback,
                  Subtask,
                  &Subtask->Event
                  );
  if (EFI_ERROR (Status)) {
      goto ErrorExit;
  }

  OldTpl = gBS->RaiseTPL (TPL_NOTIFY);
  InsertTailList (&Private->UnsubmittedSubtasks, &Subtask->Link);
  Request->UnsubmittedSubtaskNum++;
  gBS->RestoreTPL (OldTpl);

  return EFI_SUCCESS;

ErrorExit:
  // Resource cleanup if asynchronous read request has not been queued.
  if (Subtask != NULL) {
    if (Subtask->Event != NULL) {
      gBS->CloseEvent (Subtask->Event);
    }

    if (NvmeOfAsyncData != NULL) {
      FreePool (NvmeOfAsyncData);
    }
      FreePool (Subtask);
  }

  return Status;
}
/**
  Read some blocks from the device in an asynchronous manner.

  @param  Device        The pointer to the NVMEOF_DEVICE_PRIVATE_DATA data
                        structure.
  @param  Buffer        The buffer used to store the data read from the device.
  @param  Lba           The start block number.
  @param  Blocks        Total block number to be read.
  @param  Token         A pointer to the token associated with the transaction.

  @retval EFI_SUCCESS   Data are read from the device.
  @retval Others        Fail to read all the data.
**/
EFI_STATUS
NvmeOfAsyncRead (
  IN     NVMEOF_DEVICE_PRIVATE_DATA     *Device,
  OUT    VOID                           *Buffer,
  IN     UINT64                         Lba,
  IN     UINTN                          Blocks,
  IN     EFI_BLOCK_IO2_TOKEN            *Token
  )
{
  EFI_STATUS                       Status;
  UINT32                           BlockSize;
  NVMEOF_BLKIO2_REQUEST            *BlkIo2Req = NULL;
  UINT32                           MaxTransferBlocks;
  UINTN                            OrginalBlocks;
  BOOLEAN                          IsEmpty;
  EFI_TPL                          OldTpl;

  Status = EFI_SUCCESS;

  if (Buffer == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  BlockSize = spdk_nvme_ns_get_sector_size (Device->NameSpace);
  OrginalBlocks = Blocks;
  BlkIo2Req = AllocateZeroPool (sizeof (NVMEOF_BLKIO2_REQUEST));
  if (BlkIo2Req == NULL) {
      return EFI_OUT_OF_RESOURCES;
  }

  BlkIo2Req->Signature = NVMEOF_BLKIO2_REQUEST_SIGNATURE;
  BlkIo2Req->Token = Token;

  OldTpl = gBS->RaiseTPL (TPL_NOTIFY);
  InsertTailList (&Device->AsyncQueue, &BlkIo2Req->Link);
  gBS->RestoreTPL (OldTpl);

  InitializeListHead (&BlkIo2Req->SubtasksQueue);

  MaxTransferBlocks = spdk_nvme_ctrlr_get_max_xfer_size (Device->NameSpace->ctrlr);

  while (Blocks > 0) {
    if (Blocks > MaxTransferBlocks) {
        Status = AsyncReadSectors (
                   Device,
                   BlkIo2Req, (UINT64)(UINTN)Buffer,
                   Lba,
                   MaxTransferBlocks,
                   FALSE
                   );

        Blocks -= MaxTransferBlocks;
        Buffer = (VOID *)(UINTN)((UINT64)(UINTN)Buffer + MaxTransferBlocks * BlockSize);
        Lba += MaxTransferBlocks;
    } else {
        Status = AsyncReadSectors (
                   Device,
                   BlkIo2Req,
                   (UINT64)(UINTN)Buffer,
                   Lba,
                   (UINT32)Blocks,
                   TRUE
                   );
        Blocks = 0;
    }

    if (EFI_ERROR (Status)) {
      OldTpl = gBS->RaiseTPL (TPL_NOTIFY);
      IsEmpty = IsListEmpty (&BlkIo2Req->SubtasksQueue) &&
                  (BlkIo2Req->UnsubmittedSubtaskNum == 0);
      if (IsEmpty) {
        // Remove the BlockIo2 request from the device asynchronous queue.
        RemoveEntryList (&BlkIo2Req->Link);
        FreePool (BlkIo2Req);
        Status = EFI_DEVICE_ERROR;
      } else {
        // There are previous BlockIo2 subtasks still running, EFI_SUCCESS
        // should be returned to make sure that the caller does not free
        // resources still using by these requests.
        Status = EFI_SUCCESS;
        if (Token != NULL) {
            Token->TransactionStatus = EFI_DEVICE_ERROR;
        }
        BlkIo2Req->LastSubtaskSubmitted = TRUE;
      }
      gBS->RestoreTPL (OldTpl);
      break;
    }
  }

  DEBUG ((DEBUG_BLKIO, "NvmeOfAsyncRead: Lba = 0x%08Lx, Original = 0x%08Lx, "
      "Remaining = 0x%08Lx, BlockSize = 0x%x, Status = %r\n", Lba,
      (UINT64)OrginalBlocks, (UINT64)Blocks, BlockSize, Status));

  return Status;
}
/**
  Read BufferSize bytes from Lba into Buffer.

  @param  This       Indicates a pointer to the calling context.
  @param  MediaId    Id of the media, changes every time the media is replaced.
  @param  Lba        The starting Logical Block Address to read from.
  @param  BufferSize Size of Buffer, must be a multiple of device block size.
  @param  Buffer     A pointer to the destination buffer for the data. The caller is
                     responsible for either having implicit or explicit ownership of the buffer.

  @retval EFI_SUCCESS           The data was read correctly from the device.
  @retval EFI_DEVICE_ERROR      The device reported an error while performing the read.
  @retval EFI_MEDIA_CHANGED     The MediaId does not matched the current device.
  @retval EFI_BAD_BUFFER_SIZE   The Buffer was not a multiple of the block size of the device.
  @retval EFI_INVALID_PARAMETER The read request contains LBAs that are not valid,
                                or the buffer is not on proper alignment.
**/
EFI_STATUS
EFIAPI
NvmeOfBlockIoReadBlocks (
  IN  EFI_BLOCK_IO_PROTOCOL   *This,
  IN  UINT32                  MediaId,
  IN  EFI_LBA                 Lba,
  IN  UINTN                   BufferSize,
  OUT VOID                    *Buffer
  )
{
  NVMEOF_DEVICE_PRIVATE_DATA        *Device;
  EFI_STATUS                        Status;
  EFI_BLOCK_IO_MEDIA                *Media;
  UINTN                             BlockSize;
  UINTN                             NumberOfBlocks;
  UINTN                             IoAlign;
  EFI_TPL                           OldTpl;
  //
  // Check parameters.
  //
  if (This == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  if (IsListEmpty (&gNvmeOfControllerList)) {
    return EFI_INVALID_PARAMETER;
  }

  Media = This->Media;

  if (!Media->MediaPresent) {
    return EFI_NO_MEDIA;
  }
  if (MediaId != Media->MediaId) {
    return EFI_MEDIA_CHANGED;
  }

  if (Buffer == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  if (BufferSize == 0) {
    return EFI_SUCCESS;
  }

  BlockSize = Media->BlockSize;
  if ((BufferSize % BlockSize) != 0) {
    return EFI_BAD_BUFFER_SIZE;
  }

  NumberOfBlocks  = BufferSize / BlockSize;
  if ((Lba + NumberOfBlocks - 1) > Media->LastBlock) {
    return EFI_INVALID_PARAMETER;
  }

  IoAlign = Media->IoAlign;
  if (IoAlign > 0 && (((UINTN) Buffer & (IoAlign - 1)) != 0)) {
    return EFI_INVALID_PARAMETER;
  }

  OldTpl = gBS->RaiseTPL (TPL_CALLBACK);

  Device = NVMEOF_DEVICE_PRIVATE_DATA_FROM_BLOCK_IO (This);

  if (Device->qpair == NULL) {
    gBS->RestoreTPL (OldTpl);
    return EFI_INVALID_PARAMETER;
  }

  Status = NvmeOfRead (Device, Buffer, Lba, NumberOfBlocks);

  gBS->RestoreTPL (OldTpl);
  return Status;
}

/**
  Read BufferSize bytes from Lba into Buffer.

  This function reads the requested number of blocks from the device. All the
  blocks are read, or an error is returned.
  If EFI_DEVICE_ERROR, EFI_NO_MEDIA,_or EFI_MEDIA_CHANGED is returned and
  non-blocking I/O is being used, the Event associated with this request will
  not be signaled.

  @param[in]       This       Indicates a pointer to the calling context.
  @param[in]       MediaId    Id of the media, changes every time the media is
                              replaced.
  @param[in]       Lba        The starting Logical Block Address to read from.
  @param[in, out]  Token      A pointer to the token associated with the
                              transaction.
  @param[in]       BufferSize Size of Buffer, must be a multiple of device
                              block size.
  @param[out]      Buffer     A pointer to the destination buffer for the data.
                              The caller is responsible for either having
                              implicit or explicit ownership of the buffer.

  @retval EFI_SUCCESS           The read request was queued if Token->Event is
                                not NULL.The data was read correctly from the
                                device if the Token->Event is NULL.
  @retval EFI_DEVICE_ERROR      The device reported an error while performing
                                the read.
  @retval EFI_MEDIA_CHANGED     The MediaId is not for the current media.
  @retval EFI_BAD_BUFFER_SIZE   The BufferSize parameter is not a multiple of
                                the intrinsic block size of the device.
  @retval EFI_INVALID_PARAMETER The read request contains LBAs that are not
                                valid, or the buffer is not on proper
                                alignment.
  @retval EFI_OUT_OF_RESOURCES  The request could not be completed due to a
                                lack of resources.

**/
EFI_STATUS
EFIAPI
NvmeOfBlockIoReadBlocksEx (
  IN     EFI_BLOCK_IO2_PROTOCOL *This,
  IN     UINT32                 MediaId,
  IN     EFI_LBA                Lba,
  IN OUT EFI_BLOCK_IO2_TOKEN    *Token,
  IN     UINTN                  BufferSize,
  OUT    VOID                   *Buffer
  )
{
  NVMEOF_DEVICE_PRIVATE_DATA      *Device;
  EFI_STATUS                      Status;
  EFI_BLOCK_IO_MEDIA              *Media;
  UINTN                           BlockSize;
  UINTN                           NumberOfBlocks;
  UINTN                           IoAlign;
  EFI_TPL                         OldTpl;

  // Check parameters.
  if (This == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  if (IsListEmpty (&gNvmeOfControllerList)) {
    return EFI_INVALID_PARAMETER;
  }

  Media = This->Media;

  if (!Media->MediaPresent) {
    return EFI_NO_MEDIA;
  }

  if (MediaId != Media->MediaId) {
    return EFI_MEDIA_CHANGED;
  }

  if (Buffer == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  if (BufferSize == 0) {
    if ((Token != NULL) && (Token->Event != NULL)) {
      Token->TransactionStatus = EFI_SUCCESS;
      gBS->SignalEvent (Token->Event);
    }
    return EFI_SUCCESS;
  }

  BlockSize = Media->BlockSize;
  if ((BufferSize % BlockSize) != 0) {
    return EFI_BAD_BUFFER_SIZE;
  }

  NumberOfBlocks = BufferSize / BlockSize;
  if ((Lba + NumberOfBlocks - 1) > Media->LastBlock) {
    return EFI_INVALID_PARAMETER;
  }

  IoAlign = Media->IoAlign;
  if (IoAlign > 0 && (((UINTN)Buffer & (IoAlign - 1)) != 0)) {
    return EFI_INVALID_PARAMETER;
  }

  OldTpl = gBS->RaiseTPL (TPL_CALLBACK);
  Device = NVMEOF_DEVICE_PRIVATE_DATA_FROM_BLOCK_IO2 (This);
  if ((Token != NULL) && (Token->Event != NULL)) {
    Token->TransactionStatus = EFI_SUCCESS;
    Status = NvmeOfAsyncRead (Device, Buffer, Lba, NumberOfBlocks, Token);
  } else {
    Status = NvmeOfRead (Device, Buffer, Lba, NumberOfBlocks);
  }
  gBS->RestoreTPL (OldTpl);

  return Status;
}

/**
  Write some sectors to the device.

  @param  Device                 The pointer to the NVMEOF_DEVICE_PRIVATE_DATA data structure.
  @param  Buffer                 The buffer to be written into the device.
  @param  Lba                    The start block number.
  @param  Blocks                 Total block number to be written.

  @retval EFI_SUCCESS            Datum are written into the buffer.
  @retval Others                 Fail to write all the datum.

**/
EFI_STATUS
WriteSectors (
  IN NVMEOF_DEVICE_PRIVATE_DATA    *Device,
  IN UINT64                        Buffer,
  IN UINT64                        Lba,
  IN UINT32                        Blocks
  )
{
  UINT8      IsCompleted = 0;
  EFI_STATUS Status = EFI_SUCCESS;
  int        rc = 0;

  //redircting to SPDK lib write function
  Status = spdk_nvme_ns_cmd_write (Device->NameSpace,
    Device->qpair,
    (void*)Buffer,
    Lba,
    Blocks,
    IoComplete,
    &IsCompleted,
    0);
  while (!IsCompleted) {
    rc = spdk_nvme_qpair_process_completions (Device->qpair, 0);
    if (rc < 0) {
      IsCompleted = ERROR_IN_COMPLETION;
      break;
    }
  }
  
  if (IsCompleted == ERROR_IN_COMPLETION) {
    DEBUG ((DEBUG_ERROR, "WriteSectors: Error In Process Write Data\n"));
    Status = EFI_DEVICE_ERROR;
  }

  return Status;
}

/**
  Write some blocks to the device.

  @param  Device                 The pointer to the NVMEOF_DEVICE_PRIVATE_DATA data structure.
  @param  Buffer                 The buffer to be written into the device.
  @param  Lba                    The start block number.
  @param  Blocks                 Total block number to be written.

  @retval EFI_SUCCESS            Datum are written into the buffer.
  @retval Others                 Fail to write all the datum.

**/
EFI_STATUS
NvmeOfWrite (
  IN NVMEOF_DEVICE_PRIVATE_DATA         *Device,
  IN VOID                               *Buffer,
  IN UINT64                             Lba,
  IN UINTN                              Blocks
  )
{
  EFI_STATUS                       Status;
  UINT32                           BlockSize;
  UINT32                           MaxTransferBlocks;
  UINTN                            OrginalBlocks;
  BOOLEAN                          IsEmpty;
  EFI_TPL                          OldTpl;

  if (Buffer == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  //
  // Wait for the device's asynchronous I/O queue to become empty.
  //
  while (TRUE) {
    OldTpl  = gBS->RaiseTPL (TPL_NOTIFY);
    IsEmpty = IsListEmpty (&Device->AsyncQueue);
    gBS->RestoreTPL (OldTpl);

    if (IsEmpty) {
      break;
    }

    gBS->Stall (DELAY);
  }

  Status        = EFI_SUCCESS;
  BlockSize     = spdk_nvme_ns_get_sector_size (Device->NameSpace);
  OrginalBlocks = Blocks;

  MaxTransferBlocks = spdk_nvme_ctrlr_get_max_xfer_size (Device->NameSpace->ctrlr);

  while (Blocks > 0) {
    if (Blocks > MaxTransferBlocks) {
      Status = WriteSectors (Device, (UINT64)(UINTN)Buffer, Lba, MaxTransferBlocks);
      Blocks -= MaxTransferBlocks;
      Buffer  = (VOID *)(UINTN)((UINT64)(UINTN)Buffer + MaxTransferBlocks * BlockSize);
      Lba    += MaxTransferBlocks;
    } else {
      Status = WriteSectors (Device, (UINT64)(UINTN)Buffer, Lba, (UINT32)Blocks);
      Blocks = 0;
    }
    if (EFI_ERROR(Status)) {
      break;
    }
  }

  DEBUG ((DEBUG_BLKIO, "NvmeOfWrite: Lba = 0x%08Lx, Original = 0x%08Lx, "
    "Remaining = 0x%08Lx, BlockSize = 0x%x, Status = %r\n", Lba,
    (UINT64)OrginalBlocks, (UINT64)Blocks, BlockSize, Status));

  return Status;
}

/**
  Write BufferSize bytes from Lba into Buffer.

  @param  This       Indicates a pointer to the calling context.
  @param  MediaId    The media ID that the write request is for.
  @param  Lba        The starting logical block address to be written. The caller is
                     responsible for writing to only legitimate locations.
  @param  BufferSize Size of Buffer, must be a multiple of device block size.
  @param  Buffer     A pointer to the source buffer for the data.

  @retval EFI_SUCCESS           The data was written correctly to the device.
  @retval EFI_WRITE_PROTECTED   The device can not be written to.
  @retval EFI_DEVICE_ERROR      The device reported an error while performing the write.
  @retval EFI_MEDIA_CHNAGED     The MediaId does not matched the current device.
  @retval EFI_BAD_BUFFER_SIZE   The Buffer was not a multiple of the block size of the device.
  @retval EFI_INVALID_PARAMETER The write request contains LBAs that are not valid,
                                or the buffer is not on proper alignment.

**/
EFI_STATUS
EFIAPI
NvmeOfBlockIoWriteBlocks (
  IN  EFI_BLOCK_IO_PROTOCOL   *This,
  IN  UINT32                  MediaId,
  IN  EFI_LBA                 Lba,
  IN  UINTN                   BufferSize,
  IN  VOID                    *Buffer
  )
{
  EFI_STATUS                        Status;
  NVMEOF_DEVICE_PRIVATE_DATA        *Device;
  EFI_BLOCK_IO_MEDIA                *Media;
  UINTN                             BlockSize;
  UINTN                             NumberOfBlocks;
  UINTN                             IoAlign;
  EFI_TPL                           OldTpl;

  //
  // Check parameters.
  //
  if (This == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  if (IsListEmpty (&gNvmeOfControllerList)) {
    return EFI_INVALID_PARAMETER;
  }

  Media = This->Media;

  if (!Media->MediaPresent) {
    return EFI_NO_MEDIA;
  }

  if (MediaId != Media->MediaId) {
    return EFI_MEDIA_CHANGED;
  }

  if (Media->ReadOnly) {
    return EFI_WRITE_PROTECTED;
  }

  if (Buffer == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  if (BufferSize == 0) {
    return EFI_SUCCESS;
  }

  BlockSize = Media->BlockSize;
  if ((BufferSize % BlockSize) != 0) {
    return EFI_BAD_BUFFER_SIZE;
  }

  NumberOfBlocks  = BufferSize / BlockSize;
  if ((Lba + NumberOfBlocks - 1) > Media->LastBlock) {
    return EFI_INVALID_PARAMETER;
  }

  IoAlign = Media->IoAlign;
  if (IoAlign > 0 && (((UINTN) Buffer & (IoAlign - 1)) != 0)) {
    return EFI_INVALID_PARAMETER;
  }

  OldTpl = gBS->RaiseTPL (TPL_CALLBACK);

  Device = NVMEOF_DEVICE_PRIVATE_DATA_FROM_BLOCK_IO (This);

  Status = NvmeOfWrite (Device, Buffer, Lba, NumberOfBlocks);

  gBS->RestoreTPL (OldTpl);
  
  return Status;
}


/**
  Write some sectors from the device in an asynchronous manner.

  @param  Device        The pointer to the NVMEOF_DEVICE_PRIVATE_DATA data
                        structure.
  @param  Request       The pointer to the NVMEOF_BLKIO2_REQUEST data structure.
  @param  Buffer        The buffer used to store the data written to the
                        device.
  @param  Lba           The start block number.
  @param  Blocks        Total block number to be written.
  @param  IsLast        The last subtask of an asynchronous write request.

  @retval EFI_SUCCESS   Asynchronous write request has been queued.
  @retval Others        Fail to send the asynchronous request.

**/
EFI_STATUS
AsyncWriteSectors (
  IN NVMEOF_DEVICE_PRIVATE_DATA           *Device,
  IN NVMEOF_BLKIO2_REQUEST                *Request,
  IN UINT64                               Buffer,
  IN UINT64                               Lba,
  IN UINT32                               Blocks,
  IN BOOLEAN                              IsLast
  )
{
  NVMEOF_DRIVER_DATA                       *Private = NULL;
  NVMEOF_BLKIO2_SUBTASK                    *Subtask = NULL;
  NVMEOF_ASYNC_CMD_DATA                    *NvmeOfAsyncData = NULL;
  EFI_STATUS                               Status;
  EFI_TPL                                  OldTpl;

  Private = Device->Controller;
  Subtask = AllocateZeroPool (sizeof (NVMEOF_BLKIO2_SUBTASK));
  if (Subtask == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    goto ErrorExit;
  }

  Subtask->Signature = NVMEOF_BLKIO2_SUBTASK_SIGNATURE;
  Subtask->IsLast = IsLast;
  Subtask->NamespaceId = Device->NamespaceId;
  Subtask->BlockIo2Request = Request;

  NvmeOfAsyncData = AllocateZeroPool (sizeof (NVMEOF_ASYNC_CMD_DATA));
  if (NvmeOfAsyncData == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    goto ErrorExit;
  } else {
    Subtask->NvmeOfAsyncData = NvmeOfAsyncData;
  }

  NvmeOfAsyncData->Lba = Lba;
  NvmeOfAsyncData->Blocks = Blocks;
  NvmeOfAsyncData->Buffer = Buffer;
  NvmeOfAsyncData->IsRead = FALSE;
  NvmeOfAsyncData->Device = Device;
  NvmeOfAsyncData->IsCompleted = 0;

  // Create Event
  Status = gBS->CreateEvent (
                  EVT_NOTIFY_SIGNAL,
                  TPL_NOTIFY,
                  AsyncIoCallback,
                  Subtask,
                  &Subtask->Event
                  );
  if (EFI_ERROR (Status)) {
    goto ErrorExit;
  }

  OldTpl = gBS->RaiseTPL (TPL_NOTIFY);
  InsertTailList (&Private->UnsubmittedSubtasks, &Subtask->Link);
  Request->UnsubmittedSubtaskNum++;
  gBS->RestoreTPL (OldTpl);

  return EFI_SUCCESS;

ErrorExit:
  // Resource cleanup if asynchronous read request has not been queued.
  if (Subtask != NULL) {
    if (Subtask->Event != NULL) {
      gBS->CloseEvent (Subtask->Event);
    }

    if (NvmeOfAsyncData != NULL) {
      FreePool (NvmeOfAsyncData);
    }
    FreePool (Subtask);
  }

  return Status;
}


/**
  Write some blocks from the device in an asynchronous manner.

  @param  Device        The pointer to the NVMEOF_DEVICE_PRIVATE_DATA data
                        structure.
  @param  Buffer        The buffer used to store the data written to the
                        device.
  @param  Lba           The start block number.
  @param  Blocks        Total block number to be written.
  @param  Token         A pointer to the token associated with the transaction.

  @retval EFI_SUCCESS   Data are written to the device.
  @retval Others        Fail to write all the data.

**/
EFI_STATUS
NvmeOfAsyncWrite (
  IN NVMEOF_DEVICE_PRIVATE_DATA         *Device,
  IN VOID                               *Buffer,
  IN UINT64                             Lba,
  IN UINTN                              Blocks,
  IN EFI_BLOCK_IO2_TOKEN                *Token
  )
{
  EFI_STATUS                       Status;
  UINT32                           BlockSize;
  NVMEOF_BLKIO2_REQUEST            *BlkIo2Req = NULL;
  UINT32                           MaxTransferBlocks;
  UINTN                            OrginalBlocks;
  BOOLEAN                          IsEmpty;
  EFI_TPL                          OldTpl;

  if (Buffer == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  Status = EFI_SUCCESS;
  BlockSize = Device->Media.BlockSize;
  OrginalBlocks = Blocks;
  BlkIo2Req = AllocateZeroPool (sizeof (NVMEOF_BLKIO2_REQUEST));
  if (BlkIo2Req == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  BlkIo2Req->Signature = NVMEOF_BLKIO2_REQUEST_SIGNATURE;
  BlkIo2Req->Token = Token;

  OldTpl = gBS->RaiseTPL (TPL_NOTIFY);
  InsertTailList (&Device->AsyncQueue, &BlkIo2Req->Link);
  gBS->RestoreTPL (OldTpl);

  InitializeListHead (&BlkIo2Req->SubtasksQueue);

  MaxTransferBlocks = spdk_nvme_ctrlr_get_max_xfer_size (Device->NameSpace->ctrlr);

  while (Blocks > 0) {
    if (Blocks > MaxTransferBlocks) {
      Status = AsyncWriteSectors (
                 Device,
                 BlkIo2Req,
                 (UINT64)(UINTN)Buffer,
                 Lba,
                 MaxTransferBlocks,
                 FALSE
                 );

      Blocks -= MaxTransferBlocks;
      Buffer = (VOID *)(UINTN)((UINT64)(UINTN)Buffer + MaxTransferBlocks * BlockSize);
      Lba += MaxTransferBlocks;
    } else {
      Status = AsyncWriteSectors (
                 Device,
                 BlkIo2Req,
                 (UINT64)(UINTN)Buffer,
                 Lba,
                 (UINT32)Blocks,
                 TRUE
                 );

      Blocks = 0;
    }

    if (EFI_ERROR (Status)) {
      OldTpl = gBS->RaiseTPL (TPL_NOTIFY);
      IsEmpty = IsListEmpty (&BlkIo2Req->SubtasksQueue) &&
          (BlkIo2Req->UnsubmittedSubtaskNum == 0);

      if (IsEmpty) {
        // Remove the BlockIo2 request from the device asynchronous queue.
        RemoveEntryList (&BlkIo2Req->Link);
        FreePool (BlkIo2Req);
        Status = EFI_DEVICE_ERROR;
      } else {
        // There are previous BlockIo2 subtasks still running, EFI_SUCCESS
        // should be returned to make sure that the caller does not free
        // resources still using by these requests.
        
        Status = EFI_SUCCESS;
        Token->TransactionStatus = EFI_DEVICE_ERROR;
        BlkIo2Req->LastSubtaskSubmitted = TRUE;
      }
      gBS->RestoreTPL (OldTpl);
      break;
    }
  }

  DEBUG ((DEBUG_BLKIO, "NvmeOfAsyncWrite: Lba = 0x%08Lx, Original = 0x%08Lx, "
      "Remaining = 0x%08Lx, BlockSize = 0x%x, Status = %r\n", Lba,
      (UINT64)OrginalBlocks, (UINT64)Blocks, BlockSize, Status));

  return Status;
}


/**
  Write BufferSize bytes from Lba into Buffer.

  This function writes the requested number of blocks to the device. All blocks
  are written, or an error is returned.If EFI_DEVICE_ERROR, EFI_NO_MEDIA,
  EFI_WRITE_PROTECTED or EFI_MEDIA_CHANGED is returned and non-blocking I/O is
  being used, the Event associated with this request will not be signaled.

  @param[in]       This       Indicates a pointer to the calling context.
  @param[in]       MediaId    The media ID that the write request is for.
  @param[in]       Lba        The starting logical block address to be written.
                              The caller is responsible for writing to only
                              legitimate locations.
  @param[in, out]  Token      A pointer to the token associated with the
                              transaction.
  @param[in]       BufferSize Size of Buffer, must be a multiple of device
                              block size.
  @param[in]       Buffer     A pointer to the source buffer for the data.

  @retval EFI_SUCCESS           The write request was queued if Event is not
                                NULL.
                                The data was written correctly to the device if
                                the Event is NULL.
  @retval EFI_WRITE_PROTECTED   The device can not be written to.
  @retval EFI_MEDIA_CHANGED     The MediaId does not matched the current
                                device.
  @retval EFI_DEVICE_ERROR      The device reported an error while performing
                                the write.
  @retval EFI_BAD_BUFFER_SIZE   The Buffer was not a multiple of the block size
                                of the device.
  @retval EFI_INVALID_PARAMETER The write request contains LBAs that are not
                                valid, or the buffer is not on proper
                                alignment.
  @retval EFI_OUT_OF_RESOURCES  The request could not be completed due to a
                                lack of resources.

**/
EFI_STATUS
EFIAPI
NvmeOfBlockIoWriteBlocksEx (
  IN     EFI_BLOCK_IO2_PROTOCOL  *This,
  IN     UINT32                  MediaId,
  IN     EFI_LBA                 Lba,
  IN OUT EFI_BLOCK_IO2_TOKEN     *Token,
  IN     UINTN                   BufferSize,
  IN     VOID                    *Buffer
  )
{
  NVMEOF_DEVICE_PRIVATE_DATA      *Device;
  EFI_STATUS                      Status;
  EFI_BLOCK_IO_MEDIA              *Media;
  UINTN                           BlockSize;
  UINTN                           NumberOfBlocks;
  UINTN                           IoAlign;
  EFI_TPL                         OldTpl;

  // Check parameters.
  if (This == NULL) {
      return EFI_INVALID_PARAMETER;
  }

  if (IsListEmpty (&gNvmeOfControllerList)) {
    return EFI_INVALID_PARAMETER;
  }

  Media = This->Media;

  if (!Media->MediaPresent) {
      return EFI_NO_MEDIA;
  }

  if (MediaId != Media->MediaId) {
      return EFI_MEDIA_CHANGED;
  }

  if (Media->ReadOnly) {
      return EFI_WRITE_PROTECTED;
  }

  if (Buffer == NULL) {
      return EFI_INVALID_PARAMETER;
  }

  if (BufferSize == 0) {
    if ((Token != NULL) && (Token->Event != NULL)) {
        Token->TransactionStatus = EFI_SUCCESS;
        gBS->SignalEvent (Token->Event);
    }
    return EFI_SUCCESS;
  }

  BlockSize = Media->BlockSize;
  if ((BufferSize % BlockSize) != 0) {
      return EFI_BAD_BUFFER_SIZE;
  }

  NumberOfBlocks = BufferSize / BlockSize;
  if ((Lba + NumberOfBlocks - 1) > Media->LastBlock) {
      return EFI_INVALID_PARAMETER;
  }

  IoAlign = Media->IoAlign;
  if (IoAlign > 0 && (((UINTN)Buffer & (IoAlign - 1)) != 0)) {
      return EFI_INVALID_PARAMETER;
  }

  OldTpl = gBS->RaiseTPL (TPL_CALLBACK);
  Device = NVMEOF_DEVICE_PRIVATE_DATA_FROM_BLOCK_IO2 (This);
  if ((Token != NULL) && (Token->Event != NULL)) {
      Token->TransactionStatus = EFI_SUCCESS;
      Status = NvmeOfAsyncWrite (Device, Buffer, Lba, NumberOfBlocks, Token);
  } else {
      Status = NvmeOfWrite (Device, Buffer, Lba, NumberOfBlocks);
  }
  gBS->RestoreTPL (OldTpl);

  return Status;
}

/**
  Send a security protocol command to a device that receives data and/or the result
  of one or more commands sent by SendData.

  The ReceiveData function sends a security protocol command to the given MediaId.
  The security protocol command sent is defined by SecurityProtocolId and contains
  the security protocol specific data SecurityProtocolSpecificData. The function
  returns the data from the security protocol command in PayloadBuffer.

  For devices supporting the SCSI command set, the security protocol command is sent
  using the SECURITY PROTOCOL IN command defined in SPC-4.

  For devices supporting the ATA command set, the security protocol command is sent
  using one of the TRUSTED RECEIVE commands defined in ATA8-ACS if PayloadBufferSize
  is non-zero.

  If the PayloadBufferSize is zero, the security protocol command is sent using the
  Trusted Non-Data command defined in ATA8-ACS.

  If PayloadBufferSize is too small to store the available data from the security
  protocol command, the function shall copy PayloadBufferSize bytes into the
  PayloadBuffer and return EFI_WARN_BUFFER_TOO_SMALL.

  If PayloadBuffer or PayloadTransferSize is NULL and PayloadBufferSize is non-zero,
  the function shall return EFI_INVALID_PARAMETER.

  If the given MediaId does not support security protocol commands, the function shall
  return EFI_UNSUPPORTED. If there is no media in the device, the function returns
  EFI_NO_MEDIA. If the MediaId is not the ID for the current media in the device,
  the function returns EFI_MEDIA_CHANGED.

  If the security protocol fails to complete within the Timeout period, the function
  shall return EFI_TIMEOUT.

  If the security protocol command completes without an error, the function shall
  return EFI_SUCCESS. If the security protocol command completes with an error, the
  function shall return EFI_DEVICE_ERROR.

  @param  This                         Indicates a pointer to the calling context.
  @param  MediaId                      ID of the medium to receive data from.
  @param  Timeout                      The timeout, in 100ns units, to use for the execution
                                       of the security protocol command. A Timeout value of 0
                                       means that this function will wait indefinitely for the
                                       security protocol command to execute. If Timeout is greater
                                       than zero, then this function will return EFI_TIMEOUT
                                       if the time required to execute the receive data command
                                       is greater than Timeout.
  @param  SecurityProtocolId           The value of the "Security Protocol" parameter of
                                       the security protocol command to be sent.
  @param  SecurityProtocolSpecificData The value of the "Security Protocol Specific" parameter
                                       of the security protocol command to be sent.
  @param  PayloadBufferSize            Size in bytes of the payload data buffer.
  @param  PayloadBuffer                A pointer to a destination buffer to store the security
                                       protocol command specific payload data for the security
                                       protocol command. The caller is responsible for having
                                       either implicit or explicit ownership of the buffer.
  @param  PayloadTransferSize          A pointer to a buffer to store the size in bytes of the
                                       data written to the payload data buffer.

  @retval EFI_SUCCESS                  The security protocol command completed successfully.
  @retval EFI_WARN_BUFFER_TOO_SMALL    The PayloadBufferSize was too small to store the available
                                       data from the device. The PayloadBuffer contains the truncated data.
  @retval EFI_UNSUPPORTED              The given MediaId does not support security protocol commands.
  @retval EFI_DEVICE_ERROR             The security protocol command completed with an error.
  @retval EFI_NO_MEDIA                 There is no media in the device.
  @retval EFI_MEDIA_CHANGED            The MediaId is not for the current media.
  @retval EFI_INVALID_PARAMETER        The PayloadBuffer or PayloadTransferSize is NULL and
                                       PayloadBufferSize is non-zero.
  @retval EFI_TIMEOUT                  A timeout occurred while waiting for the security
                                       protocol command to execute.

**/
EFI_STATUS
EFIAPI
NvmeOfStorageSecurityReceiveData (
  IN  EFI_STORAGE_SECURITY_COMMAND_PROTOCOL    *This,
  IN  UINT32                                   MediaId,
  IN  UINT64                                   Timeout,
  IN  UINT8                                    SecurityProtocolId,
  IN  UINT16                                   SecurityProtocolSpecificData,
  IN  UINTN                                    PayloadBufferSize,
  OUT VOID                                     *PayloadBuffer,
  OUT UINTN                                    *PayloadTransferSize
  )
{
  EFI_STATUS                       Status;
  NVMEOF_DEVICE_PRIVATE_DATA       *Device;

  Status = EFI_SUCCESS;

  if ((PayloadBuffer == NULL) || (PayloadTransferSize == NULL) || (PayloadBufferSize == 0)) {
    return EFI_INVALID_PARAMETER;
  }

  Device = NVMEOF_DEVICE_PRIVATE_DATA_FROM_STORAGE_SECURITY (This);

  if (MediaId != Device->BlockIo.Media->MediaId) {
    return EFI_MEDIA_CHANGED;
  }

  if (!Device->BlockIo.Media->MediaPresent) {
    return EFI_NO_MEDIA;
  }

  Status = spdk_nvme_ctrlr_security_receive (Device->NameSpace->ctrlr, SecurityProtocolId,
    SecurityProtocolSpecificData, 0xEA, PayloadBuffer, PayloadBufferSize);
       
  return Status;
}

/**
  Send a security protocol command to a device.

  The SendData function sends a security protocol command containing the payload
  PayloadBuffer to the given MediaId. The security protocol command sent is
  defined by SecurityProtocolId and contains the security protocol specific data
  SecurityProtocolSpecificData. If the underlying protocol command requires a
  specific padding for the command payload, the SendData function shall add padding
  bytes to the command payload to satisfy the padding requirements.

  For devices supporting the SCSI command set, the security protocol command is sent
  using the SECURITY PROTOCOL OUT command defined in SPC-4.

  For devices supporting the ATA command set, the security protocol command is sent
  using one of the TRUSTED SEND commands defined in ATA8-ACS if PayloadBufferSize
  is non-zero. If the PayloadBufferSize is zero, the security protocol command is
  sent using the Trusted Non-Data command defined in ATA8-ACS.

  If PayloadBuffer is NULL and PayloadBufferSize is non-zero, the function shall
  return EFI_INVALID_PARAMETER.

  If the given MediaId does not support security protocol commands, the function
  shall return EFI_UNSUPPORTED. If there is no media in the device, the function
  returns EFI_NO_MEDIA. If the MediaId is not the ID for the current media in the
  device, the function returns EFI_MEDIA_CHANGED.

  If the security protocol fails to complete within the Timeout period, the function
  shall return EFI_TIMEOUT.

  If the security protocol command completes without an error, the function shall return
  EFI_SUCCESS. If the security protocol command completes with an error, the function
  shall return EFI_DEVICE_ERROR.

  @param  This                         Indicates a pointer to the calling context.
  @param  MediaId                      ID of the medium to receive data from.
  @param  Timeout                      The timeout, in 100ns units, to use for the execution
                                       of the security protocol command. A Timeout value of 0
                                       means that this function will wait indefinitely for the
                                       security protocol command to execute. If Timeout is greater
                                       than zero, then this function will return EFI_TIMEOUT
                                       if the time required to execute the send data command
                                       is greater than Timeout.
  @param  SecurityProtocolId           The value of the "Security Protocol" parameter of
                                       the security protocol command to be sent.
  @param  SecurityProtocolSpecificData The value of the "Security Protocol Specific" parameter
                                       of the security protocol command to be sent.
  @param  PayloadBufferSize            Size in bytes of the payload data buffer.
  @param  PayloadBuffer                A pointer to a destination buffer to store the security
                                       protocol command specific payload data for the security
                                       protocol command.

  @retval EFI_SUCCESS                  The security protocol command completed successfully.
  @retval EFI_UNSUPPORTED              The given MediaId does not support security protocol commands.
  @retval EFI_DEVICE_ERROR             The security protocol command completed with an error.
  @retval EFI_NO_MEDIA                 There is no media in the device.
  @retval EFI_MEDIA_CHANGED            The MediaId is not for the current media.
  @retval EFI_INVALID_PARAMETER        The PayloadBuffer is NULL and PayloadBufferSize is non-zero.
  @retval EFI_TIMEOUT                  A timeout occurred while waiting for the security
                                       protocol command to execute.

**/
EFI_STATUS
EFIAPI
NvmeOfStorageSecuritySendData (
  IN EFI_STORAGE_SECURITY_COMMAND_PROTOCOL    *This,
  IN UINT32                                   MediaId,
  IN UINT64                                   Timeout,
  IN UINT8                                    SecurityProtocolId,
  IN UINT16                                   SecurityProtocolSpecificData,
  IN UINTN                                    PayloadBufferSize,
  IN VOID                                     *PayloadBuffer
  )
{
  EFI_STATUS                       Status;
  NVMEOF_DEVICE_PRIVATE_DATA       *Device;

  Status = EFI_SUCCESS;

  if ((PayloadBuffer == NULL) && (PayloadBufferSize != 0)) {
    return EFI_INVALID_PARAMETER;
  }

  Device = NVMEOF_DEVICE_PRIVATE_DATA_FROM_STORAGE_SECURITY (This);

  if (MediaId != Device->BlockIo.Media->MediaId) {
    return EFI_MEDIA_CHANGED;
  }

  if (!Device->BlockIo.Media->MediaPresent) {
    return EFI_NO_MEDIA;
  }

  Status = spdk_nvme_ctrlr_security_send (Device->NameSpace->ctrlr, SecurityProtocolId,
    SecurityProtocolSpecificData, 0xEA, PayloadBuffer, PayloadBufferSize);
       
  return Status;
}

/** @file
  NvmeOf CLI is used to test NVMeOF Driver

  Copyright (c) 2020, Dell EMC All rights reserved
  Copyright (c) 2022, Intel Corporation. All rights reserved.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/
#include <Uefi.h>
#include <Library/PcdLib.h>
#include <Library/UefiLib.h>
#include <Library/UefiApplicationEntryPoint.h>
#include <Library/ShellLib.h>
#include <Library/ShellCommandLib.h>
#include <Library/ShellCEntryLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/PrintLib.h>
#include <Library/NetLib.h>
#include <Base.h> 
#include <Library/BaseLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Library/UefiRuntimeLib.h>
#include <Protocol/PlatformDriverOverride.h>
#include <Library/DevicePathLib.h>
#include <Library/HiiLib.h>
#include <Library/DebugLib.h>
#include <Protocol/BlockIo.h> 
#include <Protocol/BlockIo2.h> 
#include <Protocol/DevicePathToText.h>
#include <Protocol/NvmeOFPassthru.h>
#include "NvmeOfParser.h"
#include "NvmeOfCmdHelp.h"
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Protocol/Shell.h>
#include <Shared/NvmeOfNvData.h>
#include "NvmeOfCli.h"

#define OFFSET_OF_DEVICE_HANDLE  13

struct spdk_nvme_ctrlr   *gctrlr;
struct spdk_nvme_qpair   *gIoQpair;
NVMEOF_PASSTHROUGH_PROTOCOL *NvmeofPassThroughProtocol;

/**
Compare read and write data synchronously from device

@param  Argc argument count from cli
@retval EFI_SUCCESS
@retval EFI_INVALID_PARAMETER  Invalid parameter
**/
EFI_STATUS
EFIAPI
NvmeOfCompareReadWriteSyncData (UINTN Argc)
{
  EFI_STATUS               Status = EFI_SUCCESS;
  EFI_DEVICE_PATH_PROTOCOL *DevPath;
  EFI_BLOCK_IO_PROTOCOL    *BlockIo;
  EFI_HANDLE               BlockIoHandle;
  UINT8                    *WriteBuffer = NULL;
  UINT8                    *ReadBuffer = NULL;

  DevPath = (EFI_DEVICE_PATH_PROTOCOL*)gEfiShellProtocol->GetDevicePathFromMap (ReadWriteCliDataInBlock.Device_id);
  if (gBS->LocateDevicePath (&gEfiBlockIoProtocolGuid, &DevPath, NULL) == EFI_NOT_FOUND) {
    Print (L"\n Invalid Device path \n");
    return (SHELL_NOT_FOUND);
  }

  Status = gBS->LocateDevicePath (
                  &gEfiBlockIoProtocolGuid,
                  (EFI_DEVICE_PATH_PROTOCOL **)&DevPath,
                  &BlockIoHandle
                  );
  if (EFI_ERROR (Status)) {
    Print (L"\n Unable to locate device  path, Check if device exits \n");
    return (SHELL_NOT_FOUND);
  }

  Status = gBS->OpenProtocol (
                  BlockIoHandle,
                  &gEfiBlockIoProtocolGuid,
                  (VOID**)&BlockIo,
                  gImageHandle,
                  NULL,
                  EFI_OPEN_PROTOCOL_GET_PROTOCOL
                  );
  if (EFI_ERROR (Status)) {
    Print (L"\n Unable to open EFI block IO protocol \n");
    return (SHELL_NOT_FOUND);
  }

  WriteBuffer = AllocateZeroPool (BlockIo->Media->BlockSize * sizeof (UINT8));
  ReadBuffer = AllocateZeroPool (BlockIo->Media->BlockSize * sizeof (UINT8));

  for (UINT64 i = 0; i < ReadWriteCliDataInBlock.Numberof_block; i++) {
    static UINT64 BlockCount = 1;

    if (Argc == 10) {
      memset (WriteBuffer, ReadWriteCliDataInBlock.Pattern, BlockIo->Media->BlockSize * sizeof (UINT8));
    } else {
      memset (WriteBuffer, PATTERN, BlockIo->Media->BlockSize * sizeof (UINT8));
    }

    Status = BlockIo->WriteBlocks (
                        BlockIo,
                        BlockIo->Media->MediaId,
                        ReadWriteCliDataInBlock.Start_lba,
                        BlockIo->Media->BlockSize,
                        WriteBuffer
                        );
    memset (ReadBuffer, 0, BlockIo->Media->BlockSize * sizeof (UINT8));

    Status = BlockIo->ReadBlocks (
      BlockIo,
      BlockIo->Media->MediaId,
      ReadWriteCliDataInBlock.Start_lba,
      BlockIo->Media->BlockSize,
      ReadBuffer
    );

    if (!memcmp (WriteBuffer, ReadBuffer, sizeof (BlockIo->Media->BlockSize))) {
      if (BlockCount == ReadWriteCliDataInBlock.Numberof_block) {
        Print(L"\nSuccess \n ");
        break;
      }
      BlockCount++;
      ReadWriteCliDataInBlock.Start_lba++;
    } else {
        Print(L"\n Sync failed, Failed Block : %lld\n", (ReadWriteCliDataInBlock.Start_lba+i));
        break;
    }
  }
  FreePool (WriteBuffer);
  FreePool (ReadBuffer);
  return Status;
}

/**
Compare read and write data Asynchronously from device

 @param  Argc argument count from cli
 @retval EFI_SUCCESS
 @retval EFI_INVALID_PARAMETER  Invalid parameter

**/
EFI_STATUS
EFIAPI
NvmeOfCompareReadWriteAsyncData (UINTN Argc)
{
  EFI_STATUS               Status = EFI_SUCCESS;
  EFI_DEVICE_PATH_PROTOCOL *DevPath;
  EFI_HANDLE               BlockIoHandle;
  EFI_BLOCK_IO2_PROTOCOL   *BlockIo2;
  UINT8                    *WriteBuffer = NULL;
  UINT8                    *ReadBuffer = NULL;


  DevPath = (EFI_DEVICE_PATH_PROTOCOL*)gEfiShellProtocol->GetDevicePathFromMap (ReadWriteCliDataInBlock.Device_id);
  if (gBS->LocateDevicePath (&gEfiBlockIo2ProtocolGuid, &DevPath, NULL) == EFI_NOT_FOUND) {
    Print (L"\n Invalid Device path \n");
    return (SHELL_NOT_FOUND);
  }

  Status = gBS->LocateDevicePath (
                  &gEfiBlockIo2ProtocolGuid,
                  (EFI_DEVICE_PATH_PROTOCOL **)&DevPath,
                  &BlockIoHandle
                  );
  if (EFI_ERROR(Status)) {
    Print (L"\n Unable to locate device  path, Check if device exits \n");
    return (SHELL_NOT_FOUND);
  }

  Status = gBS->OpenProtocol (
                  BlockIoHandle,
                  &gEfiBlockIo2ProtocolGuid,
                  (VOID**)&BlockIo2,
                  gImageHandle,
                  NULL,
                  EFI_OPEN_PROTOCOL_GET_PROTOCOL
                  );
  if (EFI_ERROR (Status)) {
    Print (L"\n Unable to open EFI block IO2 protocol \n");
    return (SHELL_NOT_FOUND);
  }

  WriteBuffer = AllocateZeroPool (BlockIo2->Media->BlockSize * sizeof (UINT8));
  ReadBuffer = AllocateZeroPool (BlockIo2->Media->BlockSize * sizeof (UINT8));

  for (UINTN i = 0; i < ReadWriteCliDataInBlock.Numberof_block; i++) {
    static UINT32 BlockCount = 1;

    if (Argc == 10) {
      memset (WriteBuffer, ReadWriteCliDataInBlock.Pattern, BlockIo2->Media->BlockSize * sizeof (UINT8));
    } else {
      memset (WriteBuffer, PATTERN, BlockIo2->Media->BlockSize * sizeof (UINT8));
    }

    Status = BlockIo2->WriteBlocksEx (
                         BlockIo2,
                         BlockIo2->Media->MediaId,
                         ReadWriteCliDataInBlock.Start_lba,
                         NULL,
                         BlockIo2->Media->BlockSize,
                         WriteBuffer
                         );
    memset (ReadBuffer, 0, BlockIo2->Media->BlockSize * sizeof (UINT8));

    Status = BlockIo2->ReadBlocksEx (
                         BlockIo2,
                         BlockIo2->Media->MediaId,
                         ReadWriteCliDataInBlock.Start_lba,
                         NULL,
                         BlockIo2->Media->BlockSize,
                         ReadBuffer
                         );

    if (!memcmp (WriteBuffer, ReadBuffer, sizeof (BlockIo2->Media->BlockSize))) {
      if (BlockCount == ReadWriteCliDataInBlock.Numberof_block) {
        Print (L"\nSuccess\n ");
        break;
      }
      BlockCount++;
      ReadWriteCliDataInBlock.Start_lba++;
    } else {
        Print (L"\n Async failed, Failed Block : %lld\n", (ReadWriteCliDataInBlock.Start_lba + i));
        break;
    }
  }
  FreePool (WriteBuffer);
  FreePool (ReadBuffer);
  return Status;
}

/**
  Get namespace ID.

  @param  MapKey                 Map Key
  @retval EFI_SUCCESS            Get the ns id on success.
  @retval EFI_INVALID_PARAMETER  Invalid parameter

**/
EFI_STATUS
EFIAPI
NvmeOfCliGetNsid (CHAR16 **MapKey)
{
  EFI_STATUS               Status = EFI_SUCCESS;
  CHAR8                    MapKey8[10];
  BOOLEAN                  Flag = FALSE;
  LIST_ENTRY               *Entry = NULL;
  NVMEOF_CLI_CTRL_MAPPING  *MappingList = NULL;
  NVMEOF_CLI_CTRL_MAPPING  **CtrlStruct = NvmeofPassThroughProtocol->CliCtrlMap;

  UnicodeStrToAsciiStrS (MapKey[2], MapKey8, 10);
  NET_LIST_FOR_EACH (Entry, &(*CtrlStruct)->CliCtrlrList) {
    MappingList = NET_LIST_USER_STRUCT (Entry, NVMEOF_CLI_CTRL_MAPPING, CliCtrlrList);
    if (!AsciiStriCmp(MapKey8, MappingList->Key)) {
      gNsId = MappingList->Nsid;
      Flag = TRUE;
      break;
    }
  }
  if (Flag) {
    return Status;
  } else {
    DEBUG ((EFI_D_ERROR, "NvmeOfCliGetNsid: Error in NvmeOfCliGetNsid\n"));
    Status = EFI_INVALID_PARAMETER;
    return Status;
  }
}


/**
  Check the device if already connected

  @retval TRUE   if the device already connected
  @retval FLASE  device not connected

**/
BOOLEAN
EFIAPI
NvmeOfCliIsConnected ()
{
  BOOLEAN                  Flag = FALSE;
  CHAR8                    Clinqn[NVMEOF_CLI_MAX_SIZE];
  LIST_ENTRY               *Entry = NULL;
  NVMEOF_CLI_CTRL_MAPPING  *MappingList = NULL;
  NVMEOF_CLI_CTRL_MAPPING  **CtrlStruct = NvmeofPassThroughProtocol->CliCtrlMap; 

  NET_LIST_FOR_EACH (Entry, &(*CtrlStruct)->CliCtrlrList) {
    MappingList = NET_LIST_USER_STRUCT (Entry, NVMEOF_CLI_CTRL_MAPPING, CliCtrlrList);
    CopyMem (Clinqn, MappingList->Subnqn, ADDR_SIZE);

    if ((!AsciiStriCmp (ConnectCommandCliData.Traddr, MappingList->Traddr)) 
        && (!AsciiStrCmp (ConnectCommandCliData.Nqn, Clinqn))) {
      Flag = TRUE;
      break;
    }
  }

  return Flag;
}


/**
  Get qpair ID.

  @param  MapKey                 Map Key
  @retval EFI_SUCCESS            Get the qpair on success.
  @retval EFI_INVALID_PARAMETER  Invalid parameter

**/
EFI_STATUS
EFIAPI
NvmeOfCliGetQpair (CHAR16 **MapKey)
{
  EFI_STATUS               Status = EFI_SUCCESS;
  CHAR8                    MapKey8[10];
  BOOLEAN                  Flag = FALSE;
  LIST_ENTRY               *Entry = NULL;
  NVMEOF_CLI_CTRL_MAPPING  *MappingList = NULL;
  NVMEOF_CLI_CTRL_MAPPING  **CtrlStruct = NvmeofPassThroughProtocol->CliCtrlMap;

  UnicodeStrToAsciiStrS (MapKey[2], MapKey8, 10);
  NET_LIST_FOR_EACH (Entry, &(*CtrlStruct)->CliCtrlrList) {
    MappingList = NET_LIST_USER_STRUCT (Entry, NVMEOF_CLI_CTRL_MAPPING, CliCtrlrList);
    if (!AsciiStriCmp (MapKey8, MappingList->Key)) {
      gIoQpair = MappingList->Ioqpair;
      Flag = TRUE;
      break;
    }
  }
  if (Flag) {
    return Status;
  } else {   
    DEBUG((EFI_D_ERROR, "NvmeOfCliGetQpair: Error in NvmeOfCliGetQpair\n"));
    Status = EFI_INVALID_PARAMETER;
    return Status;
  }
}

/**
  Get Controller.

  @param  MapKey                 Map Key
  @retval EFI_SUCCESS            Get the Controller on success.
  @retval EFI_INVALID_PARAMETER  Invalid parameter

**/
EFI_STATUS
EFIAPI
NvmeOfCliGetController (CHAR16 **MapKey)
{
  EFI_STATUS              Status = EFI_SUCCESS;
  CHAR8                   MapKey8[10];
  BOOLEAN                 Flag = FALSE;
  LIST_ENTRY              *Entry = NULL;
  NVMEOF_CLI_CTRL_MAPPING *MappingList = NULL;
  NVMEOF_CLI_CTRL_MAPPING **CtrlStruct = NvmeofPassThroughProtocol->CliCtrlMap;

  UnicodeStrToAsciiStrS (MapKey[2], MapKey8, 10);
  NET_LIST_FOR_EACH (Entry, &(*CtrlStruct)->CliCtrlrList) {
    MappingList = NET_LIST_USER_STRUCT (Entry, NVMEOF_CLI_CTRL_MAPPING, CliCtrlrList);
    if (!AsciiStriCmp (MapKey8, MappingList->Key)) {
      gctrlr = MappingList->Ctrlr;
      Flag = TRUE;
      break;
    }
  }
  if (Flag) {
    return Status;
  } else {    
    DEBUG ((EFI_D_ERROR, "NvmeOfCliGetController: Error in NvmeOfCliGetController\n"));
    Status = EFI_INVALID_PARAMETER;
    return Status;
  }
}

/**
  Get Controller qpair.

  @param  MapKey                 Map Key
  @retval EFI_SUCCESS            Get the Controller qpair on success.
  @retval EFI_NOT_FOUND          Resource Not Found

**/
EFI_STATUS
EFIAPI
NvmeOfCliGetControllerQpair (CHAR16 **MapKey)
{
  EFI_STATUS               Status = EFI_SUCCESS;
  CHAR8                    MapKey8[20];
  BOOLEAN                  Flag = FALSE;
  LIST_ENTRY               *Entry = NULL;
  NVMEOF_CLI_CTRL_MAPPING  *MappingList = NULL;
  NVMEOF_CLI_CTRL_MAPPING  **CtrlStruct = NvmeofPassThroughProtocol->CliCtrlMap;

  UnicodeStrToAsciiStrS (MapKey[2], MapKey8, 20);
  NET_LIST_FOR_EACH (Entry, &(*CtrlStruct)->CliCtrlrList) {
    MappingList = NET_LIST_USER_STRUCT (Entry, NVMEOF_CLI_CTRL_MAPPING, CliCtrlrList);

    if (!AsciiStriCmp (MapKey8, MappingList->Key)) {
      ReadWriteCliData.Ctrlr    = MappingList->Ctrlr;
      ReadWriteCliData.Ioqpair  = MappingList->Ioqpair;
      ReadWriteCliData.Nsid     = MappingList->Nsid;
      Flag = TRUE;
      break;
    }
  }
  if (Flag) {
    return Status;
  } else {
    DEBUG ((EFI_D_ERROR, "NvmeOfCliGetControllerQpair: Error in NvmeOfCliGetControllerQpair\n"));
    Status = EFI_INVALID_PARAMETER;
    return Status;
  }
}

/**
  Read data from the cli cmd.

  @param  ReadData               The read data for cli cmd.

  @retval EFI_SUCCESS            Data successfully read from the device.
  @retval EFI_NOT_FOUND          Resource Not Found

**/
EFI_STATUS
EFIAPI
NvmeOfCliReadData ()
{
  EFI_STATUS         Status = EFI_SUCCESS;
  UINTN              Len = BLOCK_SIZE;
  SHELL_FILE_HANDLE  FileHandle;
  UINT64             Position = 0;
  UINT32             BlockCounter = 0;

  ReadWriteCliData.Payload = AllocateZeroPool (BLOCK_SIZE);
  if (ReadWriteCliData.Payload == NULL) {
    Print (L"ERROR: Could not allocate memory\n");
    Status = EFI_OUT_OF_RESOURCES;
    return Status;
  }

  // open the file
  Status = ShellOpenFileByName (gReadFile, &FileHandle, EFI_FILE_MODE_READ | EFI_FILE_MODE_WRITE | EFI_FILE_MODE_CREATE, 0x0);
  if (EFI_ERROR (Status)) {
    Print (L"ERROR: Could not open file\n");
    return Status;
  }
  while (1)
  {
    if (ReadWriteCliData.Blockcount == BlockCounter) {
      break;
    }
    Status = NvmeofPassThroughProtocol->NvmeOfCliRead (ReadWriteCliData);
    if (EFI_ERROR (Status)) {
      break;
    } else {
      Status = FileHandleWrite (FileHandle, &Len, ReadWriteCliData.Payload);
      Position = Position + BLOCK_SIZE;
      Status = FileHandleSetPosition (FileHandle, Position);
      SetMem (ReadWriteCliData.Payload, BLOCK_SIZE, 0);
      ReadWriteCliData.Startblock = ReadWriteCliData.Startblock + 1;
      BlockCounter++;
    }
  }
  FreePool (ReadWriteCliData.Payload);
  return Status;
}

/**
  Write some blocks to the device.

  @param  WriteData              write data
  @retval EFI_SUCCESS            Data are written into the buffer.
  @retval EFI_NOT_FOUND          Resource Not Found

**/

EFI_STATUS
EFIAPI
NvmeOfCliWriteData ()
{
  EFI_STATUS         Status = EFI_SUCCESS;
  UINTN              *Size = NULL;
  UINTN              Len = BLOCK_SIZE;
  SHELL_FILE_HANDLE  FileHandle;
  UINT64             Position = 0;

  Size = &Len;
  ReadWriteCliData.Payload = AllocateZeroPool (BLOCK_SIZE);
  if (ReadWriteCliData.Payload == NULL) {
    Print (L"ERROR: Could not allocate memory\n");
    Status = EFI_OUT_OF_RESOURCES;
    return Status;
  }

  // Open the file
  Status = ShellOpenFileByName (gWriteFile, &FileHandle, EFI_FILE_MODE_READ , 0x0);
  if (EFI_ERROR (Status)) {
    Print (L"ERROR: Could not open file\n");
    return Status;
  }
  while (1) {
    Status = FileHandleRead (FileHandle, Size, ReadWriteCliData.Payload);
	if (*Size == 0) {
	  break;
	}
	ReadWriteCliData.Datasize = (UINT64)*Size;
	Status = NvmeofPassThroughProtocol->NvmeOfCliWrite (ReadWriteCliData);
	if (EFI_ERROR (Status)) {
	  break;
	} else {
      if (*Size < BLOCK_SIZE) {
	    break;
      } else {
	    Position = Position + BLOCK_SIZE;
	    Status = FileHandleSetPosition (FileHandle, Position);
	    SetMem (ReadWriteCliData.Payload, BLOCK_SIZE, 0);
	    ReadWriteCliData.Startblock = ReadWriteCliData.Startblock + 1;
	  }
	}
  }
  FreePool (ReadWriteCliData.Payload);
  return Status;
}

/**
  This function checks if a UEFI variable with same name is present and
  deletes the variable if it have different attributes. This scenario is
  possible while using HII and trying to overwrite the same variable
  using Cli.

  @param  Attributes    Attribute for UEFI variable from Cli.
  @retval EFI_SUCCESS   No variable found or variable found with same attribute.
  @retval other         Error while setting uefi variable to size 0.

**/
EFI_STATUS
EFIAPI
NvmeOfCliCheckifVarAlreadyExists (
  IN UINT32 Attributes
  )
{
  UINTN       BufferSize = 0;
  VOID        *Buffer = NULL;
  EFI_STATUS  Status = EFI_SUCCESS;
  UINT32      LocalAttributes;

  // Check size for already existing UEFI variable.
  Status = gRT->GetVariable (
                  L"Nvmeof_Attemptconfig",
                  &gNvmeOfConfigGuid,
                  &LocalAttributes,
                  &BufferSize,
                  Buffer
                  );

  // Delete exising variable if attributes are different.
  if ((BufferSize > 0) && (LocalAttributes != Attributes)) {
    BufferSize = 0;
    // Deleting pre-existed variable by setting size 0.
    Status = gRT->SetVariable (
                    L"Nvmeof_Attemptconfig",
                    &gNvmeOfConfigGuid,
                    LocalAttributes,
                    BufferSize,
                    Buffer
                    );
    if (EFI_ERROR (Status)) {
      return Status;
    }
    Status = gRT->SetVariable (
                    L"NvmeofGlobalData",
                    &gNvmeOfConfigGuid,
                    LocalAttributes,
                    BufferSize,
                    Buffer
                    );
    if (EFI_ERROR (Status)) {
      return Status;
    }
  }
  return EFI_SUCCESS;
}

/**
  Set the Attempt data from config file.

  @param  InFileName             config file
  @retval EFI_SUCCESS            set the Attempt data.
  @retval EFI_NOT_FOUND          Resource Not Found

**/
EFI_STATUS
EFIAPI
NvmeOfCliSetAttempt (IN CONST CHAR16  *InFileName)
{
  EFI_STATUS           Status = EFI_SUCCESS;
  UINT8                Index = 0;
  UINT8                Temp = 0;
  BOOLEAN              Ascii = TRUE;
  BOOLEAN              Flag = TRUE;
  UINTN                Size = LINE_MAX;
  UINTN                LineNo = 0;
  CHAR8                *EndPointer = NULL;
  UINT8                IndexTag = 0;
  CHAR16               *FileName = NULL;
  CHAR16               *FullFileName = NULL;
  CHAR16               *ReadLine = NULL;
  CHAR8                *Line = NULL;
  CHAR8                *ReadLineTag = NULL;
  UINT8                i = 0;
  SHELL_FILE_HANDLE    FileHandle;
  CHAR8                Tag[40];
  EFI_STATUS           IpStatus;
  EFI_GUID             TransportGuidlo;
  EFI_GUID             HostRawGuid;
  CHAR16               HostIdStr[NVMEOF_NID_LEN];
  CHAR8                Line1[1024];
  UINT32               Attributes;
  NVMEOF_ATTEMPT_CONFIG_NVDATA Nvmeof_Attemptconfig[MAX_ATTEMPTS];
  NVMEOF_GLOBAL_DATA NvmeofData[1];

  ZeroMem (Nvmeof_Attemptconfig, sizeof (Nvmeof_Attemptconfig));
  ZeroMem (NvmeofData, sizeof (NvmeofData));
  
  FileName = AllocateCopyPool (StrSize (InFileName), InFileName);
  if (FileName == NULL) {
    Print (L"ERROR: Could not allocate memory\n");
    return (EFI_OUT_OF_RESOURCES);
  }
  FullFileName = ShellFindFilePath (FileName);
  if (FullFileName == NULL) {
    Print (L"ERROR: Could not find %s\n", FileName);
    Status = EFI_NOT_FOUND;
    FreePool (FileName);
    return Status;
  }
  // open the file
  Status = ShellOpenFileByName (FullFileName, &FileHandle, EFI_FILE_MODE_READ, 0);
  if (EFI_ERROR (Status)) {
    Print (L" ERROR: Could not open file\n");
    FreePool (FileName);
    return Status;
  }
  // allocate a buffer to read lines into
  ReadLine = AllocateZeroPool (Size);
  if (ReadLine == NULL) {
    Print (L" ERROR: Could not allocate memory\n");
    FreePool (FileName);
    Status = EFI_OUT_OF_RESOURCES;
    return Status;
  }
  ShellSetFilePosition (FileHandle, 0);
  // read file line by line
  for (;!ShellFileHandleEof (FileHandle); Size = 1024) {
    Status = ShellFileHandleReadLine (FileHandle, ReadLine, &Size, TRUE, &Ascii);
    if (Status == EFI_BUFFER_TOO_SMALL) {
      Status = EFI_SUCCESS;
      Flag=FALSE;
      break;
    }
    if (EFI_ERROR (Status)) {
      Flag=FALSE;
      break;
    }
    ReadLine[StrLen (ReadLine)]=L'\0';
    Status=UnicodeStrToAsciiStrS (ReadLine,Line1,1024);
    if (EFI_ERROR (Status)) {
      Print (L" Error occoured in convertion\n");
      Flag=FALSE;
      break;
    }
    if (!AsciiStriCmp (Line1, "$End")) {
      i++;
      continue;
    }
    // Skip comment lines
    if (Line1[0] == '#' 
      || Line1[1] == '#' 
      || Line1[2] == '#' 
      || !AsciiStriCmp (Line1, "$Start")
      || !AsciiStriCmp (Line1, "")) {
      continue;
    } else {
      LineNo++;
      Line=&Line1[0];
      ReadLineTag=Line;
      IndexTag=0;
      for (Index=0;ReadLineTag[Index]!=':';Index++) {
        if (ReadLineTag[Index]!=' ') {
          Tag[IndexTag]=ReadLineTag[Index];
          IndexTag++;
        }
      }
      Tag[IndexTag]='\0';
      Line=AsciiStrStr (Line,":");
      if (Line==NULL) {
        continue;
      }
      Line++;
      while (Line[0]==' ') {
        Line++;
      }
      if (Line[0] == '\0') {
        continue;
      }
      if (!AsciiStriCmp (Tag, "NvmeOfEnabled")) {
        if (!AsciiStriCmp (Line, "TRUE")) {
          NvmeofData[0].NvmeOfEnabled = TRUE;
        } else {
          NvmeofData[0].NvmeOfEnabled = FALSE;
        }
      } else if (!AsciiStriCmp (Tag, "AttemptName")) {
        CopyMem(Nvmeof_Attemptconfig[i].AttemptName, Line, NVMEOF_ATTEMPT_NAME_SIZE);
      } else if (!AsciiStriCmp (Tag, "AutoConfigureMode")) {
        Nvmeof_Attemptconfig[i].AutoConfigureMode = (UINT8) AsciiStrDecimalToUintn (Line);
      } else if (!AsciiStriCmp (Tag, "MacString")) {
        CopyMem (Nvmeof_Attemptconfig[i].MacString, Line, NVMEOF_MAX_MAC_STRING_LEN);
      } else if (!AsciiStriCmp (Tag, "PrimaryDns")) {
        IpStatus = AsciiStrToIpv4Address (Line, &EndPointer, &Nvmeof_Attemptconfig[i].PrimaryDns.v4, NULL);
        if (EFI_ERROR (IpStatus)) {
          IpStatus = AsciiStrToIpv6Address (Line, &EndPointer, &Nvmeof_Attemptconfig[i].PrimaryDns.v6, NULL);
          if (EFI_ERROR (IpStatus)) {
            Print (L"Invalid PrimaryDns for Attempt %d\n",i);
          }
        }
      } else if (!AsciiStriCmp (Tag, "SecondaryDns")) {
        IpStatus = AsciiStrToIpv4Address (Line, &EndPointer, &Nvmeof_Attemptconfig[i].SecondaryDns.v4, NULL);
        if (EFI_ERROR (IpStatus)) {
          IpStatus = AsciiStrToIpv6Address (Line, &EndPointer, &Nvmeof_Attemptconfig[i].SecondaryDns.v6, NULL);
          if (EFI_ERROR (IpStatus)) {
            Print (L"Invalid SecondaryDns for Attempt %d\n",i);
          }
        }
      } else if (!AsciiStriCmp (Tag, "TargetPort")) {
        Nvmeof_Attemptconfig[i].SubsysConfigData.NvmeofSubsysPortId = (UINT16) AsciiStrDecimalToUintn (Line);
      } else if (!AsciiStrCmp (Tag, "Enabled")) {
        Nvmeof_Attemptconfig[i].SubsysConfigData.Enabled = (UINT8)AsciiStrDecimalToUintn (Line);
      } else if (!AsciiStriCmp (Tag, "IpMode")) {
        Nvmeof_Attemptconfig[i].SubsysConfigData.NvmeofIpMode = (UINT8) AsciiStrDecimalToUintn (Line);
      } else if (!AsciiStriCmp (Tag, "LocalIp")) {
        IpStatus = AsciiStrToIpv4Address (Line, &EndPointer, &Nvmeof_Attemptconfig[i].SubsysConfigData.NvmeofSubsysHostIP.v4, NULL);
        if (EFI_ERROR (IpStatus)) {
          IpStatus = AsciiStrToIpv6Address (Line, &EndPointer, &Nvmeof_Attemptconfig[i].SubsysConfigData.NvmeofSubsysHostIP.v6, NULL);
          if (EFI_ERROR (IpStatus)) {
            Print (L"Invalid LocalIp for Attempt %d\n",i);
          }
        }
      } else if (!AsciiStriCmp (Tag, "SubnetMask")) {
        IpStatus = AsciiStrToIpv4Address (Line, &EndPointer, &Nvmeof_Attemptconfig[i].SubsysConfigData.NvmeofSubsysHostSubnetMask.v4, NULL);
        if (EFI_ERROR (IpStatus)) {   
          IpStatus = AsciiStrToIpv6Address (Line, &EndPointer, &Nvmeof_Attemptconfig[i].SubsysConfigData.NvmeofSubsysHostSubnetMask.v6, NULL);
          if (EFI_ERROR (IpStatus)) {
            Print (L"Invalid SubnetMask for Attempt %d\n",i);
          }
        }
      } else if (!AsciiStriCmp(Tag, "Gateway")) {
        IpStatus = AsciiStrToIpv4Address (Line, &EndPointer, &Nvmeof_Attemptconfig[i].SubsysConfigData.NvmeofSubsysHostGateway.v4, NULL);
        if (EFI_ERROR (IpStatus)) {
          IpStatus = AsciiStrToIpv6Address (Line, &EndPointer, &Nvmeof_Attemptconfig[i].SubsysConfigData.NvmeofSubsysHostGateway.v6, NULL);
          if (EFI_ERROR (IpStatus)) {
            Print (L"Invalid Gateway for Attempt %d\n",i);
          }
        }
      } else if (!AsciiStriCmp (Tag, "InitiatorInfoFromDhcp")) {
        if (!AsciiStriCmp (Line, "TRUE")) {
          Nvmeof_Attemptconfig[i].SubsysConfigData.HostInfoDhcp = TRUE;
        } else {
          Nvmeof_Attemptconfig[i].SubsysConfigData.HostInfoDhcp = FALSE;
        }
      } else if (!AsciiStriCmp (Tag, "TargetInfoFromDhcp")) {
        if (!AsciiStriCmp(Line, "TRUE")) {
          Nvmeof_Attemptconfig[i].SubsysConfigData.NvmeofSubsysInfoDhcp = TRUE;
        } else {
          Nvmeof_Attemptconfig[i].SubsysConfigData.NvmeofSubsysInfoDhcp = FALSE;
        }
      } else if (!AsciiStriCmp (Tag, "TargetIp")) {
        IpStatus = AsciiStrToIpv4Address (Line, &EndPointer,&Nvmeof_Attemptconfig[i].SubsysConfigData.NvmeofSubSystemIp.v4, NULL);
        if (EFI_ERROR (IpStatus)) {
          IpStatus = AsciiStrToIpv6Address (Line, &EndPointer, &Nvmeof_Attemptconfig[i].SubsysConfigData.NvmeofSubSystemIp.v6, NULL);
          if (EFI_ERROR (IpStatus)) {
            Print (L"Invalid TargetIp for Attempt %d\n",i);
          }
        }
      } else if (!AsciiStriCmp (Tag, "ServerName")) {
        CopyMem (Nvmeof_Attemptconfig[i].SubsysConfigData.ServerName, Line, NVMEOF_NAME_MAX_SIZE);
      } else if (!AsciiStriCmp (Tag, "HostNqn")) {
        CopyMem (NvmeofData[0].NvmeofHostNqn, Line, NVMEOF_NAME_MAX_SIZE);
      } else if (!AsciiStriCmp(Tag, "HostId")) {
          AsciiStrToUnicodeStrS (Line, HostIdStr, NVMEOF_NID_LEN);
          if (StrToGuid (HostIdStr, &HostRawGuid) == RETURN_SUCCESS) {
            CopyMem (NvmeofData[0].NvmeofHostId, &HostRawGuid, sizeof (HostRawGuid));
          } else {
            Print (L"Invalid HostGUID format\n");
          }
      } else if (!AsciiStriCmp (Tag, "TransportPort")) {
        Nvmeof_Attemptconfig[i].SubsysConfigData.NvmeofTransportPort = (UINT16) AsciiStrDecimalToUintn (Line);
      } else if (!AsciiStriCmp (Tag, "ControllerId")) {
        Nvmeof_Attemptconfig[i].SubsysConfigData.NvmeofSubsysControllerId = (UINT16) AsciiStrDecimalToUintn (Line);
      } else if (!AsciiStriCmp (Tag, "NID")) {
        CopyMem (Nvmeof_Attemptconfig[i].SubsysConfigData.NvmeofSubsysNid, Line, NVMEOF_NID_LEN);
      } else if (!AsciiStriCmp (Tag, "NQN")) {
        CopyMem (Nvmeof_Attemptconfig[i].SubsysConfigData.NvmeofSubsysNqn, Line, NVMEOF_NAME_MAX_SIZE);
      } else if (!AsciiStriCmp (Tag, "TransportGUID")){
        AsciiStrToGuid(Line,&TransportGuidlo);
        CopyMem (&Nvmeof_Attemptconfig[i].SubsysConfigData.NvmeofTransportGuid, &TransportGuidlo, sizeof (EFI_GUID));
      } else if (!AsciiStriCmp (Tag, "ConnectTimeout")) {
        Nvmeof_Attemptconfig[i].SubsysConfigData.NvmeofTimeout = (UINT16) AsciiStrDecimalToUintn (Line);
      } else if (!AsciiStriCmp (Tag, "ConnectRetryCount")) {
        Nvmeof_Attemptconfig[i].SubsysConfigData.NvmeofRetryCount = (UINT8) AsciiStrDecimalToUintn (Line);
      } else if (!AsciiStriCmp (Tag, "DnsMode")) {
        if (!AsciiStriCmp (Line, "TRUE")) {
          Nvmeof_Attemptconfig[i].SubsysConfigData.NvmeofDnsMode = TRUE;
        } else {
          Nvmeof_Attemptconfig[i].SubsysConfigData.NvmeofDnsMode = FALSE;
        }
      } else if (!AsciiStriCmp (Tag, "RouteMetric")) {
        Nvmeof_Attemptconfig[i].SubsysConfigData.RouteMetric = (UINT16)AsciiStrDecimalToUintn (Line);
      } else if (!AsciiStriCmp (Tag, "HostName")) {
        CopyMem(Nvmeof_Attemptconfig[i].SubsysConfigData.HostName, Line, NVMEOF_NAME_MAX_SIZE);
      } else if (!AsciiStriCmp (Tag, "HostNqnOverride")) {
        CopyMem(Nvmeof_Attemptconfig[i].SubsysConfigData.NvmeofHostNqnOverride, Line, NVMEOF_NAME_MAX_SIZE);
      } else if (!AsciiStriCmp (Tag, "HostIdOverride")) {
        AsciiStrToUnicodeStrS (Line, HostIdStr, NVMEOF_NID_LEN);
        if (StrToGuid (HostIdStr, &HostRawGuid) == RETURN_SUCCESS) {
          CopyMem (Nvmeof_Attemptconfig[i].SubsysConfigData.NvmeofHostIdOverride, &HostRawGuid, sizeof (HostRawGuid));
        }
      } else if (!AsciiStriCmp (Tag, "HostOverrideEnable")) {
        if (!AsciiStriCmp (Line, "TRUE")) {
          Nvmeof_Attemptconfig[i].SubsysConfigData.HostOverrideEnable = TRUE;
        } else if (!AsciiStriCmp (Line, "FALSE")) {
          Nvmeof_Attemptconfig[i].SubsysConfigData.HostOverrideEnable = FALSE;
        } else {
          Print (L"Invalid value for Host Override Enable\n");
          return EFI_UNSUPPORTED;
        }
      } else {
        Temp = i + 1;
        Flag = FALSE;
        Print (L"The tag %a in Attempt %d is invalid",Tag,Temp);
      }
    }
  }
  FreePool (FileName);
  FreePool (ReadLine);
  NvmeofData[0].NvmeOfTargetCount = i;
  NvmeofData[0].DriverVersion = NVMEOF_DRIVER_VERSION;
  if(NvmeofData[0].NvmeofHostId[0] == 0)
  {
    Print (L"Error, HostId field not specified \n");
    return EFI_UNSUPPORTED;
  }
  if(NvmeofData[0].NvmeofHostNqn[0] == 0)
  {
    Print (L"Error, Host NQN field not specified \n");
    return EFI_UNSUPPORTED;
  }
  for (Index = 0; Index < i; Index++) {
    if (Nvmeof_Attemptconfig[Index].SubsysConfigData.HostOverrideEnable)
    {
      if (Nvmeof_Attemptconfig[Index].SubsysConfigData.NvmeofHostIdOverride[0] == 0)
      {
        Print (L"Error, HostId Override field not specified or invalid \n");
        return EFI_UNSUPPORTED;
      }
      if (Nvmeof_Attemptconfig[Index].SubsysConfigData.NvmeofHostNqnOverride[0] == 0)
      {
        Print (L"Error, Host NQN Override field not specified \n");
        return EFI_UNSUPPORTED;
      }
    }
  }
  if(i >= MAX_ATTEMPTS){
    Print (L"Error, Max %d Attempts are supported\n", MAX_ATTEMPTS);
    return  EFI_UNSUPPORTED;
  }
  if (Flag==TRUE) {
    Attributes = EFI_VARIABLE_BOOTSERVICE_ACCESS | EFI_VARIABLE_NON_VOLATILE;
    Status = NvmeOfCliCheckifVarAlreadyExists (Attributes);
    if (EFI_ERROR (Status)) {
      return Status;
    }

    Status = gRT->SetVariable (
                    L"Nvmeof_Attemptconfig",
                    &gNvmeOfConfigGuid,
                    Attributes,
                    sizeof (Nvmeof_Attemptconfig),
                    &Nvmeof_Attemptconfig
                    );
    if (EFI_ERROR (Status)) {
      Print (L"Error occured while setting UEFI variables. Please try again\n");
      return Status;
    }
    Status = gRT->SetVariable (
                    L"NvmeofGlobalData",
                    &gNvmeOfConfigGuid,
                    Attributes,
                    sizeof (NvmeofData),
                    &NvmeofData
                    );
    if (EFI_ERROR (Status)) {
      Print (L" Error occured while setting UEFI variables. Please try again\n");
      return Status;
    }
    Print (L"\n\n!!!The UEFI variables has been set successfully!!!\n\n");
  } else {
    Print (L"Error occured while setting UEFI variables. Please check the config file\n");
  }
  return EFI_SUCCESS;
}

/**
  Check for Pass through is required or not.

  @param  *Command             config file
  @retval EFI_SUCCESS          True
  @retval EFI_NOT_FOUND        False

**/
UINTN
EFIAPI
CheckIfPassThroughRequired (IN CONST CHAR16  *Command)
{
  UINTN Found = 1;

  if (!StrCmp (Command, L"setattempt"))
    Found = 0;
  else if (!StrCmp (Command, L"help"))
    Found = 0;
  else if (!StrCmp (Command, L"readwritesync"))
    Found = 0;
  else if (!StrCmp (Command, L"readwriteasync"))
    Found = 0;
  else if (!StrCmp (Command, L"dhfilter"))
    Found = 0;

  return Found;
}

/**
  Execute the command dhfilter

  @param  InFileName    drivers info
  @param  EFI_SUCCESS   dhfilter command is success
  @retval EFI_NOT_FOUND Resource Not Found

**/

EFI_STATUS
EFIAPI
NvmeOfCliDhFilter (IN CONST CHAR16  *InFileName)
{
  EFI_STATUS         Status    = EFI_SUCCESS;
  SHELL_FILE_HANDLE  FileHandle;
  CHAR16             *FileName;
  CHAR16             *FullFileName;
  CHAR16             *ReadLine;
  CHAR8              Line1[LINE_MAX];
  BOOLEAN            Ascii     = TRUE;
  BOOLEAN            Flag      = TRUE;
  UINTN              Size      = LINE_MAX;

  FileName = AllocateCopyPool (StrSize (InFileName), InFileName);
  if (FileName == NULL) {
      Print (L"ERROR: Could not allocate memory\n");
      return (EFI_OUT_OF_RESOURCES);
  }

  FullFileName = ShellFindFilePath (FileName);
  if (FullFileName == NULL) {
      Print (L"ERROR: Could not find %s\n", FileName);
      Status = EFI_NOT_FOUND;
      FreePool (FileName);
      return Status;
  }

  // open the file
  Status = ShellOpenFileByName (FullFileName, &FileHandle, EFI_FILE_MODE_READ, 0);
  if (EFI_ERROR (Status)) {
      Print (L"ERROR: Could not open file\n");
      FreePool (FileName);
      return Status;
  }

  // allocate a buffer to read lines into
  ReadLine = AllocateZeroPool (Size);
  if (ReadLine == NULL) {
      Print (L"ERROR: Could not allocate memory\n");
      FreePool (FileName);
      Status = EFI_OUT_OF_RESOURCES;
      return Status;
  }
  ShellSetFilePosition (FileHandle, 0);
  // read file line by line
  for (; !ShellFileHandleEof (FileHandle); Size = LINE_MAX) {
      Status = ShellFileHandleReadLine (FileHandle, ReadLine, &Size, TRUE, &Ascii);
      if (Status == EFI_BUFFER_TOO_SMALL) {
          Status = EFI_SUCCESS;
          Flag = FALSE;
          break;
      }
      if (EFI_ERROR (Status)) {
          Flag = FALSE;
          break;
      }
      ReadLine[StrLen (ReadLine)] = L'\0';
      Status = UnicodeStrToAsciiStrS (ReadLine, Line1, 1024);
      if (EFI_ERROR (Status)) {
          Print (L"Error occoured in convertion\n");
          Flag = FALSE;
          break;
      }

      if (AsciiStrStr (Line1, "NVMeOF Driver"))
      {
        Print (L"\n unload ");
        for (UINT8 i = OFFSET_OF_DEVICE_HANDLE; i < StrLen(ReadLine); i++) {
          if (Line1[i] == '\"')
            break;
          Print (L"%c", Line1[i]);
        }
        Print(L" -n \n");
          break;
      }
  }

  FreePool (FileName);
  FreePool (ReadLine);

  if (Flag)
    return EFI_SUCCESS;
  else
    return Status;
}


/**
  Main function

  @param  Argc             
  @param  **Argv
  @retval EFI_SUCCESS          True
  @retval EFI_NOT_FOUND        False.
**/

INTN
EFIAPI
ShellAppMain (UINTN Argc, CHAR16 **Argv)
{
  UINT8      IntStatus = 0;
  EFI_STATUS Status = EFI_SUCCESS;
  UINTN      IsInvoke = 0;
  BOOLEAN    Flag = FALSE;


  if (Argc < 2) {
    Print (L"Sub-command is missing\n");
    PrintUsageUtility();
    return EFI_SUCCESS;
  }
  IsInvoke = CheckIfPassThroughRequired (Argv[1]);
  if(IsInvoke) {
  Status = gBS->LocateProtocol (
                  &gNvmeofPassThroughProtocolGuid,
                  NULL,
                  (VOID **)&NvmeofPassThroughProtocol
                  );
    if (EFI_ERROR (Status)) {
      DEBUG ((EFI_D_ERROR, "ShellAppMain: CliLocateProtocol error\n"));
      DEBUG ((EFI_D_ERROR, "ShellAppMain: Failed to LOCATE protocol Status is %d .\n", Status));
      return Status;
    }
  }

  if (!StrCmp (Argv[1], L"list")) {
    if (Argc == 2) {      
       NvmeofPassThroughProtocol->NvmeOfCliList ();
    } else {
      PrintUsageList ();
    }
  } else if (!StrCmp (Argv[1], L"listattempt")) {
     if (Argc == 2) {
        NvmeofPassThroughProtocol->NvmeOfCliListConnect ();
     }
     else {
       PrintUsageListConnect ();
     }
  } else if (!StrCmp (Argv[1], L"read")) {
     if (Argc == 9){
       Status = ParseRead (Argc,Argv);
       if (EFI_ERROR (Status)) {
         PrintUsageRead ();
       } else {
         Status=NvmeOfCliGetControllerQpair (Argv);
         if (EFI_ERROR (Status)) {
           Print (L"Device not found\n");
         } else {
             Status = NvmeOfCliReadData ();
         }
       }
     } else {
       PrintUsageRead ();
     }
  } else if (!StrCmp (Argv[1], L"readwritesync")) {
     if (Argc == 8 || Argc == 10) {
       Status = ParseReadWriteBlock (Argc, Argv);
       if (EFI_ERROR (Status)) {
         PrintUsageReadWriteSync ();
       } else {
         Status = NvmeOfCompareReadWriteSyncData (Argc);
         if (EFI_ERROR (Status)) {
           Print (L" NvmeOfCompareReadWriteSyncData failed \n");
         }
       }
     } else {
       PrintUsageReadWriteSync ();
     }
  } else if (!StrCmp (Argv[1], L"readwriteasync")) {
     if (Argc == 8 || Argc == 10) {
       Status = ParseReadWriteBlock (Argc, Argv);
       if (EFI_ERROR (Status)) {
         PrintUsageReadWriteAsync ();
       } else {
         Status = NvmeOfCompareReadWriteAsyncData (Argc);
         if (EFI_ERROR (Status)) {
           Print(L" NvmeOfCompareReadWriteAsyncData failed \n");
         }
       }
     } else {
       PrintUsageReadWriteAsync ();
     }
  } else if (!StrCmp (Argv[1], L"write")) {
      if (Argc == 7){
       Status = ParseWrite (Argc,Argv);
       if (EFI_ERROR (Status)) {
         PrintUsageWrite ();
       } else {
         Status = NvmeOfCliGetControllerQpair (Argv);
         if (EFI_ERROR (Status)) {
           Print(L"Device not found\n");
         }
         else
         {
           Status = NvmeOfCliWriteData ();
         }
       }
     } else {
        PrintUsageWrite ();
     }

  } else if (!StrCmp (Argv[1], L"reset")) {
     if (Argc == 3) {
       Status = NvmeOfCliGetController (Argv);
       if (EFI_ERROR (Status)) {
         Print(L"Device not found\n");
       } else {
         IntStatus = NvmeofPassThroughProtocol->NvmeOfCliReset (gctrlr,Argv);
         if (IntStatus == 0) {
           Print (L"Reset has been done successfully\n");
         }
       }
     } else {
       PrintUsageReset ();
     }
  } else if (!StrCmp (Argv[1], L"connect")) {
    
     if (Argc == 20 || Argc == 18 || Argc == 22) {
      Status = ParseConnect (Argc,Argv);
      if (EFI_ERROR (Status)) {
        PrintUsageConnect ();
      } else {
        Flag = FALSE;
        Flag = NvmeOfCliIsConnected ();
        if (!Flag) {
          Status = NvmeofPassThroughProtocol->Connect (ConnectCommandCliData);
          if (EFI_ERROR (Status)) {
            Print (L"Error occured while connect\n");
          } else {
            NvmeofPassThroughProtocol->NvmeOfCliList ();
          }
        } else {
          Print (L"The device is already connected\n");
        }
      }//back here
    } else {
       PrintUsageConnect ();
    }
  } else if (!StrCmp (Argv[1], L"disconnect")) {
     if (Argc == 3) {
        Status = NvmeOfCliGetController (Argv);
        if (EFI_ERROR (Status)) {
          Print(L"Device not found\n");
        } else {
          DisconnectData.Ctrlr = gctrlr;
          NvmeofPassThroughProtocol->NvmeOfCliDisconnect (DisconnectData, Argv);
        }
    } else {
        PrintUsageDisconnect ();
    }
  } else if (!StrCmp (Argv[1], L"identify")) {
      if (Argc == 3) {
        Status =NvmeOfCliGetController (Argv);
        if (EFI_ERROR (Status)) {
          Print (L"Device not found\n");
        } else {
          Status=NvmeOfCliGetNsid (Argv);
          if (EFI_ERROR (Status)) {
            Print (L"Device not found\n");
          } else {
            IdentifyData.NsId = gNsId;
            IdentifyData.Ctrlr = gctrlr;
            IdentifyData = NvmeofPassThroughProtocol->NvmeOfCliIdentify (IdentifyData);
          }
        }
      } else {
        PrintUsageIdentify ();
      }
  } else if (!StrCmp (Argv[1], L"version")) {
      if (Argc == 2) {
        UINTN Version;
        Version = NvmeofPassThroughProtocol->NvmeOfCliVersion ();
        Print (L"NVMeOF Driver Version : %d\n", Version);
        Print (L"NVMeOF Cli Version : %d\n", NVMEOF_CLI_VERSION);
      } else {
        PrintUsageVersion ();
     }
  } else if (!StrCmp (Argv[1], L"setattempt")) {
      if (Argc != 3) {
        PrintUsageSetAttempt ();
     } else {
       Status = NvmeOfCliSetAttempt (Argv[2]);
       if (EFI_ERROR (Status)) {
         PrintUsageSetAttempt ();
       }
     }
    } else if (!StrCmp(Argv[1], L"dhfilter")) {
       if (Argc != 3) {
           Print(L"\nUsage example : NvmeOfCli.efi dhfilter fileName\n");
       } else {
           Status = NvmeOfCliDhFilter (Argv[2]);
           if (EFI_ERROR (Status)) {
               Print(L"\nUsage example : NvmeOfCli.efi dhfilter fileName\n");
           }
       }

    } else if (!StrCmp (Argv[1], L"help")) {
      if (Argc == 2) {
        PrintUsageHelp ();
      } else if (Argc == 3) {
        Status = ParseHelp (Argc, Argv);
        if (EFI_ERROR (Status)) {
          Print(L"\n Usage example : NvmeOfCli.efi help command\n");
        } else {
          if (!StrCmp (Argv[2], L"list")) {
            PrintUsageList ();
          } else if (!StrCmp (Argv[2], L"listattempt")) {
            PrintUsageListConnect();
          } else if (!StrCmp (Argv[2], L"read")) {
            PrintUsageRead ();
          } else if (!StrCmp (Argv[2], L"write")) {
            PrintUsageWrite ();
          } else if (!StrCmp (Argv[2], L"reset")) {
            PrintUsageReset ();
          } else if (!StrCmp (Argv[2], L"connect")) {
            PrintUsageConnect ();
          } else if (!StrCmp (Argv[2], L"disconnect")) {
            PrintUsageDisconnect ();
          } else if (!StrCmp (Argv[2], L"identify")) {
            PrintUsageIdentify ();
          } else if (!StrCmp (Argv[2], L"version")) {
            PrintUsageVersion ();
          } else if (!StrCmp (Argv[2], L"setattempt")) {
            PrintUsageSetAttempt ();
          } else if (!StrCmp (Argv[2], L"readwritesync")) {
            PrintUsageReadWriteSync ();
          } else if (!StrCmp (Argv[2], L"readwriteasync")) {
            PrintUsageReadWriteAsync ();
          }
         }
      } else {
         Print (L"\n Usage example : NvmeOfCli.efi help command\n");
      }
  } else {
     Print ((CHAR16*)L"\n Unsupported option specified, check command\n");
  }     
  return EFI_SUCCESS;
}

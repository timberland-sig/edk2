/** @file
  NvmeOfDxe Cli is used to manage cli commands

  Copyright (c) 2020, Dell EMC All rights reserved
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#ifndef __NVMEOFCLI_H__
#define __NVMEOFCLI_H__

#define NVMEOF_CLI_VERSION 10 //Indicate 1.0

#define memset(dest,ch,count)      gBS->SetMem(dest,(UINTN)(count),(UINT8)(ch))
#define memcmp(buf1,buf2,count)    (int)(CompareMem(buf1,buf2,(UINTN)(count)))
#define BLOCK_SIZE 512
#define PATTERN 0xac
#define LINE_MAX  1024
#define MAX_ATTEMPTS 8

extern EFI_GUID gNvmeofPassThroughProtocolGuid;
NVMEOF_READ_WRITE_DATA_IN_BLOCK ReadWriteCliDataInBlock = { 0 };
CHAR16 *gReadFile;
CHAR16 *gWriteFile;

extern EFI_STATUS
EFIAPI
FileHandleWrite (
  IN EFI_FILE_HANDLE,
  IN OUT UINTN *,
  IN VOID *
  );

extern EFI_STATUS
EFIAPI
FileHandleRead (
  IN EFI_FILE_HANDLE,
  IN OUT UINTN *,
  IN VOID *
  );
extern EFI_STATUS
EFIAPI
FileHandleSetPosition (
  IN EFI_FILE_HANDLE,
  IN UINT64  
  );

NVMEOF_CONNECT_COMMAND ConnectCommandCliData = {0};
NVMEOF_READ_WRITE_DATA ReadWriteCliData;
NVMEOF_CLI_IDENTIFY IdentifyData;
NVMEOF_CLI_DISCONNECT DisconnectData;
UINTN gNsId;

#endif // !__NVMEOFCLI_H__

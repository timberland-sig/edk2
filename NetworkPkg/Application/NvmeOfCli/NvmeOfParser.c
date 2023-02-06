/** @file
  Cli Parser file for NvmeOf driver.

  Copyright (c) 2020, Dell EMC All rights reserved
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Uefi.h>
#include <Library/UefiLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/PrintLib.h>
#include <Library/NetLib.h>
#include <Base.h> 
#include <Library/BaseLib.h>
#include "../../NvmeOfDxe/NvmeOfCliInterface.h"

extern NVMEOF_CONNECT_COMMAND ConnectCommandCliData;
extern NVMEOF_READ_WRITE_DATA ReadWriteCliData;
extern NVMEOF_READ_WRITE_DATA_IN_BLOCK ReadWriteCliDataInBlock;
extern CHAR16 *gReadFile;
extern CHAR16 *gWriteFile;

/**
  Parse Read params

  @param[in]  Argc  Input count
  @param[in]  Argv  Input param array
  @retval EFI_SUCCESS   Parsing OK
**/

INTN
EFIAPI
ParseRead (UINTN Argc, CHAR16 **Argv)
{
  UINTN      i = 0; 
  EFI_STATUS Status = EFI_SUCCESS;

  for (i=3;i<Argc;) {
    if (StrCmp (Argv[i], L"-c")
      && StrCmp (Argv[i], L"-s")
      && StrCmp (Argv[i], L"-d")
      && StrCmp (Argv[i], L"--start-block")
      && StrCmp (Argv[i], L"--block-count")
      && StrCmp (Argv[i], L"--data")) {
      Status=EFI_INVALID_PARAMETER;
      break;
    } else {
      if (!StrCmp (Argv[i], L"--data") || !StrCmp (Argv[i], L"-d")) {
        gReadFile = Argv[i + 1];
      } else if (!StrCmp (Argv[i], L"--start-block") || !StrCmp (Argv[i], L"-s")) {
        ReadWriteCliData.Startblock = StrDecimalToUint64 (Argv[i + 1]);
      } else if (!StrCmp (Argv[i], L"--block-count") || !StrCmp (Argv[i], L"-c")) {
        ReadWriteCliData.Blockcount = (UINT32)StrDecimalToUintn (Argv[i + 1]);
      }
    }
    i = i+2;
  }
  return Status;
}

/**
  Parse ReadWriteBlock params

  @param[in]                      Argc  Input count
  @param[in]                      Argv  Input param array
  @retval EFI_SUCCESS             Parsing OK
  @retval EFI_INVALID_PARAMETER   Invalid Parameter
**/
INTN
EFIAPI
ParseReadWriteBlock (UINTN Argc, CHAR16 **Argv)
{
  EFI_STATUS status = EFI_SUCCESS;
  UINTN      i = 0;
  
  for (i = 2; i < Argc;) {
    if (StrCmp (Argv[i], L"-d") &&
        StrCmp (Argv[i], L"-s") &&
        StrCmp (Argv[i], L"-n") &&
        StrCmp (Argv[i], L"-p") &&
        StrCmp (Argv[i], L"--device-id") &&
        StrCmp (Argv[i], L"--start-lba") &&
        StrCmp (Argv[i], L"--numberof-block") &&
        StrCmp (Argv[i], L"--pattern")
      ) {
      status = EFI_INVALID_PARAMETER;
      break;
    } else {
      if(!StrCmp (Argv[i], L"--device-id") || !StrCmp (Argv[i], L"-d")) {
        CopyMem(&ReadWriteCliDataInBlock.Device_id, Argv[i + 1], 40);
      } else if (!StrCmp (Argv[i], L"--start-lba") || !StrCmp (Argv[i], L"-s")) {
        ReadWriteCliDataInBlock.Start_lba = StrDecimalToUint64 (Argv[i + 1]);
      } else if (!StrCmp (Argv[i], L"--numberof-block") || !StrCmp (Argv[i], L"-n")) {
        ReadWriteCliDataInBlock.Numberof_block = (UINT32)StrDecimalToUint64 (Argv[i + 1]);
      } else if (!StrCmp(Argv[i], L"--pattern") || !StrCmp (Argv[i], L"-p")) {
        ReadWriteCliDataInBlock.Pattern = StrHexToUint64 (Argv[i + 1]);
      } else {}  
    }
    i = i + 2;
  }
  return status;
}

/**
  Parse ReadWrite params

  @param[in]                      Argc  Input count
  @param[in]                      Argv  Input param array
  @retval EFI_SUCCESS             Parsing OK
  @retval EFI_INVALID_PARAMETER   Invalid Parameter
**/

INTN
EFIAPI
ParseWrite (UINTN Argc, CHAR16 **Argv)
{
  EFI_STATUS Status = EFI_SUCCESS;
  UINTN      i = 0;

  for (i = 3; i < Argc;) {
    if (StrCmp (Argv[i], L"-s") 
      && StrCmp (Argv[i], L"-d") 
      && StrCmp (Argv[i], L"--start-block")
      && StrCmp (Argv[i], L"--data")) {
      Status = EFI_INVALID_PARAMETER;
      break;
    } else {
      if (!StrCmp (Argv[i], L"--data") || !StrCmp (Argv[i], L"-d")) {
        gWriteFile=Argv[i + 1];
      } else if (!StrCmp (Argv[i], L"--start-block") || !StrCmp (Argv[i], L"-s")) {
        ReadWriteCliData.Startblock = StrDecimalToUint64 (Argv[i + 1]);
      }
    }
    i = i + 2;
  }
  return Status;
}

/**
  Parse ParseConnect params

  @param[in]                      Argc  Input count
  @param[in]                      Argv  Input param array
  @retval EFI_SUCCESS             Parsing OK
  @retval EFI_INVALID_PARAMETER   Invalid Parameter
**/
INTN
EFIAPI
ParseConnect (UINTN Argc, CHAR16 **Argv)
{
  EFI_STATUS                Status = EFI_SUCCESS;
  CHAR16                    *EndPointer = NULL;
  UINTN                     i = 0;
  UINTN                     Valid = 0;
  EFI_STATUS                IpStatus;
  EFI_IP_ADDRESS            Argument;
  UINTN                     Transportarg = 0;
  CHAR8                     TempStr[NVMEOF_CLI_MAX_SIZE];

  for (i=2;i<Argc;) {
    if ( StrCmp (Argv[i], L"-n") 
      && StrCmp (Argv[i], L"-q")
      && StrCmp (Argv[i], L"-t")
      && StrCmp (Argv[i], L"-a") 
      && StrCmp (Argv[i], L"-s")       
      && StrCmp (Argv[i], L"--nqn") 
      && StrCmp (Argv[i], L"--transport") 
      && StrCmp (Argv[i], L"--traddr") 
      && StrCmp (Argv[i], L"--trsvcid")
      && StrCmp (Argv[i], L"--hostnqn")
      && StrCmp (Argv[i], L"--mac") 
      && StrCmp (Argv[i], L"--ipmode")       
      && StrCmp (Argv[i], L"--localip") 
      && StrCmp (Argv[i], L"--subnetmask") 
      && StrCmp (Argv[i], L"--gateway")) {
      Status = EFI_INVALID_PARAMETER;
      break;
    } else {
      if (!StrCmp (Argv[i], L"-a") || !StrCmp (Argv[i], L"--traddr")) {
        Status = UnicodeStrToAsciiStrS (Argv[i + 1], TempStr, NVMEOF_CLI_MAX_SIZE);
        CopyMem (ConnectCommandCliData.Traddr,TempStr, NVMEOF_CLI_MAX_SIZE);
        IpStatus = StrToIpv4Address (Argv[i+1], &EndPointer, &Argument.v4, NULL);
        if (EFI_ERROR (IpStatus)) {
          IpStatus = StrToIpv6Address (Argv[i+1], &EndPointer, &Argument.v6, NULL);
          if (EFI_ERROR (IpStatus)) {
            Print (L"Invalid Ip address.\n");
            Status = EFI_INVALID_PARAMETER;
            break;
          }
        }
     } else if (!StrCmp (Argv[i], L"--trsvcid") || !StrCmp (Argv[i], L"-s")) {
        ConnectCommandCliData.Trsvcid= (UINT16)StrDecimalToUintn (Argv[i + 1]);
        Valid = StrDecimalToUintn (Argv[i+1]);
        if (Valid == 0) {
          Print (L"Invalid value of -s/--trsvcid.\n");
          Status = EFI_INVALID_PARAMETER;
          break;
       }
     } else if (!StrCmp (Argv[i], L"--nqn") ||  !StrCmp (Argv[i],L"-n")) {
        Status = UnicodeStrToAsciiStrS (Argv[i + 1], TempStr, NVMEOF_CLI_MAX_SIZE);
        CopyMem (ConnectCommandCliData.Nqn, TempStr, NVMEOF_CLI_MAX_SIZE);
     } else if (!StrCmp (Argv[i], L"--hostnqn") || !StrCmp (Argv[i],L"-q")) {
        Status = UnicodeStrToAsciiStrS (Argv[i + 1], TempStr, NVMEOF_CLI_MAX_SIZE);
        ConnectCommandCliData.UseHostNqn = 1; //Enable user hostnqn
        CopyMem (ConnectCommandCliData.Hostnqn, TempStr, NVMEOF_CLI_MAX_SIZE);
     } else if (!StrCmp (Argv[i], L"--transport") || !StrCmp (Argv[i], L"-t")) {
        CopyMem (ConnectCommandCliData.Transport, Argv[i + 1], NVMEOF_CLI_MAX_SIZE);
        Transportarg = 1;
     } else if (!StrCmp(Argv[i], L"--mac")) {
        Status = UnicodeStrToAsciiStrS (Argv[i + 1], TempStr, NVMEOF_CLI_MAX_SIZE);
        CopyMem (ConnectCommandCliData.Mac, TempStr, NVMEOF_CLI_MAX_SIZE);
     } else if (!StrCmp(Argv[i], L"--ipmode")) {
        ConnectCommandCliData.IpMode = (UINT8)StrDecimalToUintn (Argv[i + 1]);
     } else if (!StrCmp (Argv[i], L"--localip")) {
        Status = UnicodeStrToAsciiStrS (Argv[i + 1], TempStr, NVMEOF_CLI_MAX_SIZE);
        CopyMem (ConnectCommandCliData.LocalIp, TempStr, NVMEOF_CLI_MAX_SIZE);
        IpStatus = StrToIpv4Address (Argv[i + 1], &EndPointer, &Argument.v4, NULL);
        if (EFI_ERROR (IpStatus)) {
          IpStatus = StrToIpv6Address (Argv[i + 1], &EndPointer, &Argument.v6, NULL);
          if (EFI_ERROR (IpStatus)) {
            Print (L"Invalid Local Ip address.\n");
            Status = EFI_INVALID_PARAMETER;
            break;
          }
       }
     } else if (!StrCmp (Argv[i], L"--subnetmask")) {
        Status = UnicodeStrToAsciiStrS (Argv[i + 1], TempStr, NVMEOF_CLI_MAX_SIZE);
        CopyMem (ConnectCommandCliData.SubnetMask, TempStr, NVMEOF_CLI_MAX_SIZE);
        IpStatus = StrToIpv4Address (Argv[i + 1], &EndPointer, &Argument.v4, NULL);
        if (EFI_ERROR (IpStatus)) {
          IpStatus = StrToIpv6Address (Argv[i + 1], &EndPointer, &Argument.v6, NULL);
          if (EFI_ERROR (IpStatus)) {
            Print(L"Invalid SubnetMask.\n");
            Status = EFI_INVALID_PARAMETER;
            break;
          }
       }
     } else if (!StrCmp (Argv[i], L"--gateway")) {
        Status = UnicodeStrToAsciiStrS (Argv[i + 1], TempStr, NVMEOF_CLI_MAX_SIZE);
        CopyMem (ConnectCommandCliData.Gateway, TempStr, NVMEOF_CLI_MAX_SIZE);
        IpStatus = StrToIpv4Address (Argv[i + 1], &EndPointer, &Argument.v4, NULL);
        if (EFI_ERROR (IpStatus)) {
          IpStatus = StrToIpv6Address (Argv[i + 1], &EndPointer, &Argument.v6, NULL);
          if (EFI_ERROR (IpStatus)) {
            Print (L"Invalid Gateway.\n");
            Status = EFI_INVALID_PARAMETER;
            break;
          }
        }
      }
    }
    i = i+2;
  }
  if (Transportarg == 0) {
    // If no transport specificed default it to TCP
    CopyMem (ConnectCommandCliData.Transport, "tcp", NVMEOF_CLI_MAX_SIZE);
  }
  return Status;
}


/**
  Parse help command params

  @param[in]                      Argc  Input count
  @param[in]                      Argv  Input param array
  @retval EFI_SUCCESS             Parsing OK
**/
INTN
EFIAPI
ParseHelp (UINTN Argc, CHAR16 **Argv)
{
  EFI_STATUS Status = EFI_SUCCESS;
  UINTN      i = 0;

  for (i=2;i<Argc;) {
    if (StrCmp (Argv[i], L"list") 
      && StrCmp (Argv[i], L"listconnect") 
      && StrCmp (Argv[i], L"read") 
      && StrCmp (Argv[i], L"write") 
      && StrCmp (Argv[i], L"reset") 
      && StrCmp (Argv[i], L"connect") 
      && StrCmp (Argv[i], L"disconnect") 
      && StrCmp (Argv[i], L"identify") 
      && StrCmp (Argv[i], L"version")
      && StrCmp (Argv[i], L"setattempt")
      && StrCmp (Argv[i], L"readwriteasync")
      && StrCmp (Argv[i], L"readwritesync")
      ) {
      Status=EFI_INVALID_PARAMETER;
      break;
    }
    i = i+2;
  }
 return Status;
}



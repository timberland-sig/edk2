/** @file
  Cli Parser file for NvmeOf driver.

  Copyright (c) 2020, Dell EMC All rights reserved
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __NVMEOFPARSER__

#define __NVMEOFPARSER__

/**
  Parse Read params

  @param[in]  Argc  Input count
  @param[in]  Argv  Input param array
  @retval EFI_SUCCESS   Parsing OK
**/
INTN
EFIAPI
ParseRead (UINTN Argc, CHAR16 **Argv);

/**
  Parse ReadWriteBlock params

  @param[in]                      Argc  Input count
  @param[in]                      Argv  Input param array
  @retval EFI_SUCCESS             Parsing OK
  @retval EFI_INVALID_PARAMETER   Invalid Parameter
**/
INTN
EFIAPI
ParseReadWriteBlock (UINTN Argc, CHAR16 **Argv);

/**
  Parse ReadWrite params

  @param[in]                      Argc  Input count
  @param[in]                      Argv  Input param array
  @retval EFI_SUCCESS             Parsing OK
  @retval EFI_INVALID_PARAMETER   Invalid Parameter
**/

INTN
EFIAPI
ParseWrite (UINTN Argc, CHAR16 **Argv);

/**
  Parse ParseConnect params

  @param[in]                      Argc  Input count
  @param[in]                      Argv  Input param array
  @retval EFI_SUCCESS             Parsing OK
  @retval EFI_INVALID_PARAMETER   Invalid Parameter
**/
INTN
EFIAPI
ParseConnect (UINTN Argc, CHAR16 **Argv);


/**
  Parse help command params

  @param[in]                      Argc  Input count
  @param[in]                      Argv  Input param array
  @retval EFI_SUCCESS             Parsing OK
**/
INTN
EFIAPI
ParseHelp (UINTN Argc, CHAR16 **Argv);
#endif


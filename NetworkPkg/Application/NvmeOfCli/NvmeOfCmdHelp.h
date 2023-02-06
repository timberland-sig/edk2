/** @file
  Helper Functions Header to print command usage for NVMeOF CLI Application

  Copyright (c) 2020, Dell EMC All rights reserved
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/
#ifndef __NVMEOFCMDHELP__

#define __NVMEOFCMDHELP__

#include <Library/PrintLib.h>

/**
  Prints Usage for list command
**/
VOID
PrintUsageList ();

/**
  Prints Usage for listconnect command
**/
VOID
PrintUsageListConnect ();

/**
  Prints Usage for Read command
**/
VOID
PrintUsageRead ();

/**
  Prints Usage for Write command
**/
VOID
PrintUsageWrite ();

/**
  Prints Usage for Reset command
**/
VOID
PrintUsageReset ();

/**
  Prints Usage for Connect command
**/
VOID
PrintUsageConnect ();

/**
  Prints Usage for Disconnect command
**/
VOID
PrintUsageDisconnect ();

/**
  Prints Usage for Identify command
**/
VOID
PrintUsageIdentify ();

/**
  Prints Usage for Version command
**/
VOID
PrintUsageVersion ();

/**
  Prints Usage for SetAttempt command
**/
VOID
PrintUsageSetAttempt ();

/**
  Prints Usage for Read Write Sync command
**/
VOID
PrintUsageReadWriteSync ();

/**
  Prints Usage for Read Write Async command
**/
VOID
PrintUsageReadWriteAsync ();

/**
  Prints Usage when no specific sub command is given
**/
VOID
PrintUsageUtility ();

/**
  Prints list of all supported commands
**/
VOID
PrintUsageHelp ();
#endif
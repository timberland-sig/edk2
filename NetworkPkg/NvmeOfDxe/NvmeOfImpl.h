/** @file
  The shared head file for NVMEOF driver.

  Copyright (c) 2020, Dell EMC All rights reserved
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef _NVMEOF_IMPL_H_
#define _NVMEOF_IMPL_H_

#include <Uefi.h>

#include <IndustryStandard/Dhcp.h>

#include <Protocol/ComponentName.h>
#include <Protocol/ComponentName2.h>
#include <Protocol/DriverBinding.h>
#include <Protocol/DevicePath.h>
#include <Protocol/HiiConfigAccess.h>

#include <Protocol/Ip6.h>
#include <Protocol/Dhcp4.h>
#include <Protocol/Dhcp6.h>
#include <Protocol/Dns4.h>
#include <Protocol/Dns6.h>
#include <Protocol/Tcp4.h>
#include <Protocol/Tcp6.h>
#include <Protocol/Ip4Config2.h>
#include <Protocol/Ip6Config.h>

#include <Protocol/AuthenticationInfo.h>
#include <Protocol/ScsiPassThruExt.h>
#include <Protocol/AdapterInformation.h>
#include <Protocol/NetworkInterfaceIdentifier.h>

#include <Library/DpcLib.h>
#include <Library/NetLib.h>
#include <Library/TcpIoLib.h>
#include <Library/HiiLib.h>
#include <Library/UefiHiiServicesLib.h>
#include <Library/DevicePathLib.h>
#include <Library/DebugLib.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PrintLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Library/UefiLib.h>

#include <Guid/MdeModuleHii.h>
#include <Guid/EventGroup.h>
#include <Guid/Acpi.h>

#include "NvmeOfDriver.h"
#include "NvmeOfMisc.h"
#include "NvmeOfDhcp.h"
#include "NvmeOfDhcp6.h"

#define NVMEOF_DRIVER_DATA_SIGNATURE SIGNATURE_32 ('N', 'E', 'O', 'F')

#define NVMEOF_DRIVER_DATA_FROM_IDENTIFIER(Identifier) \
  CR ( \
  Identifier, \
  NVMEOF_DRIVER_DATA, \
  NvmeOfIdentifier, \
  NVMEOF_DRIVER_DATA_SIGNATURE \
  )


#endif

/** @file
  The header file of functions for configuring or getting the parameters
  relating to NVMe-oF.

Copyright (c) 2023, Dell Inc. or its subsidiaries. All Rights Reserved.<BR>
SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef _NVMEOF_CONFIG_H_
#define _NVMEOF_CONFIG_H_

#include "NvmeOfConfigNVDataStruc.h"

typedef struct _NVMEOF_FORM_CALLBACK_INFO NVMEOF_FORM_CALLBACK_INFO;

extern UINT8  NvmeOfConfigVfrBin[];
extern UINT8  NvmeOfDxeStrings[];

#define NVMEOF_CONFIG_VAR_ATTR  (EFI_VARIABLE_BOOTSERVICE_ACCESS | EFI_VARIABLE_NON_VOLATILE)

#define NVMEOF_FORM_CALLBACK_INFO_SIGNATURE  SIGNATURE_32 ('N', 'f', 'c', 'i')

#define NVMEOF_FORM_CALLBACK_INFO_FROM_FORM_CALLBACK(Callback) \
  CR ( \
  Callback, \
  NVMEOF_FORM_CALLBACK_INFO, \
  ConfigAccess, \
  NVMEOF_FORM_CALLBACK_INFO_SIGNATURE \
  )

#pragma pack(1)

///
/// HII specific Vendor Device Path definition.
///
typedef struct {
  VENDOR_DEVICE_PATH          VendorDevicePath;
  EFI_DEVICE_PATH_PROTOCOL    End;
} HII_VENDOR_DEVICE_PATH;

#pragma pack()

struct _NVMEOF_FORM_CALLBACK_INFO {
  UINT32                            Signature;
  EFI_HANDLE                        DriverHandle;
  EFI_HII_CONFIG_ACCESS_PROTOCOL    ConfigAccess;
  UINT16                            *KeyList;
  VOID                              *FormBuffer;
  EFI_HII_HANDLE                    RegisteredHandle;
  NVMEOF_ATTEMPT_CONFIG_NVDATA      *Current;
  BOOLEAN                           InitialFormLoad;
};

typedef struct {
  LIST_ENTRY    NicInfoList;
  UINT8         NicCount;
  LIST_ENTRY    AttemptInfoList;
  UINT8         AttemptCount;
} NVMEOF_CONFIG_PRIVATE_DATA;

typedef struct {
  LIST_ENTRY                      Link;
  UINT8                           AttemptIndex;
  NVMEOF_ATTEMPT_CONFIG_NVDATA    Data;
} NVMEOF_CONFIG_ATTEMPT_INFO;

typedef struct {
  LIST_ENTRY         Link;
  EFI_HANDLE         DeviceHandle;
  EFI_MAC_ADDRESS    PermanentAddress;
  UINTN              HwAddressSize;
  UINTN              VlanId;
  UINT8              NicIndex;
  BOOLEAN            Ipv6Support;
} NVMEOF_CONFIG_NIC_INFO;

/**
  Initialize the NVMe-oF configuration form.

  @param[in]  DriverBindingHandle The NVMe-oF driverbinding handle.

  @retval EFI_SUCCESS             The NVMe-oF configuration form is initialized.
  @retval EFI_OUT_OF_RESOURCES    Failed to allocate memory.

**/
EFI_STATUS
NvmeOfConfigFormInit (
  IN EFI_HANDLE  DriverBindingHandle
  );

/**
  Unload the NVMe-oF configuration form, this includes: delete all the NVMe-oF
  configuration entries, uninstall the form callback protocol, and
  free the resources used.

  @param[in]  DriverBindingHandle The NVMe-oF driverbinding handle.

  @retval EFI_SUCCESS             The NVMe-oF configuration form is unloaded.
  @retval Others                  Failed to unload the form.

**/
EFI_STATUS
NvmeOfConfigFormUnload (
  IN EFI_HANDLE  DriverBindingHandle
  );

/**
  Update the MAIN form to display the configured attempts.

**/
VOID
NvmeOfConfigUpdateAttempt (
  VOID
  );

#endif

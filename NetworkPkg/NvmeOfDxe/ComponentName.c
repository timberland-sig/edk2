/** @file
  UEFI Component Name(2) protocol implementation for NVMeOF.

  Copyright (c) 2020, Dell EMC All rights reserved
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "NvmeOfImpl.h"

//
// EFI Component Name Protocol
//
GLOBAL_REMOVE_IF_UNREFERENCED EFI_COMPONENT_NAME_PROTOCOL gNvmeOfComponentName = {
  NvmeOfComponentNameGetDriverName,
  NvmeOfComponentNameGetControllerName,
  "eng"
};

//
// EFI Component Name 2 Protocol
//
GLOBAL_REMOVE_IF_UNREFERENCED EFI_COMPONENT_NAME2_PROTOCOL gNvmeOfComponentName2 = {
  (EFI_COMPONENT_NAME2_GET_DRIVER_NAME) NvmeOfComponentNameGetDriverName,
  (EFI_COMPONENT_NAME2_GET_CONTROLLER_NAME) NvmeOfComponentNameGetControllerName,
  "en"
};

GLOBAL_REMOVE_IF_UNREFERENCED EFI_UNICODE_STRING_TABLE     mNvmeOfDriverNameTable[] = {
  {
    "eng;en",
    L"NVMeOF Driver"
  },
  {
    NULL,
    NULL
  }
};

GLOBAL_REMOVE_IF_UNREFERENCED EFI_UNICODE_STRING_TABLE  *gNvmeOfControllerNameTable = NULL;

/**
  Retrieves a Unicode string that is the user readable name of the driver.

  This function retrieves the user readable name of a driver in the form of a
  Unicode string. If the driver specified by This has a user readable name in
  the language specified by Language, then a pointer to the driver name is
  returned in DriverName, and EFI_SUCCESS is returned. If the driver specified
  by This does not support the language specified by Language,
  then EFI_UNSUPPORTED is returned.

  @param[in]  This              A pointer to the EFI_COMPONENT_NAME2_PROTOCOL or
                                EFI_COMPONENT_NAME_PROTOCOL instance.

  @param[in]  Language          A pointer to a Null-terminated ASCII string
                                array indicating the language. This is the
                                language of the driver name that the caller is
                                requesting, and it must match one of the
                                languages specified in SupportedLanguages. The
                                number of languages supported by a driver is up
                                to the driver writer. Language is specified
                                in RFC 4646 or ISO 639-2 language code format.

  @param[out]  DriverName       A pointer to the Unicode string to return.
                                This Unicode string is the name of the
                                driver specified by This in the language
                                specified by Language.

  @retval EFI_SUCCESS           The Unicode string for the Driver specified by
                                This and the language specified by Language was
                                returned in DriverName.

  @retval EFI_INVALID_PARAMETER Language is NULL.

  @retval EFI_INVALID_PARAMETER DriverName is NULL.

  @retval EFI_UNSUPPORTED       The driver specified by This does not support
                                the language specified by Language.

**/
EFI_STATUS
EFIAPI
NvmeOfComponentNameGetDriverName (
  IN  EFI_COMPONENT_NAME_PROTOCOL   *This,
  IN  CHAR8                         *Language,
  OUT CHAR16                        **DriverName
  )
{
  return LookupUnicodeString2 (
           Language,
           This->SupportedLanguages,
           mNvmeOfDriverNameTable,
           DriverName,
           (BOOLEAN) (This == &gNvmeOfComponentName)
           );
}


/**
  Update the component name for the NVMeOF instance.

  @param[in]  Ipv6Flag              TRUE if IP6 network stack is used.

  @retval EFI_SUCCESS               Update the ControllerNameTable of this instance successfully.
  @retval EFI_INVALID_PARAMETER     The input parameter is invalid.
  @retval EFI_UNSUPPORTED           Can't get the corresponding NIC info from the Controller handle.

**/
EFI_STATUS
UpdateName (
  IN   BOOLEAN     Ipv6Flag
  )
{
  EFI_STATUS                       Status;
  CHAR16                           HandleName[80];
  

  UnicodeSPrint (
    HandleName,
    sizeof (HandleName),
    L"NvmeOF (%s)",
    Ipv6Flag ? L"IPv6" : L"IPv4"
);

  if (gNvmeOfControllerNameTable != NULL) {
    FreeUnicodeStringTable (gNvmeOfControllerNameTable);
    gNvmeOfControllerNameTable = NULL;
  }

  Status = AddUnicodeString2 (
             "eng",
             gNvmeOfComponentName.SupportedLanguages,
             &gNvmeOfControllerNameTable,
             HandleName,
             TRUE
             );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  return AddUnicodeString2 (
           "en",
           gNvmeOfComponentName2.SupportedLanguages,
           &gNvmeOfControllerNameTable,
           HandleName,
           FALSE
           );
}


/**
  Retrieves a Unicode string that is the user readable name of the controller
  that is being managed by a driver.

  This function retrieves the user readable name of the controller specified by
  ControllerHandle and ChildHandle in the form of a Unicode string. If the
  driver specified by This has a user readable name in the language specified by
  Language, then a pointer to the controller name is returned in ControllerName,
  and EFI_SUCCESS is returned.  If the driver specified by This is not currently
  managing the controller specified by ControllerHandle and ChildHandle,
  then EFI_UNSUPPORTED is returned.  If the driver specified by This does not
  support the language specified by Language, then EFI_UNSUPPORTED is returned.

  @param[in]  This              A pointer to the EFI_COMPONENT_NAME2_PROTOCOL or
                                EFI_COMPONENT_NAME_PROTOCOL instance.

  @param[in]  ControllerHandle  The handle of a controller that the driver
                                specified by This is managing.  This handle
                                specifies the controller whose name is to be
                                returned.

  @param[in]  ChildHandle       The handle of the child controller to retrieve
                                the name of.  This is an optional parameter that
                                may be NULL.  It will be NULL for device
                                drivers.  It will also be NULL for a bus drivers
                                that wish to retrieve the name of the bus
                                controller.  It will not be NULL for a bus
                                driver that wishes to retrieve the name of a
                                child controller.

  @param[in]  Language          A pointer to a Null-terminated ASCII string
                                array indicating the language.  This is the
                                language of the driver name that the caller is
                                requesting, and it must match one of the
                                languages specified in SupportedLanguages. The
                                number of languages supported by a driver is up
                                to the driver writer. Language is specified in
                                RFC 4646 or ISO 639-2 language code format.

  @param[out]  ControllerName   A pointer to the Unicode string to return.
                                This Unicode string is the name of the
                                controller specified by ControllerHandle and
                                ChildHandle in the language specified by
                                Language, from the point of view of the driver
                                specified by This.

  @retval EFI_SUCCESS           The Unicode string for the user readable name in
                                the language specified by Language for the
                                driver specified by This was returned in
                                DriverName.

  @retval EFI_INVALID_PARAMETER ControllerHandle is NULL.

  @retval EFI_INVALID_PARAMETER ChildHandle is not NULL, and it is not a valid
                                EFI_HANDLE.

  @retval EFI_INVALID_PARAMETER Language is NULL.

  @retval EFI_INVALID_PARAMETER ControllerName is NULL.

  @retval EFI_UNSUPPORTED       The driver specified by This is not currently
                                managing the controller specified by
                                ControllerHandle and ChildHandle.

  @retval EFI_UNSUPPORTED       The driver specified by This does not support
                                the language specified by Language.

**/
EFI_STATUS
EFIAPI
NvmeOfComponentNameGetControllerName (
  IN  EFI_COMPONENT_NAME_PROTOCOL   *This,
  IN  EFI_HANDLE                    ControllerHandle,
  IN  EFI_HANDLE                    ChildHandle        OPTIONAL,
  IN  CHAR8                         *Language,
  OUT CHAR16                        **ControllerName
  )
{
  EFI_STATUS                      Status;

  EFI_HANDLE                      NvmeOfController;
  BOOLEAN                         Ipv6Flag;
  EFI_GUID                        *NvmeOfPrivateGuid;
  NVMEOF_PRIVATE_PROTOCOL         *NvmeOfIdentifier;

  if (ControllerHandle == NULL) {
    return EFI_UNSUPPORTED;
  }

  //
  // Get the handle of the controller we are controlling.
  //
  NvmeOfController = NetLibGetNicHandle (ControllerHandle, &gEfiTcp4ProtocolGuid);
  if (NvmeOfController != NULL) {
    NvmeOfPrivateGuid = &gNvmeOfV4PrivateGuid;
    Ipv6Flag = FALSE;
  } else {
    NvmeOfController = NetLibGetNicHandle (ControllerHandle, &gEfiTcp6ProtocolGuid);
    if (NvmeOfController != NULL) {
      NvmeOfPrivateGuid = &gNvmeOfV6PrivateGuid;
      Ipv6Flag = TRUE;
    } else {
      return EFI_UNSUPPORTED;
    }
  }

  Status = gBS->OpenProtocol (
                  NvmeOfController,
                  NvmeOfPrivateGuid,
                  (VOID **) &NvmeOfIdentifier,
                  NULL,
                  NULL,
                  EFI_OPEN_PROTOCOL_GET_PROTOCOL
                  );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  if(ChildHandle != NULL) {
    if(!Ipv6Flag) {
      //
      // Make sure this driver produced ChildHandle
      //
      Status = EfiTestChildHandle (
                 ControllerHandle,
                 ChildHandle,
                 &gEfiTcp4ProtocolGuid
                 );
      if (EFI_ERROR (Status)) {
        return Status;
      }
    } else {
      //
      // Make sure this driver produced ChildHandle
      //
      Status = EfiTestChildHandle (
                 ControllerHandle,
                 ChildHandle,
                 &gEfiTcp6ProtocolGuid
                 );
      if (EFI_ERROR (Status)) {
        return Status;
      }
    }

    //
    // Update the component name for this child handle.
    //
    Status = UpdateName (Ipv6Flag);
    if (EFI_ERROR (Status)) {
      return Status;
    }
  }

  return LookupUnicodeString2 (
           Language,
           This->SupportedLanguages,
           gNvmeOfControllerNameTable,
           ControllerName,
           (BOOLEAN)(This == &gNvmeOfComponentName)
           );
}

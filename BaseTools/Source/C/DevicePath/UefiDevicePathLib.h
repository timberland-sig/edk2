/** @file
  Definition for Device Path library.

Copyright (c) 2017, Intel Corporation. All rights reserved.<BR>
SPDX-License-Identifier: BSD-2-Clause-Patent

**/
#ifndef _UEFI_DEVICE_PATH_LIB_H_
#define _UEFI_DEVICE_PATH_LIB_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#ifdef __GNUC__
#include <unistd.h>
#else
#include <direct.h>
#endif
#include <Common/UefiBaseTypes.h>
#include <Protocol/DevicePath.h>
#include <Protocol/DevicePathUtilities.h>
#include "CommonLib.h"
#include "EfiUtilityMsgs.h"

#define END_DEVICE_PATH_LENGTH               (sizeof (EFI_DEVICE_PATH_PROTOCOL))
#define MAX_DEVICE_PATH_NODE_COUNT   1024
#define SIZE_64KB   0x00010000

//
// Private Data structure
//
typedef
EFI_DEVICE_PATH_PROTOCOL  *
(*DEVICE_PATH_FROM_TEXT) (
  IN  CHAR16 *Str
 );

typedef struct {
  CHAR16  *Str;
  UINTN   Count;
  UINTN   Capacity;
} POOL_PRINT;


typedef struct {
  CHAR16                    *DevicePathNodeText;
  DEVICE_PATH_FROM_TEXT     Function;
} DEVICE_PATH_FROM_TEXT_TABLE;

typedef struct {
  BOOLEAN ClassExist;
  UINT8   Class;
  BOOLEAN SubClassExist;
  UINT8   SubClass;
} USB_CLASS_TEXT;

#define USB_CLASS_AUDIO            1
#define USB_CLASS_CDCCONTROL       2
#define USB_CLASS_HID              3
#define USB_CLASS_IMAGE            6
#define USB_CLASS_PRINTER          7
#define USB_CLASS_MASS_STORAGE     8
#define USB_CLASS_HUB              9
#define USB_CLASS_CDCDATA          10
#define USB_CLASS_SMART_CARD       11
#define USB_CLASS_VIDEO            14
#define USB_CLASS_DIAGNOSTIC       220
#define USB_CLASS_WIRELESS         224

#define USB_CLASS_RESERVE          254
#define USB_SUBCLASS_FW_UPDATE     1
#define USB_SUBCLASS_IRDA_BRIDGE   2
#define USB_SUBCLASS_TEST          3

#define RFC_1700_UDP_PROTOCOL      17
#define RFC_1700_TCP_PROTOCOL      6

#pragma pack(1)

typedef struct {
  EFI_DEVICE_PATH_PROTOCOL  Header;
  EFI_GUID                  Guid;
  UINT8                     VendorDefinedData[1];
} VENDOR_DEFINED_HARDWARE_DEVICE_PATH;

typedef struct {
  EFI_DEVICE_PATH_PROTOCOL  Header;
  EFI_GUID                  Guid;
  UINT8                     VendorDefinedData[1];
} VENDOR_DEFINED_MESSAGING_DEVICE_PATH;

typedef struct {
  EFI_DEVICE_PATH_PROTOCOL  Header;
  EFI_GUID                  Guid;
  UINT8                     VendorDefinedData[1];
} VENDOR_DEFINED_MEDIA_DEVICE_PATH;

typedef struct {
  EFI_DEVICE_PATH_PROTOCOL  Header;
  UINT32                    Hid;
  UINT32                    Uid;
  UINT32                    Cid;
  CHAR8                     HidUidCidStr[3];
} ACPI_EXTENDED_HID_DEVICE_PATH_WITH_STR;

typedef struct {
  EFI_DEVICE_PATH_PROTOCOL  Header;
  UINT16                    NetworkProtocol;
  UINT16                    LoginOption;
  UINT64                    Lun;
  UINT16                    TargetPortalGroupTag;
  CHAR8                     TargetName[1];
} ISCSI_DEVICE_PATH_WITH_NAME;

typedef struct {
  EFI_DEVICE_PATH_PROTOCOL  Header;
  UINT8                     Nid[16];
  CHAR8                     TargetName[1];
} NVMEOF_DEVICE_PATH_WITH_NAME;

typedef struct {
  EFI_DEVICE_PATH_PROTOCOL  Header;
  EFI_GUID                  Guid;
  UINT8                     VendorDefinedData[1];
} VENDOR_DEVICE_PATH_WITH_DATA;

#pragma pack()

/**
  Returns the size of a device path in bytes.

  This function returns the size, in bytes, of the device path data structure
  specified by DevicePath including the end of device path node.
  If DevicePath is NULL or invalid, then 0 is returned.

  @param  DevicePath  A pointer to a device path data structure.

  @retval 0           If DevicePath is NULL or invalid.
  @retval Others      The size of a device path in bytes.

**/
UINTN
UefiDevicePathLibGetDevicePathSize (
   CONST EFI_DEVICE_PATH_PROTOCOL  *DevicePath
  );

/**
  Creates a new copy of an existing device path.

  This function allocates space for a new copy of the device path specified by DevicePath.
  If DevicePath is NULL, then NULL is returned.  If the memory is successfully
  allocated, then the contents of DevicePath are copied to the newly allocated
  buffer, and a pointer to that buffer is returned.  Otherwise, NULL is returned.
  The memory for the new device path is allocated from EFI boot services memory.
  It is the responsibility of the caller to free the memory allocated.

  @param  DevicePath    A pointer to a device path data structure.

  @retval NULL          DevicePath is NULL or invalid.
  @retval Others        A pointer to the duplicated device path.

**/
EFI_DEVICE_PATH_PROTOCOL *
UefiDevicePathLibDuplicateDevicePath (
   CONST EFI_DEVICE_PATH_PROTOCOL  *DevicePath
  );

/**
  Creates a new device path by appending a second device path to a first device path.

  This function creates a new device path by appending a copy of SecondDevicePath
  to a copy of FirstDevicePath in a newly allocated buffer.  Only the end-of-device-path
  device node from SecondDevicePath is retained. The newly created device path is
  returned. If FirstDevicePath is NULL, then it is ignored, and a duplicate of
  SecondDevicePath is returned.  If SecondDevicePath is NULL, then it is ignored,
  and a duplicate of FirstDevicePath is returned. If both FirstDevicePath and
  SecondDevicePath are NULL, then a copy of an end-of-device-path is returned.

  If there is not enough memory for the newly allocated buffer, then NULL is returned.
  The memory for the new device path is allocated from EFI boot services memory.
  It is the responsibility of the caller to free the memory allocated.

  @param  FirstDevicePath            A pointer to a device path data structure.
  @param  SecondDevicePath           A pointer to a device path data structure.

  @retval NULL      If there is not enough memory for the newly allocated buffer.
  @retval NULL      If FirstDevicePath or SecondDevicePath is invalid.
  @retval Others    A pointer to the new device path if success.
                    Or a copy an end-of-device-path if both FirstDevicePath and SecondDevicePath are NULL.

**/
EFI_DEVICE_PATH_PROTOCOL *
UefiDevicePathLibAppendDevicePath (
   CONST EFI_DEVICE_PATH_PROTOCOL  *FirstDevicePath,  OPTIONAL
   CONST EFI_DEVICE_PATH_PROTOCOL  *SecondDevicePath  OPTIONAL
  );

/**
  Creates a new path by appending the device node to the device path.

  This function creates a new device path by appending a copy of the device node
  specified by DevicePathNode to a copy of the device path specified by DevicePath
  in an allocated buffer. The end-of-device-path device node is moved after the
  end of the appended device node.
  If DevicePathNode is NULL then a copy of DevicePath is returned.
  If DevicePath is NULL then a copy of DevicePathNode, followed by an end-of-device
  path device node is returned.
  If both DevicePathNode and DevicePath are NULL then a copy of an end-of-device-path
  device node is returned.
  If there is not enough memory to allocate space for the new device path, then
  NULL is returned.
  The memory is allocated from EFI boot services memory. It is the responsibility
  of the caller to free the memory allocated.

  @param  DevicePath                 A pointer to a device path data structure.
  @param  DevicePathNode             A pointer to a single device path node.

  @retval NULL      If there is not enough memory for the new device path.
  @retval Others    A pointer to the new device path if success.
                    A copy of DevicePathNode followed by an end-of-device-path node
                    if both FirstDevicePath and SecondDevicePath are NULL.
                    A copy of an end-of-device-path node if both FirstDevicePath
                    and SecondDevicePath are NULL.

**/
EFI_DEVICE_PATH_PROTOCOL *
UefiDevicePathLibAppendDevicePathNode (
   CONST EFI_DEVICE_PATH_PROTOCOL  *DevicePath,     OPTIONAL
   CONST EFI_DEVICE_PATH_PROTOCOL  *DevicePathNode  OPTIONAL
  );

/**
  Creates a new device path by appending the specified device path instance to the specified device
  path.

  This function creates a new device path by appending a copy of the device path
  instance specified by DevicePathInstance to a copy of the device path specified
  by DevicePath in a allocated buffer.
  The end-of-device-path device node is moved after the end of the appended device
  path instance and a new end-of-device-path-instance node is inserted between.
  If DevicePath is NULL, then a copy if DevicePathInstance is returned.
  If DevicePathInstance is NULL, then NULL is returned.
  If DevicePath or DevicePathInstance is invalid, then NULL is returned.
  If there is not enough memory to allocate space for the new device path, then
  NULL is returned.
  The memory is allocated from EFI boot services memory. It is the responsibility
  of the caller to free the memory allocated.

  @param  DevicePath                 A pointer to a device path data structure.
  @param  DevicePathInstance         A pointer to a device path instance.

  @return A pointer to the new device path.

**/
EFI_DEVICE_PATH_PROTOCOL *
UefiDevicePathLibAppendDevicePathInstance (
   CONST EFI_DEVICE_PATH_PROTOCOL  *DevicePath,        OPTIONAL
   CONST EFI_DEVICE_PATH_PROTOCOL  *DevicePathInstance OPTIONAL
  );

/**
  Creates a copy of the current device path instance and returns a pointer to the next device path
  instance.

  This function creates a copy of the current device path instance. It also updates
  DevicePath to point to the next device path instance in the device path (or NULL
  if no more) and updates Size to hold the size of the device path instance copy.
  If DevicePath is NULL, then NULL is returned.
  If DevicePath points to a invalid device path, then NULL is returned.
  If there is not enough memory to allocate space for the new device path, then
  NULL is returned.
  The memory is allocated from EFI boot services memory. It is the responsibility
  of the caller to free the memory allocated.
  If Size is NULL, then ASSERT().

  @param  DevicePath                 On input, this holds the pointer to the current
                                     device path instance. On output, this holds
                                     the pointer to the next device path instance
                                     or NULL if there are no more device path
                                     instances in the device path pointer to a
                                     device path data structure.
  @param  Size                       On output, this holds the size of the device
                                     path instance, in bytes or zero, if DevicePath
                                     is NULL.

  @return A pointer to the current device path instance.

**/
EFI_DEVICE_PATH_PROTOCOL *
UefiDevicePathLibGetNextDevicePathInstance (
   EFI_DEVICE_PATH_PROTOCOL    **DevicePath,
   UINTN                          *Size
  );

/**
  Creates a device node.

  This function creates a new device node in a newly allocated buffer of size
  NodeLength and initializes the device path node header with NodeType and NodeSubType.
  The new device path node is returned.
  If NodeLength is smaller than a device path header, then NULL is returned.
  If there is not enough memory to allocate space for the new device path, then
  NULL is returned.
  The memory is allocated from EFI boot services memory. It is the responsibility
  of the caller to free the memory allocated.

  @param  NodeType                   The device node type for the new device node.
  @param  NodeSubType                The device node sub-type for the new device node.
  @param  NodeLength                 The length of the new device node.

  @return The new device path.

**/
EFI_DEVICE_PATH_PROTOCOL *
UefiDevicePathLibCreateDeviceNode (
   UINT8                           NodeType,
   UINT8                           NodeSubType,
   UINT16                          NodeLength
  );

/**
  Determines if a device path is single or multi-instance.

  This function returns TRUE if the device path specified by DevicePath is
  multi-instance.
  Otherwise, FALSE is returned.
  If DevicePath is NULL or invalid, then FALSE is returned.

  @param  DevicePath                 A pointer to a device path data structure.

  @retval  TRUE                      DevicePath is multi-instance.
  @retval  FALSE                     DevicePath is not multi-instance, or DevicePath
                                     is NULL or invalid.

**/
BOOLEAN
UefiDevicePathLibIsDevicePathMultiInstance (
   CONST EFI_DEVICE_PATH_PROTOCOL  *DevicePath
  );

/**
  Convert text to the binary representation of a device node.

  @param TextDeviceNode  TextDeviceNode points to the text representation of a device
                         node. Conversion starts with the first character and continues
                         until the first non-device node character.

  @return A pointer to the EFI device node or NULL if TextDeviceNode is NULL or there was
          insufficient memory or text unsupported.

**/
EFI_DEVICE_PATH_PROTOCOL *
UefiDevicePathLibConvertTextToDeviceNode (
   CONST CHAR16 *TextDeviceNode
  );

/**
  Convert text to the binary representation of a device path.


  @param TextDevicePath  TextDevicePath points to the text representation of a device
                         path. Conversion starts with the first character and continues
                         until the first non-device node character.

  @return A pointer to the allocated device path or NULL if TextDeviceNode is NULL or
          there was insufficient memory.

**/
EFI_DEVICE_PATH_PROTOCOL *
UefiDevicePathLibConvertTextToDevicePath (
   CONST CHAR16 *TextDevicePath
  );

EFI_DEVICE_PATH_PROTOCOL *
CreateDeviceNode (
   UINT8                           NodeType,
   UINT8                           NodeSubType,
   UINT16                          NodeLength
  );

BOOLEAN
IsDevicePathMultiInstance (
   CONST EFI_DEVICE_PATH_PROTOCOL  *DevicePath
  );

EFI_DEVICE_PATH_PROTOCOL *
GetNextDevicePathInstance (
    EFI_DEVICE_PATH_PROTOCOL    **DevicePath,
   UINTN                          *Size
  );

EFI_DEVICE_PATH_PROTOCOL *
AppendDevicePathInstance (
   CONST EFI_DEVICE_PATH_PROTOCOL  *DevicePath,        OPTIONAL
   CONST EFI_DEVICE_PATH_PROTOCOL  *DevicePathInstance OPTIONAL
  );

EFI_DEVICE_PATH_PROTOCOL *
AppendDevicePathNode (
   CONST EFI_DEVICE_PATH_PROTOCOL  *DevicePath,     OPTIONAL
   CONST EFI_DEVICE_PATH_PROTOCOL  *DevicePathNode  OPTIONAL
  );
EFI_DEVICE_PATH_PROTOCOL *
AppendDevicePath (
   CONST EFI_DEVICE_PATH_PROTOCOL  *FirstDevicePath,  OPTIONAL
   CONST EFI_DEVICE_PATH_PROTOCOL  *SecondDevicePath  OPTIONAL
  );

EFI_DEVICE_PATH_PROTOCOL *
DuplicateDevicePath (
   CONST EFI_DEVICE_PATH_PROTOCOL  *DevicePath
  );

UINTN
GetDevicePathSize (
   CONST EFI_DEVICE_PATH_PROTOCOL  *DevicePath
  );

CHAR16 *
ConvertDeviceNodeToText (
   CONST EFI_DEVICE_PATH_PROTOCOL  *DeviceNode,
   BOOLEAN                         DisplayOnly,
   BOOLEAN                         AllowShortcuts
  );

CHAR16 *
ConvertDevicePathToText (
   CONST EFI_DEVICE_PATH_PROTOCOL   *DevicePath,
   BOOLEAN                          DisplayOnly,
   BOOLEAN                          AllowShortcuts
  );

EFI_DEVICE_PATH_PROTOCOL *
ConvertTextToDeviceNode (
   CONST CHAR16 *TextDeviceNode
  );

EFI_DEVICE_PATH_PROTOCOL *
ConvertTextToDevicePath (
   CONST CHAR16 *TextDevicePath
  );

#endif

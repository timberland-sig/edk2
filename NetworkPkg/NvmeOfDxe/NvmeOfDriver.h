/** @file
  NvmeOfDxe driver is used to manage non-volatile memory subsystem which follows
  NVM Express Over Fabric TCP specification.

  Copyright (c) 2020, Dell EMC All rights reserved
  Copyright (c) 2022, Intel Corporation. All rights reserved.
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef _NVMEOF_DRIVER_H_
#define _NVMEOF_DRIVER_H_

#include <Uefi.h>
#include <IndustryStandard/Nvme.h>
#include <Protocol/ComponentName.h>
#include <Protocol/ComponentName2.h>
#include <Protocol/DriverBinding.h>
#include <Protocol/LoadedImage.h>
#include <Protocol/DevicePath.h>
#include <Protocol/PciIo.h>
#include <Protocol/DriverSupportedEfiVersion.h>
#include <Protocol/DevicePath.h>
#include <Protocol/BlockIo.h>
#include <Protocol/BlockIo2.h>
#include <Protocol/DiskInfo.h>
#include <Protocol/StorageSecurityCommand.h>

#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/PrintLib.h>
#include <Library/UefiLib.h>
#include <Library/DevicePathLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiDriverEntryPoint.h>
#include <Library/ReportStatusCodeLib.h>
#include <Library/TcpIoLib.h>

#include <Shared/NvmeOfNvData.h>

#include <edk_sock.h>

#define SKIP_KEEP_AIVE_COUNTER  3

#define NVMEOF_V4_PRIVATE_GUID \
  { \
    0x2c0c6c78, 0x45e2, 0x11eb, { 0xb7, 0x50, 0xdf, 0xed, 0xa1, 0x7d, 0x89, 0x14} \
  }

#define NVMEOF_V6_PRIVATE_GUID \
  { \
    0x2f886a63, 0xe8cf, 0x4788, { 0xa5, 0x75, 0x50, 0x32, 0xed, 0x42, 0x3f, 0x5f} \
  }
#define NVMEOF_INITIATOR_NAME_VAR_NAME L"N_NAME"

// Device Path defines
#define  NET_IFTYPE_ETHERNET   0x01

#define NVMEOF_PROTOCOL_GUID_STRING_LEN   100

// DHCP/DNS defines
#define NVMEOF_CHECK_MEDIA_GET_DHCP_WAITING_TIME    EFI_TIMER_PERIOD_SECONDS(20)
#define NVMEOF_GET_MAPPING_TIMEOUT                  100000000U

#define NVMEOF_DRIVER_DATA_SIGNATURE SIGNATURE_32 ('N', 'E', 'O', 'F')
#define NVMEOF_DRIVER_VERSION 10 //Indicate 1.0
//
// Nvme async transfer timer interval, set by experience.
//
#define NVMEOF_HC_ASYNC_TIMER                       EFI_TIMER_PERIOD_MILLISECONDS (1)

//
// Kato timer interval
//
#define NVMEOF_KATO_TIMER                           EFI_TIMER_PERIOD_MILLISECONDS (60000)
#define NVMEOF_KATO_TIMOUT                          EFI_TIMER_PERIOD_MILLISECONDS (70000)

extern void
nvmeof_read_complete ();
//
// Statments to retrieve the private data from produced protocols.
//

#define NVMEOF_DEVICE_PRIVATE_DATA_FROM_BLOCK_IO2(a) \
  CR (a, \
      NVMEOF_DEVICE_PRIVATE_DATA, \
      BlockIo2, \
      NVMEOF_DEVICE_PRIVATE_DATA_SIGNATURE \
      )

#define NVMEOF_DEVICE_PRIVATE_DATA_FROM_BLOCK_IO(a) \
  CR (a, \
      NVMEOF_DEVICE_PRIVATE_DATA, \
      BlockIo, \
      NVMEOF_DEVICE_PRIVATE_DATA_SIGNATURE \
      )

#define NVMEOF_DEVICE_PRIVATE_DATA_FROM_STORAGE_SECURITY(a)\
  CR (a,                                                 \
      NVMEOF_DEVICE_PRIVATE_DATA,                        \
      StorageSecurity,                                   \
      NVMEOF_DEVICE_PRIVATE_DATA_SIGNATURE               \
      )

//
// NvmeOf block I/O 2 request.
//
#define NVMEOF_BLKIO2_REQUEST_SIGNATURE      SIGNATURE_32 ('N', 'O', '2', 'R')

typedef struct _NVMEOF_BLKIO2_REQUEST {
  UINT32                                   Signature;
  LIST_ENTRY                               Link;
  EFI_BLOCK_IO2_TOKEN                      *Token;
  UINTN                                    UnsubmittedSubtaskNum;
  BOOLEAN                                  LastSubtaskSubmitted;
  //
  // The queue for NvmeOf read/write sub-tasks of a BlockIo2 request.
  //
  LIST_ENTRY                               SubtasksQueue;
} NVMEOF_BLKIO2_REQUEST;

#define NVMEOF_BLKIO2_REQUEST_FROM_LINK(a) \
  CR (a, NVME_BLKIO2_REQUEST, Link, NVME_BLKIO2_REQUEST_SIGNATURE)

#define NVMEOF_BLKIO2_SUBTASK_SIGNATURE      SIGNATURE_32 ('N', 'O', '2', 'S')

typedef struct _NVMEOF_DEVICE_PRIVATE_DATA     NVMEOF_DEVICE_PRIVATE_DATA;
struct _NVMEOF_ASYNC_CMD_DATA {
  UINT64                             Buffer;
  UINT64                             Lba;
  UINT32                             Blocks;
  UINT8                              IsCompleted;
  BOOLEAN                            IsRead;
  NVMEOF_DEVICE_PRIVATE_DATA         *Device;
};

typedef struct _NVMEOF_ASYNC_CMD_DATA NVMEOF_ASYNC_CMD_DATA;

typedef struct _NVMEOF_BLKIO2_SUBTASK {
  UINT32                                   Signature;
  LIST_ENTRY                               Link;
  BOOLEAN                                  IsLast;
  UINT32                                   NamespaceId;
  EFI_EVENT                                Event;
  NVMEOF_ASYNC_CMD_DATA                    *NvmeOfAsyncData;
  //
  // The BlockIo2 request this subtask belongs to
  //
  NVMEOF_BLKIO2_REQUEST                    *BlockIo2Request;
} NVMEOF_BLKIO2_SUBTASK;

#define NVMEOF_BLKIO2_SUBTASK_FROM_LINK(a) \
  CR (a, NVMEOF_BLKIO2_SUBTASK, Link, NVMEOF_BLKIO2_SUBTASK_SIGNATURE)


typedef struct _NVMEOF_PRIVATE_PROTOCOL {
    UINT32  Reserved;
} NVMEOF_PRIVATE_PROTOCOL;

typedef struct _NVME_CONTROLLER_PRIVATE_DATA  NVME_CONTROLLER_PRIVATE_DATA;
typedef struct _NVME_DEVICE_PRIVATE_DATA      NVME_DEVICE_PRIVATE_DATA;
typedef struct _NVMEOF_ATTEMPT_ENTRY          NVMEOF_ATTEMPT_ENTRY;

extern NVMEOF_PRIVATE_PROTOCOL          NVMEOF_Identifier;
extern BOOLEAN                          gDriverInRuntime;

#define NVMEOF_DISCOVERY_NQN "nqn.2014-08.org.nvmexpress.discovery"
typedef struct _NVMEOF_DRIVER_DATA {
  UINT32                          Signature;
  EFI_HANDLE                      Image;
  EFI_HANDLE                      Controller;
  NVMEOF_PRIVATE_PROTOCOL         NvmeOfIdentifier;
  EFI_EVENT                       ExitBootServiceEvent;
  EFI_HANDLE                      ChildHandle;
  TCP_IO                          *TcpIo;
  BOOLEAN                         IsDiscoveryNqn;
  EFI_HANDLE                      NvmeOfProtocolHandle;
  LIST_ENTRY                      UnsubmittedSubtasks;
  EFI_EVENT                       TimerEvent;
  NVMEOF_ATTEMPT_ENTRY            *Attempt;
  BOOLEAN                         IsStopping;
} NVMEOF_DRIVER_DATA;

#define NVMEOF_DEVICE_PRIVATE_DATA_SIGNATURE     SIGNATURE_32 ('N','V','O','F')

typedef struct _NVMEOF_DEVICE_PRIVATE_DATA {
  UINT32                                Signature;
  UINT32                                NamespaceId;
  const struct spdk_uuid                *NamespaceUuid;
  struct spdk_nvme_ns                   *NameSpace;
  struct spdk_nvme_qpair                *qpair;
  EFI_BLOCK_IO_MEDIA                    Media;
  EFI_BLOCK_IO_PROTOCOL                 BlockIo;
  EFI_BLOCK_IO2_PROTOCOL                BlockIo2;
  EFI_DISK_INFO_PROTOCOL                DiskInfo;
  EFI_DEVICE_PATH_PROTOCOL              *DevicePath;
  EFI_STORAGE_SECURITY_COMMAND_PROTOCOL StorageSecurity;
  EFI_HANDLE                            DeviceHandle;
  EFI_HANDLE                            ControllerHandle;
  NVMEOF_DRIVER_DATA                    *Controller;
  CHAR16                                ModelName[100];
  EFI_UNICODE_STRING_TABLE              *ControllerNameTable;
  LIST_ENTRY                            AsyncQueue;
  EFI_LBA                               NumBlocks;
  TCP_IO                                *TcpIo;
  UINT16                                Asqsz;
  UINT8                                 NamespaceIdType;
} NVMEOF_DEVICE_PRIVATE_DATA;

#define NVMEOF_DEVICE_PRIVATE_DATA_FROM_DISK_INFO(a) \
  CR (a, \
      NVMEOF_DEVICE_PRIVATE_DATA, \
      DiskInfo, \
      NVMEOF_DEVICE_PRIVATE_DATA_SIGNATURE \
      )

extern EFI_GUID  gNvmeOfV4PrivateGuid; 
extern EFI_GUID  gNvmeOfV6PrivateGuid;
extern EFI_DRIVER_BINDING_PROTOCOL                gNvmeOfDriverBinding;
extern EFI_COMPONENT_NAME_PROTOCOL                gNvmeOfComponentName;
extern EFI_COMPONENT_NAME2_PROTOCOL               gNvmeOfComponentName2;
extern EFI_DRIVER_SUPPORTED_EFI_VERSION_PROTOCOL  gNvmeOfDriverSupportedEfiVersion;

#define NVMEOF_NAME_IFR_MIN_SIZE   4
#define NVMEOF_NAME_IFR_MAX_SIZE   223


typedef struct {
  CHAR16          PortString[NVMEOF_NAME_IFR_MAX_SIZE];
  LIST_ENTRY      NicInfoList;
  UINT8           NicCount;
  UINT8           CurrentNic;
  UINT8           MaxNic;
  BOOLEAN         Ipv6Flag;
  UINT8           AttemptCount;
  LIST_ENTRY      AttemptConfigs;    // User configured Attempt list.
  UINT8           ProcessedAttemptCount;
  LIST_ENTRY      ProcessedAttempts; // Processed Attempt list.
  CHAR8           InitiatorName[NVMEOF_NAME_MAX_SIZE];
  UINTN           InitiatorNameLength;
} NVMEOF_NIC_PRIVATE_DATA;
extern NVMEOF_NIC_PRIVATE_DATA        *mNicPrivate;

typedef struct _NVMEOF_CONTROLLER_DATA_ {
  struct spdk_nvme_ctrlr *ctrlr;
  EFI_HANDLE              NicHandle;
  LIST_ENTRY             Link;
  UINT8                  KeepAliveErrorCounter;
} NVMEOF_CONTROLLER_DATA;

typedef struct _NVMEOF_DEVICE_DATA_ {
  NVMEOF_DEVICE_PRIVATE_DATA         *Device;
  LIST_ENTRY                         deviceDataList;
} NVMEOF_DEVICE_DATA;


extern LIST_ENTRY   gNvmeOfControllerList;

#define IP_MODE_IP4               0
#define IP_MODE_IP6               1
#define IP_MODE_AUTOCONFIG        2
#define IP_MODE_AUTOCONFIG_IP4    3
#define IP_MODE_AUTOCONFIG_IP6    4

typedef struct {
  LIST_ENTRY      Link;
  UINT32          HwAddressSize;
  EFI_MAC_ADDRESS PermanentAddress;
  UINT8           NicIndex;
  UINT16          VlanId;
  UINTN           BusNumber;
  UINTN           DeviceNumber;
  UINTN           FunctionNumber;
  BOOLEAN         Ipv6Available;
} NVMEOF_NIC_INFO;

typedef struct _NVMEOF_ATTEMPT_ENTRY {
  LIST_ENTRY                      Link;
  NVMEOF_ATTEMPT_CONFIG_NVDATA    Data;
  struct spdk_edk_sock_ctx        SocketContext;
} NVMEOF_ATTEMPT_ENTRY;

#define MAX_SUBSYSTEMS_SUPPORTED  8
#define NID_MAX                   32
typedef struct _NVMEOF_NQN_NID {
  CHAR8       SubsystemNqn[NVMEOF_NAME_MAX_SIZE];
  UINT8       Nid[NID_MAX][NVMEOF_NID_LEN];
  UINT8       Nsid[NID_MAX];
  UINT8       NamespaceCount;
} NVMEOF_NQN_NID;
extern NVMEOF_NQN_NID gNvmeOfNqnNidMap[];
extern UINT8 NqnNidMapINdex;

typedef struct _NVMEOF_NBFT {
  UINT8                         DeviceAdapterIndex;
  BOOLEAN                       IsDiscoveryNqn;
  NVMEOF_DEVICE_PRIVATE_DATA    *Device;
  NVMEOF_ATTEMPT_CONFIG_NVDATA  *AttemptData;
} NVMEOF_NBFT;
extern NVMEOF_NBFT gNvmeOfNbftList[];
extern UINT8 gNvmeOfNbftListIndex;
extern CHAR8 *gNvmeOfRootPath;

/**
  Callback function for Async IO tasks timer

  @param[in]  Event     The Event this notify function registered to.
  @param[in]  Context   Pointer to the context data registered to the
                        Event.

**/
VOID
EFIAPI
ProcessAsyncTaskList (
  IN EFI_EVENT                    Event,
  IN VOID*                        Context
  );
/**
  Retrieves a Unicode string that is the user readable name of the driver.

  This function retrieves the user readable name of a driver in the form of a
  Unicode string. If the driver specified by This has a user readable name in
  the language specified by Language, then a pointer to the driver name is
  returned in DriverName, and EFI_SUCCESS is returned. If the driver specified
  by This does not support the language specified by Language,
  then EFI_UNSUPPORTED is returned.

  @param  This[in]              A pointer to the EFI_COMPONENT_NAME2_PROTOCOL or
                                EFI_COMPONENT_NAME_PROTOCOL instance.

  @param  Language[in]          A pointer to a Null-terminated ASCII string
                                array indicating the language. This is the
                                language of the driver name that the caller is
                                requesting, and it must match one of the
                                languages specified in SupportedLanguages. The
                                number of languages supported by a driver is up
                                to the driver writer. Language is specified
                                in RFC 4646 or ISO 639-2 language code format.

  @param  DriverName[out]       A pointer to the Unicode string to return.
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
  IN  EFI_COMPONENT_NAME_PROTOCOL  *This,
  IN  CHAR8                        *Language,
  OUT CHAR16                       **DriverName
  );

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

  @param  This[in]              A pointer to the EFI_COMPONENT_NAME2_PROTOCOL or
                                EFI_COMPONENT_NAME_PROTOCOL instance.

  @param  ControllerHandle[in]  The handle of a controller that the driver
                                specified by This is managing.  This handle
                                specifies the controller whose name is to be
                                returned.

  @param  ChildHandle[in]       The handle of the child controller to retrieve
                                the name of.  This is an optional parameter that
                                may be NULL.  It will be NULL for device
                                drivers.  It will also be NULL for a bus drivers
                                that wish to retrieve the name of the bus
                                controller.  It will not be NULL for a bus
                                driver that wishes to retrieve the name of a
                                child controller.

  @param  Language[in]          A pointer to a Null-terminated ASCII string
                                array indicating the language.  This is the
                                language of the driver name that the caller is
                                requesting, and it must match one of the
                                languages specified in SupportedLanguages. The
                                number of languages supported by a driver is up
                                to the driver writer. Language is specified in
                                RFC 4646 or ISO 639-2 language code format.

  @param  ControllerName[out]   A pointer to the Unicode string to return.
                                This Unicode string is the name of the
                                controller specified by ControllerHandle and
                                ChildHandle in the language specified by
                                Language from the point of view of the driver
                                specified by This.

  @retval EFI_SUCCESS           The Unicode string for the user readable name in
                                the language specified by Language for the
                                driver specified by This was returned in
                                DriverName.

  @retval EFI_INVALID_PARAMETER ControllerHandle is NULL.

  @retval EFI_INVALID_PARAMETER ChildHandle is not NULL and it is not a valid
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
  IN  EFI_COMPONENT_NAME_PROTOCOL           *This,
  IN  EFI_HANDLE                            ControllerHandle,
  IN  EFI_HANDLE                            ChildHandle        OPTIONAL,
  IN  CHAR8                                 *Language,
  OUT CHAR16                                **ControllerName
  );

/**
  Tests to see if this driver supports a given controller. If a child device is provided,
  it further tests to see if this driver supports creating a handle for the specified child device.

  This function checks to see if the driver specified by This supports the device specified by
  ControllerHandle. Drivers will typically use the device path attached to
  ControllerHandle and/or the services from the bus I/O abstraction attached to
  ControllerHandle to determine if the driver supports ControllerHandle. This function
  may be called many times during platform initialization. In order to reduce boot times, the tests
  performed by this function must be very small, and take as little time as possible to execute. This
  function must not change the state of any hardware devices, and this function must be aware that the
  device specified by ControllerHandle may already be managed by the same driver or a
  different driver. This function must match its calls to AllocatePages() with FreePages(),
  AllocatePool() with FreePool(), and OpenProtocol() with CloseProtocol().
  Since ControllerHandle may have been previously started by the same driver, if a protocol is
  already in the opened state, then it must not be closed with CloseProtocol(). This is required
  to guarantee the state of ControllerHandle is not modified by this function.

  @param[in]  This                 A pointer to the EFI_DRIVER_BINDING_PROTOCOL instance.
  @param[in]  ControllerHandle     The handle of the controller to test. This handle
                                   must support a protocol interface that supplies
                                   an I/O abstraction to the driver.
  @param[in]  RemainingDevicePath  A pointer to the remaining portion of a device path.  This
                                   parameter is ignored by device drivers, and is optional for bus
                                   drivers. For bus drivers, if this parameter is not NULL, then
                                   the bus driver must determine if the bus controller specified
                                   by ControllerHandle and the child controller specified
                                   by RemainingDevicePath are both supported by this
                                   bus driver.

  @retval EFI_SUCCESS              The device specified by ControllerHandle and
                                   RemainingDevicePath is supported by the driver specified by This.
  @retval EFI_ALREADY_STARTED      The device specified by ControllerHandle and
                                   RemainingDevicePath is already being managed by the driver
                                   specified by This.
  @retval EFI_ACCESS_DENIED        The device specified by ControllerHandle and
                                   RemainingDevicePath is already being managed by a different
                                   driver or an application that requires exclusive access.
                                   Currently not implemented.
  @retval EFI_UNSUPPORTED          The device specified by ControllerHandle and
                                   RemainingDevicePath is not supported by the driver specified by This.
**/
EFI_STATUS
EFIAPI
NvmeOfIp4DriverBindingSupported (
  IN EFI_DRIVER_BINDING_PROTOCOL  *This,
  IN EFI_HANDLE                   Controller,
  IN EFI_DEVICE_PATH_PROTOCOL     *RemainingDevicePath
  );

EFI_STATUS
EFIAPI
NvmeOfIp6DriverBindingSupported (
  IN EFI_DRIVER_BINDING_PROTOCOL  *This,
  IN EFI_HANDLE                   Controller,
  IN EFI_DEVICE_PATH_PROTOCOL     *RemainingDevicePath
  );
/**
  Starts a device controller or a bus controller.

  The Start() function is designed to be invoked from the EFI boot service ConnectController().
  As a result, much of the error checking on the parameters to Start() has been moved into this
  common boot service. It is legal to call Start() from other locations,
  but the following calling restrictions must be followed or the system behavior will not be deterministic.
  1. ControllerHandle must be a valid EFI_HANDLE.
  2. If RemainingDevicePath is not NULL, then it must be a pointer to a naturally aligned
     EFI_DEVICE_PATH_PROTOCOL.
  3. Prior to calling Start(), the Supported() function for the driver specified by This must
     have been called with the same calling parameters, and Supported() must have returned EFI_SUCCESS.

  @param[in]  This                 A pointer to the EFI_DRIVER_BINDING_PROTOCOL instance.
  @param[in]  ControllerHandle     The handle of the controller to start. This handle
                                   must support a protocol interface that supplies
                                   an I/O abstraction to the driver.
  @param[in]  RemainingDevicePath  A pointer to the remaining portion of a device path.  This
                                   parameter is ignored by device drivers, and is optional for bus
                                   drivers. For a bus driver, if this parameter is NULL, then handles
                                   for all the children of Controller are created by this driver.
                                   If this parameter is not NULL and the first Device Path Node is
                                   not the End of Device Path Node, then only the handle for the
                                   child device specified by the first Device Path Node of
                                   RemainingDevicePath is created by this driver.
                                   If the first Device Path Node of RemainingDevicePath is
                                   the End of Device Path Node, no child handle is created by this
                                   driver.

  @retval EFI_SUCCESS              The device was started.
  @retval EFI_DEVICE_ERROR         The device could not be started due to a device error.Currently not implemented.
  @retval EFI_OUT_OF_RESOURCES     The request could not be completed due to a lack of resources.
  @retval Others                   The driver failded to start the device.

**/
EFI_STATUS
EFIAPI
NvmeOfIp4DriverBindingStart (
  IN EFI_DRIVER_BINDING_PROTOCOL  *This,
  IN EFI_HANDLE                   Controller,
  IN EFI_DEVICE_PATH_PROTOCOL     *RemainingDevicePath
  );

EFI_STATUS
EFIAPI
NvmeOfIp6DriverBindingStart (
  IN EFI_DRIVER_BINDING_PROTOCOL  *This,
  IN EFI_HANDLE                   Controller,
  IN EFI_DEVICE_PATH_PROTOCOL     *RemainingDevicePath
  );
/**
  Stops a device controller or a bus controller.

  The Stop() function is designed to be invoked from the EFI boot service DisconnectController().
  As a result, much of the error checking on the parameters to Stop() has been moved
  into this common boot service. It is legal to call Stop() from other locations,
  but the following calling restrictions must be followed or the system behavior will not be deterministic.
  1. ControllerHandle must be a valid EFI_HANDLE that was used on a previous call to this
     same driver's Start() function.
  2. The first NumberOfChildren handles of ChildHandleBuffer must all be a valid
     EFI_HANDLE. In addition, all of these handles must have been created in this driver's
     Start() function, and the Start() function must have called OpenProtocol() on
     ControllerHandle with an Attribute of EFI_OPEN_PROTOCOL_BY_CHILD_CONTROLLER.

  @param[in]  This              A pointer to the EFI_DRIVER_BINDING_PROTOCOL instance.
  @param[in]  ControllerHandle  A handle to the device being stopped. The handle must
                                support a bus specific I/O protocol for the driver
                                to use to stop the device.
  @param[in]  NumberOfChildren  The number of child device handles in ChildHandleBuffer.
  @param[in]  ChildHandleBuffer An array of child handles to be freed. May be NULL
                                if NumberOfChildren is 0.

  @retval EFI_SUCCESS           The device was stopped.
  @retval EFI_DEVICE_ERROR      The device could not be stopped due to a device error.

**/
EFI_STATUS
EFIAPI
NvmeOfIp4DriverBindingStop (
  IN  EFI_DRIVER_BINDING_PROTOCOL     *This,
  IN  EFI_HANDLE                      Controller,
  IN  UINTN                           NumberOfChildren,
  IN  EFI_HANDLE                      *ChildHandleBuffer
  );

EFI_STATUS
EFIAPI
NvmeOfIp6DriverBindingStop (
  IN  EFI_DRIVER_BINDING_PROTOCOL     *This,
  IN  EFI_HANDLE                      Controller,
  IN  UINTN                           NumberOfChildren,
  IN  EFI_HANDLE                      *ChildHandleBuffer
  );

/**
  Unload the NVMeOF driver.

  @param[in]  ImageHandle          The handle of the driver image.

  @retval     EFI_SUCCESS          The driver is unloaded.
  @retval     EFI_DEVICE_ERROR     An unexpected error occurred.

**/
EFI_STATUS
EFIAPI
NvmeOfDriverUnload (
  IN EFI_HANDLE  ImageHandle
  );

/**
  Install the device specific protocols like bolck device,
  device path and device info

  @param[in]  Device                     The handle of the device.
  @param[in]  SecuritySendRecvSupported  Security suupported flag.

  @retval     EFI_SUCCESS          The device protocols installed successfully.
  @retval     EFI_DEVICE_ERROR     An unexpected error occurred.

**/
EFI_STATUS
NvmeOfInstallDeviceProtocols (
  NVMEOF_DEVICE_PRIVATE_DATA *Device,
  BOOLEAN                    SecuritySendRecvSupported
  );

#endif

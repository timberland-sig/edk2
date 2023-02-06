/** @file
  Miscellaneous definitions for NvmeOf driver.

  Copyright (c) 2020, Dell EMC All rights reserved
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef _NVMEOF_MISC_H_
#define _NVMEOF_MISC_H_

#include "spdk/nvme.h"

// IPv4 Device Path Node Length
#define IP4_NODE_LEN_NEW_VERSIONS    27

// IPv6 Device Path Node Length
#define IP6_NODE_LEN_OLD_VERSIONS    43
#define IP6_NODE_LEN_NEW_VERSIONS    60

// The ignored field StaticIpAddress's offset in old IPv6 Device Path
#define IP6_OLD_IPADDRESS_OFFSET      42

//NID offset in struct spdk_nvme_ns_id_desc
#define NID_OFFSET_IN_STRUCT            4

typedef struct _NVMEOF_DRIVER_DATA    NVMEOF_DRIVER_DATA;
typedef struct _NVMEOF_CONTROLLER_PRIVATE_DATA    NVMEOF_CONTROLLER_PRIVATE_DATA;
typedef struct _NVMEOF_NAMESPACE_DATA    NVMEOF_NAMESPACE_DATA;

/**
  Tests whether a controller handle is being managed by Nvme driver.

  This function tests whether the driver specified by DriverBindingHandle is
  currently managing the controller specified by ControllerHandle.  This test
  is performed by evaluating if the protocol specified by ProtocolGuid is
  present on ControllerHandle and is was opened by DriverBindingHandle and Nic
  Device handle with an attribute of EFI_OPEN_PROTOCOL_BY_DRIVER.
  If ProtocolGuid is NULL, then ASSERT().

  @param  ControllerHandle     A handle for a controller to test.
  @param  DriverBindingHandle  Specifies the driver binding handle for the
  driver.
  @param  ProtocolGuid         Specifies the protocol that the driver specified
  by DriverBindingHandle opens in its Start()
  function.

  @retval EFI_SUCCESS          ControllerHandle is managed by the driver
  specified by DriverBindingHandle.
  @retval EFI_UNSUPPORTED      ControllerHandle is not managed by the driver
  specified by DriverBindingHandle.

 **/
EFI_STATUS
EFIAPI
NvmeOfTestManagedDevice (
  IN  EFI_HANDLE       ControllerHandle,
  IN  EFI_HANDLE       DriverBindingHandle,
  IN  EFI_GUID         *ProtocolGuid
  );

/**
  Create the NvmeOf driver data.

  @param[in] Image      The handle of the driver image.
  @param[in] Controller The handle of the controller.

  @return The NvmeOf driver data created.
  @retval NULL Other errors as indicated.

 **/

NVMEOF_DRIVER_DATA *
NvmeOfCreateDriverData (
  IN EFI_HANDLE  Image,
  IN EFI_HANDLE  Controller
  );

/**
  Delete the recorded NIC info from global structure. Also delete corresponding
  attempts.

  @param[in]  Controller         The handle of the controller.

  @retval EFI_SUCCESS            The operation is completed.
  @retval EFI_NOT_FOUND          The NIC info to be deleted is not recorded.

 **/
EFI_STATUS
NvmeOfRemoveNic (
  IN EFI_HANDLE  Controller
  );


/**
  Check wheather the Controller handle is configured to use DHCP protocol.

  @param[in]  Controller           The handle of the controller.
  @param[in]  IpVersion            IP_VERSION_4 or IP_VERSION_6.
  @param[in]  AttemptConfigData    Pointer containing all attempts.
  @param[in]  TargetCount          Number of attempts.

  @retval TRUE                     The handle of the controller need the Dhcp protocol.
  @retval FALSE                    The handle of the controller does not need the Dhcp protocol.

**/
BOOLEAN
NvmeOfDhcpIsConfigured (
  IN EFI_HANDLE                     Controller,
  IN UINT8                          IpVersion,
  IN NVMEOF_ATTEMPT_CONFIG_NVDATA  *AttemptConfigData,
  IN UINT8                          TargetCount
  );

/**
  Check whether the Controller handle is configured to use DNS protocol.

  @param[in]  Controller           The handle of the controller.
  @param[in]  AttemptConfigData    Pointer containing all attempts.
  @param[in]  TargetCount          Number of attempts.

  @retval TRUE                     The handle of the controller need the Dns protocol.
  @retval FALSE                    The handle of the controller does not need the Dns protocol.

**/
BOOLEAN
NvmeOfDnsIsConfigured (
  IN EFI_HANDLE                     Controller,
  IN NVMEOF_ATTEMPT_CONFIG_NVDATA  *AttemptConfigData,
  IN UINT8                          TargetCount
  );


/**
  Record the NIC information in a global structure.

  @param[in]  Controller         The handle of the controller.
  @param[in]  Image              Handle of the image.

  @retval EFI_SUCCESS            The operation is completed.
  @retval EFI_OUT_OF_RESOURCES   Do not have sufficient resource to finish this
                                 operation.

**/
EFI_STATUS
NvmeOfSaveNic (
  IN EFI_HANDLE  Controller,
  IN EFI_HANDLE  Image
  );


/**
  Read the EFI variable (VendorGuid/Name) and return a dynamically allocated
  buffer, and the size of the buffer. If failure, return NULL.

  @param[in]   Name                   String part of EFI variable name.
  @param[in]   VendorGuid             GUID part of EFI variable name.
  @param[out]  VariableSize           Returns the size of the EFI variable that was read.

  @return Dynamically allocated memory that contains a copy of the EFI variable.
  @return Caller is responsible freeing the buffer.
  @retval NULL                   Variable was not read.

**/
VOID *
NvmeOfGetVariableAndSize (
  IN  CHAR16   *Name,
  IN  EFI_GUID *VendorGuid,
  OUT UINTN    *VariableSize
  );


/**
  Read the attempt configuration data from the UEFI variable.

  @retval EFI_NOT_FOUND - No config data in UEFI variable
  @retval EFI_OUT_OF_RESOURCES - Unable to allocate buffer to read data
  @retval EFI_SUCCESS - Read attempt data successfully
**/
EFI_STATUS
EFIAPI
NvmeOfReadConfigData (void);

/**
  Read the UEFI variable.

  @param [OUT] Contents          Contents read from UEFI variable
  @param [OUT] TargetCount       Size of contents read from UEFI variable

  @retval EFI_NOT_FOUND - No config data in UEFI variable
  @retval EFI_SUCCESS - Read attempt data successfully
**/
EFI_STATUS
EFIAPI
NvmeOfReadAttemptVariables (
  OUT VOID      **Contents,
  OUT UINT8     *TargetCount
  );

/**
  Convert the mac address into a hexadecimal encoded "-" separated string.

  @param[in]  Mac     The mac address.
  @param[in]  Len     Length in bytes of the mac address.
  @param[in]  VlanId  VLAN ID of the network device.
  @param[out] Str     The storage to return the mac string.

**/
VOID
NvmeOfMacAddrToStr (
  IN  EFI_MAC_ADDRESS  *Mac,
  IN  UINT32           Len,
  IN  UINT16           VlanId,
  OUT CHAR16           *Str
  );

/**
  Get the various configuration data.

  @param[in] Image      The handle of the driver image.
  @param[in] Controller The handle of the controller.
  @param[in/out]  AttemptConfigData   Attempt data.

  @retval EFI_SUCCESS            The configuration data is retrieved.
  @retval EFI_NOT_FOUND          This NVMeOF driver is not configured yet.
  @retval EFI_OUT_OF_RESOURCES   Failed to allocate memory.

**/
EFI_STATUS
NvmeOfGetConfigData (
  IN EFI_HANDLE  Image,
  IN EFI_HANDLE  Controller,
  IN OUT NVMEOF_ATTEMPT_CONFIG_NVDATA *AttemptConfigData
  );

/**
  Get the recorded NIC info from global structure by the Index.

  @param[in]  NicIndex          The index indicates the position of NIC info.

  @return Pointer to the NIC info, or NULL if not found.

**/
NVMEOF_NIC_INFO *
NvmeOfGetNicInfoByIndex (
  IN UINT8      NicIndex
  );
  
/**
  Convert the formatted IP address into the binary IP address.

  @param[in]   Str               The UNICODE string.
  @param[in]   IpMode            Indicates whether the IP address is v4 or v6.
  @param[out]  Ip                The storage to return the ASCII string.

  @retval EFI_SUCCESS            The binary IP address is returned in Ip.
  @retval EFI_INVALID_PARAMETER  The IP string is malformatted or IpMode is
                                 invalid.

**/
EFI_STATUS
NvmeOfAsciiStrToIp (
  IN  CHAR8             *Str,
  IN  UINT8             IpMode,
  OUT EFI_IP_ADDRESS    *Ip
  );

/**
Convert the hexadecimal encoded NID string into the 16 - byte NID.

@param[in]   Str             The hexadecimal encoded NID string.
@param[out]  NID             Storage to return the 16 byte NID.

@retval EFI_SUCCESS            The 16-byte NID is stored in NID.
@retval EFI_INVALID_PARAMETER  The string is malformatted.

**/
EFI_STATUS
NvmeOfAsciiStrToNid (
  IN  CHAR8  *Str,
  OUT CHAR8  *NID
  );

/**
Get the attempt for NIC being used.

@param[out]  AttemptData       Pointer to attempt structure for the NIC

@retval EFI_SUCCESS            Found attempt for the NIC
@retval EFI_NOT_FOUND          No attempt for current NIC

**/
EFI_STATUS
NvmeOfGetAttemptForCurrentNic (
  OUT NVMEOF_ATTEMPT_CONFIG_NVDATA **AttemptData
  );

/**
  Get the device path of the nvmeof tcp connection and update it.

  @param  Private       Drivers private structure.
  @param  AttemptData   Attempt configuration data

  @return The updated device path.
  @retval NULL Other errors as indicated.

**/
EFI_DEVICE_PATH_PROTOCOL *
NvmeOfGetTcpConnectionDevicePath (
  NVMEOF_DEVICE_PRIVATE_DATA       *Device,
  IN NVMEOF_ATTEMPT_CONFIG_NVDATA  *AttemptData
  );

/**
  Create device path for the namespace.

  @param  Private         Drivers private structure.
  @param  Device          Device for which path has to be created
  @param  DevicePathNode  Device path for the namespace
  @param  AttemptData     Attempt configuration data

  @return EFI_SUCCESS     In case device path creation is successful
  @retval ERROR           In case of failures
**/
EFI_STATUS
NvmeOfBuildDevicePath (
  IN NVMEOF_DRIVER_DATA            *Private,
  NVMEOF_DEVICE_PRIVATE_DATA       *Device,
  OUT EFI_DEVICE_PATH_PROTOCOL     **DevicePathNode,
  IN NVMEOF_ATTEMPT_CONFIG_NVDATA  *AttemptData
  );

/**
  Create subsystem and NID/NSID map for the namespaces of the subsystem

  @param  Ctrlr           SPDK instance representing a subsystem
  @param  NqnNidMap       Structure containing fields for subsystem and NID/NSID

  @return NamespaceCount  Number of namespaces in the map.
**/
UINT8
NvmeOfCreateNqnNamespaceMap (
  IN struct spdk_nvme_ctrlr  *Ctrlr,
  OUT NVMEOF_NQN_NID         *NqnNidMap
  );

/**
  Function to filter namespaces.

  @param  NqnNidMap       Structure contaning subsystem and its namespace ids.

  @return Filtered namespace map
**/
NVMEOF_NQN_NID*
NvmeOfFilterNamespaces (
  NVMEOF_NQN_NID      *NqnNidMap
  );

/**
  Create UEFI variable containing the information discovered using the
  discovery NQN. Two variables will be exported: 1-Count of subsystems
  discovred. 2-Array of structures containing the information.

  @return Status  Status returned by SetVariable function.
**/
EFI_STATUS
NvmeOfSetDiscoveryInfo (VOID);

/**
  Checks the driver enable condition

  @retval TRUE - Driver loading enabled
  @retval FALSE - Driver loading disabled
**/
BOOLEAN
EFIAPI
NvmeOfGetDriverEnable (void);

/**
  Calculate the prefix length of the IPv4 subnet mask.

  @param[in]  SubnetMask The IPv4 subnet mask.

  @return     The prefix length of the subnet mask.
  @retval 0   Other errors as indicated.
**/
UINT8
NvmeOfGetSubnetMaskPrefixLength (
  IN EFI_IPv4_ADDRESS  *SubnetMask
  );

/**
  Retrieve the IPv6 Address/Prefix/Gateway from the established TCP connection, these informations
  will be filled in the NVMeOF Boot Firmware Table.

  @param[in, out]  NvData      The connection data.

  @retval     EFI_SUCCESS      Get the NIC information successfully.
  @retval     Others           Other errors as indicated.

**/
EFI_STATUS
NvmeOfGetIp6NicInfo (
  IN OUT NVMEOF_SUBSYSTEM_CONFIG_NVDATA  *NvData,
  IN TCP_IO                              *TcpIo
  );

/**
  Save Root Path for NBFT.

  @param[in]  RootPath Root path string.
  @param[in]  Length    Length of Root path string.

**/
VOID
NvmeOfSaveRootPathForNbft (
  IN  CHAR8 *RootPath,
  IN  UINT32 Length
  );

/**
  Convert IP address to string

  @param[in]  IpAddr      IP to convert
  @param[out] IpAddrStr   IP string.
  @param[in]  Ipv6Flag    IP type.
**/
VOID
NvmeOfIpToStr (
  IN EFI_IP_ADDRESS IpAddr,
  OUT CHAR8         *IpAddrStr,
  IN BOOLEAN        Ipv6Flag
  );

/**
  Function to check if attempt configured for this NIC

  @param[in]  Controller           The handle of the controller.
  @param[in]  AttemptConfigData    Pointer containing all attempts.
  @param[in]  TargetCount          Number of attempts.

  @retval TRUE                     Attempt present
  @retval FALSE                    Attempt not present

**/
BOOLEAN
NvmeOfIsAttemptPresentForMac (
  IN EFI_HANDLE                    Controller,
  IN NVMEOF_ATTEMPT_CONFIG_NVDATA  *AttemptConfigData,
  IN UINT8                         TargetCount
  );

/**
  Function to check if valid UUID passed.

  @param[in]  Uuid           UUID string to be validated.

  @retval TRUE               Valid UUID
  @retval FALSE              Invalid UUID
**/
BOOLEAN
IsUuidValid (
  IN CHAR8 *Uuid
  );

/**
  Function to check if NIDs are valid and equal

  @param[in]  Nid1           NID1 to be checked
  @param[in]  Nid2           NID2 to be checked

  @retval TRUE               NIDs are valid and equal
  @retval FALSE              NIDs are invalid or unequal
**/
BOOLEAN
NvmeOfMatchNid (
  IN CHAR8 *Nid1,
  IN CHAR8 *Nid2
  );

/**
  Check whether an HBA adapter supports NVMeOF offload capability
  If yes, return EFI_SUCCESS.

  @retval EFI_SUCCESS              Offload capability supported.
  @retval EFI_NOT_FOUND            Offload capability not supported.
**/
EFI_STATUS
NvmeOfCheckOffloadCapableUsingAip (
  VOID
  );

/**
  Gets Driver Image Path
  @param[in] EFI_HANDLE  Handle,
  @param[out] CHAR16     **Name

  @retval EFI_SUCCESS    Getting Operation Status.
  @retval EFI_ERROR      Error in operation
**/
EFI_STATUS
NvmeOfGetDriverImageName (
  IN EFI_HANDLE  TheHandle,
  OUT CHAR16     **Name
  );

/**
  Gets Namespace ID Type
  @param[in] const struct spdk_nvme_ns  *NameSpace,
  @param[in] const struct spdk_uuid     *NamespaceUuid

  @retval EFI_SUCCESS                   Return NID type.
**/
UINT8
NvmeOfFindNidType (
  const struct spdk_nvme_ns  *NameSpace,
  const struct spdk_uuid     *NamespaceUuid
  );

/**
  Abort the session when the transition from BS to RT is initiated.

  @param[in]  Event   The event signaled.
  @param[in]  Context The NvmeOf driver data.

**/
VOID
EFIAPI
NvmeOfOnExitBootService (
  IN EFI_EVENT  Event,
  IN VOID       *Context
  );

/**
  Flip global flag to prevent NBFT changes in DB->Stop()
  during ExitBootServices().

  @param[in]  Event   The event signaled.
  @param[in]  Context NULL.

**/
VOID
EFIAPI
NvmeOfBeforeEBS (
  IN EFI_EVENT  Event,
  IN VOID       *Context
  );

#endif

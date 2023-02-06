/** @file
  Header containing structures of non-volatile variables holding configuration
  used by NvmeOfDxe driver.

  Copyright (c) 2020, Dell EMC All rights reserved
  Copyright (c) 2022, Intel Corporation. All rights reserved.
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/
#ifndef _NVMEOF_NVDATA_H_
#define _NVMEOF_NVDATA_H_

#include <Uefi.h>

#define NVMEOF_NID_LEN              64
#define NVMEOF_ATTEMPT_NAME_SIZE    12
#define NVMEOF_MAX_MAC_STRING_LEN   96
#define NVMEOF_NAME_MAX_SIZE        224
#define NVMEOF_NQN_MAX_LEN          224
#define NVMEOF_KEY_MAX_SIZE         255
#define NVMEOF_HOSTID_MAX_SIZE      16
#define HOST_SECURITY_PATH_MAX_LEN  255

#define NVMEOF_DRIVER_VERSION       10 // Indicate 1.0

#pragma pack(push, 1)
typedef struct _NVMEOF_SUBSYSTEM_CONFIG_NVDATA {
  UINT16            NvmeofSubsysPortId;
  UINT8             Enabled;
  UINT8             NvmeofIpMode;

  EFI_IP_ADDRESS    NvmeofSubsysHostIP;
  EFI_IP_ADDRESS    NvmeofSubsysHostSubnetMask;
  EFI_IP_ADDRESS    NvmeofSubsysHostGateway;

  BOOLEAN           HostInfoDhcp;
  BOOLEAN           NvmeofSubsysInfoDhcp;
  //Name or IP
  EFI_IP_ADDRESS    NvmeofSubSystemIp;
  CHAR8             ServerName[NVMEOF_NAME_MAX_SIZE];
  UINT8             NvmeofPrefixLength;
  //Subsytem transport port or discovery port
  UINT16            NvmeofTransportPort;
  UINT16            NvmeofSubsysControllerId;
  CHAR8             NvmeofSubsysNid[NVMEOF_NID_LEN];
  //Subsystem or discovery controller NQN
  CHAR8             NvmeofSubsysNqn[NVMEOF_NQN_MAX_LEN];

  //Define which transport to be used in NVMeOf
  EFI_GUID          NvmeofTransportGuid;
  // Timeout value in milliseconds.
  UINT16            NvmeofTimeout;
  UINT8             NvmeofRetryCount;
  // Flag indicate whether the Target address is expressed as URL format.
  BOOLEAN           NvmeofDnsMode;
  BOOLEAN           NvmeofSecurityEnabled;
  CHAR8             SecurityKeyPath[NVMEOF_KEY_MAX_SIZE];

  UINT16            RouteMetric;
  CHAR8             HostName[NVMEOF_NAME_MAX_SIZE];
  CHAR8             NvmeofHostNqnOverride[NVMEOF_NAME_MAX_SIZE];
  UINT8             NvmeofHostIdOverride[NVMEOF_HOSTID_MAX_SIZE];
  BOOLEAN           HostOverrideEnable;
} NVMEOF_SUBSYSTEM_CONFIG_NVDATA;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct _NVMEOF_ATTEMPT_CONFIG_NVDATA {
  UINT8                            NicIndex;
  BOOLEAN                          DhcpSuccess;
  BOOLEAN                          ValidnBFTPath;
  BOOLEAN                          ValidPath;
  UINT8                            AutoConfigureMode;
  CHAR8                            AttemptName[NVMEOF_ATTEMPT_NAME_SIZE];
  CHAR8                            MacString[NVMEOF_MAX_MAC_STRING_LEN];
  EFI_IP_ADDRESS                   PrimaryDns;
  EFI_IP_ADDRESS                   SecondaryDns;
  EFI_IP_ADDRESS                   DhcpServer;
  NVMEOF_SUBSYSTEM_CONFIG_NVDATA   SubsysConfigData;
  UINT8                            NvmeofAuthType;
  BOOLEAN                          AutoConfigureSuccess;
  UINT8                            Actived;
} NVMEOF_ATTEMPT_CONFIG_NVDATA;
#pragma pack(pop)

typedef struct _NVMEOF_GLOBAL_DATA {
  BOOLEAN   NvmeOfEnabled;
  UINT8     NvmeOfTargetCount;
  UINT16    DriverVersion;
  CHAR8     NvmeofHostNqn[NVMEOF_NAME_MAX_SIZE];
  UINT8     NvmeofHostId[NVMEOF_HOSTID_MAX_SIZE];
  CHAR16    NvmeofHostSecurityPath[HOST_SECURITY_PATH_MAX_LEN];
} NVMEOF_GLOBAL_DATA;

#endif /* _NVMEOF_NVDATA_H_ */
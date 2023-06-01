/** @file
  Define NVData structures used by the NVMe-oF configuration component.

Copyright (c) 2023, Dell Technologies All rights reserved
SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef _NVMEOF_NVDATASTRUC_H_
#define _NVMEOF_NVDATASTRUC_H_

#include <Guid/NvmeOfConfigHii.h>

#define CONFIGURATION_VARSTORE_ID  0x8888

#define FORMID_MAIN_FORM     1
#define FORMID_ATTEMPT_FORM  2
#define FORMID_MAC_FORM      3

#define NVMEOF_MAX_ATTEMPTS_NUM  FixedPcdGet8 (PcdMaxNvmeOfAttemptNumber)

#define NVMEOF_NAME_IFR_MIN_SIZE  4
#define NVMEOF_NAME_IFR_MAX_SIZE  223
#define NVMEOF_NAME_MAX_SIZE      224

#define NVMEOF_NID_IFR_MIN_SIZE  0
#define NVMEOF_NID_IFR_MAX_SIZE  45
#define NVMEOF_NID_MAX_LEN       46

#define NVMEOF_NID_EUI64_LEN  27
#define NVMEOF_NID_EUI64_STR  "eui:"
#define NVMEOF_NID_GUID_LEN   45
#define NVMEOF_NID_GUID_STR   "nvme-nguid:"
#define NVMEOF_NID_UUID_LEN   37
#define NVMEOF_NID_UUID_STR   "urn:uuid:"

#define ATTEMPT_NAME_SIZE  12

#define CONNECT_MIN_RETRY        0
#define CONNECT_MAX_RETRY        16
#define CONNECT_DEFAULT_RETRY    3
#define CONNECT_DEFAULT_TIMEOUT  10000

#define NVMEOF_DISABLED  0
#define NVMEOF_ENABLED   1

#define NVMEOF_ATTEMPT_DISABLED  0
#define NVMEOF_ATTEMPT_ENABLED   1

#define HOST_SECURITY_PATH_MAX_LEN  255

#define NVMEOF_DNS_DISABLED   0
#define NVMEOF_DNS_ENABLED    1
#define NVMEOF_DHCP_DISABLED  0
#define NVMEOF_DHCP_ENABLED   1

#define IP_MODE_IP4         0
#define IP_MODE_IP6         1
#define IP_MODE_AUTOCONFIG  2

#define IP4_STR_IFR_MIN_SIZE  7
#define IP4_STR_IFR_MAX_SIZE  15
#define IP4_STR_MAX_SIZE      16

#define IP_STR_IFR_MIN_SIZE  2
#define IP_STR_IFR_MAX_SIZE  39
#define IP_STR_MAX_SIZE      40

#define NVMEOF_MAX_MAC_STRING_LEN  96

#define SUBSYS_PORT_MIN_NUM  0
#define SUBSYS_PORT_MAX_NUM  65535
#define SUBSYS_PORT_DEFAULT  4420

#define NVMEOF_AUTH_TYPE_NONE  0
#define NVMEOF_AUTH_TYPE_CHAP  1

#define KEY_HOST_NQN  0x101
#define KEY_HOST_ID   0x102

#define KEY_IP_MODE      0x104
#define KEY_DHCP_ENABLE  0x105
#define KEY_HOST_IP      0x106
#define KEY_SUBNET_MASK  0x107
#define KEY_GATE_WAY     0x108

#define KEY_SUBSYS_NQN  0x10a
#define KEY_SUBSYS_IP   0x10b
#define KEY_SUBSYS_NID  0x10c

#define KEY_SELECT_MAC             0x119
#define KEY_SAVE_ATTEMPT_CONFIG    0x120
#define KEY_ATTEMPT_NAME           0x121

#define ATTEMPT_ENTRY_LABEL     0x9000
#define KEY_ATTEMPT_ENTRY_BASE  0xa000

#define LABEL_END  0xffff

#define KEY_MAC_ENTRY_BASE  0x2000
#define MAC_ENTRY_LABEL     0x3000

//
// Macro used for subsystem URL
//
#define NVMEOF_SUBSYS_URI_MIN_SIZE  0
#define NVMEOF_SUBSYS_URI_MAX_SIZE  255

#pragma pack(1)

typedef struct _NVMEOF_CONFIG_IFR_NVDATA {
  CHAR16    NvmeofHostNqn[NVMEOF_NAME_MAX_SIZE];
  CHAR16    NvmeofHostId[NVMEOF_NID_MAX_LEN];

  CHAR16    AttemptName[ATTEMPT_NAME_SIZE];
  CHAR16    NvmeofSubsysMacString[NVMEOF_MAX_MAC_STRING_LEN];

  UINT8     NvmeofSubsysSecuritySecretType;

  UINT8     Enabled;

  UINT8     IpMode;
  UINT8     ConnectRetryCount;

  UINT8     HostInfoDhcp;
  UINT8     NvmeofSubsysInfoDhcp;

  CHAR16    NvmeofSubsysHostIp[IP_STR_MAX_SIZE];
  CHAR16    NvmeofSubsysHostSubnetMask[IP4_STR_MAX_SIZE];
  CHAR16    NvmeofSubsysHostGateway[IP_STR_MAX_SIZE];

  // Name or IP
  CHAR16    NvmeofSubsysIp[NVMEOF_SUBSYS_URI_MAX_SIZE];
  // Subsytem transport port or discovery port
  UINT16    NvmeofTargetPort;
  CHAR16    NvmeofSubsysNid[NVMEOF_NID_MAX_LEN];

  CHAR16    NvmeofSubsysControllerId;
  // Subsystem or discovery controller NQN
  CHAR16    NvmeofSubsysNqn[NVMEOF_NAME_MAX_SIZE];

  CHAR16    NvmeofSubsysSecurityPath[HOST_SECURITY_PATH_MAX_LEN];
  UINT8     NvmeofAuthType;
} NVMEOF_CONFIG_IFR_NVDATA;
#pragma pack()

#endif

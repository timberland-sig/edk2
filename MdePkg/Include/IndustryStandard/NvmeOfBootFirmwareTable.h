/** @file
  The definition for NVMeOF Boot Firmware Table.

  Copyright (c) 2020, Dell EMC All rights reserved
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef _NVMEOF_BFT_H_
#define _NVMEOF_BFT_H_

#define EFI_ACPI_NVMEOF_BFT_REVISION             0x01
#define EFI_ACPI_NVMEOF_BFT_MINOR_REVISION       0x00
#define EFI_ACPI_NVMEOF_BFT_STRUCTURE_ALIGNMENT  8

///
/// Structure Type/ID
///
#define  EFI_ACPI_NVMEOF_BFT_RESERVED_STRUCTURE_ID                       0
#define  EFI_ACPI_NVMEOF_BFT_CONTROL_STRUCTURE_ID                        1
#define  EFI_ACPI_NVMEOF_BFT_HOST_DESCRIPTOR_ID                          2
#define  EFI_ACPI_NVMEOF_BFT_HFI_HEADER_DESCRIPTOR_ID                    3
#define  EFI_ACPI_NVMEOF_BFT_SUBSYSTEM_NAMESPACE_DESCRIPTOR_ID           4
#define  EFI_ACPI_NVMEOF_BFT_SECURITY_DESCRIPTOR_ID                      5
#define  EFI_ACPI_NVMEOF_BFT_DISCOVERY_DESCRIPTOR_ID                     6
#define  EFI_ACPI_NVMEOF_BFT_HFI_TRANSPORT_DESCRIPTOR_ID                 7
#define  EFI_ACPI_NVMEOF_BFT_SUBSYSTEM_NAMESPACE_INFO_EXT_DESCRIPTOR_ID  9

///
/// IP_PREFIX_ORIGIN Enumeration
///
typedef enum {
  IpPrefixOriginOther = 0,
  IpPrefixOriginManual,
  IpPrefixOriginWellKnown,
  IpPrefixOriginDhcp,
  IpPrefixOriginRouterAdvertisement,
  IpPrefixOriginUnchanged = 16
} IP_PREFIX_VALUE;

#pragma pack(1)

///
/// nBF Table Header
///
typedef struct {
  UINT32  Signature;
  UINT32  Length;
  UINT8   Revision;
  UINT8   Checksum;
  UINT8   OemId[6];
  UINT64  OemTableId;
  UINT32  OemRevision;
  UINT32  CreatorId;
  UINT32  CreatorRevision;
  UINT32  HeapOffset;
  UINT32  HeapLength;
  UINT32  DrvDevPathSignatureOffset;
  UINT16  DrvDevPathSignatureLength;
  UINT8   MinorRevision;
  UINT8   Reserved[13];
} EFI_ACPI_NVMEOF_BFT_HEADER;

///
/// Control Structure
///
typedef struct {
  UINT8   StructureId;
  UINT8   MajorRevision;
  UINT8   MinorRevision;
  UINT8   Reserved;
  UINT16  Length;
  UINT8   Flags;
  UINT8   Reserved2;
  UINT32  HostDescOffset;
  UINT16  HostDescLength;
  UINT8   HostDescVersion;
  UINT8   Reserved3;
  //Host Fabric Descriptor Information
  UINT32  HfiDescOffset;
  UINT16  HfiDescLength;
  UINT8   HfiDescVersion;
  UINT8   NumHfis;
  //Subsystem Namespace Descriptor Information
  UINT32  SubSytemDescOffset;
  UINT16  SubSystemDescLength;
  UINT8   SubSystemVersion;
  UINT8   NumNamespaces;
  //Security Profile Information
  UINT32  SecurityProfileDescOffset;
  UINT16  SecurityProfileDescLength;
  UINT8   SecurityProfileVersion;
  UINT8   NumSecurityProfiles;
  //Discovery Descriptor Information
  UINT32  DiscoDescOffset;
  UINT16  DiscoDescLengh;
  UINT8   DiscoDescVersion;
  UINT8   NumDiscoveryEntires;
  UINT8   Reserved4[16];
   
}EFI_ACPI_NVMEOF_BFT_CONTROL_STRUCTURE;
  
#define EFI_ACPI_NVMEOF_BFT_CONTROL_STRUCTURE_VERSION             0x1
#define EFI_ACPI_NVMEOF_BFT_CONTROL_STRUCTURE_MINOR_VERSION       0x0
#define EFI_ACPI_NVMEOF_BFT_CONTROL_STRUCTURE_FLAG_BLOCK_VALID    BIT0
#define EFI_ACPI_NVMEOF_BFT_CONTROL_STRUCTURE_FLAG_BOOT_FAILOVER  BIT1

///
/// Host Structure
///
typedef struct {
  UINT8   StructureId;
  UINT8   Flags;
  UINT8   HostIdentifier[16];
  UINT32  HostNqnOffset;
  UINT16  HostNqnLen;
  UINT8   Reserved[8];
} EFI_ACPI_NVMEOF_BFT_HOST_DESCRIPTOR;

typedef enum {
  EFI_ACPI_NVMEOF_BFT_ADMINISTRAIVE_HOST_NOTINDICATED = 0,
  EFI_ACPI_NVMEOF_BFT_ADMINISTRAIVE_HOST_UNSELECTED,
  EFI_ACPI_NVMEOF_BFT_ADMINISTRAIVE_HOST_SELECTED,
  EFI_ACPI_NVMEOF_BFT_ADMINISTRAIVE_HOST_RESERVED
} EFI_ACPI_NVMEOF_BFT_ADMINISTRAIVE_HOST_DESC_LIST;

#define EFI_ACPI_NVMEOF_BFT_HOST_DESCRIPTOR_VERSION           0x1
#define EFI_ACPI_NVMEOF_BFT_HOST_DESCRIPTOR_FLAG_BLOCK_VALID  BIT0
#define EFI_ACPI_NVMEOF_BFT_HOSTID_CONFIGURED                 BIT1
#define EFI_ACPI_NVMEOF_BFT_HOSTNQN_CONFIGURED                BIT2
#define EFI_ACPI_NVMEOF_BFT_ADMINISTRAIVE_HOST_DESC_SHIFT      0x3

///
/// HFI Header Structure
///
typedef struct {
  UINT8   StructureId;
  UINT8   Index;
  UINT8   Flags;
  UINT8   HfiTransportType;
  UINT8   Reserved2[12];
  UINT32  InfoStructureOffset;
  UINT16  InfoStructureLen;
  UINT8   Reserved[10];
} EFI_ACPI_NVMEOF_BFT_HFI_HEADER_DESCRIPTOR;

#define EFI_ACPI_NVMEOF_BFT_HFI_HEADER_DESCRIPTOR_VERSION           0x1
#define EFI_ACPI_NVMEOF_BFT_HFI_HEADER_DESCRIPTOR_FLAG_BLOCK_VALID  BIT0

///
/// HFI Transport Header Info Structure
///
typedef struct {
  UINT8   StructureId;
  UINT8   Version;
  UINT8   HfiTransportType;
  UINT8   HfiTransportInfoVersion;
  UINT16  HfiIndex;
} EFI_ACPI_NVMEOF_BFT_HFI_TRANSPORT_INFO_DESCRIPTOR_HEADER;
///
/// HFI Tcp Transport  Info Structure
///
typedef struct {
  EFI_ACPI_NVMEOF_BFT_HFI_TRANSPORT_INFO_DESCRIPTOR_HEADER  Header;
  UINT8                                                     TransportFlags;
  UINT32                                                    PciLocation;
  UINT8                                                     Mac[6];
  UINT16                                                    VLanTag;
  UINT8                                                     Origin;
  EFI_IPv6_ADDRESS                                          IpAddress;
  UINT8                                                     SubnetMaskPrefix;
  EFI_IPv6_ADDRESS                                          Gateway;
  UINT8                                                     Reserved;
  UINT16                                                    RouteMetric;
  EFI_IPv6_ADDRESS                                          PrimaryDns;
  EFI_IPv6_ADDRESS                                          SecondaryDns;
  EFI_IPv6_ADDRESS                                          DhcpServer;
  UINT32                                                    HostNameOffset;
  UINT16                                                    HostNameLength;
  UINT8                                                     Reserved2[18];
} EFI_ACPI_NVMEOF_BFT_HFI_TRANSPORT_INFO_DESCRIPTOR_TCP;

#define EFI_ACPI_NVMEOF_BFT_HFI_TRANSPORT_INFO_DESCRIPTOR_VERSION             0x1
#define EFI_ACPI_NVMEOF_BFT_HFI_TRANSPORT_INFO_HFI_TYPE_TCPIP                 0x3
#define EFI_ACPI_NVMEOF_BFT_HFI_TRANSPORT_INFO_VERSION                        0x1
#define EFI_ACPI_NVMEOF_BFT_HFI_TRANSPORT_DESCRIPTOR_FLAG_BLOCK_VALID         BIT0
#define EFI_ACPI_NVMEOF_BFT_HFI_TRANSPORT_DESCRIPTOR_FLAG_GLOBAL_LOCAL_ROUTE  BIT1
#define EFI_ACPI_NVMEOF_BFT_HFI_TRANSPORT_DESCRIPTOR_FLAG_DHCP_OVERRIDE       BIT2

///
/// Subsystem Namespace Structure
///
typedef struct {
  UINT8   StructureId;
  UINT16  Index;
  UINT16  Flags;
  UINT8   TransportType;
  UINT16  TransportSpecificFlag;
  UINT8   PrimaryDiscoveryCtrlrIndex;
  UINT8   Reserved;
  UINT32  SubsystemTransportAdressOffset;
  UINT16  SubsystemTransportAdressLength;
  UINT32  SubsystemTransportServiceIdOffset;
  UINT16  SubsystemTransportServiceIdLength;
  UINT16  SubsytemPortId;
  UINT32  Nsid;
  UINT8   NidType;
  UINT8   Nid[16];
  UINT8   SecurityStructureIndex;
  UINT8   PrimaryHfiDescriptorIndex;
  UINT8   Reserved2;
  UINT32  HfiAssociationOffset;
  UINT16  HfiAssociationLen;
  UINT32  SubsystemNamespaceNqnOffset;
  UINT16  SubsystemNamespaceNqnLen; 
  UINT32  SsnsExtendedInfoOffset;
  UINT16  SsnsExtendedInfoLength;
  UINT8   Reserved3[62];
} EFI_ACPI_NVMEOF_BFT_SUBSYSTEM_NAMESPACE_DESCRIPTOR;

#define EFI_ACPI_NVMEOF_BFT_SUBSYSTEM_DESCRIPTOR_VERSION                        0x1
#define EFI_ACPI_NVMEOF_BFT_SUBSYSTEM_DESCRIPTOR_FLAG_BLOCK_VALID               BIT0
#define EFI_ACPI_NVMEOF_BFT_SUBSYSTEM_DESCRIPTOR_FLAG_NON_BOOTABLE_ENTRY        BIT1
#define EFI_ACPI_NVMEOF_BFT_SUBSYSTEM_DESCRIPTOR_FLAG_USE_SECURITY_FIELD        BIT2
#define EFI_ACPI_NVMEOF_BFT_SUBSYSTEM_DESCRIPTOR_FLAG_DHCP_ROOTPATH             BIT3
#define EFI_ACPI_NVMEOF_BFT_SUBSYSTEM_DESCRIPTOR_FLAG_USE_SSNS_EXT_INFO         BIT4
#define EFI_ACPI_NVMEOF_BFT_SUBSYSTEM_DESCRIPTOR_FLAG_SEPERATE_DISCOVERY_CTRLR  BIT5
#define EFI_ACPI_NVMEOF_BFT_SUBSYSTEM_DESCRIPTOR_FLAG_DISCOVERED_NAMESPACE      BIT6
#define EFI_ACPI_NVMEOF_BFT_SUBSYSTEM_DESCRIPTOR_FLAG_UNAVAILABLE_NAMESPACE_0   BIT7
#define EFI_ACPI_NVMEOF_BFT_SUBSYSTEM_DESCRIPTOR_FLAG_UNAVAILABLE_NAMESPACE_1   BIT8

#define EFI_ACPI_NVMEOF_BFT_SUBSYSTEM_DESCRIPTOR_TRANSPORTFLAG_VALID              BIT0
#define EFI_ACPI_NVMEOF_BFT_SUBSYSTEM_DESCRIPTOR_TRANSPORTFLAG_PDU_HEADER_DIGEST  BIT1
#define EFI_ACPI_NVMEOF_BFT_SUBSYSTEM_DESCRIPTOR_TRANSPORTFLAG_DATA_DIGEST        BIT2

///
/// SSNS Extended Info Structure
///
typedef struct {
  UINT8   StructureId;
  UINT8   Version;
  UINT16  SsnsIndex;
  UINT32  Flags;
  UINT16  ControllerId;
  UINT16  Asqsz;
  UINT32  DhcpRootPathOffset;
  UINT16  DhcpRootPathLength;
} EFI_ACPI_NVMEOF_BFT_SUBSYSTEM_EXT_INFO_DESCRIPTOR;

#define EFI_ACPI_NVMEOF_BFT_SUBSYSTEM_EXT_INFO_DESCRIPTOR_VERSION                       0x1
#define EFI_ACPI_NVMEOF_BFT_SUBSYSTEM_EXT_INFO_DESCRIPTOR_VERSION_FLAG_STRUCTURE_VALID  BIT0
#define EFI_ACPI_NVMEOF_BFT_SUBSYSTEM_EXT_INFO_DESCRIPTOR_ASQZ_ADMINSTRATIVE            BIT1


///
/// Security Structure
///
typedef struct {
  UINT8   StructureId;
  UINT8   Index;
  UINT16  Flags;
  UINT8   SecretType;
  UINT8   Reserved;
  UINT32  SecureChannelAllowedAlgorithmsOffset;
  UINT16  SecureChannelAllowedAlgorithmsLen;
  UINT32  AuthenticationProtocolsAllowedOffset;
  UINT16  AuthenticationProtocolsAllowedLen;
  UINT32  CipherSuiteNameOffset;
  UINT16  CipherSuiteNameLen;
  UINT32  SupportedDhGroupsOffset;
  UINT16  SupportedDhGroupsLen;
  UINT32  SecureHashFunctionOffset;
  UINT16  SecureHashFunctionLen;
  UINT32  SecretKeyPathOffset;
  UINT16  SecretKeyPathLen;
  UINT8   Reserved2[22];
} EFI_ACPI_NVMEOF_BFT_SECURITY_DESCRIPTOR;

#define EFI_ACPI_NVMEOF_BFT_SECURITY_DESCRIPTOR_VERSION  0x1

typedef enum {
  EFI_ACPI_NVMEOF_BFT_SECURITY_FLAG_INBAND_AUTH_REQD_NOTSUPPORTED = 0,
  EFI_ACPI_NVMEOF_BFT_SECURITY_FLAG_INBAND_AUTH_REQD_SUPPORTED_NOTREQD,
  EFI_ACPI_NVMEOF_BFT_SECURITY_FLAG_INBAND_AUTH_REQD_SUPPORTED_REQD,
} EFI_ACPI_NVMEOF_BFT_SECURITY_ENUM_FLAG_INBAND_AUTH_REQD;

typedef enum {
  EFI_ACPI_NVMEOF_BFT_SECURITY_FLAG_AUTH_POLICY_LIST_RESERVED = 0,
  EFI_ACPI_NVMEOF_BFT_SECURITY_FLAG_AUTH_POLICY_LIST_AUTHPROTOCOL,
  EFI_ACPI_NVMEOF_BFT_SECURITY_FLAG_AUTH_POLICY_LIST_AUTHPROTOCOL_ADMINISTRATIVE,
} EFI_ACPI_NVMEOF_BFT_SECURITY_ENUM_FLAG_AUTH_POLICY_LIST;

typedef enum {
  EFI_ACPI_NVMEOF_BFT_SECURITY_FLAG_SECURE_CH_NEG_NOTSUPPORTED = 0,
  EFI_ACPI_NVMEOF_BFT_SECURITY_FLAG_SECURE_CH_NEG_SUPPORTED_NOTREQD,
  EFI_ACPI_NVMEOF_BFT_SECURITY_FLAG_SECURE_CH_NEG_SUPPORTED_REQD,
} EFI_ACPI_NVMEOF_BFT_SECURITY_ENUM_FLAG_SECURE_CH_NEG_REQD;

typedef enum {
  EFI_ACPI_NVMEOF_BFT_SECURITY_FLAG_SECURITY_CH_OBJREF_RESERVED = 0,
  EFI_ACPI_NVMEOF_BFT_SECURITY_FLAG_SECURITY_CH_OBJREF_LIST,
  EFI_ACPI_NVMEOF_BFT_SECURITY_FLAG_SECURITY_CH_OBJREF_LIST_ADMINISTRATIVE,
} EFI_ACPI_NVMEOF_BFT_SECURITY_ENUM_FLAG_SECURITY_POLICY_LIST;


#define EFI_ACPI_NVMEOF_BFT_SECURITY_DESCRIPTOR_FLAG_BLOCK_VALID                 BIT0
#define EFI_ACPI_NVMEOF_BFT_SECURITY_DESCRIPTOR_FLAG_INBAND_AUTH_REQD_SHIFT      0x1
#define EFI_ACPI_NVMEOF_BFT_SECURITY_DESCRIPTOR_FLAG_AUTH_POLICY_LIST_SHIFT      0x3
#define EFI_ACPI_NVMEOF_BFT_SECURITY_DESCRIPTOR_FLAG_SECURE_CH_NEGO_REQD_SHIFT   0x5
#define EFI_ACPI_NVMEOF_BFT_SECURITY_DESCRIPTOR_FLAG_SECURITY_POLICY_LIST_SHIFT  0x7
#define EFI_ACPI_NVMEOF_BFT_SECURITY_DESCRIPTOR_FLAG_CIPHER_SUITE_RESTRICTED     BIT9
#define EFI_ACPI_NVMEOF_BFT_SECURITY_DESCRIPTOR_FLAG_AUTH_PARAMS_RESTRICTED      BIT10
#define EFI_ACPI_NVMEOF_BFT_SECURITY_DESCRIPTOR_FLAG_SECURE_HASH_POLICY_LIST     BIT11

// Secret type in heap
#define SECRET_TYPE_NONE             BIT0
#define SECRET_TYPE_REDFISH_HFI_URI  BIT1
#define SECRET_TYPE_OEM_URI          BIT2

///
/// Discovery Structure
///
typedef struct {
  UINT8   StructureId;
  UINT8   Flags;
  UINT8   Index;
  UINT8   HfiDescriptorIndex;
  UINT8   SecurityProfileDescriptorIndex;
  UINT8   Reserved;
  UINT32  DiscoveryCtrlrAddrOffset;
  UINT16  DiscoveryCtrlrAddrLen;
  UINT32  DiscoveryCtrlrNqnOffset;
  UINT16  DiscoveryCtrlrNqnLen;
  UINT8   Reserved1[14];
} EFI_ACPI_NVMEOF_BFT_DISCOVERY_DESCRIPTOR;

#define EFI_ACPI_NVMEOF_BFT_DISCOVERY_DESCRIPTOR_VERSION           0x1
#define EFI_ACPI_NVMEOF_BFT_DISCOVERY_DESCRIPTOR_FLAG_BLOCK_VALID  BIT0


#pragma pack()

#endif


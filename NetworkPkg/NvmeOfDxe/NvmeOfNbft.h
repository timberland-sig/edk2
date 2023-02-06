/** @file
  Function definitions for nBFT.

  Copyright (c) 2020, Dell EMC All rights reserved
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef _NVMEOF_NBFT_H_
#define _NVMEOF_NBFT_H_

#include <IndustryStandard/Acpi.h>
#include <IndustryStandard/NvmeOfBootFirmwareTable.h>
#include <Protocol/AcpiTable.h>

#define NBFT_MAX_SIZE       (100*1024)
#define NBFT_HEAP_SIZE      (40*1024)

#define NBFT_ROUNDUP(size)  NET_ROUNDUP ((size), EFI_ACPI_NVMEOF_BFT_STRUCTURE_ALIGNMENT)

#define NVMEOF_TRANSPORT_TCP  3

#define ASL_COMPILER_ID       0         // Asl Compiler ID : "INTL"
#define ASL_COMPILER_REVISION 20100528  //Asl Compiler Revision : 20100528


typedef struct {
  UINT8 *Heap;
  UINT32 Length;
} NVMEOF_NBFT_HEAP;

typedef struct {
  LIST_ENTRY  Link;
  UINT8                                     **HeapRef;
  CHAR8                                     MacString[NVMEOF_MAX_MAC_STRING_LEN];
  BOOLEAN                                   HostOverrideEnable;
  EFI_ACPI_NVMEOF_BFT_HFI_HEADER_DESCRIPTOR  *HfiHeaderRef;
} NVMEOF_PROCESSED_MAC;

typedef struct {
  LIST_ENTRY  Link;
  UINT8       Nid[16];
} NVMEOF_PROCESSED_NAMESPACE;

typedef struct {
  LIST_ENTRY  Link;
  EFI_IP_ADDRESS    NvmeofSubsysHostIP;
  EFI_IP_ADDRESS    NvmeofSubSystemIp;
} NVMEOF_PROCESSED_IP_ADDR;

typedef struct {
  UINT8           AdapterIndex;
  BOOLEAN         Ipv6Flag;
  UINT8           SecurityProfileIndex;
  UINT16          Port;
  EFI_IP_ADDRESS  TransportAddress;
  CHAR8           Nqn[NVMEOF_NQN_MAX_LEN];
} NVMEOF_DISCOVERY_DETAILS;

/**
  Publish and remove the NVMeOF Boot Firmware Table.

**/
VOID
NvmeOfPublishNbft (
  IN BOOLEAN HostStructureOnly
  );

#endif

/** @file
  Passthrough Header file for NvmeOf driver.
  
  Contains definations for Passthru whose 
  role is for invoking NVMeOF CLI and BM functionality

  Copyright (c) 2020, Dell EMC All rights reserved
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef _UEFI_NVMEOF_PASSTHRU_H_
#define _UEFI_NVMEOF_PASSTHRU_H_

#define NVMEOF_CLI_MAX_SIZE       224
#define BLOCK_SIZE                512
#define ADDR_SIZE                 40
#define KEY_SIZE                  20

typedef struct _NVMEOF_CONNECT_COMMAND {
  CHAR8                    Transport[NVMEOF_CLI_MAX_SIZE]; //Transport type
  CHAR8                    Nqn[NVMEOF_CLI_MAX_SIZE];//NVMe Qualified Name
  CHAR8                    Traddr[NVMEOF_CLI_MAX_SIZE];//Transport address
  UINT16                   Trsvcid;//Transport service id
  UINTN                    Hostid;//Host identity
  CHAR8                    Mac[NVMEOF_CLI_MAX_SIZE];
  UINT8                    IpMode;
  CHAR8                    LocalIp[NVMEOF_CLI_MAX_SIZE];
  CHAR8                    SubnetMask[NVMEOF_CLI_MAX_SIZE];
  CHAR8                    Gateway[NVMEOF_CLI_MAX_SIZE];
  UINT16                   ConnectTimeout;
  CHAR8                    Hostnqn[NVMEOF_CLI_MAX_SIZE];//User-defined hostnqn
  CHAR8                    UseHostNqn; // Indicate whether to use hostnqn
} NVMEOF_CONNECT_COMMAND;

typedef struct _NVMEOF_CLI_CTRL_MAPPING {
  struct spdk_nvme_ctrlr   *Ctrlr;
  struct spdk_nvme_qpair   *Ioqpair;
  CHAR8                    Key[KEY_SIZE];
  UINTN                    Nsid;
  UINT16                   Cntliduser;
  CHAR8                    Traddr[ADDR_SIZE];
  CHAR8                    Trsvcid[ADDR_SIZE];
  CHAR8                    Subnqn[NVMEOF_CLI_MAX_SIZE];
  LIST_ENTRY               CliCtrlrList;
} NVMEOF_CLI_CTRL_MAPPING;

typedef struct _NVMEOF_READ_WRITE_DATA {
  UINT64                   Startblock;
  UINT32                   Blockcount;
  UINT64                   Datasize;
  CHAR16                   *Data;
  struct spdk_nvme_ctrlr   *Ctrlr;
  struct spdk_nvme_qpair   *Ioqpair;
  UINTN                    Nsid;
  UINTN                    FileSize;
  VOID                     *Payload;
} NVMEOF_READ_WRITE_DATA;

typedef struct _NVMEOF_CLI_IDENTIFY {
  struct spdk_nvme_ctrlr             *Ctrlr;
  const struct spdk_nvme_ctrlr_data  *Cdata;
  UINTN                              NsId;
} NVMEOF_CLI_IDENTIFY;

typedef struct _NVMEOF_CLI_DISCONNECT {
  struct spdk_nvme_ctrlr   *Ctrlr;
  CHAR8                    Devicekey[KEY_SIZE];
} NVMEOF_CLI_DISCONNECT;

typedef struct _NVMEOF_READ_WRITE_DATA_IN_BLOCK {
  CHAR16 Device_id[5];
  UINT64 Start_lba;
  UINT32 Numberof_block;
  UINT64 Pattern;
} NVMEOF_READ_WRITE_DATA_IN_BLOCK;

typedef struct _NVMEOF_PASSTHROUGH_PROTOCOL {
  EFI_STATUS(EFIAPI *Connect)(NVMEOF_CONNECT_COMMAND);
  EFI_STATUS(EFIAPI *NvmeOfCliRead)(NVMEOF_READ_WRITE_DATA);
  EFI_STATUS(EFIAPI *NvmeOfCliWrite)(NVMEOF_READ_WRITE_DATA);
  NVMEOF_CLI_IDENTIFY(EFIAPI *NvmeOfCliIdentify)(NVMEOF_CLI_IDENTIFY);
  VOID(EFIAPI *NvmeOfCliDisconnect)(NVMEOF_CLI_DISCONNECT, CHAR16 **);
  UINT8(EFIAPI *NvmeOfCliReset)(struct spdk_nvme_ctrlr *, CHAR16 **);
  VOID(EFIAPI *NvmeOfCliList)();
  VOID(EFIAPI *NvmeOfCliListConnect)();
  UINTN(EFIAPI *NvmeOfCliVersion)();
  NVMEOF_CLI_CTRL_MAPPING **CliCtrlMap;
  EFI_STATUS(EFIAPI *GetBootDesc)(EFI_HANDLE, CHAR16 *);
} NVMEOF_PASSTHROUGH_PROTOCOL;

extern EFI_GUID gNvmeofPassThroughProtocolGuid;

#endif
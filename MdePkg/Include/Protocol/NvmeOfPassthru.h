/** @file
  Header for NVMe-oF Passthrough protocol.

  It is meant to be implemented by EDKII NVMe-oF drivers
  to allow for external control over the driver (e.g. from CLI utilities).
  It also shall allow boot manager libraries to obtain an NVMe-oF-specific
  boot description of a given, supported handle.

  Copyright (c) 2020 - 2023, Dell Inc. or its subsidiaries. All Rights Reserved.<BR>
  Copyright (c) 2023, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef _EDKII_NVMEOF_PASSTHRU_H_
#define _EDKII_NVMEOF_PASSTHRU_H_

#define NVMEOF_CLI_MAX_SIZE  224
#define BLOCK_SIZE           512
#define ADDR_SIZE            40
#define KEY_SIZE             20

typedef struct _EDKII_NVMEOF_PASSTHRU_PROTOCOL EDKII_NVMEOF_PASSTHRU_PROTOCOL;

typedef struct _NVMEOF_CONNECT_COMMAND {
  CHAR8     Transport[NVMEOF_CLI_MAX_SIZE]; // Transport type
  CHAR8     Nqn[NVMEOF_CLI_MAX_SIZE];       // NVMe Qualified Name
  CHAR8     Traddr[NVMEOF_CLI_MAX_SIZE];    // Transport address
  UINT16    Trsvcid;                        // Transport service id
  UINTN     Hostid;                         // Host identity
  CHAR8     Mac[NVMEOF_CLI_MAX_SIZE];
  UINT8     IpMode;
  CHAR8     LocalIp[NVMEOF_CLI_MAX_SIZE];
  CHAR8     SubnetMask[NVMEOF_CLI_MAX_SIZE];
  CHAR8     Gateway[NVMEOF_CLI_MAX_SIZE];
  UINT16    ConnectTimeout;
  CHAR8     Hostnqn[NVMEOF_CLI_MAX_SIZE]; // User-defined hostnqn
  CHAR8     UseHostNqn;                   // Indicate whether to use hostnqn
} NVMEOF_CONNECT_COMMAND;

typedef struct _NVMEOF_CLI_CTRL_MAPPING {
  VOID          *Ctrlr;
  VOID          *Ioqpair;
  CHAR8         Key[KEY_SIZE];
  UINTN         Nsid;
  UINT16        Cntliduser;
  CHAR8         Traddr[ADDR_SIZE];
  CHAR8         Trsvcid[ADDR_SIZE];
  CHAR8         Subnqn[NVMEOF_CLI_MAX_SIZE];
  LIST_ENTRY    CliCtrlrList;
} NVMEOF_CLI_CTRL_MAPPING;

typedef struct _NVMEOF_READ_WRITE_DATA {
  UINT64    Startblock;
  UINT32    Blockcount;
  UINT64    Datasize;
  CHAR16    *Data;
  VOID      *Ctrlr;
  VOID      *Ioqpair;
  UINTN     Nsid;
  UINTN     FileSize;
  VOID      *Payload;
} NVMEOF_READ_WRITE_DATA;

typedef struct _NVMEOF_CLI_IDENTIFY {
  VOID          *Ctrlr;
  CONST VOID    *Cdata;
  UINTN         NsId;
} NVMEOF_CLI_IDENTIFY;

typedef struct _NVMEOF_CLI_DISCONNECT {
  VOID     *Ctrlr;
  CHAR8    Devicekey[KEY_SIZE];
} NVMEOF_CLI_DISCONNECT;

typedef struct _NVMEOF_READ_WRITE_DATA_IN_BLOCK {
  CHAR16    Device_id[5];
  UINT64    Start_lba;
  UINT32    Numberof_block;
  UINT64    Pattern;
} NVMEOF_READ_WRITE_DATA_IN_BLOCK;

/**
  This function is used to establish connection to the NVMe-oF Target device.

  @param[in]    This              Pointer to the EDKII_NVMEOF_PASSTHRU_PROTOCOL instance.
  @param[in]    ConnectData       Pointer to the NVMEOF_CONNECT_COMMAND structure containing
                                  information required to connect NVMe-oF Target.

  @retval EFI_SUCCESS            Connected successfully.
  @retval EFI_OUT_OF_RESOURCES   Out of Resources.
  @retval EFI_NOT_FOUND          Resource Not Found.
  @retval EFI_INVALID_PARAMETER  Invalid Parameter.

**/
typedef
EFI_STATUS
(EFIAPI *EDKII_NVMEOF_PASSTHRU_CONNECT)(
  IN  EDKII_NVMEOF_PASSTHRU_PROTOCOL  *This,
  IN  NVMEOF_CONNECT_COMMAND          *ConnectData
  );

/**
  This function is used to read one block of data from the NVMe-oF target device.

  @param[in]    This      Pointer to the EDKII_NVMEOF_PASSTHRU_PROTOCOL instance.
  @param[in]    ReadData  Pointer to the NVMEOF_READ_WRITE_DATA structure.

  @retval EFI_SUCCESS            Data successfully read from the device.
  @retval EFI_INVALID_PARAMETER  Invalid parameter.
  @retval EFI_DEVICE_ERROR       Error reading from device.

**/
typedef
EFI_STATUS
(EFIAPI *EDKII_NVMEOF_PASSTHRU_READ)(
  IN  EDKII_NVMEOF_PASSTHRU_PROTOCOL  *This,
  IN  NVMEOF_READ_WRITE_DATA          *ReadData
  );

/**
  This function is used to write one block of data to the NVMe-oF target device.

  @param[in]    This       Pointer to the EDKII_NVMEOF_PASSTHRU_PROTOCOL instance.
  @param[in]    WriteData  Pointer to the NVMEOF_READ_WRITE_DATA structure.

  @retval EFI_SUCCESS            Data are written into the buffer.
  @retval EFI_INVALID_PARAMETER  Invalid parameter.
  @retval EFI_DEVICE_ERROR       Fail to write all the data.

**/
typedef
EFI_STATUS
(EFIAPI *EDKII_NVMEOF_PASSTHRU_WRITE)(
  IN  EDKII_NVMEOF_PASSTHRU_PROTOCOL  *This,
  IN  NVMEOF_READ_WRITE_DATA          *WriteData
  );

/**
  This function is used to print the target controller and namespace information by
  sending NVMe identify command.

  @param[in]    This          Pointer to the EDKII_NVMEOF_PASSTHRU_PROTOCOL instance.
  @param[in]    IdentifyData  Pointer to NVMEOF_CLI_IDENTIFY structure.

  @return ReturnData          Pointer to NVMEOF_CLI_IDENTIFY structure that contains information
                              retrieved from Target device.

**/
typedef
NVMEOF_CLI_IDENTIFY
(EFIAPI *EDKII_NVMEOF_PASSTHRU_IDENTIFY)(
  IN  EDKII_NVMEOF_PASSTHRU_PROTOCOL  *This,
  IN  NVMEOF_CLI_IDENTIFY             *IdentifyData
  );

/**
  This function is used to disconnect the NVMe-oF Target device.

  @param[in]    This            Pointer to the EDKII_NVMEOF_PASSTHRU_PROTOCOL instance.
  @param[in]    DisconnectData  Pointer to the NVMEOF_CLI_DISCONNECT structure.
  @param[in]    Key             String representing the namespace of a controller to disconnect.

  @retval    None

**/
typedef
VOID
(EFIAPI *EDKII_NVMEOF_PASSTHRU_DISCONNECT)(
  IN  EDKII_NVMEOF_PASSTHRU_PROTOCOL  *This,
  IN  NVMEOF_CLI_DISCONNECT           *DisconnectData,
  IN  CHAR16                          **Key
  );

/**
  This function is used to reset the target controller.

  @param[in]    This    Pointer to the EDKII_NVMEOF_PASSTHRU_PROTOCOL instance.
  @param[in]    Ctrlr   Pointer to Abstract the SPDK structure (struct spdk_nvme_ctrlr).
  @param[in]    Key     String representing the namespace of a controller to reset.

  @retval EFI_SUCCESS         Device reset completed.
  @retval EFI_DEVICE_ERROR    The device is not functioning properly and could not reset.

**/
typedef
UINT8
(EFIAPI *EDKII_NVMEOF_PASSTHRU_RESET)(
  IN  EDKII_NVMEOF_PASSTHRU_PROTOCOL  *This,
  IN  VOID                            *Ctrlr,
  IN  CHAR16                          **Key
  );

/**
  This function is used to list the namespaces of the target controller connected through
  EDKII_NVMEOF_PASSTHRU_PROTOCOL Connect member function.

  @param[in]    This    Pointer to the EDKII_NVMEOF_PASSTHRU_PROTOCOL instance.

  @retval None

**/
typedef
VOID
(EFIAPI *EDKII_NVMEOF_PASSTHRU_LIST)(
  IN  EDKII_NVMEOF_PASSTHRU_PROTOCOL  *This
  );

/**
  This function is used to list the namespaces of the target controller connected through
  NVMe-oF Driver.

  @param[in]    This    Pointer to the EDKII_NVMEOF_PASSTHRU_PROTOCOL instance.

  @retval None

**/
typedef
VOID
(EFIAPI *EDKII_NVMEOF_PASSTHRU_LIST_CONNECT)(
  IN  EDKII_NVMEOF_PASSTHRU_PROTOCOL  *This
  );

/**
  This function is used to get the NVMe-oF Driver Version.

  @param[in]    This    Pointer to the EDKII_NVMEOF_PASSTHRU_PROTOCOL instance.

  @return    NVMe-oF Driver Version.

**/
typedef
UINTN
(EFIAPI *EDKII_NVMEOF_PASSTHRU_VERSION)(
  IN  EDKII_NVMEOF_PASSTHRU_PROTOCOL  *This
  );

/**
  This function is used to get the information of target controllers.

  @param[in]    This    Pointer to the EDKII_NVMEOF_PASSTHRU_PROTOCOL instance.

  @return    Address of Pointer to NVMEOF_CLI_CTRL_MAPPING structure.

**/
typedef
NVMEOF_CLI_CTRL_MAPPING **
(EFIAPI *EDKII_NVMEOF_PASSTHRU_GET_CTRL_MAP)(
  IN  EDKII_NVMEOF_PASSTHRU_PROTOCOL  *This
  );

/**
  Obtain NVMe-oF-specific boot description for a given supported handle.

  Caller is responsible for freeing the memory for the description allocated
  by this function.

  @param[in]  This                Pointer to EDKII_NVMEOF_PASSTHRU_PROTOCOL instance.
  @param[in]  Handle              Handle for which to obtain boot description.
  @param[out] Description         Resultant boot description.

  @retval EFI_SUCCESS             Boot description was successfully created.
  @retval EFI_UNSUPPORTED         Handle is not related to an NVMe-oF namespace.
**/
typedef
EFI_STATUS
(EFIAPI *EDKII_NVMEOF_PASSTHRU_GET_BOOT_DESC)(
  IN  EDKII_NVMEOF_PASSTHRU_PROTOCOL   *This,
  IN  EFI_HANDLE                       Handle,
  OUT CHAR16                           **Description
  );

struct _EDKII_NVMEOF_PASSTHRU_PROTOCOL {
  EDKII_NVMEOF_PASSTHRU_CONNECT          Connect;
  EDKII_NVMEOF_PASSTHRU_READ             Read;
  EDKII_NVMEOF_PASSTHRU_WRITE            Write;
  EDKII_NVMEOF_PASSTHRU_IDENTIFY         Identify;
  EDKII_NVMEOF_PASSTHRU_DISCONNECT       Disconnect;
  EDKII_NVMEOF_PASSTHRU_RESET            Reset;
  EDKII_NVMEOF_PASSTHRU_LIST             List;
  EDKII_NVMEOF_PASSTHRU_LIST_CONNECT     ListConnect;
  EDKII_NVMEOF_PASSTHRU_VERSION          Version;
  EDKII_NVMEOF_PASSTHRU_GET_CTRL_MAP     GetCtrlMap;
  EDKII_NVMEOF_PASSTHRU_GET_BOOT_DESC    GetBootDesc;
};

extern EFI_GUID gEdkiiNvmeofPassThruProtocolGuid;

#endif /* _EDKII_NVMEOF_PASSTHRU_H_ */

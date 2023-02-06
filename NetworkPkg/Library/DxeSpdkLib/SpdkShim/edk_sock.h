/*-
 *   edk_socket.h - Header file for EDK Socket

 *   Copyright (c) 2020, Dell EMC All rights reserved
 *   SPDX-License-Identifier: BSD-2-Clause-Patent
 */

#ifndef _EDK_SOCK_H_
#define _EDK_SOCK_H_

#include "spdk/stdinc.h"
#include "spdk/log.h"
#include "spdk/sock.h"
#include "spdk/util.h"
#include "spdk_internal/sock.h"
#include <Library/NetLib.h>
#include <Library/TcpIoLib.h>

#define MAX_TMPBUF 1024
#define PORTNUMLEN 32
#define IOV_BATCH_SIZE 64
#define TIMEOUT 1000

struct spdk_edk_sock_ctx {
  // Sourced externally
  EFI_HANDLE      Controller;       // Adapter handle with TCP ServiceBinding
  BOOLEAN         IsIp6;            // Hint on IP address family
  EFI_IP_ADDRESS  StationIp;        // Client-side IP address
  EFI_IP_ADDRESS  SubnetMask;       // Client-side subnet mask
  EFI_IP_ADDRESS  GatewayIp;        // Client's gateway IP address

  // Provided by socket
  TCP_IO          *TcpIo;           // Socket's TCP_IO
};

struct spdk_edk_sock {
  struct spdk_sock                  base;
  bool                              Ipv6Flag;
  TCP_IO                            TcpIo;
  EFI_EVENT                         TimeoutEvent;
  TCP_IO_CONFIG_DATA                TcpIoConfig;
  struct spdk_edk_sock_ctx          *Context;

  // Receive context
  NET_BUF                           *Pdu;
  UINTN                             MaxPduLen;
  EFI_TCP4_IO_TOKEN                 RxToken;
  EFI_TCP4_RECEIVE_DATA             RxData;
  EFI_EVENT                         RxEvent;
  BOOLEAN                           RxPending;
  UINTN                             RxHead;
  NET_FRAGMENT                      *RxFragment;
  UINT32                            RxFragmentCount;

  TAILQ_ENTRY (spdk_edk_sock)       link;
};

#define __edk_sock(sock) (struct spdk_edk_sock *)sock

int
edk_sock_strtoip4 (
  const CHAR8      *String, 
  EFI_IPv4_ADDRESS *Ip4Address
  );

int
edk_sock_strtoip6 (
  const CHAR8      *String,
  EFI_IPv6_ADDRESS *Ip6Address
  );

struct spdk_sock *
edk_sock_connect (
  const char            *ip, 
  int                   port, 
  struct spdk_sock_opts *opts
  );

int
edk_sock_close (
  struct spdk_sock *_sock
  );

int
_sock_flush (
  struct spdk_sock *sock
  );

void
edk_sock_writev_async (
  struct spdk_sock         *sock, 
  struct spdk_sock_request *req
  );

int
posix_sock_flush (
  struct spdk_sock *_sock
  );

ssize_t
edk_sock_readv (
  struct spdk_sock *_sock, 
  struct iovec     *iov, 
  int              iovcnt
  );

#endif

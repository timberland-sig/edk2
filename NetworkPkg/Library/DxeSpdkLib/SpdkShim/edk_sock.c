/** @file
  edk_socket.c - Implements EDK sockets for SPDK.

Copyright (c) 2021 - 2024, Dell Inc. or its subsidiaries. All Rights Reserved.<BR>
Copyright (c) 2022 - 2023, Intel Corporation. All rights reserved.<BR>
SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "edk_sock.h"

#define SPDK_SOCK_OPTS_FIELD_OK(opts, field)  (offsetof(struct spdk_sock_opts, field) + sizeof(opts->field) <= (opts->opts_size))

int  errno;

int
edk_sock_strtoip4 (
  const CHAR8       *String,
  EFI_IPv4_ADDRESS  *Ip4Address
  )
{
  UINTN  Status      = EFI_SUCCESS;
  CHAR8  *EndPointer = NULL;

  Status = AsciiStrToIpv4Address (String, &EndPointer, Ip4Address, NULL);
  if (RETURN_ERROR (Status) || (*EndPointer != L'\0')) {
    return -1;
  } else {
    return 0;
  }
}

int
edk_sock_strtoip6 (
  const CHAR8       *String,
  EFI_IPv6_ADDRESS  *Ip6Address
  )
{
  UINTN  Status      = EFI_SUCCESS;
  CHAR8  *EndPointer = NULL;

  Status = AsciiStrToIpv6Address (String, &EndPointer, Ip6Address, NULL);
  if (RETURN_ERROR (Status) || (*EndPointer != L'\0')) {
    return -1;
  } else {
    return 0;
  }
}

static struct spdk_sock_impl_opts  g_spdk_edk_sock_impl_opts = {
  .recv_buf_size        = MIN_SO_RCVBUF_SIZE,
  .send_buf_size        = MIN_SO_SNDBUF_SIZE,
  .enable_recv_pipe     = false,
  .enable_zerocopy_send = false,
  .enable_quickack      = false,
  .enable_placement_id  = false,
};

VOID
EFIAPI
edk_sock_rx_notify (
  IN EFI_EVENT  Event,
  IN VOID       *Context
  )
{
  struct spdk_edk_sock  *sock;

  ASSERT (Event != NULL);
  ASSERT (Context != NULL);

  sock = (struct spdk_edk_sock *)Context;

  // SPDK_NOTICELOG ("Receive event triggered.\n");
  // SPDK_NOTICELOG ("Status: %r\n", sock->RxToken.Tcp4Token.CompletionToken.Status);
  sock->RxPending = TRUE;
  return;
}

EFI_STATUS
edk_sock_setup_rx_token (
  IN struct spdk_edk_sock  *sock
  )
{
  EFI_STATUS  Status;
  TCP_IO      *TcpIo;

  TcpIo = &sock->TcpIo;

  // Prepare TCP Rx token
  sock->RxData.DataLength                      = sock->RxFragment->Len;
  sock->RxData.FragmentCount                   = 1;
  sock->RxData.FragmentTable[0].FragmentLength = sock->RxFragment->Len;
  sock->RxData.FragmentTable[0].FragmentBuffer = sock->RxFragment->Bulk;

  sock->RxToken.Tcp4Token.Packet.RxData = &sock->RxData;
  sock->RxHead                          = 0;

  sock->RxToken.Tcp4Token.CompletionToken.Event = sock->RxEvent;

  // Put the packet on receive.
  sock->RxPending = FALSE;
  if (TcpIo->TcpVersion == TCP_VERSION_4) {
    Status = sock->TcpIo.Tcp.Tcp4->Receive (
                                     TcpIo->Tcp.Tcp4,
                                     &sock->RxToken.Tcp4Token
                                     );
  } else {
    Status = sock->TcpIo.Tcp.Tcp6->Receive (
                                     TcpIo->Tcp.Tcp6,
                                     &sock->RxToken.Tcp6Token
                                     );
  }

  return Status;
}

struct spdk_sock *
edk_sock_connect (
  const char             *ip,
  int                    port,
  struct spdk_sock_opts  *opts
  )
{
  struct                              spdk_edk_sock      *sock   = NULL;
  struct                              spdk_sock          *s_sock = NULL;
  struct                              spdk_edk_sock_ctx  *SockContext;
  char                                                   buf[MAX_TMPBUF];
  char                                                   *p     = NULL;
  EFI_STATUS                                             Status = EFI_SUCCESS;
  EFI_IPv6_ADDRESS                                       ip6_addr;
  EFI_IPv4_ADDRESS                                       ip4_addr;
  TCP_IO_CONFIG_DATA                                     *TcpIoConfig   = NULL;
  TCP4_IO_CONFIG_DATA                                    *Tcp4IoConfig  = NULL;
  TCP6_IO_CONFIG_DATA                                    *Tcp6IoConfig  = NULL;
  TCP_IO                                                 *TcpIo         = NULL;
  UINT8                                                  *Packet        = NULL;
  struct edk_spdk_sock_opts                              *edk_sock_opts = __edk_sock_opts (opts);

  opts = __spdk_sock_opts (edk_sock_opts);

  sock = calloc (1, sizeof (*sock));
  if (sock == NULL) {
    SPDK_ERRLOG ("sock allocation failed\n");
    goto Exit;
  }

  if (ip == NULL) {
    goto ErrorExit;
  }

  if (ip[0] == '[') {
    snprintf (buf, sizeof (buf), "%s", ip + 1);
    p = strchr (buf, ']');
    if (p != NULL) {
      *p = '\0';
    }

    ip = (const char *)&buf[0];
  }

  ASSERT (edk_sock_opts->ctx != NULL);

  SockContext   = (struct spdk_edk_sock_ctx *)edk_sock_opts->ctx;
  sock->Context = SockContext;
  TcpIoConfig   = &sock->TcpIoConfig;
  TcpIo         = &sock->TcpIo;

  if (!SockContext->IsIp6) {
    Tcp4IoConfig = &TcpIoConfig->Tcp4IoConfigData;
    edk_sock_strtoip4 (ip, &ip4_addr);
    CopyMem (&Tcp4IoConfig->LocalIp, &SockContext->StationIp.v4, sizeof (EFI_IPv4_ADDRESS));
    CopyMem (&Tcp4IoConfig->SubnetMask, &SockContext->SubnetMask.v4, sizeof (EFI_IPv4_ADDRESS));
    CopyMem (&Tcp4IoConfig->Gateway, &SockContext->GatewayIp.v4, sizeof (EFI_IPv4_ADDRESS));
    CopyMem (&Tcp4IoConfig->RemoteIp, &ip4_addr, sizeof (EFI_IPv4_ADDRESS));

    Tcp4IoConfig->RemotePort  = port;
    Tcp4IoConfig->ActiveFlag  = TRUE;
    Tcp4IoConfig->StationPort = 0;
  } else {
    Tcp6IoConfig = &TcpIoConfig->Tcp6IoConfigData;
    edk_sock_strtoip6 (ip, &ip6_addr);
    CopyMem (&Tcp6IoConfig->RemoteIp, &ip6_addr, sizeof (EFI_IPv6_ADDRESS));
    Tcp6IoConfig->RemotePort  = port;
    Tcp6IoConfig->ActiveFlag  = TRUE;
    Tcp6IoConfig->StationPort = 0;
  }

  //
  // Create the TCP IO for this connection.
  //
  ASSERT (gImageHandle != NULL);

  if (SockContext->Controller == NULL) {
    SPDK_ERRLOG ("Controller is NULL\n");
    goto ErrorExit;
  }

  Status = gBS->CreateEvent (
                  EVT_TIMER,
                  TPL_CALLBACK,
                  NULL,
                  NULL,
                  &sock->TimeoutEvent
                  );
  if (EFI_ERROR (Status)) {
    SPDK_ERRLOG ("Error setting timer: %r\n", Status);
    goto ErrorExit;
  }

  Status = TcpIoCreateSocket (
             gImageHandle,
             SockContext->Controller,
             (UINT8)(!SockContext->IsIp6 ? TCP_VERSION_4 : TCP_VERSION_6),
             TcpIoConfig,
             TcpIo
             );
  if (EFI_ERROR (Status)) {
    SPDK_ERRLOG ("Socket creation failed: %r\n", Status);
    goto ErrorExit2;
  }

  //
  // Start the timer, and wait Timeout seconds to establish the TCP connection.
  //
  Status = gBS->SetTimer (
                  sock->TimeoutEvent,
                  TimerRelative,
                  MultU64x32 (TIMEOUT, TICKS_PER_MS)
                  );
  if (EFI_ERROR (Status)) {
    SPDK_ERRLOG ("Error setting timer: %r\n", Status);
    goto ErrorExit3;
  }

  Status = TcpIoConnect (TcpIo, NULL);
  gBS->SetTimer (sock->TimeoutEvent, TimerCancel, 0);

  if (EFI_ERROR (Status)) {
    SPDK_ERRLOG ("TcpIoConnect error: %d\n", Status);
    goto ErrorExit3;
  }

  // TCP connection is established.
  // Get max TCP packet size
  if (TcpIo->TcpVersion == TCP_VERSION_4) {
    EFI_IP4_MODE_DATA  Ip4Config;

    Status = TcpIo->Tcp.Tcp4->GetModeData (
                                TcpIo->Tcp.Tcp4,
                                NULL,
                                NULL,
                                &Ip4Config,
                                NULL,
                                NULL
                                );
    ASSERT_EFI_ERROR (Status);
    SPDK_NOTICELOG ("IPv4 MaxPacketSize: %d\n", Ip4Config.MaxPacketSize);
    sock->MaxPduLen = Ip4Config.MaxPacketSize - sizeof (TCP_HEAD);
    SPDK_NOTICELOG ("sock->MaxPduLen: %d\n", sock->MaxPduLen);
  } else {
    EFI_IP6_MODE_DATA  Ip6Config;
    Status = TcpIo->Tcp.Tcp6->GetModeData (
                                TcpIo->Tcp.Tcp6,
                                NULL,
                                NULL,
                                &Ip6Config,
                                NULL,
                                NULL
                                );
    ASSERT_EFI_ERROR (Status);
    SPDK_NOTICELOG ("IPv6 MaxPacketSize: %d\n", Ip6Config.MaxPacketSize);
    sock->MaxPduLen = Ip6Config.MaxPacketSize - sizeof (TCP_HEAD);
    SPDK_NOTICELOG ("sock->MaxPduLen: %d\n", sock->MaxPduLen);
  }

  // Allocate Rx PDU.
  sock->Pdu = NetbufAlloc (sock->MaxPduLen);

  if (sock->Pdu == NULL) {
    SPDK_ERRLOG ("Failed to allocate PDU NET_BUF\n");
    ASSERT (FALSE);
    goto ErrorExit3;
  }

  // Allocate full range within the PDU
  Packet = NetbufAllocSpace (sock->Pdu, sock->MaxPduLen, NET_BUF_TAIL);

  if (Packet == NULL) {
    SPDK_ERRLOG ("Failed to allocate PDU space\n");
    ASSERT (FALSE);
    goto ErrorExit4;
  }

  // Allocate fragments (check if required)
  sock->RxFragment = AllocatePool (sizeof (NET_FRAGMENT));
  if (sock->RxFragment == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    goto ErrorExit4;
  }

  sock->RxFragmentCount = 1;
  NetbufBuildExt (sock->Pdu, sock->RxFragment, &sock->RxFragmentCount);

  // Create Rx event
  Status = gBS->CreateEvent (
                  EVT_NOTIFY_SIGNAL,
                  TPL_NOTIFY,
                  edk_sock_rx_notify,
                  sock,
                  &sock->RxEvent
                  );

  if (EFI_ERROR (Status)) {
    SPDK_ERRLOG ("Failed to create Rx event: %r\n", Status);
    ASSERT (FALSE);
    goto ErrorExit5;
  }

  // Setup Rx token
  Status = edk_sock_setup_rx_token (sock);

  if (EFI_ERROR (Status)) {
    SPDK_ERRLOG ("Failed to setup Rx token: %r\n", Status);
    ASSERT (FALSE);
    goto ErrorExit6;
  }

  SPDK_NOTICELOG ("Receive established.\n");

  sock->Context->TcpIo = TcpIo;
  s_sock               = &sock->base;
  goto Exit;

ErrorExit6:
  gBS->CloseEvent (sock->RxEvent);
ErrorExit5:
  FreePool (sock->RxFragment);
ErrorExit4:
  NetbufFree (sock->Pdu);
ErrorExit3:
  TcpIoDestroySocket (TcpIo);
ErrorExit2:
  gBS->CloseEvent (sock->TimeoutEvent);
ErrorExit:
  free (sock);
Exit:
  return s_sock;
}

int
edk_sock_close (
  struct spdk_sock  *_sock
  )
{
  struct spdk_edk_sock  *sock = __edk_sock (_sock);
  TCP_IO                *TcpIo;
  EFI_TCP4_PROTOCOL     *Tcp4;
  EFI_TCP6_PROTOCOL     *Tcp6;

  TcpIo = &sock->TcpIo;

  // Cancel Rx token
  if (TcpIo->TcpVersion == TCP_VERSION_4) {
    Tcp4 = TcpIo->Tcp.Tcp4;
    Tcp4->Cancel (Tcp4, &sock->RxToken.Tcp4Token.CompletionToken);
  } else {
    Tcp6 = TcpIo->Tcp.Tcp6;
    Tcp6->Cancel (Tcp6, &sock->RxToken.Tcp6Token.CompletionToken);
  }

  // Cleanup Rx context
  gBS->CloseEvent (sock->RxEvent);
  FreePool (sock->RxFragment);
  NetbufFree (sock->Pdu);

  assert (TAILQ_EMPTY (&_sock->pending_reqs));
  TcpIoDestroySocket (&sock->TcpIo);
  gBS->CloseEvent (sock->TimeoutEvent);
  sock->Context->TcpIo = NULL;
  free (sock);
  sock = NULL;

  return 0;
}

int
_sock_flush (
  struct spdk_sock  *s_sock
  )
{
  struct spdk_edk_sock      *sock  = __edk_sock (s_sock);
  int                       retval = 0;
  EFI_STATUS                Status = EFI_SUCCESS;
  struct iovec              iovs[64];
  int                       iovcnt      = 0;
  struct spdk_sock_request  *req        = NULL;
  int                       i           = 0;
  unsigned int              offset      = 0;
  char                      *writebuf   = NULL;
  char                      *bufptr     = NULL;
  int                       vecsize     = 0;
  NET_BUF                   *Pdu        = NULL;
  UINT8                     *Packet     = NULL;
  int                       req_counter = 0;

  if (s_sock->cb_cnt > 0) {
    SPDK_ERRLOG ("cb_cnt > 0 : %d\n", s_sock->cb_cnt);
    return 0;
  }

  /* Gather an iov */
  iovcnt = 0;
  req    = TAILQ_FIRST (&s_sock->queued_reqs);
  while (req) {
    if ((iovcnt + req->iovcnt)  > IOV_BATCH_SIZE) {
      break;
    }

    req_counter++;
    SPDK_DEBUGLOG (nvme, "Getting Queued Requests. req_counter:%d\n", req_counter);
    offset = req->internal.offset;

    for (i = 0; i < req->iovcnt; i++) {
      /* Consume any offset first */
      if (offset >= SPDK_SOCK_REQUEST_IOV (req, i)->iov_len) {
        offset -= SPDK_SOCK_REQUEST_IOV (req, i)->iov_len;
        continue;
      }

      iovs[iovcnt].iov_base = (uint8_t *)SPDK_SOCK_REQUEST_IOV (req, i)->iov_base + offset;
      iovs[iovcnt].iov_len  = SPDK_SOCK_REQUEST_IOV (req, i)->iov_len - offset;
      iovcnt++;

      offset = 0;

      if (iovcnt >= IOV_BATCH_SIZE) {
        break;
      }
    }

    if (iovcnt >= IOV_BATCH_SIZE) {
      break;
    }

    req = TAILQ_NEXT (req, internal.link);
  }

  if (iovcnt == 0) {
    SPDK_DEBUGLOG (nvme, "iovec count is 0\n");
    return 0;
  }

  // Get full count
  for (i = 0; i < iovcnt; i++) {
    vecsize += iovs[i].iov_len;
  }

  SPDK_DEBUGLOG (nvme, "Bytes to transmit: %d\n", vecsize);

  writebuf = (char *)AllocateZeroPool (vecsize);
  if (writebuf == NULL) {
    SPDK_DEBUGLOG (nvme, "malloc error\n");
    return -1;
  }

  bufptr = writebuf;
  int  index = 0;

  for (index = 0; index < iovcnt; index++) {
    CopyMem (bufptr, iovs[index].iov_base, iovs[index].iov_len);
    bufptr += iovs[index].iov_len;
  }

  Pdu = NetbufAlloc (vecsize);
  if (Pdu == NULL) {
    SPDK_DEBUGLOG (nvme, "Error while NetbufAlloc \n");
    return -1;
  }

  Packet = NetbufAllocSpace (Pdu, vecsize, NET_BUF_TAIL);
  if (Packet == NULL) {
    SPDK_DEBUGLOG (nvme, "Error while NetbufAllocSpace \n");
    retval = -1;
    free (writebuf);
    NetbufFree (Pdu);
    return retval;
  }

  CopyMem (Packet, writebuf, vecsize);

  NetbufCopy (Pdu, 0, vecsize, (UINT8 *)Packet);

  //
  // Send it to the NvmeOf target.
  //
  Status = TcpIoTransmit (&sock->TcpIo, Pdu);
  if (EFI_ERROR (Status)) {
    SPDK_ERRLOG ("Error while TcpIoTransmit .%d\n", Status);
    retval = -1;
    free (writebuf);
    NetbufFree (Pdu);
    return retval;
  }

  SPDK_DEBUGLOG (nvme, "Transmit Complete\n");

  req = TAILQ_FIRST (&s_sock->queued_reqs);
  for (index = 0; index < req_counter; index++) {
    s_sock->cb_cnt++;
    req->cb_fn (req->cb_arg, 0);
    TAILQ_REMOVE (&s_sock->queued_reqs, req, internal.link);
    s_sock->cb_cnt--;
    s_sock->queued_iovcnt -= req->iovcnt;
    if (s_sock->queued_iovcnt < 0) {
      s_sock->queued_iovcnt = 0;
    }

    req = TAILQ_NEXT (req, internal.link);
  }

  free (writebuf);
  NetbufFree (Pdu);
  return retval;
}

void
edk_sock_writev_async (
  struct spdk_sock          *s_sock,
  struct spdk_sock_request  *req
  )
{
  int  rc = 0;

  spdk_sock_request_queue (s_sock, req);

  /* If there are a sufficient number queued, just flush them out immediately. */
  if (s_sock->queued_iovcnt >= IOV_BATCH_SIZE) {
    rc = _sock_flush (s_sock);
    if (rc) {
      spdk_sock_abort_requests (s_sock);
    }
  }
}

static ssize_t
edk_sock_writev (
  struct spdk_sock  *s_sock,
  struct iovec      *iov,
  int               iovcnt
  )
{
  return 0;
}

int
edk_sock_flush (
  struct spdk_sock  *s_sock
  )
{
  return _sock_flush (s_sock);
}

ssize_t
edk_sock_readv (
  struct spdk_sock  *s_sock,
  struct iovec      *iov,
  int               iovcnt
  )
{
  struct spdk_edk_sock  *sock = __edk_sock (s_sock);

  EFI_STATUS         Status;
  UINTN              i;
  TCP_IO             *TcpIo;
  EFI_TCP4_PROTOCOL  *Tcp4;
  EFI_TCP6_PROTOCOL  *Tcp6;
  UINTN              TotalRequested = 0;
  UINTN              Available;
  NET_BUF            *Pdu;
  UINTN              TotalToCopy;
  UINTN              LeftToCopy;
  UINTN              ToCopy;
  UINTN              Copied;
  UINT8              *Payload;
  UINT8              *PayloadPtr;

  TcpIo = &sock->TcpIo;

  // Get full count
  for (i = 0; i < iovcnt; i++) {
    TotalRequested += iov[i].iov_len;
  }

  // SPDK_NOTICELOG ("Bytes to Read: %d\n", TotalRequested);

  if (TcpIo->TcpVersion == TCP_VERSION_4) {
    Tcp4 = TcpIo->Tcp.Tcp4;
    if (Tcp4 == NULL) {
      return EFI_DEVICE_ERROR;
    }
  } else {
    Tcp6 = TcpIo->Tcp.Tcp6;
    if (Tcp6 == NULL) {
      return EFI_DEVICE_ERROR;
    }
  }

  // Test if there is Rx pending
  if (!sock->RxPending) {
    // SPDK_NOTICELOG ("Rx not pending. Calling Tcp->Poll()\n");

    // No Rx pending. Poll once and re-check.
    if (TcpIo->TcpVersion == TCP_VERSION_4) {
      Tcp4->Poll (Tcp4);
    } else {
      Tcp6->Poll (Tcp6);
    }

    if (!sock->RxPending) {
      // Still no packets. Exit with nothing.
      // SPDK_NOTICELOG ("Still no packets. Return EAGAIN.\n");
      errno = EAGAIN;
      return -1;
    }
  }

  // Test if Rx token was successful.
  // If not, put Rx token back.
  if (TcpIo->TcpVersion == TCP_VERSION_4) {
    Status = sock->RxToken.Tcp4Token.CompletionToken.Status;
  } else {
    Status = sock->RxToken.Tcp6Token.CompletionToken.Status;
  }

  if (EFI_ERROR (Status)) {
    Status = edk_sock_setup_rx_token (sock);
    if (EFI_ERROR (Status)) {
      errno = EFAULT;
    } else {
      errno = EAGAIN;
    }

    return -1;
  }

  // There is Rx data pending. How much?
  Available = sock->RxData.DataLength - sock->RxHead;

  // SPDK_NOTICELOG ("Bytes available: %d\n", Available);

  Pdu         = sock->Pdu;
  Payload     = NetbufGetByte (Pdu, sock->RxHead, NULL);
  PayloadPtr  = Payload;
  TotalToCopy = MIN (Available, TotalRequested);
  LeftToCopy  = TotalToCopy;

  // SPDK_NOTICELOG ("Bytes to copy: %d\n", TotalToCopy);

  for (i = 0; (i < iovcnt) && (LeftToCopy > 0); i++) {
    ToCopy = MIN (iov[i].iov_len, LeftToCopy);

    // SPDK_NOTICELOG ("Copying %d bytes to iov[%d]\n", ToCopy, i);

    CopyMem (iov[i].iov_base, PayloadPtr, ToCopy);

    sock->RxHead += ToCopy;
    LeftToCopy   -= ToCopy;
  }

  Copied = TotalToCopy - LeftToCopy;
  // SPDK_NOTICELOG ("Bytes copied: %d\n", Copied);

  // Whole Rx packet was processed. Move Rx token back to TCP driver.
  if (Copied == Available) {
    // SPDK_NOTICELOG ("RX packet exhausted\n");
    Status = edk_sock_setup_rx_token (sock);
    if (EFI_ERROR (Status)) {
      errno = EFAULT;
      return -1;
    }
  }

  return Copied;
}

static int
edk_sock_getaddr (
  struct spdk_sock  *s_sock,
  char              *saddr,
  int               slen,
  uint16_t          *sport,
  char              *caddr,
  int               clen,
  uint16_t          *cport
  )
{
  return 0;
}

enum edk_sock_create_type {
  SPDK_SOCK_CREATE_CONNECT,
};

static int
edk_sock_set_recvbuf (
  struct spdk_sock  *s_sock,
  int               sz
  )
{
  return 0;
}

static int
edk_sock_set_sendbuf (
  struct spdk_sock  *s_sock,
  int               sz
  )
{
  return 0;
}

static struct spdk_sock *
edk_sock_listen (
  const char             *ip,
  int                    port,
  struct spdk_sock_opts  *opts
  )
{
  return 0;
}

static struct spdk_sock *
edk_sock_accept (
  struct spdk_sock  *_sock
  )
{
  return NULL;
}

static ssize_t
edk_sock_recv (
  struct spdk_sock  *s_sock,
  void              *buf,
  size_t            len
  )
{
  struct iovec  iov[1];

  iov[0].iov_base = buf;
  iov[0].iov_len  = len;

  return edk_sock_readv (s_sock, iov, 1);
}

static bool
edk_sock_is_ipv6 (
  struct spdk_sock  *_sock
  )
{
  struct spdk_edk_sock  *sock = __edk_sock (_sock);

  assert (sock != NULL);
  assert (sock->Context != NULL);

  return (sock->Context->IsIp6);
}

static bool
edk_sock_is_ipv4 (
  struct spdk_sock  *s_sock
  )
{
  struct spdk_edk_sock  *sock = __edk_sock (s_sock);

  assert (sock != NULL);
  assert (sock->Context != NULL);

  return (!sock->Context->IsIp6);
}

static bool
edk_sock_is_connected (
  struct spdk_sock  *s_sock
  )
{
  return true;
}

static struct spdk_sock_group_impl *
edk_sock_group_impl_create (
  void
  )
{
  return NULL;
}

static int
edk_sock_set_recvlowat (
  struct spdk_sock  *s_sock,
  int               nbytes
  )
{
  return 0;
}

static int
edk_sock_group_impl_add_sock (
  struct spdk_sock_group_impl  *_group,
  struct spdk_sock             *_sock
  )
{
  return 0;
}

static int
edk_sock_group_impl_remove_sock (
  struct spdk_sock_group_impl  *_group,
  struct spdk_sock             *_sock
  )
{
  return 0;
}

static int
edk_sock_group_impl_poll (
  struct spdk_sock_group_impl  *_group,
  int                          max_events,
  struct spdk_sock             **socks
  )
{
  return 0;
}

static int
edk_sock_group_impl_close (
  struct spdk_sock_group_impl  *_group
  )
{
  return 0;
}

static int
edk_sock_impl_get_opts (
  struct spdk_sock_impl_opts  *opts,
  size_t                      *len
  )
{
  if (!opts || !len) {
    errno = EINVAL;
    return -1;
  }

  memset (opts, 0, *len);

  #define FIELD_OK(field) \
  offsetof(struct spdk_sock_impl_opts, field) + sizeof(opts->field) <= *len

  #define GET_FIELD(field) \
  if (FIELD_OK(field)) { \
    opts->field = g_spdk_edk_sock_impl_opts.field; \
  }

  GET_FIELD (recv_buf_size);
  GET_FIELD (send_buf_size);
  GET_FIELD (enable_recv_pipe);
  GET_FIELD (enable_zerocopy_send);
  GET_FIELD (enable_quickack);
  GET_FIELD (enable_placement_id);

  #undef GET_FIELD
  #undef FIELD_OK

  *len = spdk_min (*len, sizeof (g_spdk_edk_sock_impl_opts));
  return 0;
}

static int
edk_sock_impl_set_opts (
  const struct spdk_sock_impl_opts  *opts,
  size_t                            len
  )
{
  if (!opts) {
    errno = EINVAL;
    return -1;
  }

  #define FIELD_OK(field) \
  offsetof(struct spdk_sock_impl_opts, field) + sizeof(opts->field) <= len

  #define SET_FIELD(field) \
  if (FIELD_OK(field)) { \
    g_spdk_edk_sock_impl_opts.field = opts->field; \
  }

  SET_FIELD (recv_buf_size);
  SET_FIELD (send_buf_size);
  SET_FIELD (enable_recv_pipe);
  SET_FIELD (enable_zerocopy_send);
  SET_FIELD (enable_quickack);
  SET_FIELD (enable_placement_id);

  #undef SET_FIELD
  #undef FIELD_OK
  return 0;
}

/*
 * opts The opts allocated in the current library.
 * opts_user The opts passed by the caller.
 * */
// Defined here for access of static function
static void
edk_sock_init_opts (
  struct spdk_sock_opts  *opts,
  struct spdk_sock_opts  *opts_user
  )
{
  assert (opts);
  assert (opts_user);

  opts->opts_size = sizeof (*opts);
  spdk_sock_get_default_opts (opts);

  /* reset the size according to the user */
  opts->opts_size = opts_user->opts_size;
  if (SPDK_SOCK_OPTS_FIELD_OK (opts, priority)) {
    opts->priority = opts_user->priority;
  }

  if (SPDK_SOCK_OPTS_FIELD_OK (opts, zcopy)) {
    opts->zcopy = opts_user->zcopy;
  }

  if (SPDK_SOCK_OPTS_FIELD_OK (opts, ack_timeout)) {
    opts->ack_timeout = opts_user->ack_timeout;
  }

  if (SPDK_SOCK_OPTS_FIELD_OK (opts, impl_opts)) {
    opts->impl_opts = opts_user->impl_opts;
  }

  if (SPDK_SOCK_OPTS_FIELD_OK (opts, impl_opts)) {
    opts->impl_opts_size = opts_user->impl_opts_size;
  }
}

struct spdk_net_impl  g_edksock_net_impl = {
  .name                   = "edksock",
  .getaddr                = edk_sock_getaddr,
  .connect                = edk_sock_connect,
  .listen                 = edk_sock_listen,
  .accept                 = edk_sock_accept,
  .close                  = edk_sock_close,
  .recv                   = edk_sock_recv,
  .readv                  = edk_sock_readv,
  .writev                 = edk_sock_writev,
  .writev_async           = edk_sock_writev_async,
  .flush                  = edk_sock_flush,
  .set_recvlowat          = edk_sock_set_recvlowat,
  .set_recvbuf            = edk_sock_set_recvbuf,
  .set_sendbuf            = edk_sock_set_sendbuf,
  .is_ipv6                = edk_sock_is_ipv6,
  .is_ipv4                = edk_sock_is_ipv4,
  .is_connected           = edk_sock_is_connected,
  .group_impl_create      = edk_sock_group_impl_create,
  .group_impl_add_sock    = edk_sock_group_impl_add_sock,
  .group_impl_remove_sock = edk_sock_group_impl_remove_sock,
  .group_impl_poll        = edk_sock_group_impl_poll,
  .group_impl_close       = edk_sock_group_impl_close,
  .get_opts               = edk_sock_impl_get_opts,
  .set_opts               = edk_sock_impl_set_opts,
};

/**
  Shim implementation of `spdk_sock_connect_ext`, adjusted for EDK2 use.
--
  Create a socket using the specific sock implementation, connect the socket
  to the specified address and port (of the server), and then return the socket.
  This function is used by client.

  @param ip IP address of the server.
  @param port Port number of the server.
  @param impl_name The sock_implementation to use, such as "posix". If impl_name is
  specified, it will *only* try to connect on that impl. If it is NULL, it will try
  all the sock implementations in order and uses the first sock implementation which
  can connect.
  @param opts The sock option pointer provided by the user which should not be NULL pointer.

  @return a pointer to the connected socket on success, or NULL on failure.
**/
struct spdk_sock *
edk_spdk_sock_connect_ext (
  const char             *ip,
  int                    port,
  const char             *_impl_name,
  struct spdk_sock_opts  *opts
  )
{
  struct spdk_sock           *sock;
  struct spdk_sock_opts      opts_local;
  struct edk_spdk_sock_opts  *edk_sock_opts = __edk_sock_opts (opts);

  opts = __spdk_sock_opts (edk_sock_opts);
  DEBUG ((DEBUG_INFO, "opts is %p\n", opts));
  DEBUG ((DEBUG_INFO, "edk_sock_opts->ctx = %p\n", edk_sock_opts->ctx));

  if (opts == NULL) {
    SPDK_ERRLOG ("the opts should not be NULL pointer\n");
    return NULL;
  }

  DEBUG ((DEBUG_INFO, "Creating a client socket using impl %a \n", g_edksock_net_impl.name));
  edk_sock_init_opts (&opts_local, opts);
  edk_sock_opts->base = &opts_local;
  sock                = g_edksock_net_impl.connect (ip, port, (struct spdk_sock_opts *)edk_sock_opts);
  if (sock != NULL) {
    /* Copy the contents, both the two structures are the same ABI version */
    memcpy (&sock->opts, &opts_local, sizeof (sock->opts));

    /* Clear out impl_opts to make sure we don't keep reference to a dangling
      * pointer */
    sock->opts.impl_opts = NULL;
    sock->net_impl       = &g_edksock_net_impl;
    TAILQ_INIT (&sock->queued_reqs);
    TAILQ_INIT (&sock->pending_reqs);

    return sock;
  }

  return NULL;
}

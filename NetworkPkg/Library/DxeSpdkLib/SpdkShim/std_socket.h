/*  $NetBSD: socket.h,v 1.82 2006/06/27 03:49:08 mrg Exp $  */

/*
 * Copyright (C) 1995, 1996, 1997, and 1998 WIDE Project.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Copyright (c) 1982, 1985, 1986, 1988, 1993, 1994
 * The Regents of the University of California. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * @(#)socket.h  8.6 (Berkeley) 5/3/95
 */

#ifndef _STD_SOCKET_H_
#define _STD_SOCKET_H_

#ifdef __cplusplus
extern "C" {
#endif

/*
* Data types.
*/
#ifndef sa_family_t
typedef __sa_family_t sa_family_t;
#define sa_family_t __sa_family_t
#endif

#ifndef socklen_t
typedef __socklen_t socklen_t;
#define socklen_t __socklen_t
#endif


#ifndef in_port_t
typedef __in_port_t in_port_t;
#define in_port_t __in_port_t
#endif

#ifndef in_addr_t
typedef __in_addr_t in_addr_t;
#define in_addr_t __in_addr_t
#endif

/*
 * Socket types.
 */
#define SOCK_STREAM     1       /* stream socket */
#define SOCK_DGRAM      2       /* datagram socket */
#define SOCK_RAW        3       /* raw-protocol interface */
#define SOCK_RDM        4       /* reliably-delivered message */
#define SOCK_SEQPACKET  5       /* sequenced packet stream */

/*
* Option flags per-socket.
*/
#define SO_DEBUG        0x0001      /* turn on debugging info recording */
#define SO_ACCEPTCONN   0x0002      /* socket has had listen() */
#define SO_REUSEADDR    0x0004      /* allow local address reuse */
#define SO_KEEPALIVE    0x0008      /* keep connections alive */
#define SO_DONTROUTE    0x0010      /* just use interface addresses */
#define SO_BROADCAST    0x0020      /* permit sending of broadcast msgs */
#define SO_USELOOPBACK  0x0040      /* bypass hardware when possible */
#define SO_LINGER       0x0080      /* linger on close if data present */
#define SO_OOBINLINE    0x0100      /* leave received OOB data in line */
#define SO_REUSEPORT    0x0200      /* allow local address & port reuse */
#define SO_TIMESTAMP    0x0400      /* timestamp received dgram traffic */


/*
* Additional options, not kept in so_options.
*/
#define SO_SNDBUF       0x1001      /* send buffer size */
#define SO_RCVBUF       0x1002      /* receive buffer size */
#define SO_SNDLOWAT     0x1003      /* send low-water mark */
#define SO_RCVLOWAT     0x1004      /* receive low-water mark */
#define SO_SNDTIMEO     0x1005      /* send timeout */
#define SO_RCVTIMEO     0x1006      /* receive timeout */
#define SO_ERROR        0x1007      /* get error status and clear */
#define SO_TYPE         0x1008      /* get socket type */
#define SO_OVERFLOWED   0x1009      /* datagrams: return packets dropped */

     /*
    * Structure used for manipulating linger option.
    */
struct  linger {
    int l_onoff;        /* option on/off */
    int l_linger;       /* linger time in seconds */
};

/*
 * Level number for (get/set)sockopt() to apply to socket itself.
 */
#define SOL_SOCKET  0xffff      /* options for socket level */

 /*
    * Address families.
    */
#define AF_UNSPEC       0           /* unspecified */
#define AF_LOCAL        1           /* local to host (pipes, portals) */
#define AF_UNIX         AF_LOCAL    /* backward compatibility */
#define AF_INET         2           /* internetwork: UDP, TCP, etc. */
#define AF_IMPLINK      3           /* arpanet imp addresses */
#define AF_PUP          4           /* pup protocols: e.g. BSP */
#define AF_CHAOS        5           /* mit CHAOS protocols */
#define AF_NS           6           /* XEROX NS protocols */
#define AF_ISO          7           /* ISO protocols */
#define AF_OSI          AF_ISO
#define AF_ECMA         8           /* european computer manufacturers */
#define AF_DATAKIT      9           /* datakit protocols */
#define AF_CCITT        10          /* CCITT protocols, X.25 etc */
#define AF_SNA          11          /* IBM SNA */
#define AF_DECnet       12          /* DECnet */
#define AF_DLI          13          /* DEC Direct data link interface */
#define AF_LAT          14          /* LAT */
#define AF_HYLINK       15          /* NSC Hyperchannel */
#define AF_APPLETALK    16          /* Apple Talk */
#define AF_ROUTE        17          /* Internal Routing Protocol */
#define AF_LINK         18          /* Link layer interface */

#define AF_COIP         20          /* connection-oriented IP, aka ST II */
#define AF_CNT          21          /* Computer Network Technology */

#define AF_IPX          23          /* Novell Internet Protocol */
#define AF_INET6        24          /* IP version 6 */

#define AF_ISDN         26          /* Integrated Services Digital Network*/
#define AF_E164         AF_ISDN     /* CCITT E.164 recommendation */
#define AF_NATM         27          /* native ATM access */
#define AF_ARP          28          /* (rev.) addr. res. prot. (RFC 826) */

#define AF_BLUETOOTH    31

#define AF_MAX          32

#define _SS_MAXSIZE     128
#define _SS_ALIGNSIZE   (sizeof(int64_t))
#define _SS_PAD1SIZE    (_SS_ALIGNSIZE - 2)
#define _SS_PAD2SIZE    (_SS_MAXSIZE - 2 - \
                _SS_PAD1SIZE - _SS_ALIGNSIZE)

/*
* Structure used by kernel to store most
* addresses.
*/
struct sockaddr {
    uint8_t     sa_len;         /* total length */
    sa_family_t sa_family;      /* address family */
    char        sa_data[14];    /* actually longer; address value */
};

struct sockaddr_storage {
    uint8_t     ss_len;         /* address length */
    sa_family_t ss_family;      /* address family */
    char        __ss_pad1[_SS_PAD1SIZE];
    int64_t     __ss_align;     /* force desired structure storage alignment */
    char        __ss_pad2[_SS_PAD2SIZE];
};
struct addrinfo {
    int                             ai_flags;       /**< AI_PASSIVE, AI_CANONNAME */
    int                             ai_family;      /**< PF_xxx */
    int                             ai_socktype;    /**< SOCK_xxx */
    int                             ai_protocol;    /**< 0 or IPPROTO_xxx for IPv4 and IPv6 */
    socklen_t                       ai_addrlen;     /**< length of ai_addr */
#if defined(__alpha__) || (defined(__i386__) && defined(_LP64))
    int                             __ai_pad0;      /* ABI compatibility */
#endif
    char                             *ai_canonname; /**< canonical name for hostname */
    struct sockaddr                  *ai_addr;      /**< binary address */
    struct addrinfo                  *ai_next;      /**< next structure in linked list */
};

struct in_addr {
    in_addr_t s_addr;
};
/*
 * Socket address, internet style.
 */
struct sockaddr_in {
    uint8_t         sin_len;
    sa_family_t     sin_family;
    in_port_t       sin_port;
    struct in_addr  sin_addr;
    __int8_t        sin_zero[8];
};

#endif

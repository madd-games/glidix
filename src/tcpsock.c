/*
	Glidix kernel

	Copyright (c) 2014-2016, Madd Games.
	All rights reserved.
	
	Redistribution and use in source and binary forms, with or without
	modification, are permitted provided that the following conditions are met:
	
	* Redistributions of source code must retain the above copyright notice, this
	  list of conditions and the following disclaimer.
	
	* Redistributions in binary form must reproduce the above copyright notice,
	  this list of conditions and the following disclaimer in the documentation
	  and/or other materials provided with the distribution.
	
	THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
	AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
	IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
	DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
	FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
	DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
	SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
	CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
	OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
	OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <glidix/socket.h>
#include <glidix/memory.h>
#include <glidix/string.h>
#include <glidix/sched.h>
#include <glidix/errno.h>
#include <glidix/semaphore.h>
#include <glidix/netif.h>
#include <glidix/console.h>
#include <glidix/common.h>

/**
 * TCP socket states.
 */
#define	TCP_CLOSED				0
#define	TCP_LISTEN				1
#define	TCP_CONNECTING				2
#define	TCP_ESTABLISHED				3

/**
 * TCP segment flags
 */
#define	TCP_FIN					(1 << 0)
#define	TCP_SYN					(1 << 1)
#define	TCP_RST					(1 << 2)
#define	TCP_PSH					(1 << 3)
#define	TCP_ACK					(1 << 4)
#define	TCP_URG					(1 << 5)
#define	TCP_ECE					(1 << 6)
#define	TCP_CWR					(1 << 7)

typedef struct
{
	uint16_t				srcport;
	uint16_t				dstport;
	uint32_t				seqno;
	uint32_t				ackno;
	uint8_t					dataOffsetNS;
	uint8_t					flags;
	uint16_t				winsz;
	uint16_t				checksum;
	uint16_t				urgptr;
} PACKED TCPSegment;

/**
 * TCP SOCKET
 *
 * Implementation of the TCP protocol in Glidix.
 */
typedef struct
{
	Socket					header_;
	struct sockaddr				sockname;
	struct sockaddr				peername;
	int					shutflags;
	uint32_t				nextSeqNo;
	uint32_t				nextAckNo;
	int					state;
} TCPSocket;

static int tcpsock_bind(Socket *sock, const struct sockaddr *addr, size_t addrlen)
{
	TCPSocket *tcpsock = (TCPSocket*) sock;
	if (addrlen < sizeof(struct sockaddr))
	{
		ERRNO = EAFNOSUPPORT;
		return -1;
	};
	
	if (tcpsock->state != TCP_CLOSED)
	{
		ERRNO = EAFNOSUPPORT;
		return -1;
	};
	
	if (tcpsock->sockname.sa_family != AF_UNSPEC)
	{
		ERRNO = EINVAL;
		return -1;
	};
	
	if (addr->sa_family != sock->domain)
	{
		ERRNO = EAFNOSUPPORT;
		return -1;
	};
	
	uint16_t port;
	if (addr->sa_family == AF_INET)
	{
		const struct sockaddr_in *inaddr = (const struct sockaddr_in*) addr;
		port = inaddr->sin_port;
	}
	else
	{
		const struct sockaddr_in6 *inaddr = (const struct sockaddr_in6*) addr;
		port = inaddr->sin6_port;
	};
	
	// convert port to machine order and then ensure we have permission to bind to it
	port = __builtin_bswap16(port);
	if (port < 1024)
	{
		if (getCurrentThread()->creds->euid != 0)
		{
			// only root can bind to ports below 1024
			ERRNO = EACCES;
			return -1;
		};
	};
	
	if (port >= 49152)
	{
		// nobody can bind to ephemeral ports explicitly
		ERRNO = EACCES;
		return -1;
	};
	
	if (ClaimSocketAddr(addr, &tcpsock->sockname) != 0)
	{
		ERRNO = EADDRINUSE;
		return -1;
	};
	
	return 0;
};

Socket *CreateTCPSocket()
{
	TCPSocket *tcpsock = NEW(TCPSocket);
	memset(tcpsock, 0, sizeof(TCPSocket));
	Socket *sock = (Socket*) tcpsock;
	
	sock->bind = tcpsock_bind;
	
	return sock;
};

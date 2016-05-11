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
 * Entry in the inbound queue.
 */
typedef struct UDPInbound_
{
	struct UDPInbound_*			next;
	struct sockaddr				srcaddr;
	uint64_t				socklen;
	size_t					size;
	char					payload[];
} UDPInbound;

/**
 * UDP SOCKET
 *
 * Pretty much a standard implementation. bind() must be used to listen to messages on a port.
 * sendto() and recvfrom() send/receive messages, connect() sets the default peer.
 */
typedef struct
{
	Socket					header_;
	struct sockaddr				sockname;
	struct sockaddr				peername;
	int					shutflags;
	Semaphore				queueLock;
	Semaphore				counter;
	UDPInbound*				first;
	UDPInbound*				last;
} UDPSocket;

typedef struct
{
	uint16_t				srcport;
	uint16_t				dstport;
	uint16_t				len;
	uint16_t				checksum;
	uint8_t					payload[];
} PACKED UDPPacket;

/**
 * UDP packet with IPv6 pseudo-header for checksum calculation.
 */
typedef struct
{
	uint8_t					srcaddr[16];
	uint8_t					dstaddr[16];
	uint16_t				udplenHigh;		// this is always zeroes
	uint16_t				udplen;
	uint8_t					zeroes[3];
	uint8_t					proto;			// IPPROTO_UDP
	UDPPacket				packet;
} UDPEncap;

static int udpsock_bind(Socket *sock, const struct sockaddr *addr, size_t addrlen)
{
	UDPSocket *udpsock = (UDPSocket*) sock;
	if (addrlen < sizeof(struct sockaddr))
	{
		ERRNO = EAFNOSUPPORT;
		return -1;
	};
	
	if (udpsock->sockname.sa_family != AF_UNSPEC)
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
	
	if (ClaimSocketAddr(addr, &udpsock->sockname) != 0)
	{
		ERRNO = EADDRINUSE;
		return -1;
	};
	
	return 0;
};

static int udpsock_getsockname(Socket *sock, struct sockaddr *addr, size_t *addrlenptr)
{
	size_t addrlen = *addrlenptr;
	if (addrlen > sizeof(struct sockaddr))
	{
		addrlen = (*addrlenptr) = sizeof(struct sockaddr);
	};
	
	UDPSocket *udpsock = (UDPSocket*) sock;
	memcpy(addr, &udpsock->sockname, addrlen);
	return 0;
};

static int udpsock_connect(Socket *sock, const struct sockaddr *addr, size_t addrlen)
{
	UDPSocket *udpsock = (UDPSocket*) sock;
	if (addrlen < sizeof(struct sockaddr))
	{
		ERRNO = EAFNOSUPPORT;
		return -1;
	};
	
	if (addr->sa_family != sock->domain)
	{
		ERRNO = EAFNOSUPPORT;
		return -1;
	};
	
	if (udpsock->sockname.sa_family == AF_UNSPEC)
	{
		// bind to an ephemeral port
		if (sock->domain == AF_INET)
		{
			// the address is already zeroed (by the memset() during the creation of the socket)
			struct sockaddr_in *inaddr = (struct sockaddr_in*) &udpsock->sockname;
			inaddr->sin_family = AF_INET;
			inaddr->sin_port = AllocPort();
		}
		else
		{
			struct sockaddr_in6 *inaddr = (struct sockaddr_in6*) &udpsock->sockname;
			inaddr->sin6_family = AF_INET6;
			inaddr->sin6_port = AllocPort();
		};
	};
	
	memcpy(&udpsock->peername, addr, sizeof(struct sockaddr));
	return 0;
};

static int udpsock_getpeername(Socket *sock, struct sockaddr *addr, size_t *addrlenptr)
{
	size_t addrlen = *addrlenptr;
	if (addrlen > sizeof(struct sockaddr))
	{
		addrlen = (*addrlenptr) = sizeof(struct sockaddr);
	};
	
	UDPSocket *udpsock = (UDPSocket*) sock;
	memcpy(addr, &udpsock->peername, addrlen);
	return 0;
};

static void udpsock_shutdown(Socket *sock, int how)
{
	UDPSocket *udpsock = (UDPSocket*) sock;
	udpsock->shutflags |= how;
};

static ssize_t udpsock_sendto(Socket *sock, const void *message, size_t msgsize, int flags, const struct sockaddr *addr, size_t addrlen)
{
	UDPSocket *udpsock = (UDPSocket*) sock;
	struct sockaddr destaddr;
	
	if (udpsock->shutflags & SHUT_WR)
	{
		ERRNO = EPIPE;
		return -1;
	};
	
	if (msgsize > 65528)
	{
		ERRNO = EMSGSIZE;
		return -1;
	};

	if (addr != NULL)
	{
		if (addrlen < sizeof(struct sockaddr))
		{
			ERRNO = EAFNOSUPPORT;
			return -1;
		};
		
		if (addr->sa_family != sock->domain)
		{
			ERRNO = EAFNOSUPPORT;
			return -1;
		};
		
		memcpy(&destaddr, addr, sizeof(struct sockaddr));
	}
	else
	{
		if (udpsock->peername.sa_family == AF_UNSPEC)
		{
			ERRNO = EDESTADDRREQ;
			return -1;
		};
		
		memcpy(&destaddr, &udpsock->peername, sizeof(struct sockaddr));
	};
	
	if (destaddr.sa_family != sock->domain)
	{
		ERRNO = EAFNOSUPPORT;
		return -1;
	};
	
	if (udpsock->sockname.sa_family == AF_UNSPEC)
	{
		// bind to an ephemeral port
		if (sock->domain == AF_INET)
		{
			// the address is already zeroed (by the memset() during the creation of the socket)
			struct sockaddr_in *inaddr = (struct sockaddr_in*) &udpsock->sockname;
			inaddr->sin_family = AF_INET;
			inaddr->sin_port = AllocPort();
		}
		else
		{
			struct sockaddr_in6 *inaddr = (struct sockaddr_in6*) &udpsock->sockname;
			inaddr->sin6_family = AF_INET6;
			inaddr->sin6_port = AllocPort();
		};
	};
	
	if (sock->domain == AF_INET)
	{
		struct sockaddr_in *insrc = (struct sockaddr_in*) &udpsock->sockname;
		struct sockaddr_in *indst = (struct sockaddr_in*) &destaddr;
		
		UDPPacket *packet = (UDPPacket*) kmalloc(sizeof(UDPPacket) + msgsize);
		packet->srcport = insrc->sin_port;
		packet->dstport = indst->sin_port;
		packet->len = __builtin_bswap16((uint16_t) msgsize+8);
		packet->checksum = 0;
		memcpy(packet->payload, message, msgsize);
		
		int status = sendPacket(&udpsock->sockname, &destaddr, packet, sizeof(UDPPacket) + msgsize,
					IPPROTO_UDP | (sock->options[GSO_SNDFLAGS] & PKT_MASK), sock->options[GSO_SNDTIMEO], sock->ifname);
		kfree(packet);
		
		if (status < 0)
		{
			ERRNO = -status;
			return (ssize_t)-1;
		};
	}
	else
	{
		// IPv6
		static uint8_t zeroes[16] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
		struct sockaddr_in6 *insrc = (struct sockaddr_in6*) &udpsock->sockname;
		struct sockaddr_in6 *indst = (struct sockaddr_in6*) &destaddr;
		
		// we have to do this because the checksum requires the source address to be known.
		if (memcmp(&insrc->sin6_addr, zeroes, 16) == 0)
		{
			getDefaultAddr6(&insrc->sin6_addr, &indst->sin6_addr);
		};
		
		size_t packetSize = sizeof(UDPPacket) + msgsize;
		UDPEncap *encap = (UDPEncap*) kmalloc(sizeof(UDPEncap) + msgsize);
		memset(encap, 0, sizeof(UDPEncap));
		memcpy(encap->srcaddr, &insrc->sin6_addr, 16);
		memcpy(encap->dstaddr, &indst->sin6_addr, 16);
		encap->udplen = __builtin_bswap16((uint16_t) msgsize+8);
		encap->proto = IPPROTO_UDP;
		encap->packet.srcport = insrc->sin6_port;
		encap->packet.dstport = indst->sin6_port;
		encap->packet.len = encap->udplen;
		encap->packet.checksum = 0;
		memcpy(encap->packet.payload, message, msgsize);
		encap->packet.checksum = ipv4_checksum(encap, sizeof(UDPEncap) + msgsize);
		
		int status = sendPacket(&udpsock->sockname, &destaddr, &encap->packet, packetSize,
					IPPROTO_UDP | (sock->options[GSO_SNDFLAGS] & PKT_MASK), sock->options[GSO_SNDTIMEO], sock->ifname);
		kfree(encap);
		
		if (status < 0)
		{
			ERRNO = -status;
			return (ssize_t) -1;
		};
	};
	
	return (ssize_t) msgsize;
};

static void udpsock_packet(Socket *sock, const struct sockaddr *src, const struct sockaddr *dest, size_t addrlen,
			const void *packet, size_t size, int proto)
{
	static uint8_t zeroes[16] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
	UDPSocket *udpsock = (UDPSocket*) sock;
	if (proto != IPPROTO_UDP)
	{
		return;
	};
	
	if (dest->sa_family != sock->domain)
	{
		return;
	};

	if (udpsock->sockname.sa_family == AF_UNSPEC)
	{
		// not bound
		return;
	};
	
	UDPPacket *udp = (UDPPacket*) packet;
	
	if (sock->domain == AF_INET)
	{
		struct sockaddr_in *sockname = (struct sockaddr_in*) &udpsock->sockname;
		const struct sockaddr_in *indst = (const struct sockaddr_in*) dest;
		
		if (sockname->sin_addr.s_addr != 0)
		{
			if (sockname->sin_addr.s_addr != indst->sin_addr.s_addr)
			{
				// not destined to this socket
				return;
			};
		};
		
		if (sockname->sin_port != udp->dstport)
		{
			// not destined to this port
			return;
		};
	}
	else
	{
		struct sockaddr_in6 *sockname = (struct sockaddr_in6*) &udpsock->sockname;
		const struct sockaddr_in6 *indst = (const struct sockaddr_in6*) dest;
		
		if (memcmp(&sockname->sin6_addr, zeroes, 16) != 0)
		{
			if (memcmp(&sockname->sin6_addr, &indst->sin6_addr, 16) != 0)
			{
				// not destined to this socket
				return;
			};
		};
		
		if (sockname->sin6_port != udp->dstport)
		{
			// not destined to this port
			return;
		};
	};
	
	// the packet is destined to us. don't bother with checksums since the data is protected by link layer
	// protocols anyway, but make sure the size field is valid
	if (__builtin_bswap16(udp->len) > size)
	{
		return;
	};
	
	UDPInbound *inbound = (UDPInbound*) kmalloc(sizeof(UDPInbound) + __builtin_bswap16(udp->len));
	inbound->next = NULL;
	
	// copy the source address into the inbound desription but also place the port number in
	memcpy(&inbound->srcaddr, src, sizeof(struct sockaddr));
	if (src->sa_family == AF_INET)
	{
		((struct sockaddr_in*)&inbound->srcaddr)->sin_port = udp->srcport;
		inbound->socklen = sizeof(struct sockaddr_in);
	}
	else
	{
		((struct sockaddr_in6*)&inbound->srcaddr)->sin6_port = udp->srcport;
		inbound->socklen = sizeof(struct sockaddr_in6);
	};
	
	inbound->size = (size_t) __builtin_bswap16(udp->len) - 8;
	memcpy(inbound->payload, udp->payload, inbound->size);
	
	semInit(&udpsock->queueLock);
	if (udpsock->last == NULL)
	{
		udpsock->first = udpsock->last = inbound;
	}
	else
	{
		udpsock->last->next = inbound;
		udpsock->last = inbound;
	};
	semSignal(&udpsock->queueLock);
	
	semSignal(&udpsock->counter);
};

static ssize_t udpsock_recvfrom(Socket *sock, void *buffer, size_t len, int flags, struct sockaddr *addr, size_t *addrlen)
{
	UDPSocket *udpsock = (UDPSocket*) sock;
	
	if (sock->fp->oflag & O_NONBLOCK)
	{
		if (semWaitNoblock(&udpsock->counter, 1) == -1)
		{
			getCurrentThread()->therrno = EWOULDBLOCK;
			return -1;
		};
	}
	else
	{
		int status = semWaitTimeout(&udpsock->counter, 1, sock->options[GSO_RCVTIMEO]);
		
		if (status < 0)
		{
			if (status == SEM_INTERRUPT)
			{
				ERRNO = EINTR;
			}
			else
			{
				ERRNO = ETIMEDOUT;
			};
			
			return -1;
		};
	};
	
	// packet inbound
	semWait(&udpsock->queueLock);
	UDPInbound *inbound = udpsock->first;
	
	if ((flags & MSG_PEEK) == 0)
	{
		udpsock->first = inbound->next;
		if (udpsock->first == NULL)
		{
			udpsock->last = NULL;
		};
	};
	
	if (len > inbound->size)
	{
		len = inbound->size;
	};
	
	size_t realAddrLen = inbound->socklen;
	if (addrlen != NULL)
	{
		if ((*addrlen) < realAddrLen)
		{
			realAddrLen = *addrlen;
		}
		else
		{
			*addrlen = realAddrLen;
		};
	}
	else
	{
		realAddrLen = 0;
	};
	
	if (addr != NULL)
	{
		memcpy(addr, &inbound->srcaddr, realAddrLen);
	};
	
	memcpy(buffer, inbound->payload, len);
	semSignal(&udpsock->queueLock);
	
	if ((flags & MSG_PEEK) == 0)
	{
		kfree(inbound);
	}
	else
	{
		// just peeking so "return" the packet
		semSignal2(&udpsock->counter, 1);
	};
	
	return (ssize_t) len;
};

Socket *CreateUDPSocket()
{
	UDPSocket *udpsock = NEW(UDPSocket);
	memset(udpsock, 0, sizeof(UDPSocket));
	Socket *sock = (Socket*) udpsock;
	
	sock->bind = udpsock_bind;
	sock->getsockname = udpsock_getsockname;
	sock->connect = udpsock_connect;
	sock->getpeername = udpsock_getpeername;
	sock->shutdown = udpsock_shutdown;
	sock->sendto = udpsock_sendto;
	sock->packet = udpsock_packet;
	sock->recvfrom = udpsock_recvfrom;

	semInit(&udpsock->queueLock);
	semInit2(&udpsock->counter, 0);
	
	return sock;
};

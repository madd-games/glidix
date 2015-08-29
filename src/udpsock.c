/*
	Glidix kernel

	Copyright (c) 2014-2015, Madd Games.
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

typedef struct UDPIncomingMessage_
{
	struct UDPIncomingMessage_*	next;
	struct sockaddr			src;
	size_t				size;
	char				payload[];
} UDPIncomingMessage;

/**
 * UDP SOCKET
 *
 * Used for transmitted UDP datagrams; it has the type SOCK_DGRAM and protocol IPPROTO_UDP. If no protocol
 * was given to a socket() call with type SOCK_DGRAM, then UDP is the default.
 */
typedef struct
{
	Socket				header_;
	struct sockaddr			sockname;		// AF_UNSPEC meaning unspecified
	struct sockaddr			peername;		// AF_UNSPEC meaning unspecified
	int				shutflags;		// starts at zero, set with shutdown() calls
	UDPIncomingMessage*		queue;
	Semaphore			queueLock;
	Semaphore			packetCounter;
} UDPSocket;

/**
 * The UDP header. NOTE: This is in NETWORK byte order!
 */
typedef struct
{
	uint16_t			srcport;
	uint16_t			dstport;
	uint16_t			len;
	uint16_t			checksum;
	char				payload[];
} PACKED UDPHeader;

/**
 * The IPv6 pseudo-header, used in checksum calculation.
 */
typedef struct
{
	char				srcaddr[16];
	char				dstaddr[16];
	uint32_t			udplen;
	uint8_t				zeroes[3];
	uint8_t				proto;
	UDPHeader			udp;
} PACKED PseudoHeader;

typedef struct
{
	IPHeader4			ip;
	UDPHeader			udp;
} PACKED UDPPacket4;

typedef struct
{
	IPHeader6			ip;
	UDPHeader			udp;
} PACKED UDPPacket6;

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
		if (getCurrentThread()->euid != 0)
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

int udpsock_getsockname(Socket *sock, struct sockaddr *addr, size_t *addrlenptr)
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

int udpsock_connect(Socket *sock, const struct sockaddr *addr, size_t addrlen)
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

int udpsock_getpeername(Socket *sock, struct sockaddr *addr, size_t *addrlenptr)
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

void udpsock_shutdown(Socket *sock, int how)
{
	UDPSocket *udpsock = (UDPSocket*) sock;
	udpsock->shutflags |= how;
};

ssize_t udpsock_sendto(Socket *sock, const void *message, size_t msgsize, int flags, const struct sockaddr *addr, size_t addrlen)
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
		// in IPv4, we don't need to bother with checksums (just set the checksum field to zero).
		// so basically we just wrap the payload in an IP and UDP packet.
		size_t packsize = sizeof(UDPPacket4) + msgsize;
		UDPPacket4 *packet = (UDPPacket4*) kmalloc(packsize);
		
		const struct sockaddr_in *insrc = (const struct sockaddr_in*) &udpsock->sockname;
		const struct sockaddr_in *indst = (const struct sockaddr_in*) &destaddr;
		
		PacketInfo4 info;
		info.saddr.s_addr = insrc->sin_addr.s_addr;
		info.daddr.s_addr = indst->sin_addr.s_addr;
		info.proto = IPPROTO_UDP;
		info.size = sizeof(UDPHeader) + msgsize;
		info.hop = 64;
		ipv4_info2header(&info, &packet->ip);
		
		packet->udp.srcport = insrc->sin_port;
		packet->udp.dstport = indst->sin_port;
		packet->udp.len = __builtin_bswap16((uint16_t) (sizeof(UDPHeader) + msgsize));
		packet->udp.checksum = 0;
		
		memcpy(packet->udp.payload, message, msgsize);
		if (sendPacket(&destaddr, packet, packsize) != 0)
		{
			kfree(packet);
			ERRNO = ENETUNREACH;
			return -1;
		};
		
		kfree(packet);
		return 0;
	}
	else
	{
		// in IPv6 we must calculate the checksum. so what we'll do first is wrap the payload in a UDP packet
		// and an IPv6 pseudo-header, compute the checksum, then unwrap the UDP header and payload and wrap it
		// into an IPv6 packet.
		PseudoHeader *pseudo = (PseudoHeader*) kmalloc(sizeof(PseudoHeader) + msgsize);
		
		const struct sockaddr_in6 *insrc = (const struct sockaddr_in6*) &udpsock->sockname;
		const struct sockaddr_in6 *indst = (const struct sockaddr_in6*) &destaddr;
		
		memcpy(pseudo->srcaddr, &insrc->sin6_addr, 16);
		memcpy(pseudo->dstaddr, &indst->sin6_addr, 16);
		pseudo->udplen = __builtin_bswap32((uint32_t) (sizeof(UDPHeader) + msgsize));
		memset(pseudo->zeroes, 0, 3);
		pseudo->proto = IPPROTO_UDP;
		
		pseudo->udp.srcport = insrc->sin6_port;
		pseudo->udp.dstport = indst->sin6_port;
		pseudo->udp.len = __builtin_bswap16((uint16_t) (sizeof(UDPHeader) + msgsize));
		pseudo->udp.checksum = 0;			// zero before calculation
		memcpy(pseudo->udp.payload, message, msgsize);
		pseudo->udp.checksum = ipv4_checksum(pseudo, sizeof(PseudoHeader) + msgsize);
		
		// now unwrap the UDP data and wrap it into an IPv6+UDP packet.
		size_t packsize = sizeof(UDPPacket6) + msgsize;
		UDPPacket6 *packet = (UDPPacket6*) kmalloc(packsize);
		
		PacketInfo6 info;
		memcpy(&info.saddr, &insrc->sin6_addr, 16);
		memcpy(&info.daddr, &indst->sin6_addr, 16);
		info.proto = IPPROTO_UDP;
		info.size = sizeof(UDPHeader) + msgsize;
		info.hop = 64;
		info.flowinfo = indst->sin6_flowinfo;
		ipv6_info2header(&info, &packet->ip);
		memcpy(&packet->udp, &pseudo->udp, sizeof(UDPHeader) + msgsize);
		
		// we can now free the pseudo-header
		kfree(pseudo);
		
		if (sendPacket(&destaddr, packet, packsize) != 0)
		{
			kfree(packet);
			ERRNO = ENETUNREACH;
			return -1;
		};
		
		kfree(packet);
		return 0;
	};
};

Socket* CreateUDPSocket()
{
	UDPSocket *udpsock = (UDPSocket*) kmalloc(sizeof(UDPSocket));
	memset(udpsock, 0, sizeof(UDPSocket));
	Socket *sock = (Socket*) udpsock;
	
	semInit(&udpsock->queueLock);
	semInit2(&udpsock->packetCounter, 0);
	
	sock->bind = udpsock_bind;
	sock->getsockname = udpsock_getsockname;
	sock->connect = udpsock_connect;
	sock->getpeername = udpsock_getpeername;
	sock->shutdown = udpsock_shutdown;
	sock->sendto = udpsock_sendto;
	
	return sock;
};

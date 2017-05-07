/*
	Glidix kernel

	Copyright (c) 2014-2017, Madd Games.
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

#define	UDP_ALLOWED_SNDFLAGS			(PKT_DONTROUTE|PKT_DONTFRAG)

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
 * Entry in the socket multicast group list.
 */
typedef struct
{
	struct in6_addr				addr;
	uint32_t				scope;
} UDPGroup;

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
	UDPGroup*				groups;
	size_t					numGroups;
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
	if (addrlen < INET_SOCKADDR_LEN)
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
	else if (addr->sa_family == AF_INET6)
	{
		const struct sockaddr_in6 *inaddr = (const struct sockaddr_in6*) addr;
		port = inaddr->sin6_port;
	}
	else
	{
		ERRNO = EAFNOSUPPORT;
		return -1;
	};
	
	// convert port to machine order and then ensure we have permission to bind to it
	port = __builtin_bswap16(port);
	
	if ((port < 1024) && (port != 0))
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
	
	if (port != 0)
	{
		if (ClaimSocketAddr(addr, &udpsock->sockname) != 0)
		{
			ERRNO = EADDRINUSE;
			return -1;
		};
	}
	else
	{
		struct sockaddr chaddr;
		memcpy(&chaddr, addr, INET_SOCKADDR_LEN);
		
		if (chaddr.sa_family == AF_INET)
		{
			((struct sockaddr_in*)&chaddr)->sin_port = AllocPort();
		}
		else
		{
			((struct sockaddr_in6*)&chaddr)->sin6_port = AllocPort();
		};

		if (ClaimSocketAddr(&chaddr, &udpsock->sockname) != 0)
		{
			ERRNO = EADDRINUSE;
			return -1;
		};
	};
	
	return 0;
};

static int udpsock_getsockname(Socket *sock, struct sockaddr *addr, size_t *addrlenptr)
{
	size_t addrlen = *addrlenptr;
	if (addrlen > INET_SOCKADDR_LEN)
	{
		addrlen = (*addrlenptr) = INET_SOCKADDR_LEN;
	};
	
	UDPSocket *udpsock = (UDPSocket*) sock;
	memcpy(addr, &udpsock->sockname, addrlen);
	return 0;
};

static int udpsock_connect(Socket *sock, const struct sockaddr *addr, size_t addrlen)
{
	UDPSocket *udpsock = (UDPSocket*) sock;
	if (addrlen < INET_SOCKADDR_LEN)
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
	
	memcpy(&udpsock->peername, addr, INET_SOCKADDR_LEN);
	return 0;
};

static int udpsock_getpeername(Socket *sock, struct sockaddr *addr, size_t *addrlenptr)
{
	size_t addrlen = *addrlenptr;
	if (addrlen > INET_SOCKADDR_LEN)
	{
		addrlen = (*addrlenptr) = INET_SOCKADDR_LEN;
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

static void udpsock_close(Socket *sock)
{
	UDPSocket *udpsock = (UDPSocket*) sock;
	
	semWait(&udpsock->queueLock);
	UDPInbound *inbound = udpsock->first;
	while (inbound != NULL)
	{
		UDPInbound *next = inbound->next;
		kfree(inbound);
		inbound = next;
	};
	udpsock->first = udpsock->last = NULL;
	
	if (sock->domain == AF_INET)
	{
		const struct sockaddr_in *inaddr = (const struct sockaddr_in*) &udpsock->sockname;
		FreePort(inaddr->sin_port);
	}
	else
	{
		const struct sockaddr_in6 *inaddr = (const struct sockaddr_in6*) &udpsock->sockname;
		FreePort(inaddr->sin6_port);
	};
	
	kfree(udpsock->groups);
	udpsock->groups = NULL;
	udpsock->numGroups = 0;
	
	semSignal(&udpsock->queueLock);
	kfree(sock);
};

static uint16_t udpChecksum4(const struct sockaddr_in *src, const struct sockaddr_in *dest, const void *msg, size_t size)
{
	// TODO: compute checksum
	return 0;
};

static uint16_t udpChecksum6(const struct sockaddr_in6 *insrc, const struct sockaddr_in6 *indst, const void *msg, size_t size)
{
	static uint8_t zeroes[16] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
	
	if (isMappedAddress46((const struct sockaddr*)indst))
	{
		struct sockaddr_in src4, dst4;
		remapAddress64(insrc, &src4);
		remapAddress64(indst, &dst4);
		return udpChecksum4(&src4, &dst4, msg, size);
	};
	
	// we have to do this because the checksum requires the source address to be known.
	struct sockaddr_in6 copyaddr;
	if (memcmp(&insrc->sin6_addr, zeroes, 16) == 0)
	{
		memcpy(&copyaddr, insrc, sizeof(struct sockaddr_in6));
		getDefaultAddr6(&copyaddr.sin6_addr, &indst->sin6_addr);
		insrc = &copyaddr;
	};
	
	size_t packetSize = sizeof(UDPPacket) + size;
	UDPEncap *encap = (UDPEncap*) kmalloc(sizeof(UDPEncap) + size);
	memset(encap, 0, sizeof(UDPEncap));
	memcpy(encap->srcaddr, &insrc->sin6_addr, 16);
	memcpy(encap->dstaddr, &indst->sin6_addr, 16);
	encap->udplen = __builtin_bswap16(packetSize);
	encap->proto = IPPROTO_UDP;
	encap->packet.srcport = insrc->sin6_port;
	encap->packet.dstport = indst->sin6_port;
	encap->packet.len = encap->udplen;
	encap->packet.checksum = 0;
	memcpy(encap->packet.payload, msg, size);
	uint16_t checksum = ipv4_checksum(encap, sizeof(UDPEncap) + size);
	
	kfree(encap);
	return checksum;
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
		if (addrlen < INET_SOCKADDR_LEN)
		{
			ERRNO = EAFNOSUPPORT;
			return -1;
		};
		
		if (addr->sa_family != sock->domain)
		{
			ERRNO = EAFNOSUPPORT;
			return -1;
		};
		
		memcpy(&destaddr, addr, INET_SOCKADDR_LEN);
	}
	else
	{
		if (udpsock->peername.sa_family == AF_UNSPEC)
		{
			ERRNO = EDESTADDRREQ;
			return -1;
		};
		
		memcpy(&destaddr, &udpsock->peername, INET_SOCKADDR_LEN);
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
	
	sock->options[GSO_SNDFLAGS] &= UDP_ALLOWED_SNDFLAGS;
	
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
		
		packet->checksum = udpChecksum4(insrc, indst, message, msgsize);
		
		int status = sendPacketEx(&udpsock->sockname, &destaddr, packet, sizeof(UDPPacket) + msgsize,
					IPPROTO_UDP, sock->options, sock->ifname);
					
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
		struct sockaddr_in6 *insrc = (struct sockaddr_in6*) &udpsock->sockname;
		struct sockaddr_in6 *indst = (struct sockaddr_in6*) &destaddr;
		
		UDPPacket *packet = (UDPPacket*) kmalloc(sizeof(UDPPacket) + msgsize);
		packet->srcport = insrc->sin6_port;
		packet->dstport = indst->sin6_port;
		packet->len = __builtin_bswap16((uint16_t) msgsize+8);
		packet->checksum = 0;
		memcpy(packet->payload, message, msgsize);
		
		packet->checksum = udpChecksum6(insrc, indst, message, msgsize);
		
		int status = sendPacketEx(&udpsock->sockname, &destaddr, packet, sizeof(UDPPacket) + msgsize,
					IPPROTO_UDP, sock->options, sock->ifname);
		kfree(packet);
		
		if (status < 0)
		{
			ERRNO = -status;
			return (ssize_t)-1;
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
	
	struct sockaddr_in6 src6, dest6;
	int havev4addr = 0;
	if ((dest->sa_family == AF_INET) && (sock->domain == AF_INET6))
	{
		remapAddress46((const struct sockaddr_in*) src, &src6);
		remapAddress46((const struct sockaddr_in*) dest, &dest6);
		
		src = (struct sockaddr*) &src6;
		dest = (struct sockaddr*) &dest6;
		havev4addr = 1;
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
		
		if ((sockname->sin6_scope_id != 0) && (indst->sin6_scope_id != sockname->sin6_scope_id))
		{
			return;
		};
		
		if (indst->sin6_addr.s6_addr[0] == 0xFF)
		{
			// check if we have joined the appropriate multicast group
			semWait(&udpsock->queueLock);
			size_t i;
			int found = 0;
			for (i=0; i<udpsock->numGroups; i++)
			{
				UDPGroup *group = &udpsock->groups[i];
				if (memcmp(&group->addr, indst, 16) == 0)
				{
					if ((group->scope == indst->sin6_scope_id) || (group->scope == 0))
					{
						found = 1;
						break;
					};
				};
			};
			semSignal(&udpsock->queueLock);
			
			if (!found)
			{
				return;
			};
		}
		else
		{
			if (memcmp(&sockname->sin6_addr, zeroes, 16) != 0)
			{
				if (memcmp(&sockname->sin6_addr, &indst->sin6_addr, 16) != 0)
				{
					// all IPv6 sockets accept IPv4 datagrams
					if (!havev4addr)
					{
						// not destined to this socket
						return;
					};
				};
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
	// TODO: or should checksums be implemented?
	if (__builtin_bswap16(udp->len) > size)
	{
		return;
	};
	
	UDPInbound *inbound = (UDPInbound*) kmalloc(sizeof(UDPInbound) + __builtin_bswap16(udp->len));
	inbound->next = NULL;
	
	// copy the source address into the inbound desription but also place the port number in
	memcpy(&inbound->srcaddr, src, INET_SOCKADDR_LEN);
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
	
	semWait(&udpsock->queueLock);
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

	int status = semWaitGen(&udpsock->counter, 1, SEM_W_FILE(sock->fp->oflag), sock->options[GSO_RCVTIMEO]);
	if (status < 0)
	{
		ERRNO = -status;
		return -1;
	};
	
	// packet inbound
	semWait(&udpsock->queueLock);
	UDPInbound *inbound = udpsock->first;
	if (inbound == NULL)
	{
		ERRNO = EPIPE;
		return -1;
	};
	
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

static int udpsock_mcast(Socket *sock, int op, const struct in6_addr *addr, uint32_t scope)
{	
	UDPSocket *udpsock = (UDPSocket*) sock;
	UDPGroup group;
	memcpy(&group.addr, addr, 16);
	group.scope = scope;
	
	semWait(&udpsock->queueLock);

	if (udpsock->shutflags & SHUT_RD)
	{
		semSignal(&udpsock->queueLock);
		ERRNO = EINVAL;
		return -1;
	};
	
	if (op == MCAST_JOIN_GROUP)
	{
		// see if we're already in this group
		size_t i;
		for (i=0; i<udpsock->numGroups; i++)
		{
			if (memcmp(&udpsock->groups[i], &group, sizeof(UDPGroup)) == 0)
			{
				// already in this group
				semSignal(&udpsock->queueLock);
				return 0;
			};
		};
		
		// we must add this group to the list
		if (udpsock->numGroups == MCAST_MAX_GROUPS)
		{
			// we've joined too many multicast groups
			semSignal(&udpsock->queueLock);
			ERRNO = E2BIG;
			return -1;
		};
		
		i = udpsock->numGroups++;
		UDPGroup *newList = (UDPGroup*) kmalloc(sizeof(UDPGroup)*udpsock->numGroups);
		memcpy(newList, udpsock->groups, sizeof(UDPGroup)*i);
		memcpy(&newList[i], &group, sizeof(UDPGroup));
		kfree(udpsock->groups);
		udpsock->groups = newList;
		semSignal(&udpsock->queueLock);
		return 0;
	}
	else if (op == MCAST_LEAVE_GROUP)
	{
		// see if we're in this group at all
		size_t i;
		for (i=0; i<udpsock->numGroups; i++)
		{
			if (memcmp(&udpsock->groups[i], &group, sizeof(UDPGroup)) == 0)
			{
				break;
			};
		};
		
		if (i == udpsock->numGroups)
		{
			// we're already not in this group
			semSignal(&udpsock->queueLock);
			return 0;
		};
		
		udpsock->numGroups--;
		UDPGroup *newList = (UDPGroup*) kmalloc(sizeof(UDPGroup)*udpsock->numGroups);
		memcpy(newList, udpsock->groups, sizeof(UDPGroup)*i);
		memcpy(&newList[i], &udpsock->groups[i+1], sizeof(UDPGroup)*(udpsock->numGroups-i));
		kfree(udpsock->groups);
		udpsock->groups = newList;
		
		semSignal(&udpsock->queueLock);
		return 0;
	}
	else
	{
		semSignal(&udpsock->queueLock);
		ERRNO = EINVAL;
		return -1;
	};
};

static void udpsock_pollinfo(Socket *sock, Semaphore **sems)
{
	UDPSocket *udpsock = (UDPSocket*) sock;
	sems[PEI_READ] = &udpsock->counter;
	sems[PEI_WRITE] = vfsGetConstSem();
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
	sock->mcast = udpsock_mcast;
	sock->close = udpsock_close;
	sock->pollinfo = udpsock_pollinfo;

	semInit(&udpsock->queueLock);
	semInit2(&udpsock->counter, 0);
	
	return sock;
};

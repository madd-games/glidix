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

typedef struct RawPacket_
{
	struct RawPacket_*		next;
	struct sockaddr			src;
	size_t				addrlen;
	size_t				size;
	char				data[];
} RawPacket;

/**
 * RAW SOCKET
 *
 * A raw socket can be used to transmit arbitrary transport-layer data. A raw socket can start receiving
 * packets after it is bound to an address, but can send them without binding. If a raw socket is bound
 * to the wildcard address (all zeroes), it receives every packet marked with the specified protocol.
 * If it is bound to a specific address, only packets directed to that address and the given protocol are
 * received. If the protocol is IPPROTO_ANY, then:
 *  - no packets are received.
 *  - when you send a packet, it must include an IP header. The destination address passed to sendto() is
 *    considered to be a gateway (which may pass through another gateway according to the routing table).
 * Please note that if you bind to the wildcard address, then sending a packet shall cause the socket to
 * bind to the address of the corresponding interface.
 */
typedef struct
{
	Socket				header_;
	
	/**
	 * The address to which this socket is bound. The family is AF_UNSPEC if unbound.
	 */
	struct sockaddr			addr;
	
	/**
	 * Semaphore controlling access to the packet queue.
	 */
	Semaphore			queueLock;
	
	/**
	 * Semaphore counting the number of packets waiting on the socket.
	 */
	Semaphore			packetCounter;
	
	/**
	 * First and last packets in queue.
	 */
	RawPacket*			first;
	RawPacket*			last;
} RawSocket;

static int rawsock_bind(Socket *sock, const struct sockaddr *addr, size_t addrlen)
{
	if ((addr->sa_family != AF_INET) && (addr->sa_family != AF_INET6) && (addr->sa_family != AF_UNSPEC))
	{
		ERRNO = EAFNOSUPPORT;
		return -1;
	};
	
	if (addrlen > sizeof(struct sockaddr))
	{
		ERRNO = EAFNOSUPPORT;
		return -1;
	};
	
	RawSocket *rawsock = (RawSocket*) sock;
	memcpy(&rawsock->addr, addr, addrlen);
	return 0;
};

static ssize_t rawsock_sendto(Socket *sock, const void *message, size_t len, int flags, const struct sockaddr *addr, size_t addrlen)
{
	RawSocket *rawsock = (RawSocket*) sock;
	
	if (flags != 0)
	{
		ERRNO = EINVAL;
		return -1;
	};
	
	if (addrlen < 2)
	{
		ERRNO = EINVAL;
		return -1;
	};
	
	if (addr->sa_family == AF_INET)
	{
		if (addrlen < sizeof(struct sockaddr_in))
		{
			ERRNO = EINVAL;
			return -1;
		};
	}
	else if (addr->sa_family == AF_INET6)
	{
		if (addrlen < sizeof(struct sockaddr_in6))
		{
			ERRNO = EINVAL;
			return -1;
		};
	}
	else
	{
		ERRNO = EAFNOSUPPORT;
		return -1;
	};
	
	int status = sendPacketEx((struct sockaddr*) &rawsock->addr, addr, message, len,
		sock->proto, sock->options, sock->ifname);

	if (status < 0)
	{
		ERRNO = -status;
		return -1;
	};
	
	return 0;
};

static void rawsock_packet(Socket *sock, const struct sockaddr *src, const struct sockaddr *dest, size_t addrlen,
			const void *packet, size_t size, int proto)
{
	if ((dest->sa_family != AF_INET) && (dest->sa_family != AF_INET6)) return;
	
	RawSocket *rawsock = (RawSocket*) sock;
	if (rawsock->addr.sa_family != AF_UNSPEC)
	{
		if (rawsock->addr.sa_family != dest->sa_family)
		{
			// different family, discard packet
			return;
		};
		
		// make sure the addresses match
		if (rawsock->addr.sa_family == AF_INET)
		{
			struct sockaddr_in *inaddr = (struct sockaddr_in*) &rawsock->addr;
			const struct sockaddr_in *indest = (const struct sockaddr_in*) dest;
			
			if (inaddr->sin_addr.s_addr != 0)
			{
				if (memcmp(&inaddr->sin_addr, &indest->sin_addr, 4) != 0)
				{
					// different destination, discard
					return;
				};
			};
		}
		else
		{
			// AF_INET6
			struct sockaddr_in6 *inaddr = (struct sockaddr_in6*) &rawsock->addr;
			const struct sockaddr_in6 *indest = (struct sockaddr_in6*) dest;
			
			static const uint64_t zeroAddr[] = {0, 0};
			if (memcmp(&inaddr->sin6_addr, zeroAddr, 16) != 0)
			{
				if (
					(memcmp(&inaddr->sin6_addr, &indest->sin6_addr, 16) != 0)
					&& (indest->sin6_addr.s6_addr[0] != 0xFF)
				)
				{
					// different destination, discard
					return;
				};
			};
		};
	};
	
	if (sock->proto != IPPROTO_RAW)
	{
		if (sock->proto != proto)
		{
			// different protocol, discard
			return;
		};
	
		RawPacket *rawpack = (RawPacket*) kmalloc(sizeof(RawPacket) + size);
		rawpack->next = NULL;
		memcpy(&rawpack->src, src, addrlen);
		rawpack->addrlen = addrlen;
		rawpack->size = size;
		memcpy(rawpack->data, packet, size);
	
		semWait(&rawsock->queueLock);
		if (rawsock->first == NULL)
		{
			rawsock->first = rawpack;
			rawsock->last = rawpack;
		}
		else
		{
			rawsock->last->next = rawpack;
			rawsock->last = rawpack;
		};
		semSignal(&rawsock->queueLock);
	
		// announce to any waiting processes that a packet was received
		semSignal2(&rawsock->packetCounter, 1);
	};
};

static ssize_t rawsock_recvfrom(Socket *sock, void *buffer, size_t len, int flags, struct sockaddr *addr, size_t *addrlen)
{
	RawSocket *rawsock = (RawSocket*) sock;

	int status = semWaitGen(&rawsock->packetCounter, 1, SEM_W_FILE(sock->fp->oflag), sock->options[GSO_RCVTIMEO]);
	if (status < 0)
	{
		ERRNO = -status;
		return -1;
	};
	
	// packet inbound
	semWait(&rawsock->queueLock);
	RawPacket *packet = rawsock->first;
	
	if ((flags & MSG_PEEK) == 0)
	{
		rawsock->first = packet->next;
		if (rawsock->first == NULL)
		{
			rawsock->last = NULL;
		};
	};
	
	if (len > packet->size)
	{
		len = packet->size;
	};
	
	size_t realAddrLen = packet->addrlen;
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
		memcpy(addr, &packet->src, realAddrLen);
	};
	
	memcpy(buffer, packet->data, len);
	semSignal(&rawsock->queueLock);
	
	if ((flags & MSG_PEEK) == 0)
	{
		kfree(packet);
	}
	else
	{
		// just peeking so "return" the packet
		semSignal2(&rawsock->packetCounter, 1);
	};
	
	return (ssize_t) len;
};

static void rawsock_close(Socket *sock)
{
	RawSocket *rawsock = (RawSocket*) sock;
	RawPacket *packet = rawsock->first;
	
	while (packet != NULL)
	{
		RawPacket *next = packet->next;
		kfree(packet);
		packet = next;
	};
};

static int rawsock_getsockname(Socket *sock, struct sockaddr *addr, size_t *addrlenptr)
{
	if ((*addrlenptr) > sizeof(struct sockaddr))
	{
		*addrlenptr = sizeof(struct sockaddr);
	};
	
	size_t len = *addrlenptr;
	RawSocket *rawsock = (RawSocket*) sock;
	memcpy(addr, &rawsock->addr, len);
	return 0;
};

static void rawsock_pollinfo(Socket *sock, Semaphore **sems)
{
	RawSocket *rawsock = (RawSocket*) sock;
	sems[PEI_READ] = &rawsock->packetCounter;
	sems[PEI_WRITE] = vfsGetConstSem();
};

Socket *CreateRawSocket()
{
	RawSocket *rawsock = (RawSocket*) kmalloc(sizeof(RawSocket));
	memset(rawsock, 0, sizeof(RawSocket));
	Socket *sock = (Socket*) rawsock;
	
	sock->bind = rawsock_bind;
	sock->sendto = rawsock_sendto;
	sock->packet = rawsock_packet;
	sock->recvfrom = rawsock_recvfrom;
	sock->getsockname = rawsock_getsockname;
	sock->close = rawsock_close;
	sock->pollinfo = rawsock_pollinfo;
	
	semInit(&rawsock->queueLock);
	semInit2(&rawsock->packetCounter, 0);
	
	return sock;
};

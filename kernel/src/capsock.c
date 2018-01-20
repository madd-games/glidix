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

/**
 * Entry in the inbound queue.
 */
typedef struct CapInbound_
{
	struct CapInbound_*			next;
	struct sockaddr				srcaddr;
	uint64_t				socklen;
	size_t					size;
	char					payload[];
} CapInbound;

/**
 * CAPTURE SOCKET
 *
 * A socket used to capture packets moving around interfaces.
 */
typedef struct
{
	Socket					header_;
	struct sockaddr_cap			sockname;
	int					shutflags;
	Semaphore				queueLock;
	Semaphore				counter;
	CapInbound*				first;
	CapInbound*				last;
} CapSocket;

static int capsock_bind(Socket *sock, const struct sockaddr *addr, size_t addrlen)
{
	CapSocket *capsock = (CapSocket*) sock;
	if (addrlen < sizeof(struct sockaddr_cap))
	{
		ERRNO = EAFNOSUPPORT;
		return -1;
	};
	
	if (addr->sa_family != AF_CAPTURE)
	{
		ERRNO = EAFNOSUPPORT;
		return -1;
	};
	
	if (capsock->sockname.scap_family != AF_UNSPEC)
	{
		ERRNO = EINVAL;
		return -1;
	};
	
	memcpy(&capsock->sockname, addr, sizeof(struct sockaddr_cap));
	return 0;
};

static void capsock_shutdown(Socket *sock, int how)
{
	CapSocket *capsock = (CapSocket*) sock;
	if (how & SHUT_RD)
	{
		semWait(&capsock->queueLock);
		CapInbound *inbound = capsock->first;
		while (inbound != NULL)
		{
			CapInbound *next = inbound->next;
			kfree(inbound);
			inbound = next;
		};
		capsock->first = capsock->last = NULL;
		semSignal(&capsock->queueLock);
	};
	capsock->shutflags |= how;
};

static ssize_t capsock_recvfrom(Socket *sock, void *buffer, size_t len, int flags, struct sockaddr *addr, size_t *addrlen)
{
	CapSocket *capsock = (CapSocket*) sock;

	int status = semWaitGen(&capsock->counter, 1, SEM_W_FILE(sock->fp->oflags), sock->options[GSO_RCVTIMEO]);
	if (status < 0)
	{
		ERRNO = -status;
		return -1;
	};
	
	// packet inbound
	semWait(&capsock->queueLock);
	CapInbound *inbound = capsock->first;
	if (inbound == NULL)
	{
		ERRNO = EPIPE;
		return -1;
	};
	
	if ((flags & MSG_PEEK) == 0)
	{
		capsock->first = inbound->next;
		if (capsock->first == NULL)
		{
			capsock->last = NULL;
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
	semSignal(&capsock->queueLock);
	
	if ((flags & MSG_PEEK) == 0)
	{
		kfree(inbound);
	}
	else
	{
		// just peeking so "return" the packet
		semSignal2(&capsock->counter, 1);
	};
	
	return (ssize_t) len;
};

static int capsock_packet(Socket *sock, const struct sockaddr *src, const struct sockaddr *dest, size_t addrlen,
			const void *packet, size_t size, int proto)
{
	CapSocket *capsock = (CapSocket*) sock;
	if (capsock->shutflags != 0) return SOCK_CONT;
	if (src->sa_family != AF_CAPTURE) return SOCK_CONT;
	if (capsock->sockname.scap_family == AF_UNSPEC) return SOCK_CONT;
	
	const struct sockaddr_cap *caddr = (const struct sockaddr_cap*) src;
	if (strcmp(caddr->scap_ifname, capsock->sockname.scap_ifname) == 0)
	{
		if (sock->type == SOCK_DGRAM)
		{
			if (proto != IPPROTO_IP)
			{
				return SOCK_CONT;
			};
		}
		else
		{
			if (proto != sock->proto)
			{
				return SOCK_CONT;
			};
		};
		
		CapInbound *inbound = (CapInbound*) kmalloc(sizeof(CapInbound) + size);
		inbound->next = NULL;

		memcpy(&inbound->srcaddr, src, sizeof(struct sockaddr_cap));
	
		inbound->size = size;
		memcpy(inbound->payload, packet, size);
	
		semWait(&capsock->queueLock);
		if (capsock->last == NULL)
		{
			capsock->first = capsock->last = inbound;
		}
		else
		{
			capsock->last->next = inbound;
			capsock->last = inbound;
		};
		semSignal(&capsock->queueLock);
	
		semSignal(&capsock->counter);
	};
	
	return SOCK_CONT;
};

Socket *CreateCaptureSocket(int type, int proto)
{
	if (type == SOCK_DGRAM)
	{
		if (proto != 0) return NULL;
	}
	else if (type == SOCK_RAW)
	{
		if ((proto != IF_LOOPBACK) && (proto != IF_ETHERNET) && (proto != IF_TUNNEL)) return NULL;
	}
	else
	{
		return NULL;
	};
	
	CapSocket *capsock = NEW(CapSocket);
	memset(capsock, 0, sizeof(CapSocket));
	Socket *sock = (Socket*) capsock;
	
	sock->bind = capsock_bind;
	sock->packet = capsock_packet;
	sock->recvfrom = capsock_recvfrom;
	sock->shutdown = capsock_shutdown;
	
	semInit(&capsock->queueLock);
	semInit2(&capsock->counter, 0);
	
	return sock;
};

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
#include <glidix/sched.h>
#include <glidix/errno.h>
#include <glidix/memory.h>
#include <glidix/semaphore.h>
#include <glidix/string.h>
#include <glidix/console.h>
#include <glidix/netif.h>

Socket* CreateRawSocket();				/* rawsock.c */
Socket* CreateUDPSocket();				/* udpsock.c */

static Semaphore sockLock;
static Socket sockList;

void initSocket()
{
	semInit(&sockLock);
	memset(&sockList, 0, sizeof(Socket));
	
};

static void sock_close(File *fp)
{
	semWait(&sockLock);
	if (fp->fsdata == NULL)
	{
		semSignal(&sockLock);
		return;
	};

	Socket *sock = (Socket*) fp->fsdata;
	fp->fsdata = NULL;
	
	if (sock->shutdown != NULL)
	{
		sock->shutdown(sock, SHUT_RDWR);
	};

	if (sock->prev != NULL) sock->prev->next = sock->next;
	if (sock->next != NULL) sock->next->prev = sock->prev;
	semSignal(&sockLock);
	
	if (sock->close != NULL)
	{
		sock->close(sock);
	};
	
	kfree(sock);
};

static ssize_t sock_write(File *fp, const void *buffer, size_t len)
{
	return SendtoSocket(fp, buffer, len, 0, NULL, 0);
};

static ssize_t sock_read(File *fp, void *buffer, size_t len)
{
	return RecvfromSocket(fp, buffer, len, 0, NULL, NULL);
};

File* CreateSocket(int domain, int type, int proto)
{
	if ((domain != AF_INET) && (domain != AF_INET6))
	{
		getCurrentThread()->therrno = EAFNOSUPPORT;
		return NULL;
	};

	Socket *sock;	
	if (type == SOCK_RAW)
	{
		if (getCurrentThread()->euid != 0)
		{
			getCurrentThread()->therrno = EACCES;
			return NULL;
		};
		
		sock = CreateRawSocket();
	}
	else if (type == SOCK_DGRAM)
	{
		switch (proto)
		{
		case 0:
		case IPPROTO_UDP:
			sock = CreateUDPSocket();
			break;
		default:
			getCurrentThread()->therrno = EPROTONOSUPPORT;
			return NULL;
		};
	}
	else
	{
		// TODO: support other socket types tbh
		getCurrentThread()->therrno = EPROTONOSUPPORT;
		return NULL;
	};
	
	sock->domain = domain;
	sock->type = type;
	sock->proto = proto;
	
	// append the socket to the beginning of the socket list.
	// (the order does not acutally matter by adding to the beginning is faster).
	semWait(&sockLock);
	sock->prev = &sockList;
	sock->next = sockList.next;
	sockList.next = sock;
	semSignal(&sockLock);
	
	File *fp = (File*) kmalloc(sizeof(File));
	memset(fp, 0, sizeof(File));
	fp->fsdata = sock;
	fp->oflag = O_RDWR | O_SOCKET;
	fp->close = sock_close;
	fp->write = sock_write;
	fp->read = sock_read;
	
	sock->fp = fp;
	return fp;
};

int BindSocket(File *fp, const struct sockaddr *addr, size_t addrlen)
{
	if (addrlen < 2)
	{
		getCurrentThread()->therrno = EAFNOSUPPORT;
		return -1;
	};
	
	if ((fp->oflag & O_SOCKET) == 0)
	{
		getCurrentThread()->therrno = ENOTSOCK;
		return -1;
	};
	
	Socket *sock = (Socket*) fp->fsdata;
	if (sock->bind == NULL)
	{
		getCurrentThread()->therrno = EOPNOTSUPP;
		semSignal(&sockLock);
		return -1;
	};
	
	int result = sock->bind(sock, addr, addrlen);
	return result;
};

ssize_t SendtoSocket(File *fp, const void *message, size_t len, int flags, const struct sockaddr *addr, size_t addrlen)
{
	if ((fp->oflag & O_SOCKET) == 0)
	{
		getCurrentThread()->therrno = ENOTSOCK;
		return -1;
	};
	
	Socket *sock = (Socket*) fp->fsdata;
	if (sock->sendto == NULL)
	{
		getCurrentThread()->therrno = EOPNOTSUPP;
		semSignal(&sockLock);
		return -1;
	};
	
	ssize_t result = sock->sendto(sock, message, len, flags, addr, addrlen);
	
	return result;
};

ssize_t RecvfromSocket(File *fp, void *message, size_t len, int flags, struct sockaddr *addr, size_t *addrlen)
{
	if ((fp->oflag & O_SOCKET) == 0)
	{
		getCurrentThread()->therrno = ENOTSOCK;
		return -1;
	};
	
	Socket *sock = (Socket*) fp->fsdata;
	if (sock->recvfrom == NULL)
	{
		getCurrentThread()->therrno = EOPNOTSUPP;
		semSignal(&sockLock);
		return -1;
	};
	
	ssize_t result = sock->recvfrom(sock, message, len, flags, addr, addrlen);
	
	return result;
};

void passPacketToSocket(const struct sockaddr *src, const struct sockaddr *dest, size_t addrlen,
			const void *packet, size_t size, int proto, uint64_t dataOffset)
{
	if ((proto == IPPROTO_ICMP) && (src->sa_family == AF_INET))
	{
		ICMPPacket *icmp = (ICMPPacket*) ((char*)packet + dataOffset);
		if ((icmp->type == 8) && (icmp->code == 0))
		{
			void *response = kmalloc(size);
			memcpy(response, packet, size);
			
			IPHeader4 *header = (IPHeader4*) response;
			uint32_t temp = header->saddr;
			header->saddr = header->daddr;
			header->daddr = temp;
			
			PingPongPacket *pong = (PingPongPacket*) ((char*)response + dataOffset);
			pong->type = 0;
			pong->checksum = 0;
			pong->checksum = ipv4_checksum(pong, sizeof(PingPongPacket));
			
			sendPacket(src, response, size);
			kfree(response);
		};
	};
	
	semWait(&sockLock);
	
	Socket *sock = &sockList;
	while (sock != NULL)
	{
		if (sock->packet != NULL)
		{
			sock->packet(sock, src, dest, addrlen, packet, size, proto, dataOffset);
		};
		
		sock = sock->next;
	};
	
	semSignal(&sockLock);
};

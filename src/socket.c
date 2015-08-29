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
#include <glidix/spinlock.h>

Socket* CreateRawSocket();				/* rawsock.c */
Socket* CreateUDPSocket();				/* udpsock.c */

static Semaphore sockLock;
static Socket sockList;

static Spinlock portLock;
static uint8_t *ethports;

void initSocket()
{
	semInit(&sockLock);
	memset(&sockList, 0, sizeof(Socket));
	
	spinlockRelease(&portLock);
	ethports = (uint8_t*) kmalloc(2048);		// 16384 ports, 1 byte for each 8
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
		return -1;
	};
	
	ssize_t result = sock->recvfrom(sock, message, len, flags, addr, addrlen);
	
	return result;
};

int SocketGetsockname(File *fp, struct sockaddr *addr, size_t *addrlen)
{
	if ((fp->oflag & O_SOCKET) == 0)
	{
		getCurrentThread()->therrno = ENOTSOCK;
		return -1;
	};
	
	Socket *sock = (Socket*) fp->fsdata;
	if (sock->getsockname == NULL)
	{
		getCurrentThread()->therrno = EOPNOTSUPP;
		return -1;
	};
	
	return sock->getsockname(sock, addr, addrlen);
};

int ShutdownSocket(File *fp, int how)
{
	if ((fp->oflag & O_SOCKET) == 0)
	{
		getCurrentThread()->therrno = ENOTSOCK;
		return -1;
	};
	
	Socket *sock = (Socket*) fp->fsdata;
	if (sock->shutdown == NULL)
	{
		getCurrentThread()->therrno = EOPNOTSUPP;
		return -1;
	};
	
	sock->shutdown(sock, how);
	return 0;
};

int ConnectSocket(File *fp, const struct sockaddr *addr, size_t addrlen)
{
	if ((fp->oflag & O_SOCKET) == 0)
	{
		getCurrentThread()->therrno = ENOTSOCK;
		return -1;
	};
	
	Socket *sock = (Socket*) fp->fsdata;
	if (sock->connect == NULL)
	{
		getCurrentThread()->therrno = EOPNOTSUPP;
		return -1;
	};
	
	sock->connect(sock, addr, addrlen);
	return 0;
};

int SocketGetpeername(File *fp, struct sockaddr *addr, size_t *addrlen)
{
	if ((fp->oflag & O_SOCKET) == 0)
	{
		getCurrentThread()->therrno = ENOTSOCK;
		return -1;
	};
	
	Socket *sock = (Socket*) fp->fsdata;
	if (sock->getpeername == NULL)
	{
		getCurrentThread()->therrno = EOPNOTSUPP;
		return -1;
	};
	
	return sock->getpeername(sock, addr, addrlen);
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

int ClaimSocketAddr(const struct sockaddr *addr, struct sockaddr *dest)
{
	int isAnyAddr = 0;
	uint16_t claimingPort;
	static uint64_t zeroAddr[] = {0, 0};
	
	if (addr->sa_family == AF_INET)
	{
		const struct sockaddr_in *inaddr = (const struct sockaddr_in*) addr;
		if (inaddr->sin_addr.s_addr == 0)
		{
			isAnyAddr = 1;
		};
		
		if (inaddr->sin_port == 0)
		{
			// cannot claim port 0!
			return -1;
		};
		
		claimingPort = inaddr->sin_port;
	}
	else
	{
		// IPv6
		const struct sockaddr_in6 *inaddr = (const struct sockaddr_in6*) addr;
		if (memcmp(&inaddr->sin6_addr, &zeroAddr, 16) == 0)
		{
			isAnyAddr = 1;
		};
		
		if (inaddr->sin6_port == 0)
		{
			return -1;
		};
		
		claimingPort = inaddr->sin6_port;
	};
	
	int status = 0;
	
	semWait(&sockLock);
	Socket *sock;
	for (sock=&sockList; sock!=NULL; sock=sock->next)
	{
		struct sockaddr otherAddr;
		size_t addrlen = sizeof(struct sockaddr);
		
		if (sock->getsockname != NULL)
		{
			if (sock->getsockname(sock, &otherAddr, &addrlen) == 0)
			{
				if (otherAddr.sa_family == AF_INET)
				{
					struct sockaddr_in *otherinaddr = (struct sockaddr_in*) &otherAddr;
					if (isAnyAddr)
					{
						// we are binding to all addresses so just check the port
						if (claimingPort == otherinaddr->sin_port)
						{
							status = -1;
							break;
						};
					}
					else if (otherinaddr->sin_addr.s_addr == 0)
					{
						// bound to all addresses; so if the ports match, we cannot bind
						if (claimingPort == otherinaddr->sin_port)
						{
							status = -1;
							break;
						};
					}
					else if (addr->sa_family == AF_INET)
					{
						// if that socket is bound to an IPv4 address and we are likewise binding to
						// IPv4, we must ensure that the address or port is different
						const struct sockaddr_in *inaddr = (const struct sockaddr_in*) addr;
						if ((inaddr->sin_addr.s_addr == otherinaddr->sin_addr.s_addr)
							&& (inaddr->sin_port == otherinaddr->sin_port))
						{
							status = -1;
							break;
						};
					};
				}
				else if (otherAddr.sa_family == AF_INET6)
				{
					struct sockaddr_in6 *otherinaddr = (struct sockaddr_in6*) &otherAddr;
					if (isAnyAddr)
					{
						// we are binding to all addresses so just check the port
						if (claimingPort == otherinaddr->sin6_port)
						{
							status = -1;
							break;
						};
					}
					else if (memcmp(&otherinaddr->sin6_addr, &zeroAddr, 16) == 0)
					{
						// bound to all addresses; so check the port
						if (claimingPort == otherinaddr->sin6_port)
						{
							status = -1;
							break;
						};
					}
					else if (addr->sa_family == AF_INET6)
					{
						// that socket is bound to an IPv6 address so make sure we are not binding to
						// the same IPv6 address and port
						const struct sockaddr_in6 *inaddr = (const struct sockaddr_in6*) addr;
						if ((memcmp(&inaddr->sin6_addr, &otherinaddr->sin6_addr, 16) == 0)
							&& (inaddr->sin6_port == otherinaddr->sin6_port))
						{
							status = -1;
							break;
						};
					};
				};
			};
		};
	};
	
	if (status == 0)
	{
		memcpy(dest, addr, sizeof(struct sockaddr));
	};
	
	semSignal(&sockLock);
	return status;
};

uint16_t AllocPort()
{
	spinlockAcquire(&portLock);
	
	int index;
	for (index=0; index<2048; index++)
	{
		if (ethports[index] != 0xFF)
		{
			int bitoff;
			for (bitoff=0; bitoff<8; bitoff++)
			{
				if ((ethports[index] & (1 << bitoff)) == 0)
				{
					ethports[index] |= (1 << bitoff);
					spinlockRelease(&portLock);
					
					uint16_t result = (uint16_t) (49152 + 8 * index + bitoff);
					return __builtin_bswap16(result);
				};
			};
		};
	};
	
	spinlockRelease(&portLock);
	return 0;
};

void FreePort(uint16_t port)
{
	port = __builtin_bswap16(port);
	if (port >= 49152)
	{
		int portoff = (int) port - 49152;
		int index = portoff / 8;
		int bitoff = portoff % 8;
		
		spinlockAcquire(&portLock);
		ethports[index] &= ~(1 << bitoff);
		spinlockRelease(&portLock);
	};
};

int SetSocketOption(File *fp, int proto, int option, uint64_t value)
{
	if ((fp->oflag & O_SOCKET) == 0)
	{
		ERRNO = ENOTSOCK;
		return -1;
	};
	
	Socket *sock = (Socket*) fp->fsdata;
	if (proto != 0)
	{
		// not SOL_SOCKET
		ERRNO = EINVAL;
		return -1;
	};
	
	if ((option < 0) || (option >= GSO_COUNT))
	{
		ERRNO = EINVAL;
		return -1;
	};
	
	sock->options[option] = value;
	return 0;
};

uint64_t GetSocketOption(File *fp, int proto, int option)
{
	if ((fp->oflag & O_SOCKET) == 0)
	{
		ERRNO = ENOTSOCK;
		return 0;
	};
	
	Socket *sock = (Socket*) fp->fsdata;
	if (proto != 0)
	{
		// not SOL_SOCKET
		ERRNO = EINVAL;
		return 0;
	};
	
	if ((option < 0) || (option >= GSO_COUNT))
	{
		ERRNO = EINVAL;
		return 0;
	};
	
	ERRNO = 0;
	return sock->options[option];
};

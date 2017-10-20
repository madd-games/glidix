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
#include <glidix/sched.h>
#include <glidix/errno.h>
#include <glidix/memory.h>
#include <glidix/semaphore.h>
#include <glidix/string.h>
#include <glidix/console.h>
#include <glidix/netif.h>
#include <glidix/mutex.h>
#include <glidix/icmp.h>
#include <glidix/ipreasm.h>

Socket* CreateRawSocket();				/* rawsock.c */
Socket* CreateUDPSocket();				/* udpsock.c */
Socket* CreateTCPSocket();				/* tcpsock.c */
Socket* CreateCaptureSocket(int type, int proto);	/* capsock.c */
Socket* CreateUnixSocket(int type);			/* unixsock.c */

static Semaphore sockLock;
static Socket sockList;

static Mutex portLock;
static uint8_t *ephports;

/**
 * If 1, then packet forwarding is enabled.
 */
int optForwardPackets = 1;

void initSocket()
{
	semInit(&sockLock);
	memset(&sockList, 0, sizeof(Socket));
	
	mutexInit(&portLock);
	ephports = (uint8_t*) kmalloc(2048);		// 16384 ports, 1 byte for each 8
};

static void sock_close(File *fp)
{
	if (fp->fsdata == NULL)
	{
		return;
	};

	Socket *sock = (Socket*) fp->fsdata;
	fp->fsdata = NULL;
	
	if (sock->shutdown != NULL)
	{
		sock->shutdown(sock, SHUT_RDWR);
	};

	if (sock->close != NULL)
	{
		sock->close(sock);
	}
	else
	{
		FreeSocket(sock);
	};
};

static ssize_t sock_write(File *fp, const void *buffer, size_t len)
{
	return SendtoSocket(fp, buffer, len, 0, NULL, 0);
};

static ssize_t sock_read(File *fp, void *buffer, size_t len)
{
	return RecvfromSocket(fp, buffer, len, 0, NULL, NULL);
};

static void sock_pollinfo(File *fp, Semaphore **sems)
{	
	Socket *sock = (Socket*) fp->fsdata;
	if (sock->pollinfo == NULL)
	{
		return;
	};
	
	sock->pollinfo(sock, sems);
};

File* MakeSocketFile(Socket *sock)
{
	File *fp = (File*) kmalloc(sizeof(File));
	memset(fp, 0, sizeof(File));
	fp->refcount = 1;
	fp->fsdata = sock;
	fp->oflag = O_RDWR | O_SOCKET;
	fp->close = sock_close;
	fp->write = sock_write;
	fp->read = sock_read;
	fp->pollinfo = sock_pollinfo;
	
	return fp;
};

File* CreateSocket(int domain, int type, int proto)
{
	if ((domain != AF_INET) && (domain != AF_INET6) && (domain != AF_CAPTURE) && (domain != AF_UNIX))
	{
		ERRNO = EAFNOSUPPORT;
		return NULL;
	};

	if ((proto < 0) || (proto > 255))
	{
		ERRNO = EPROTONOSUPPORT;
		return NULL;
	};
	
	Socket *sock;
	if (domain == AF_CAPTURE)
	{
		if (!havePerm(XP_RAWSOCK))
		{
			ERRNO = EPERM;
			return NULL;
		};
		
		sock = CreateCaptureSocket(type, proto);
		if (sock == NULL) return NULL;
	}
	else if (domain == AF_UNIX)
	{
		if (proto != 0)
		{
			ERRNO = EPROTONOSUPPORT;
			return NULL;
		};
		
		sock = CreateUnixSocket(type);
		if (sock == NULL) return NULL;
	}
	else
	{
		if (type == SOCK_RAW)
		{
			if (!havePerm(XP_RAWSOCK))
			{
				ERRNO = EPERM;
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
				ERRNO = EPROTONOSUPPORT;
				return NULL;
			};
		}
		else if (type == SOCK_STREAM)
		{
			switch (proto)
			{
			case 0:
			case IPPROTO_TCP:
				sock = CreateTCPSocket();
				break;
			default:
				ERRNO = EPROTONOSUPPORT;
				return NULL;
			};
		}
		else
		{
			ERRNO = EPROTONOSUPPORT;
			return NULL;
		};
	};
	
	sock->domain = domain;
	sock->type = type;
	sock->proto = proto;
	
	memset(sock->options, 0, 8*GSO_COUNT);
	
	if (domain != AF_UNIX)
	{
		// append the socket to the beginning of the socket list.
		// (the order does not acutally matter but adding to the beginning is faster).
		semWait(&sockLock);
		sock->prev = &sockList;
		sock->next = sockList.next;
		if (sock->next != NULL) sock->next->prev = sock;
		sockList.next = sock;
		semSignal(&sockLock);
	};
	
	File *fp = MakeSocketFile(sock);
	
	sock->fp = fp;
	return fp;
};

int BindSocket(File *fp, const struct sockaddr *addr, size_t addrlen)
{
	if (addrlen < 2)
	{
		ERRNO = EAFNOSUPPORT;
		return -1;
	};
	
	if ((fp->oflag & O_SOCKET) == 0)
	{
		ERRNO = ENOTSOCK;
		return -1;
	};
	
	Socket *sock = (Socket*) fp->fsdata;
	if (sock->bind == NULL)
	{
		ERRNO = EOPNOTSUPP;
		return -1;
	};
	
	int result = sock->bind(sock, addr, addrlen);
	return result;
};

int SocketListen(File *fp, int backlog)
{
	if ((fp->oflag & O_SOCKET) == 0)
	{
		ERRNO = ENOTSOCK;
		return -1;
	};
	
	Socket *sock = (Socket*) fp->fsdata;
	if (sock->listen == NULL)
	{
		ERRNO = EOPNOTSUPP;
		return -1;
	};
	
	int result = sock->listen(sock, backlog);
	return result;
};

File* SocketAccept(File *fp, struct sockaddr *addr, size_t *addrlenptr)
{
	if (*addrlenptr < 2)
	{
		ERRNO = EAFNOSUPPORT;
		return NULL;
	};
	
	if ((fp->oflag & O_SOCKET) == 0)
	{
		ERRNO = ENOTSOCK;
		return NULL;
	};
	
	Socket *sock = (Socket*) fp->fsdata;
	if (sock->accept == NULL)
	{
		ERRNO = EOPNOTSUPP;
		return NULL;
	};
	
	Socket *newsock = sock->accept(sock, addr, addrlenptr);
	if (newsock == NULL)
	{
		// ERRNO set by sock->accept()
		return NULL;
	};
	
	File *newfp = MakeSocketFile(newsock);
	newsock->fp = newfp;
	
	return newfp;
};

ssize_t SendtoSocket(File *fp, const void *message, size_t len, int flags, const struct sockaddr *addr, size_t addrlen)
{
	if ((fp->oflag & O_SOCKET) == 0)
	{
		ERRNO = ENOTSOCK;
		return -1;
	};
	
	Socket *sock = (Socket*) fp->fsdata;
	if (sock->sendto == NULL)
	{
		ERRNO = EOPNOTSUPP;
		return -1;
	};
	
	ssize_t result = sock->sendto(sock, message, len, flags, addr, addrlen);
	
	return result;
};

ssize_t RecvfromSocket(File *fp, void *message, size_t len, int flags, struct sockaddr *addr, size_t *addrlen)
{
	if ((fp->oflag & O_SOCKET) == 0)
	{
		ERRNO = ENOTSOCK;
		return -1;
	};
	
	Socket *sock = (Socket*) fp->fsdata;
	if (sock->recvfrom == NULL)
	{
		ERRNO = EOPNOTSUPP;
		return -1;
	};
	
	ssize_t result = sock->recvfrom(sock, message, len, flags, addr, addrlen);
	
	return result;
};

int SocketGetsockname(File *fp, struct sockaddr *addr, size_t *addrlen)
{
	if ((fp->oflag & O_SOCKET) == 0)
	{
		ERRNO = ENOTSOCK;
		return -1;
	};
	
	Socket *sock = (Socket*) fp->fsdata;
	if (sock->getsockname == NULL)
	{
		ERRNO = EOPNOTSUPP;
		return -1;
	};
	
	return sock->getsockname(sock, addr, addrlen);
};

int ShutdownSocket(File *fp, int how)
{
	if ((fp->oflag & O_SOCKET) == 0)
	{
		ERRNO = ENOTSOCK;
		return -1;
	};
	
	Socket *sock = (Socket*) fp->fsdata;
	if (sock->shutdown == NULL)
	{
		ERRNO = EOPNOTSUPP;
		return -1;
	};
	
	sock->shutdown(sock, how);
	return 0;
};

int ConnectSocket(File *fp, const struct sockaddr *addr, size_t addrlen)
{
	if ((fp->oflag & O_SOCKET) == 0)
	{
		ERRNO = ENOTSOCK;
		return -1;
	};
	
	Socket *sock = (Socket*) fp->fsdata;
	if (sock->connect == NULL)
	{
		ERRNO = EOPNOTSUPP;
		return -1;
	};
	
	return sock->connect(sock, addr, addrlen);
};

int SocketGetpeername(File *fp, struct sockaddr *addr, size_t *addrlen)
{
	if ((fp->oflag & O_SOCKET) == 0)
	{
		ERRNO = ENOTSOCK;
		return -1;
	};
	
	Socket *sock = (Socket*) fp->fsdata;
	if (sock->getpeername == NULL)
	{
		ERRNO = EOPNOTSUPP;
		return -1;
	};
	
	return sock->getpeername(sock, addr, addrlen);
};

int SocketBindif(File *fp, const char *ifname)
{
	if (ifname != NULL)
	{
		if (strlen(ifname) > 15)
		{
			ERRNO = EINVAL;
			return -1;
		};
	};
	
	if ((fp->oflag & O_SOCKET) == 0)
	{
		ERRNO = ENOTSOCK;
		return -1;
	};
	
	Socket *sock = (Socket*) fp->fsdata;
	if (ifname != NULL) strcpy(sock->ifname, ifname);
	else sock->ifname[0] = 0;
	return 0;
};

int isValidAddr(const struct sockaddr *addr)
{
	static uint8_t zeroes[16] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
	if (addr->sa_family == AF_INET)
	{
		const struct sockaddr_in *inaddr = (const struct sockaddr_in*) addr;
		if (memcmp(&inaddr->sin_addr, zeroes, 4) == 0)
		{
			return 0;
		};
	}
	else
	{
		const struct sockaddr_in6 *inaddr = (const struct sockaddr_in6*) addr;
		if (memcmp(&inaddr->sin6_addr, zeroes, 16) == 0)
		{
			return 0;
		};
	};
	
	return 1;
};

void passPacketToSocket(const struct sockaddr *src, const struct sockaddr *dest, size_t addrlen,
			const void *packet, size_t size, int proto, uint64_t dataOffset, const char *ifname)
{
	if (!isValidAddr(dest))
	{
		// drop the packet silently
		return;
	};
	
	if (!isValidAddr(src))
	{
		// drop the packet silently because the source is invalid
		return;
	};
	
	struct sockaddr_in6 dest2;
	if (!isLocalAddr(dest))
	{
		int shouldForward = 1;
		if (dest->sa_family == AF_INET6)
		{
			memcpy(&dest2, dest, sizeof(struct sockaddr_in6));
			dest2.sin6_scope_id = 0;
			if (isLocalAddr((struct sockaddr*)&dest2))
			{
				dest = (struct sockaddr*) &dest2;
				shouldForward = 0;
			};
		};
		
		// not our address, forward if enabled
		if (optForwardPackets && shouldForward)
		{
			struct sockaddr fake_src;
			fake_src.sa_family = AF_UNSPEC;
			
			uint8_t hop;
			if (dest->sa_family == AF_INET)
			{
				hop = ((IPHeader4*)packet)->hop;
			}
			else
			{
				hop = ((IPHeader6*)packet)->hop;
			};
			
			if (hop == 0)
			{
				// hop count exceeded; do not forward
				sendErrorPacket(&fake_src, src, ETIMEDOUT, packet, size);
			}
			else
			{
				int status = sendPacket(&fake_src, dest, packet, size, IPPROTO_RAW, NT_SECS(1), NULL);
				if (status < 0)
				{
					// send an error packet to the source
					if (status == -ETIMEDOUT)
					{
						status = -EHOSTUNREACH;
					};
				
					fake_src.sa_family = AF_UNSPEC;
					sendErrorPacket(&fake_src, src, -status, packet, size);
				};
			};
		};
		
		if (shouldForward) return;
	};

	// if the packet is fragmented, pass it to the reassembler
	size_t realSize;
	if (src->sa_family == AF_INET)
	{
		PacketInfo4 info;
		ipv4_header2info((IPHeader4*) packet, &info);
		//kprintf("RECEIVED PACKET WITH: fragOff=%d, size=%d, moreFrags=%d\n", (int) info.fragOff, (int) info.size, info.moreFrags);
		if ((info.fragOff != 0) || (info.moreFrags))
		{
			const struct sockaddr_in *insrc = (const struct sockaddr_in*) src;
			const struct sockaddr_in *indst = (const struct sockaddr_in*) dest;
			
			char srcaddr[16];
			char destaddr[16];
			
			memset(srcaddr, 0, 16);
			memset(destaddr, 0, 16);
			
			memcpy(srcaddr, &insrc->sin_addr, 4);
			memcpy(destaddr, &indst->sin_addr, 4);
			
			if (info.size > size) return;
			
			ipreasmPass(AF_INET, srcaddr, destaddr, proto, info.id, info.fragOff, 
					((char*)packet + dataOffset), info.size, info.moreFrags);
			return;
		};
		
		if (info.proto == IPPROTO_ICMP)
		{
			onICMPPacket(src, dest, (char*)packet + dataOffset, info.size);
		};
		
		realSize = info.size;
	}
	else
	{
		PacketInfo6 info;
		ipv6_header2info((IPHeader6*) packet, &info);
		if ((info.fragOff != 0) || (info.moreFrags))
		{
			const struct sockaddr_in6 *insrc = (const struct sockaddr_in6*) src;
			const struct sockaddr_in6 *indst = (const struct sockaddr_in6*) dest;
			
			char srcaddr[16];
			char destaddr[16];
			
			memcpy(srcaddr, &insrc->sin6_addr, 16);
			memcpy(destaddr, &indst->sin6_addr, 16);
			
			if (info.size > size) return;
			
			ipreasmPass(AF_INET6, srcaddr, destaddr, proto, info.id, info.fragOff,
					((char*)packet + dataOffset), info.size, info.moreFrags);
			return;
		}
		else if (info.proto == IPPROTO_ICMPV6)
		{
			onICMP6Packet(src, dest, (char*)packet + dataOffset, info.size);
		};
		
		realSize = info.size;
	};
	
	if (realSize > size) return;
	onTransportPacket(src, dest, addrlen, (char*)packet + dataOffset, realSize, proto, ifname);
};

void onTransportPacket(const struct sockaddr *src, const struct sockaddr *dest, size_t addrlen, const void *packet, size_t size, int proto, const char *ifname)
{
	semWait(&sockLock);
	
	Socket *sock = &sockList;
	while (sock != NULL)
	{
		int thisOneOK = 1;
		if (sock->ifname[0] != 0)
		{
			if (strcmp(ifname, sock->ifname) != 0)
			{
				thisOneOK = 0;
			};
		};
		
		if (thisOneOK)
		{
			if (sock->packet != NULL)
			{
				if (sock->packet(sock, src, dest, addrlen, packet, size, proto) == SOCK_STOP) break;
			};
		};
		
		sock = sock->next;
	};
	
	semSignal(&sockLock);
};

int ClaimSocketAddr(const struct sockaddr *addr, struct sockaddr *dest, const char *ifname)
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
				if (ifname[0] != 0 && sock->ifname[0] != 0)
				{
					if (strcmp(ifname, sock->ifname) != 0) continue;
				};
				
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
	mutexLock(&portLock);
	
	int index;
	for (index=0; index<2048; index++)
	{
		if (ephports[index] != 0xFF)
		{
			int bitoff;
			for (bitoff=0; bitoff<8; bitoff++)
			{
				if ((ephports[index] & (1 << bitoff)) == 0)
				{
					ephports[index] |= (1 << bitoff);
					mutexUnlock(&portLock);
					
					uint16_t result = (uint16_t) (49152 + 8 * index + bitoff);
					return __builtin_bswap16(result);
				};
			};
		};
	};
	
	mutexUnlock(&portLock);
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
		
		mutexLock(&portLock);
		ephports[index] &= ~(1 << bitoff);
		mutexUnlock(&portLock);
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

int SocketMulticast(File *fp, int op, const struct in6_addr *addr, uint32_t scope)
{
	if ((fp->oflag & O_SOCKET) == 0)
	{
		ERRNO = ENOTSOCK;
		return -1;
	};
	
	Socket *sock = (Socket*) fp->fsdata;
	if (sock->domain != AF_INET6)
	{
		ERRNO = EPROTONOSUPPORT;
		return -1;
	};
	
	if (sock->mcast == NULL)
	{
		ERRNO = EOPNOTSUPP;
		return -1;
	};
	
	return sock->mcast(sock, op, addr, scope);
};

void FreeSocket(Socket *sock)
{
	semWait(&sockLock);
	if (sock->prev != NULL) sock->prev->next = sock->next;
	if (sock->next != NULL) sock->next->prev = sock->prev;
	semSignal(&sockLock);
	
	kfree(sock);
};

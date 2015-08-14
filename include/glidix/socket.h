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

#ifndef __glidix_socket_h
#define __glidix_socket_h

#include <glidix/common.h>
#include <glidix/netif.h>
#include <glidix/vfs.h>

#define	SOCK_STREAM			1
#define	SOCK_DGRAM			2
#define	SOCK_RAW			3

#define	SHUT_RD				1
#define	SHUT_WR				2
#define	SHUT_RDWR			3

#define	MSG_EOR				(1 << 0)
#define	MSG_OOB				(1 << 1)
#define	MSG_PEEK			(1 << 2)
#define	MSG_WAITALL			(1 << 3)

typedef struct Socket_
{
	File*				fp;
	int				domain, type, proto;
	
	struct Socket_*			prev;
	struct Socket_*			next;
	
	/**
	 * Called to handle a bind() on this socket.
	 */
	int (*bind)(struct Socket_ *sock, const struct sockaddr *addr, size_t addrlen);
	
	/**
	 * Called to handle a write(), send() or sendto(). For write() and send(), "addr" is set to NULL
	 * and "addrlen" to 0; for sendto(), they are the values passed to the function.
	 */
	ssize_t (*sendto)(struct Socket_ *sock, const void *message, size_t size, int flags, const struct sockaddr *addr, size_t addrlen);
	
	/**
	 * Similarly, this is called by read(), recv() and recvfrom().
	 */
	ssize_t (*recvfrom)(struct Socket_ *sock, void *buffer, size_t len, int flags, struct sockaddr *addr, size_t *addrlen);
	
	/**
	 * This is called by close() and shutdown(). In the case of close(), 'how' is set to SHUT_RDWR.
	 */
	void (*shutdown)(struct Socket_ *sock, int how);
	
	/**
	 * This is called by close() after calling shutdown(). This function is responsible for freeing any resources
	 * used by the socket. Do not free the Socket structure itself! close() does that after this call.
	 */
	void (*close)(struct Socket_ *sock);
	
	/**
	 * This is called on every socket upon the reception of a packet. "src" and "dest" represent the source and
	 * destination address of the packet (usually they have the same address family). "addrlen" is the size of
	 * both address structures. "packet" and "size" point to the packet (including the IP header), and specify
	 * the size of the packet. "proto" is the protocol given on the IP header, and dataOffset is the offset into
	 * the packet to the payload. The socket should ignore the packet if it does not belong to it.
	 */
	void (*packet)(struct Socket_ *sock, const struct sockaddr *src, const struct sockaddr *dest, size_t addrlen,
			const void *packet, size_t size, int proto, uint64_t dataOffset);

	/**
	 * Called to handle a connect() on this socket.
	 */
	int (*connect)(struct Socket_ *sock, const struct sockaddr *addr, size_t addrlen);
	
	/**
	 * Called to handle getsockname() on this socket.
	 */
	int (*getsockname)(struct Socket_ *sock, struct sockaddr *addr, size_t *addrlenptr);
} Socket;

typedef struct
{
	uint8_t			type;
	uint8_t			code;
	uint16_t		checksum;
	uint32_t		rest;
	char			payload[];
} PACKED ICMPPacket;

typedef struct
{
	uint8_t			type;
	uint8_t			code;
	uint16_t		checksum;
	uint16_t		id;
	uint16_t		seq;
	char			payload[];
} PACKED PingPongPacket;

/**
 * This is called from onPacket() once an address is obtained. This will try to deliver the packet to all interested
 * sockets.
 */
void passPacketToSocket(const struct sockaddr *src, const struct sockaddr *dest, size_t addrlen,
			const void *packet, size_t size, int proto, uint64_t dataOffset);

/**
 * Create a socket file description. The returned description will be marked with the O_SOCKET flag, and the 'fsdata'
 * field will point to a structure prefixed with a Socket structure. On error, NULL is returned and errno set accordingly.
 */
File* CreateSocket(int domain, int type, int proto);

/**
 * Attempts to bind the socket referred to by the given file description to the specified address. The return value and errno
 * setting are standard for the POSIX bind() function. Returns ENOTSOCK if the description is not a socket.
 */
int BindSocket(File *fp, const struct sockaddr *addr, size_t addrlen);

/**
 * Sends a message through a socket - this implements both send() and sendto() userspace functions.
 */
ssize_t SendtoSocket(File *fp, const void *message, size_t len, int flags, const struct sockaddr *addr, size_t addrlen);

/**
 * Receive a message from a socket - this implements both recv() and recvfrom() userspace functions.
 */
ssize_t RecvfromSocket(File *fp, void *message, size_t len, int flags, struct sockaddr *addr, size_t *addrlen);

/**
 * Implements getsockname().
 */
int SocketGetsockname(File *fp, struct sockaddr *addr, size_t *addrlenptr);

/**
 * Implements shutdown().
 */
int ShutdownSocket(File *fp, int how);

#endif

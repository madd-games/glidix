/*
	Glidix Runtime

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

#ifndef _SYS_SOCKET_H
#define _SYS_SOCKET_H

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/types.h>

/* address families */
#define	AF_UNSPEC			0
#define	AF_LOCAL			1
#define	AF_UNIX				AF_LOCAL
#define	AF_INET				2
#define	AF_INET6			3
#define	AF_CAPTURE			4

#define	SOCK_STREAM			1
#define	SOCK_DGRAM			2
#define	SOCK_RAW			3
#define	SOCK_SEQPACKET			4

#define	SOCK_CLOEXEC			(1 << 4)

#define	SHUT_RD				1
#define	SHUT_WR				2
#define	SHUT_RDWR			3

#define	MSG_EOR				(1 << 0)
#define	MSG_OOB				(1 << 1)
#define	MSG_PEEK			(1 << 2)
#define	MSG_WAITALL			(1 << 3)

#define	SOL_SOCKET			0

#define	SO_RCVTIMEO			0
#define	SO_SNDTIMEO			1
#define	SO_ERROR			2

struct sockaddr
{
	sa_family_t			sa_family;		/* AF_* */
	char				sa_data[26];
};

struct sockaddr_storage
{
	sa_family_t			ss_family;
	char				ss_data[256];
};

/* implemented by libglidix directly */
int socket(int domain, int type, int proto);
int bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
ssize_t send(int socket, const void *buffer, size_t length, int flags);
ssize_t sendto(int socket, const void *message, size_t length, int flags, const struct sockaddr *dest_addr, socklen_t dest_len);
ssize_t recv(int socket, void *buffer, size_t length, int flags);
ssize_t recvfrom(int socket, void *buffer, size_t length, int flags, struct sockaddr *address, socklen_t *address_len);
int getsockname(int socket, struct sockaddr *address, socklen_t *address_len);
int getpeername(int socket, struct sockaddr *address, socklen_t *address_len);
int shutdown(int socket, int how);
int connect(int socket, const struct sockaddr *addr, socklen_t addrlen);
int setsockopt(int sockfd, int level, int optname, const void *optval, socklen_t optlen);
int getsockopt(int sockfd, int level, int optname, void *optval, socklen_t *optlen);
int listen(int sockfd, int backlog);
int accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen);
int accept4(int sockfd, struct sockaddr *addr, socklen_t *addrlen, int flags);

#ifdef __cplusplus
};	/* extern "C" */
#endif

#endif

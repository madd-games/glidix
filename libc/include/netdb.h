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

#ifndef _NETDB_H
#define _NETDB_H

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/socket.h>

#define	EAI_AGAIN			1
#define	EAI_BADFLAGS			2
#define	EAI_FAIL			3
#define	EAI_FAMILY			4
#define	EAI_MEMORY			5
#define	EAI_NONAME			6
#define	EAI_SERVICE			7
#define	EAI_SOCKTYPE			8
#define	EAI_SYSTEM			9
#define	EAI_OVERFLOW			10

#define	AI_PASSIVE			(1 << 0)
#define	AI_CANONNAME			(1 << 1)
#define	AI_NUMERICHOST			(1 << 2)
#define	AI_NUMERICSERV			(1 << 3)
#define	AI_V4MAPPED			(1 << 4)
#define	AI_ALL				(1 << 5)
#define	AI_ADDRCONFIG			(1 << 6)

#define	HOST_NOT_FOUND			1
#define	NO_DATA				2
#define	NO_RECOVERY			3
#define	TRY_AGAIN			4

struct hostent
{
	char*				h_name;
	char**				h_aliases;
	int				h_addrtype;
	int				h_length;
	char**				h_addr_list;
};

struct addrinfo
{
	int				ai_flags;
	int				ai_family;
	int				ai_socktype;
	int				ai_protocol;
	socklen_t			ai_addrlen;
	struct sockaddr*		ai_addr;
	char*				ai_canonname;
	struct addrinfo*		ai_next;
	
	/**
	 * Private (glidix only).
	 */
	struct sockaddr_storage		__ai_storage;
};

extern int h_errno;

struct hostent *gethostbyname(const char *name);
int getaddrinfo(const char *nodename, const char *servname, const struct addrinfo *hints, struct addrinfo **out);
void freeaddrinfo(struct addrinfo *ai);
const char* gai_strerror(int errcode);
#define	h_addr	h_addr_list[0]

#ifdef __cplusplus
};	/* extern "C" */
#endif

#endif

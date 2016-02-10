/*
	Glidix Runtime

	Copyright (c) 2014-2016, Madd Games.
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

#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>

/**
 * TODO: No DNS resolutions happen yet!
 */

struct addr_list
{
	struct addrinfo*			head;
	struct addrinfo*			tail;
};

static int __getsrvport(const char *servname, int *resp)
{
	int result = 0;
	for (; *servname!=0; servname++)
	{
		char c = *servname;
		if ((c < '0') || (c > '9'))
		{
			return -1;
		};
		
		result = result * 10 + (c-'0');
	};
	
	*resp = result;
	return 0;
};

void __add_addr(struct addr_list *list, const struct addrinfo *hints, struct addrinfo *info)
{
	info->ai_addr = (struct sockaddr*) &info->__ai_storage;
	
	if (hints != NULL)
	{
		if ((hints->ai_family != AF_UNSPEC) && (hints->ai_family != info->ai_family))
		{
			return;
		};
		
		if ((hints->ai_socktype != 0) && (hints->ai_socktype != info->ai_socktype))
		{
			return;
		};
		
		if (hints->ai_protocol != 0)
		{
			info->ai_protocol = hints->ai_protocol;
		};
		
		if ((hints->ai_flags & AI_V4MAPPED) && (info->ai_addr->sa_family == AF_INET))
		{
			char addr4[4];
			memcpy(addr4, &(((struct sockaddr_in*)info->ai_addr)->sin_addr), 4);
			unsigned short portno = ((struct sockaddr_in*)info->ai_addr)->sin_port;
			
			struct sockaddr_in6 *inaddr = (struct sockaddr_in6*) info->ai_addr;
			inaddr->sin6_family = AF_INET6;
			inaddr->sin6_port = htons(portno);
			inaddr->sin6_flowinfo = 0;
			
			memset(&inaddr->sin6_addr.s6_addr[0], 0, 10);
			memset(&inaddr->sin6_addr.s6_addr[10], 0xFF, 2);
			memcpy(&inaddr->sin6_addr.s6_addr[12], addr4, 4);
			
			inaddr->sin6_scope_id = 0;
			info->ai_addrlen = sizeof(struct sockaddr_in6);
		};
	};
	
	struct addrinfo *ai = (struct addrinfo*) malloc(sizeof(struct addrinfo));
	memcpy(ai, info, sizeof(struct addrinfo));
	ai->ai_addr = (struct sockaddr*) &ai->__ai_storage;
	ai->ai_next = NULL;
	
	if (list->head == NULL)
	{
		list->head = list->tail = ai;
	}
	else
	{
		list->tail->ai_next = ai;
		list->tail = ai;
	};
};

int getaddrinfo(const char *nodename, const char *servname, const struct addrinfo *hints, struct addrinfo **out)
{
	int portno = 0;
	
	if (servname != NULL)
	{
		if (__getsrvport(servname, &portno) != 0)
		{
			return EAI_SERVICE;
		};
	};
	
	struct addr_list list = {NULL, NULL};
	
	if (nodename == NULL)
	{
		struct addrinfo wildcard_v4, wildcard_v6;
		memset(&wildcard_v4, 0, sizeof(struct addrinfo));
		memset(&wildcard_v6, 0, sizeof(struct addrinfo));
		
		wildcard_v6.ai_family = AF_INET6;
		wildcard_v6.ai_socktype = SOCK_STREAM;
		wildcard_v6.ai_addrlen = sizeof(struct sockaddr_in6);

		struct sockaddr_in6 *inaddr6 = (struct sockaddr_in6*) &wildcard_v6.__ai_storage;
		inaddr6->sin6_family = AF_INET6;
		inaddr6->sin6_port = htons((unsigned short) portno);
		
		__add_addr(&list, hints, &wildcard_v6);
		wildcard_v6.ai_socktype = SOCK_DGRAM;
		__add_addr(&list, hints, &wildcard_v6);
		
		wildcard_v4.ai_family = AF_INET;
		wildcard_v4.ai_socktype = SOCK_STREAM;
		wildcard_v4.ai_addrlen = sizeof(struct sockaddr_in);
		
		struct sockaddr_in *inaddr = (struct sockaddr_in*) &wildcard_v4.__ai_storage;
		inaddr->sin_family = AF_INET;
		inaddr->sin_port = htons((unsigned short) portno);
		
		__add_addr(&list, hints, &wildcard_v4);
		wildcard_v4.ai_socktype = SOCK_DGRAM;
		__add_addr(&list, hints, &wildcard_v4);
	}
	else
	{
		char addrbuf[16];
		if (inet_pton(AF_INET, nodename, addrbuf))
		{
			// we have an IPv4 address
			struct addrinfo info;
			memset(&info, 0, sizeof(struct addrinfo));
			
			info.ai_family = AF_INET;
			info.ai_socktype = SOCK_STREAM;
			info.ai_addrlen = sizeof(struct sockaddr_in);
			
			struct sockaddr_in *inaddr = (struct sockaddr_in*) &info.__ai_storage;
			inaddr->sin_family = AF_INET;
			memcpy(&inaddr->sin_addr, addrbuf, 4);
			inaddr->sin_port = htons((unsigned short) portno);
			
			__add_addr(&list, hints, &info);
			info.ai_socktype = SOCK_DGRAM;
			__add_addr(&list, hints, &info);
		}
		else if (inet_pton(AF_INET6, nodename, addrbuf))
		{
			// we have an IPv6 address
			struct addrinfo info;
			memset(&info, 0, sizeof(struct addrinfo));
			
			info.ai_family = AF_INET6;
			info.ai_socktype = SOCK_STREAM;
			info.ai_addrlen = sizeof(struct sockaddr_in6);
			
			struct sockaddr_in6 *inaddr = (struct sockaddr_in6*) &info.__ai_storage;
			inaddr->sin6_family = AF_INET6;
			memcpy(&inaddr->sin6_addr, addrbuf, 16);
			inaddr->sin6_port = htons((unsigned short) portno);
			__add_addr(&list, hints, &info);
			info.ai_socktype = SOCK_DGRAM;
			__add_addr(&list, hints, &info);
		}
		else
		{
			// TODO: name resolution with DNS
			if (strcmp(nodename, "localhost") == 0)
			{
				struct addrinfo info4, info6;
				memset(&info4, 0, sizeof(struct addrinfo));
				memset(&info6, 0, sizeof(struct addrinfo));
				
				info6.ai_family = AF_INET6;
				info6.ai_socktype = SOCK_STREAM;
				info6.ai_addrlen = sizeof(struct sockaddr_in6);
				
				struct sockaddr_in6 *inaddr6 = (struct sockaddr_in6*) &info6.__ai_storage;
				inaddr6->sin6_family = AF_INET6;
				inet_pton(AF_INET6, "::1", &inaddr6->sin6_addr);
				inaddr6->sin6_port = htons((unsigned short) portno);
				__add_addr(&list, hints, &info6);
				info6.ai_socktype = SOCK_DGRAM;
				__add_addr(&list, hints, &info6);
				
				info4.ai_family = AF_INET;
				info4.ai_socktype = SOCK_STREAM;
				info4.ai_addrlen = sizeof(struct sockaddr_in);
				
				struct sockaddr_in *inaddr = (struct sockaddr_in*) &info4.__ai_storage;
				inaddr->sin_family = AF_INET;
				inet_pton(AF_INET, "127.0.0.1", &inaddr->sin_addr);
				inaddr->sin_port = htons((unsigned short) portno);
				__add_addr(&list, hints, &info4);
				info4.ai_socktype = SOCK_DGRAM;
				__add_addr(&list, hints, &info4);
			};
		};
	};
	
	if (list.head == NULL)
	{
		return EAI_NONAME;
	};
	
	*out = list.head;
	return 0;
};

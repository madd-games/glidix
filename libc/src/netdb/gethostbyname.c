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

#include <netdb.h>
#include <string.h>
#include <stdlib.h>
#include <netinet/in.h>

static struct hostent hostentBuffer;
static char* aliases[1] = {NULL};
static char nodename[256];
static struct in_addr addr;
static struct in_addr* addrArray[2] = {&addr, NULL};
int h_errno;
struct hostent *gethostbyname(const char *name)
{
	// OBSOLETE DO NOT USE
	// This only exists for backwards compatiblity with some old POSIX software.
	// Only supports IPv4!
	if (strlen(name) >= 256)
	{
		h_errno = HOST_NOT_FOUND;
		return NULL;
	};
	
	struct addrinfo hints;
	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_INET;
	
	struct addrinfo *res;
	if (getaddrinfo(name, NULL, &hints, &res) != 0)
	{
		h_errno = HOST_NOT_FOUND;
		return NULL;
	};
	
	struct sockaddr_in *inaddr = (struct sockaddr_in*) res->ai_addr;
	strcpy(nodename, name);
	memcpy(&addr, &inaddr->sin_addr, 4);
	hostentBuffer.h_name = nodename;
	hostentBuffer.h_aliases = aliases;
	hostentBuffer.h_addrtype = AF_INET;
	hostentBuffer.h_length = 4;
	hostentBuffer.h_addr_list = (char**) addrArray;
	freeaddrinfo(res);
	return &hostentBuffer;
};

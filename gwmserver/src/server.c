/*
	Glidix GUI

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

#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/un.h>
#include <libddi.h>
#include <libgwm.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <pthread.h>
#include <inttypes.h>
#include <libgwm.h>

void* clientThreadFunc(void *context)
{
	int sockfd = (int) (uintptr_t) context;
	
	while (1)
	{
		GWMCommand cmd;
		
		ssize_t sz = read(sockfd, &cmd, sizeof(GWMCommand));
		if (sz == -1)
		{
			// TODO: error reporting?
			break;
		};
		
		if (sz == 0)
		{
			// client disconnected
			// TODO: delete its windows etc
			break;
		};
		
		// TODO: handle the message and respond.
	};
	
	close(sockfd);
	return NULL;
};

void runServer(int sockfd)
{
	struct sockaddr_storage caddr;
	while (1)
	{
		socklen_t addrlen = sizeof(struct sockaddr_storage);
		int client = accept(sockfd, (struct sockaddr*) &caddr, &addrlen);
		if (client != -1)
		{
			pthread_t thread;
			if (pthread_create(&thread, NULL, clientThreadFunc, (void*) (uintptr_t) client) == 0)
			{
				pthread_detach(thread);
			}
			else
			{
				// TODO: error reporting
				close(client);
			};
		};
	};
};

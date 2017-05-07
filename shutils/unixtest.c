/*
	Glidix Shell Utilities

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
#include <sys/un.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

int main()
{
	printf("Creating socket...\n");
	int sockfd = socket(AF_UNIX, SOCK_SEQPACKET, 0);
	if (sockfd == -1)
	{
		perror("socket");
		return 1;
	};
	
	printf("Binding to /run/test.sock...\n");
	struct sockaddr_un addr;
	addr.sun_family = AF_UNIX;
	strcpy(addr.sun_path, "/run/test.sock");
	
	if (bind(sockfd, (struct sockaddr*) &addr, sizeof(struct sockaddr_un)) != 0)
	{
		perror("bind");
		return 1;
	};
	
	printf("Listening...\n");
	if (listen(sockfd, 5) != 0)
	{
		perror("listen");
		return 1;
	};
	
	printf("Accepting...\n");
	while (1)
	{
		struct sockaddr_un cliaddr;
		socklen_t addrlen = sizeof(struct sockaddr_un);
		int client = accept(sockfd, (struct sockaddr*) &cliaddr, &addrlen);
		if (client == -1)
		{
			perror("accept");
			return 1;
		};
	
		printf("Sending message...\n");
		write(client, "hello world", 12);
		
		printf("Waiting for message back...\n");
		char msgback[64];
		ssize_t sz = read(client, msgback, 64);
		if (sz == -1)
		{
			perror("read");
		}
		else
		{
			msgback[sz] = 0;
			printf("Message: '%s' (%ld bytes)\n", msgback, sz);
		};
		
		close(client);
		printf("OK, looping again...\n");
	};
	
	return 0;
};

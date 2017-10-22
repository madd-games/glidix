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

#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main()
{
	int sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sockfd == -1)
	{
		perror("socket");
		return 1;
	};
	
	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(struct sockaddr_in));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(7777);
	
	if (bind(sockfd, (struct sockaddr*) &addr, sizeof(struct sockaddr_in)) != 0)
	{
		perror("bind");
		close(sockfd);
		return 1;
	};
	
	if (listen(sockfd, 5) != 0)
	{
		perror("listen");
		close(sockfd);
		return 1;
	};
	
	while (1)
	{
		struct sockaddr_in addr;
		socklen_t len = sizeof(struct sockaddr_in);
		int client = accept(sockfd, (struct sockaddr*) &addr, &len);
		if (client == -1)
		{
			perror("accept");
			close(sockfd);
			return 1;
		};
		
		char buf[INET_ADDRSTRLEN];
		inet_ntop(AF_INET, &addr.sin_addr, buf, INET_ADDRSTRLEN);
		printf("[qotd] accepted a connection from %s:%hu; sending 'hello world'\n", buf, ntohs(addr.sin_port));
		write(client, "hello world", 11);
		close(client);
	};
	
	return 0;
};

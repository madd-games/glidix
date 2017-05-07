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
	
	printf("Attempting to connect to /run/test.sock...\n");
	struct sockaddr_un addr;
	addr.sun_family = AF_UNIX;
	strcpy(addr.sun_path, "/run/test.sock");
	
	if (connect(sockfd, (struct sockaddr*) &addr, sizeof(struct sockaddr_un)) != 0)
	{
		perror("connect");
		return 1;
	};
	
	printf("Trying to receive...\n");
	char msg[64];
	ssize_t sz = read(sockfd, msg, 64);
	if (sz == -1)
	{
		perror("read");
		return 1;
	};
	
	msg[sz] = 0;
	printf("Got message: '%s' (%ld bytes)\n", msg, sz);
	
	printf("Trying to reply...\n");
	write(sockfd, "TEST MSG", 9);
	
	printf("Done, bye bye.\n");
	close(sockfd);
	return 0;
};

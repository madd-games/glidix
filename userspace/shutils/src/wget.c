/*
	Glidix kernel

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
#include <sys/glidix.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include <netdb.h>

char *progName;

void usage()
{
	fprintf(stderr, "USAGE:\t%s <url>\n", progName);
	fprintf(stderr, "\tDownloads a file indicated by the URL.\n");
};

int main(int argc, char *argv[])
{
	progName = argv[0];
	
	if (argc != 2)
	{
		usage();
		return 1;
	};
	
	char *url = argv[1];
	if (strlen(url) < strlen("http://"))
	{
		fprintf(stderr, "%s: unsupported URL: %s\n", argv[0], url);
		return 1;
	};
	
	if (memcmp(url, "http://", strlen("http://")) != 0)
	{
		fprintf(stderr, "%s: unsupported URL: %s\n", argv[0], url);
		return 1;
	};
	
	char *server = &url[strlen("http://")];
	if (*server == 0)
	{
		fprintf(stderr, "%s: no server specified\n", argv[0]);
		return 1;
	};
	
	char *pathname = strchr(server, '/');
	if (pathname == NULL)
	{
		fprintf(stderr, "%s: no path on server specified\n", argv[0]);
		return 1;
	};
	
	char nodename[256];
	if ((pathname-server) > 255)
	{
		fprintf(stderr, "%s: server name too long\n", argv[0]);
		return 1;
	};
	
	memset(nodename, 0, 256);
	memcpy(nodename, server, pathname-server);
	// TODO: break up nodename into nodename and portno
	const char *port = "80";
	
	struct addrinfo hints;
	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;
	hints.ai_flags = 0;
	
	struct addrinfo *info;
	int status = getaddrinfo(nodename, port, &hints, &info);
	if (status != 0)
	{
		fprintf(stderr, "%s: failed to resolve '%s': %s\n", argv[0], nodename, gai_strerror(status));
		return 1;
	};
	
	int sockfd;
	struct addrinfo *ai;
	
	printf("Connecting to %s...\n", nodename);
	for (ai=info; ai!=NULL; ai=ai->ai_next)
	{
		char paddr[512];
		if (ai->ai_addr->sa_family == AF_INET)
		{
			char tmpbuf[INET_ADDRSTRLEN];
			const struct sockaddr_in *inaddr = (const struct sockaddr_in*) ai->ai_addr;
			inet_ntop(AF_INET, &inaddr->sin_addr, tmpbuf, INET_ADDRSTRLEN);
			
			sprintf(paddr, "%s:%hu", tmpbuf, ntohs(inaddr->sin_port));
		}
		else if (ai->ai_addr->sa_family == AF_INET6)
		{
			char tmpbuf[INET6_ADDRSTRLEN];
			const struct sockaddr_in6 *inaddr = (const struct sockaddr_in6*) ai->ai_addr;
			inet_ntop(AF_INET6, &inaddr->sin6_addr, tmpbuf, INET6_ADDRSTRLEN);
			
			sprintf(paddr, "[%s]:%hu", tmpbuf, ntohs(inaddr->sin6_port));
		}
		else
		{
			continue;
		};
		
		printf("Trying %s... ", paddr);
		
		sockfd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
		if (sockfd == -1)
		{
			printf("socket: %s\n", strerror(errno));
			continue;
		};
		
		if (connect(sockfd, ai->ai_addr, ai->ai_addrlen) != 0)
		{
			printf("connect: %s\n", strerror(errno));
			close(sockfd);
			continue;
		};
		
		printf("Good\n");
		break;
	};
	
	if (ai == NULL)
	{
		fprintf(stderr, "%s: failed to connect to %s\n", argv[0], nodename);
		return 1;
	};
	
	freeaddrinfo(info);
	
	printf("Sending HTTP request... ");
	const char *format = "GET %s HTTP/1.1\r\nHost: %s\r\nUser-Agent: glidix-shutils-wget\r\n\r\n";
	ssize_t requestSize = (ssize_t) (strlen(format)+strlen(pathname)+strlen(nodename)+1);
	char *request = (char*) malloc((size_t)requestSize);
	sprintf(request, format, pathname, nodename);
	
	ssize_t sz = write(sockfd, request, strlen(request));
	if (sz == -1)
	{
		printf("%s\n", strerror(errno));
		close(sockfd);
		return 1;
	};
	
	if (sz < strlen(request))
	{
		printf("Failed to send the whole request\n");
		close(sockfd);
		return 1;
	};
	
	printf("Done\n");

	int fd;
	if (strlen(pathname) == 1)
	{
		fd = open("index.html", O_RDWR | O_CREAT | O_TRUNC | O_EXCL, 0644);
	}
	else
	{
		fd = open(&pathname[1], O_RDWR | O_CREAT | O_TRUNC | O_EXCL, 0644);
	};
	
	if (fd == -1)
	{
		fprintf(stderr, "%s: cannot open output file: %s\n", argv[0], strerror(errno));
		close(sockfd);
		return 1;
	};
	
	printf("Awaiting response...\n");
	char *headers = (char*) malloc(1);
	headers[0] = 0;
	size_t headSize = 0;
	
	while (strstr(headers, "\r\n\r\n") == NULL)
	{
		char rcvbuf[1024];
		ssize_t sizeGot = read(sockfd, rcvbuf, 1024);
		
		if (sizeGot == -1)
		{
			printf("%s\n", strerror(errno));
			close(sockfd);
			return 1;
		};
		
		if (sizeGot == 0)
		{
			printf("EOF before end of headers\n");
			close(sockfd);
			return 1;
		};
		
		size_t offset = headSize;
		headSize += sizeGot;
		
		headers = realloc(headers, headSize+1);
		memcpy(&headers[offset], rcvbuf, sizeGot);
		headers[headSize] = 0;
	};
	
	char *terminator = strstr(headers, "\r\n\r\n");
	*terminator = 0;
	
	terminator += 4;
	size_t dataOff = terminator - headers;
	size_t toWrite = headSize - dataOff;
	
	if (toWrite > 0)
	{
		write(fd, terminator, toWrite);
	};
	
	// parse the headers now
	char *respline = strtok(headers, "\r\n");
	printf("%s\n", respline);
	
	ssize_t contentLen = -1;
	char *line;
	
	while ((line = strtok(NULL, "\r\n")) != NULL)
	{
		if (strlen(line) > strlen("Content-Length: "))
		{
			if (memcmp(line, "Content-Length: ", strlen("Content-Length: ")) == 0)
			{
				if (sscanf(&line[strlen("Content-Length: ")], "%ld", &contentLen) != 1)
				{
					fprintf(stderr, "%s: invalid Content-Length!\n", argv[0]);
					close(fd);
					close(sockfd);
					return 1;
				};
			};
		};
	};
	
	printf("Downloading...\n");
	
	ssize_t downloaded = (ssize_t) toWrite;
	while ((downloaded < contentLen) || (contentLen == -1))
	{
		if (contentLen == -1)
		{
			printf("\rDownloaded: %20ld bytes", downloaded);
		}
		else
		{
			ssize_t percent = (100*downloaded)/contentLen;
			ssize_t scale = (70*downloaded)/contentLen;
			ssize_t pad = 70 - scale;
			printf("\r[");
			while (scale--)
			{
				printf("=");
			};
			
			while (pad--)
			{
				printf(" ");
			};
			
			printf("] %3ld%%", percent);
		};
		
		char buf[4096];
		ssize_t sz = read(sockfd, buf, 4096);
		
		if (sz == -1)
		{
			int error = errno;
			printf("\r[XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX] ERR%%\n");
			printf("read: %s\n", strerror(errno));
			close(fd);
			close(sockfd);
			return 1;
		};
		
		if (sz == 0)
		{
			break;
		};
		
		write(fd, buf, (size_t)sz);
		downloaded += sz;
	};
	
	close(fd);
	close(sockfd);
	
	if (contentLen != -1)
	{
		if (downloaded < contentLen)
		{
			printf("\n%s: connection terminated before full file received\n");
			return 1;
		};
	};
	
	printf("\r[======================================================================] 100%%\n");
	return 0;
};

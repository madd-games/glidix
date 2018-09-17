/*
	Glidix Package Manager

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

#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <netdb.h>
#include <arpa/inet.h>

#include "libgpm.h"

static void gpmFetch(GPMContext *ctx, FILE *out, const char *url)
{
	if (strlen(url) < strlen("http://"))
	{
		fprintf(ctx->log, "ERROR: Unsupported URL: %s\n", url);
		return;
	};
	
	if (memcmp(url, "http://", strlen("http://")) != 0)
	{
		fprintf(ctx->log, "ERROR: Unsupported URL: %s\n", url);
		return;
	};
	
	const char *server = &url[strlen("http://")];
	if (*server == 0)
	{
		fprintf(ctx->log, "ERROR: Unsupported URL: %s\n", url);
		return;
	};
	
	const char *pathname = strchr(server, '/');
	if (pathname == NULL)
	{
		fprintf(ctx->log, "ERROR: Unsupported URL: %s\n", url);
		return;
	};
	
	char nodename[256];
	if ((pathname-server) > 255)
	{
		fprintf(ctx->log, "ERROR: Unsupported URL: %s\n", url);
		return;
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
		fprintf(ctx->log, "ERROR: Failed to resolve '%s': %s\n", nodename, gai_strerror(status));
		return;
	};
	
	int sockfd;
	struct addrinfo *ai;
	
	fprintf(ctx->log, "Connecting to %s...\n", nodename);
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
		
		fprintf(ctx->log, "Trying %s... ", paddr);
		
		sockfd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
		if (sockfd == -1)
		{
			fprintf(ctx->log, "socket: %s\n", strerror(errno));
			continue;
		};
		
		if (connect(sockfd, ai->ai_addr, ai->ai_addrlen) != 0)
		{
			fprintf(ctx->log, "connect: %s\n", strerror(errno));
			close(sockfd);
			continue;
		};
		
		fprintf(ctx->log, "Good\n");
		break;
	};
	
	if (ai == NULL)
	{
		fprintf(ctx->log, "ERROR: failed to connect to %s\n", nodename);
		return;
	};
	
	freeaddrinfo(info);
	
	const char *format = "GET %s HTTP/1.1\r\nHost: %s\r\nUser-Agent: gpm-reindex\r\n\r\n";
	ssize_t requestSize = (ssize_t) (strlen(format)+strlen(pathname)+strlen(nodename)+1);
	char *request = (char*) malloc((size_t)requestSize);
	sprintf(request, format, pathname, nodename);
	fprintf(ctx->log, "Sending HTTP request:\n%s\n***END REQUEST**\n", request);

	ssize_t sz = write(sockfd, request, strlen(request));
	if (sz == -1)
	{
		fprintf(ctx->log, "ERROR:%s\n", strerror(errno));
		close(sockfd);
		return;
	};
	
	if (sz < strlen(request))
	{
		fprintf(ctx->log, "Failed to send the whole request\n");
		close(sockfd);
		return;
	};
	
	fprintf(ctx->log, "Awaiting response...\n");
	char *headers = (char*) malloc(1);
	headers[0] = 0;
	size_t headSize = 0;
	
	while (strstr(headers, "\r\n\r\n") == NULL)
	{
		char rcvbuf[1024];
		ssize_t sizeGot = read(sockfd, rcvbuf, 1);
		
		if (sizeGot == -1)
		{
			printf("%s\n", strerror(errno));
			close(sockfd);
			return;
		};
		
		if (sizeGot == 0)
		{
			printf("EOF before end of headers\n");
			close(sockfd);
			return;
		};
		
		size_t offset = headSize;
		headSize += sizeGot;
		
		headers = realloc(headers, headSize+1);
		memcpy(&headers[offset], rcvbuf, sizeGot);
		headers[headSize] = 0;
	};
	
	char *terminator = strstr(headers, "\r\n\r\n");
	*terminator = 0;
	
	// parse the headers now
	char *respline = strtok(headers, "\r\n");
	fprintf(ctx->log, "Response line: %s\n", respline);
	
	ssize_t contentLen = -1;
	char *line;
	int chunked = 0;
	
	while ((line = strtok(NULL, "\r\n")) != NULL)
	{
		if (strlen(line) > strlen("Content-Length: "))
		{
			if (memcmp(line, "Content-Length: ", strlen("Content-Length: ")) == 0)
			{
				if (sscanf(&line[strlen("Content-Length: ")], "%ld", &contentLen) != 1)
				{
					fprintf(ctx->log, "ERROR: Invalid Content-Length!\n");
					close(sockfd);
					return;
				};
			};
		};
		
		if (strcmp(line, "Transfer-Encoding: chunked") == 0)
		{
			chunked = 1;
		};
	};
	
	fprintf(ctx->log, "Downloading...\n");
	fprintf(out, "\n# Source: %s\n", url);
	
	if (chunked)
	{
		while (1)
		{
			size_t nextChunkSize = 0;
			while (1)
			{
				char c;
				if (read(sockfd, &c, 1) != 1)
				{
					fprintf(ctx->log, "\nerror: premature end of stream\n");
					return;
				};
				
				if (c >= '0' && c <= '9')
				{
					nextChunkSize = (nextChunkSize << 4) | (c - '0');
				}
				else if (c >= 'a' && c <= 'f')
				{
					nextChunkSize = (nextChunkSize << 4) | (c - 'a' + 10);
				}
				else if (c >= 'A' && c <= 'F')
				{
					nextChunkSize = (nextChunkSize << 4) | (c - 'A' + 10);
				}
				else if (c == '\r')
				{
					break;
				}
				else
				{
					fprintf(ctx->log, "\nerror: invalid character in chunk header\n");
					return;
				};
			};
			
			char c;
			if (read(sockfd, &c, 1) != 1)
			{
				fprintf(ctx->log, "\nerror: premature end of stream\n");
				return;
			};
			
			if (c != '\n')
			{
				fprintf(ctx->log, "\nerror: invalid termination of chunk header\n");
				return;
			};
			
			if (nextChunkSize == 0)
			{
				break;
			};
			
			while (nextChunkSize > 0)
			{
				char buffer[1024];
				
				size_t toRecv = nextChunkSize;
				if (toRecv > 1024) toRecv = 1024;
				
				ssize_t actuallyRecv = read(sockfd, buffer, toRecv);
				if (actuallyRecv < 2)
				{
					fprintf(ctx->log, "\nerror: failed to read chunk data\n");
					return;
				};
				
				fwrite(buffer, (size_t) actuallyRecv, 1, out);
				nextChunkSize -= actuallyRecv;
			};
			
			if (read(sockfd, &c, 1) != 1)
			{
				fprintf(ctx->log, "\nerror: premature end of stream\n");
				return;
			};
			
			if (c != '\r')
			{
				fprintf(ctx->log, "\nerror: invalid chunk terminator\n");
				return;
			};
			
			if (read(sockfd, &c, 1) != 1)
			{
				fprintf(ctx->log, "\nerror: premature end of stream\n");
				return;
			};
			
			if (c != '\n')
			{
				fprintf(ctx->log, "\nerror: invalid chunk terminator\n");
				return;
			};
		};
	}
	else
	{
		ssize_t downloaded = 0;
		while ((downloaded < contentLen) || (contentLen == -1))
		{
			char buf[4096];
			ssize_t sz = read(sockfd, buf, 4096);
			
			if (sz == -1)
			{
				int error = errno;
				fprintf(ctx->log, "read: %s\n", strerror(error));
				close(sockfd);
				return;
			};
			
			if (sz == 0)
			{
				break;
			};
			
			fwrite(buf, (size_t)sz, 1, out);
			downloaded += sz;
		};

		if (contentLen != -1)
		{
			if (downloaded < contentLen)
			{
				fprintf(ctx->log, "\nERROR: Connection terminated before full file received\n");
				close(sockfd);
				return;
			};
		};
	};
	
	close(sockfd);
	fprintf(ctx->log, "Download OK.\n");
};

int gpmReindex(GPMContext *ctx)
{
	fprintf(ctx->log, "Re-indexing...\n");
	
	char *confpath;
	if (asprintf(&confpath, "%s/repos.conf", ctx->basedir) == -1)
	{
		fprintf(ctx->log, "ERROR: Out of memory\n");
		return -1;
	};
	
	FILE *fp = fopen(confpath, "r");
	if (fp == NULL)
	{
		fprintf(ctx->log, "ERROR: Failed to open `%s`: %s\n", confpath, strerror(errno));
		free(confpath);
		return -1;
	};
	
	free(confpath);
	
	char linebuf[1024];
	char *line;
	
	char *indexpath;
	if (asprintf(&indexpath, "%s/repos.index", ctx->basedir) == -1)
	{
		fprintf(ctx->log, "ERROR: Out of memory\n");
		fclose(fp);
		return -1;
	};
	
	FILE *out = fopen(indexpath, "w");
	if (out == NULL)
	{
		fprintf(ctx->log, "ERROR: Failed to open `%s` for write: %s\n", indexpath, strerror(errno));
		free(indexpath);
		fclose(fp);
		return -1;
	};
	
	free(indexpath);
	
	fprintf(out, "# Package index. NOTE: This file was automatically generated by GPM. Do not edit!\n");
	
	while ((line = fgets(linebuf, 1024, fp)) != NULL)
	{
		char *endline = strchr(line, '\n');
		if (endline != NULL) *endline = 0;
		
		char *comment = strchr(line, '#');
		if (comment != NULL) *comment = 0;
		
		while (*line == ' ' || *line == '\t') line++;
		while (*line != 0 && (line[strlen(line)-1] == ' ' || line[strlen(line)-1] == '\t')) line[strlen(line)-1] = 0;
		if (*line == 0) continue;
		
		gpmFetch(ctx, out, line);
	};
	
	fclose(fp);
	
	fprintf(out, "\n# END OF INDEX\n");
	fprintf(ctx->log, "*** REINDEX COMPLETE ***\n");
	fclose(out);
	
	return 0;
};

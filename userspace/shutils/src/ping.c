/*
	Glidix Shell Utilities

	Copyright (c) 2014-2015, Madd Games.
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

#ifndef __glidix__
#	error This program works on Glidix only!
#endif

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

char *progName;

typedef struct
{
	uint8_t			type;
	uint8_t			code;
	uint16_t		checksum;
	uint16_t		id;
	uint16_t		seq;
	char			payload[];
} __attribute__ ((packed)) PingPongPacket;

uint16_t checksum(void* vdata,size_t length)
{
	// Cast the data pointer to one that can be indexed.
	char* data=(char*)vdata;

	// Initialise the accumulator.
	uint32_t acc=0xffff;

	// Handle complete 16-bit blocks.
	size_t i;
	for (i=0;i+1<length;i+=2) {
		uint16_t word;
		memcpy(&word,data+i,2);
		acc+=__builtin_bswap16(word);
		if (acc>0xffff)
		{
			acc-=0xffff;
		}
	}

	// Handle any partial block at the end of the data.
	if (length&1)
	{
		uint16_t word=0;
		memcpy(&word,data+length-1,1);
		acc+=__builtin_bswap16(word);
		if (acc>0xffff)
		{
			acc-=0xffff;
		}
	}

	// Return the checksum in network byte order.
	return __builtin_bswap16(~acc);
};

void usage()
{
	fprintf(stderr, "USAGE:\t%s inet_addr\n", progName);
	fprintf(stderr, "\tTest whether the specified IPv4 address is reachable\n");
};

int main(int argc, char *argv[])
{
	progName = argv[0];
	
	if (argc != 2)
	{
		usage();
		return 1;
	};
	
	struct sockaddr_in addr;
	addr.sin_family = AF_INET;
	
	if (inet_pton(AF_INET, argv[1], &addr.sin_addr) != 1)
	{
		fprintf(stderr, "%s: %s: %s\n", argv[0], argv[1], strerror(errno));
		return 1;
	};
	
	int sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
	if (sockfd == -1)
	{
		fprintf(stderr, "%s: socket: %s\n", argv[0], strerror(errno));
		return 1;
	};
	
	// 10 second timeout
	if (_glidix_setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, 10000000000) != 0)
	{
		fprintf(stderr, "%s: setsockopt SO_RCVTIMEO: %s\n", argv[0], strerror(errno));
		return 1;
	};
	
	struct sockaddr_in wildcard_addr;
	memset(&wildcard_addr, 0, sizeof(struct sockaddr_in));
	wildcard_addr.sin_family = AF_INET;
	
	if (bind(sockfd, (struct sockaddr*) &wildcard_addr, sizeof(struct sockaddr_in)) != 0)
	{
		fprintf(stderr, "%s: bind: %s\n", argv[0], strerror(errno));
		return 1;
	};
	
	PingPongPacket ping;
	ping.type = 8;
	ping.code = 0;
	ping.checksum = 0;
	ping.id = getpid();
	ping.seq = 0;
	
	printf("PING %s\n", argv[1]);
	clock_t pingtimes[5];
	int i;
	for (i=0; i<5; i++)
	{
		ping.checksum = 0;
		ping.seq = i;
		ping.checksum = checksum(&ping, sizeof(PingPongPacket));
		
		pingtimes[i] = clock();
		if (sendto(sockfd, &ping, sizeof(PingPongPacket), 0, (struct sockaddr*) &addr, sizeof(struct sockaddr_in)) == -1)
		{
			fprintf(stderr, "%s: sendto: %s\n", argv[0], strerror(errno));
			close(sockfd);
			return 1;
		};
	};
	
	int gotBack = 0;
	while (gotBack < 5)
	{
		struct sockaddr_in src;
		PingPongPacket pong;
		
		socklen_t addrlen = sizeof(struct sockaddr_in);
		ssize_t count = recvfrom(sockfd, &pong, sizeof(PingPongPacket), 0, (struct sockaddr*) &src, &addrlen);
		if (count == -1)
		{
			fprintf(stderr, "%s: recvfrom: %s\n", argv[0], strerror(errno));
			close(sockfd);
			return 1;
		};

		clock_t pongtime = clock();
		
		char buffer[INET_ADDRSTRLEN];
		inet_ntop(AF_INET, &src.sin_addr, buffer, INET_ADDRSTRLEN);
		
		if (checksum(&pong, sizeof(PingPongPacket)) != 0)
		{
			// discard
			continue;
		};
		
		if (pong.type != 0)
		{
			// discard
			continue;
		};
		
		if (pong.id != getpid())
		{
			// discard (this is for another process)
			continue;
		};
		
		printf("PONG from %s: id=%d, seq=%d, time=%u ns\n", buffer, pong.id, pong.seq, pongtime-pingtimes[pong.seq]);
		gotBack++;
	};
	
	return 0;
};

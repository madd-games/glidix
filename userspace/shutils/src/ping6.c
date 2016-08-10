/*
	Glidix Shell Utilities

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
#include <netdb.h>

char *progName;

typedef struct
{
	uint8_t			type;
	uint8_t			code;
	uint16_t		checksum;
	uint16_t		id;
	uint16_t		seq;
} PingPong6Packet;

typedef struct
{
	uint8_t			src[16];
	uint8_t			dest[16];
	uint32_t		len;
	uint8_t			zeroes[3];
	uint8_t			proto;			// IPPROTO_ICMPV6
	PingPong6Packet		payload;
} PseudoHeader;

void usage()
{
	fprintf(stderr, "USAGE:\t%s hostname\n", progName);
	fprintf(stderr, "\tTest whether the specified IPv6 host is reachable.\n");
	fprintf(stderr, "\tFor IPv4, use `ping'.\n");
};

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

int main(int argc, char *argv[])
{
	progName = argv[0];
	
	if (argc != 2)
	{
		usage();
		return 1;
	};
	
	struct addrinfo hints;
	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_INET6;
	
	struct addrinfo *addrs;
	int status = getaddrinfo(argv[1], NULL, &hints, &addrs);
	if (status != 0)
	{
		fprintf(stderr, "%s: getaddrinfo %s: %s\n", argv[0], argv[1], gai_strerror(status));
		return 1;
	};
	
	int sockfd = socket(AF_INET6, SOCK_RAW, IPPROTO_ICMPV6);
	if (sockfd == -1)
	{
		fprintf(stderr, "%s: socket: %s\n", argv[0], strerror(errno));
		return 1;
	};
	
	// we need to know the source address in order to place it in the pseudo-header
	// for checksum computation. to do this, we first send out a zero-sized packet,
	// and let the kernel bind the socket to an address.
	PseudoHeader ping;
	memset(&ping, 0, sizeof(PseudoHeader));
	
	if (sendto(sockfd, &ping, 0, 0, addrs->ai_addr, sizeof(struct sockaddr_in6)) == -1)
	{
		fprintf(stderr, "%s: initial sendto: %s\n", progName, strerror(errno));
		return 1;
	};
	
	struct sockaddr_in6 insrc, indst;
	memcpy(&indst, addrs->ai_addr, sizeof(struct sockaddr_in6));
	socklen_t addrlen = sizeof(struct sockaddr_in6);
	
	if (getsockname(sockfd, (struct sockaddr*) &insrc, &addrlen) != 0)
	{
		fprintf(stderr, "%s: getsockname: %s\n", progName, strerror(errno));
		return 1;
	};
	
	// we can now construct the ping request
	PseudoHeader pong;
	memcpy(ping.src, &insrc.sin6_addr, 16);
	memcpy(ping.dest, &indst.sin6_addr, 16);
	ping.len = htonl(sizeof(PingPong6Packet));
	memset(ping.zeroes, 0, 3);
	ping.proto = IPPROTO_ICMPV6;
	ping.payload.type = 128;
	ping.payload.code = 0;
	ping.payload.id = (uint16_t) getpid();
	ping.payload.seq = 0;

	memcpy(pong.src, &indst.sin6_addr, 16);
	memcpy(pong.dest, &insrc.sin6_addr, 16);
	pong.len = htonl(sizeof(PingPong6Packet));
	memset(pong.zeroes, 0, 3);
	pong.proto = IPPROTO_ICMPV6;
	
	char destaddrstr[INET6_ADDRSTRLEN];
	inet_ntop(AF_INET6, &indst.sin6_addr, destaddrstr, INET6_ADDRSTRLEN);
	printf("PING %s (%s)\n", argv[1], destaddrstr);
	
	while (1)
	{
		ping.payload.checksum = 0;
		ping.payload.seq++;
		ping.payload.checksum = checksum(&ping, sizeof(PseudoHeader));
		
		clock_t sendtime = clock();
		if (sendto(sockfd, &ping.payload, sizeof(PingPong6Packet), 0,
			(struct sockaddr*)&indst, sizeof(struct sockaddr_in6)) == -1)
		{
			fprintf(stderr, "%s: sendto: %s\n", progName, strerror(errno));
			continue;
		};
		
		while (1)
		{
			struct sockaddr_in6 rcvaddr;
			addrlen = sizeof(struct sockaddr_in6);
			ssize_t size = recvfrom(sockfd, &pong.payload, sizeof(PingPong6Packet), 0,
					(struct sockaddr*) &rcvaddr, &addrlen);
			if (size == -1)
			{
				continue;
			};
			
			clock_t endTime = clock();
			
			if (checksum(&pong, sizeof(PseudoHeader)) != 0)
			{
				// bad checksum
				continue;
			};
			
			if ((pong.payload.type == 129) && (pong.payload.code == 0))
			{
				if ((pong.payload.id == ping.payload.id) && (pong.payload.seq == ping.payload.seq))
				{
					char buffer[INET6_ADDRSTRLEN];
					inet_ntop(AF_INET6, &rcvaddr.sin6_addr, buffer, INET6_ADDRSTRLEN);
					fprintf(stderr, "Reply from %s: id=%hu, seq=%hu, time=%u ns\n",
						buffer, pong.payload.id, pong.payload.seq, endTime-sendtime);
					break;
				};
			};
		};
	};

	close(sockfd);	
	return 0;
};

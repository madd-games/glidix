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
	fprintf(stderr, "USAGE:\t%s hostname\n", progName);
	fprintf(stderr, "\tTest whether the specified IPv4 host is reachable.\n");
	fprintf(stderr, "\tFor IPv6, use `ping6'.\n");
};

const char *getUnreachMsg(uint8_t code)
{
	switch (code)
	{
	case 0:
		return "Network unreacheable";
	case 1:
		return "Host unreacheable";
	case 2:
		return "Protocol unreacheable";
	case 3:
		return "Port unreacheable";
	case 4:
		return "Packet too big";
	case 5:
		return "Source route failed";
	default:
		return "Unknown error";
	};
};

const char *getTimexMsg(uint8_t code)
{
	switch (code)
	{
	case 0:
		return "Hop limit exceeded";
	case 1:
		return "Fragment reassembly time exceeded";
	default:
		return "Unknown error";
	};
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
	hints.ai_family = AF_INET;
	
	struct addrinfo *addrs;
	int status = getaddrinfo(argv[1], NULL, &hints, &addrs);
	if (status != 0)
	{
		fprintf(stderr, "%s: getaddrinfo %s: %s\n", argv[0], argv[1], gai_strerror(status));
		return 1;
	};
	
	int sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
	if (sockfd == -1)
	{
		fprintf(stderr, "%s: socket: %s\n", argv[0], strerror(errno));
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
	
	PingPongPacket ping, pong;
	ping.type = 8;
	ping.code = 0;
	ping.checksum = 0;
	ping.id = getpid();
	ping.seq = 0;
	
	char destaddrstr[INET_ADDRSTRLEN];
	inet_ntop(AF_INET, &((struct sockaddr_in*)(addrs->ai_addr))->sin_addr, destaddrstr, INET_ADDRSTRLEN);
	
	printf("PING %s (%s)\n", argv[1], destaddrstr);
	int i;
	for (i=0;; i++)
	{
		sleep(1);			// don't hog the CPU and network too much
		
		struct sockaddr_in src;
		
		ping.checksum = 0;
		ping.seq = i;
		ping.checksum = checksum(&ping, sizeof(PingPongPacket));
		
		clock_t sendTime = clock();
		if (sendto(sockfd, &ping, sizeof(PingPongPacket), 0, addrs->ai_addr, sizeof(struct sockaddr_in)) == -1)
		{
			fprintf(stderr, "%s: sendto: %s\n", argv[0], strerror(errno));
			continue;
		};
		
		while (1)
		{
			socklen_t addrlen = sizeof(struct sockaddr_in);
			ssize_t count = recvfrom(sockfd, &pong, sizeof(PingPongPacket), 0, (struct sockaddr*) &src, &addrlen);
		
			if (count == -1)
			{
				fprintf(stderr, "%s: recvfrom: %s\n", argv[0], strerror(errno));
				break;
			};
		
			clock_t endtime = clock();
			char buffer[INET_ADDRSTRLEN];
			inet_ntop(AF_INET, &src.sin_addr, buffer, INET_ADDRSTRLEN);
		
			if (checksum(&pong, sizeof(PingPongPacket)) != 0)
			{
				// invalid checksum
				fprintf(stderr, "Reply from %s: invalid checksum\n", buffer);
				break;
			};
		
			if (pong.type != 0)
			{
				// that's not a PONG!
				if (pong.type == 8)
				{
					// ping-back; ignore
					continue;
				}
				else if (pong.type == 3)
				{
					fprintf(stderr, "Reply from %s: Destination unreachable: %s\n", buffer, getUnreachMsg(pong.code));
				}
				else if (pong.type == 11)
				{
					fprintf(stderr, "Reply from %s: Time exceeded: %s\n", buffer, getTimexMsg(pong.code));
				}
				else
				{
					fprintf(stderr, "Reply from %s: Unknown response\n", buffer);
				};
			
				break;
			};
		
			fprintf(stderr, "Reply from %s: id=%d, seq=%d, time=%u ns\n", buffer, pong.id, pong.seq, endtime-sendTime);
			break;
		};
	};
	
	return 0;
};

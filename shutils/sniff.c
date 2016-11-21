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

#include <sys/glidix.h>
#include <sys/socket.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

unsigned char buffer[32*1024];

int main(int argc, char *argv[])
{
	const char *ifname = NULL;
	int socktype = SOCK_DGRAM;
	int protocol = 0;
	const char *outfile = NULL;
	
	if (((argc & 1) == 0) || (argc < 2))
	{
		fprintf(stderr, "USAGE:\t%s -i <interface> [-o <filename>] [-p protocol]\n", argv[0]);
		fprintf(stderr, "\tCapture packets on an interface and write to a file.\n");
		fprintf(stderr, "\tProtocols:\n");
		fprintf(stderr, "\t\tip -- IPv4/IPv6 packets\n");
		fprintf(stderr, "\t\tether -- Ethernet frames\n");
		return 1;
	};
	
	int i;
	for (i=1; i<argc; i+=2)
	{
		if (strcmp(argv[i], "-i") == 0)
		{
			if (ifname != NULL)
			{
				fprintf(stderr, "%s: interface (-i) specified more than once\n", argv[0]);
				return 1;
			};
			
			ifname = argv[i+1];
		}
		else if (strcmp(argv[i], "-o") == 0)
		{
			if (outfile != NULL)
			{
				fprintf(stderr, "%s: output file (-o) specified more than once\n", argv[0]);
				return 1;
			};
			
			outfile = argv[i+1];
		}
		else if (strcmp(argv[i], "-p") == 0)
		{
			const char *proto = argv[i+1];
			if (strcmp(proto, "ip") == 0)
			{
				socktype = SOCK_DGRAM;
				protocol = 0;
			}
			else if (strcmp(proto, "ether") == 0)
			{
				socktype = SOCK_RAW;
				protocol = IF_ETHERNET;
			}
			else
			{
				fprintf(stderr, "%s: invalid protocol: %s\n", argv[0], proto);
				return 1;
			};
		}
		else
		{
			fprintf(stderr, "%s: invalid command-line option: %s\n", argv[0], argv[i]);
			return 1;
		};
	};
	
	if (ifname == NULL)
	{
		fprintf(stderr, "%s: no interface specified\n", argv[0]);
		return 1;
	};
	
	if (strlen(ifname) > 15)
	{
		fprintf(stderr, "%s: invalid interface name: %s\n", argv[0], ifname);
		return 1;
	};
	
	if (outfile == NULL)
	{
		char *buf = (char*) malloc(strlen(ifname) + 5);
		strcpy(buf, ifname);
		strcat(buf, ".cap");
		outfile = buf;
	};
	
	int sockfd = socket(AF_CAPTURE, socktype, protocol);
	if (sockfd == -1)
	{
		fprintf(stderr, "%s: cannot create capture socket: %s\n", argv[0], strerror(errno));
		return 1;
	};
	
	struct sockaddr_cap addr;
	memset(&addr, 0, sizeof(struct sockaddr_cap));
	addr.scap_family = AF_CAPTURE;
	strcpy(addr.scap_ifname, ifname);
	
	if (bind(sockfd, (struct sockaddr*)&addr, sizeof(struct sockaddr_cap)) != 0)
	{
		fprintf(stderr, "%s: cannot bind to %s: %s\n", argv[0], ifname, strerror(errno));
		return 1;
	};
	
	FILE *fp = fopen(outfile, "w");
	if (fp == NULL)
	{
		fprintf(stderr, "%s: cannot open output file %s: %s\n", argv[0], outfile, strerror(errno));
		return 1;
	};
	
	fprintf(stderr, "Capturing on %s to %s\n", ifname, outfile);
	int numPackets = 0;
	
	while (1)
	{
		fprintf(stderr, "\rPackets captured: %d             ", numPackets);
		fflush(stderr);
		
		ssize_t size = read(sockfd, buffer, 32*1024);
		if (size == -1)
		{
			fprintf(stderr, "\rread: %s\n", strerror(errno));
			continue;
		};
		
		numPackets++;
		
		off_t offset;
		for (offset=0; offset<size; offset+=16)
		{
			fprintf(fp, "%06lx ", offset);
			
			off_t byteOff;
			for (byteOff=offset; byteOff<(offset+16); byteOff++)
			{
				if (byteOff < size)
				{
					fprintf(fp, "%02hhx ", buffer[byteOff]);
				}
				else
				{
					fprintf(fp, "   ");
				};
			};
			
			char ascii[17];
			ascii[16] = 0;
			memset(ascii, '.', 16);
			
			size_t canWrite = (size_t) (size - offset);
			if (canWrite > 16) canWrite = 16;
			memcpy(ascii, &buffer[offset], canWrite);
			ascii[canWrite] = 0;
			
			int i;
			for (i=0; i<canWrite; i++)
			{
				char c = ascii[i];
				if ((c < 32) || (c >= 127))
				{
					ascii[i] = '.';
				};
			};
			
			fprintf(fp, "%s\n", ascii);
		};
		
		fflush(fp);
	};
	
	close(sockfd);
	fclose(fp);
	return 0;
};

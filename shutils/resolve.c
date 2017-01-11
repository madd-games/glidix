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

#include <stdio.h>
#include <netdb.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>

const char *progName;

void usage()
{
	fprintf(stderr, "USAGE:\t%s <nodename> <service> [-f <family>] [-p <protocol>] [-t <type>]\n", progName);
	fprintf(stderr, "\tPrint a list of addresses that match a node name.\n");
	fprintf(stderr, "\tfamily - either 4 or 6\n");
	fprintf(stderr, "\tprotocol - a protocol number (0 default)\n");
	fprintf(stderr, "\ttype - STREAM or DGRAM (default both)\n");
};

int main(int argc, char *argv[])
{
	progName = argv[0];
	
	if (argc < 2)
	{
		usage();
		return 1;
	};
	
	struct addrinfo hints;
	memset(&hints, 0, sizeof(struct addrinfo));
	
	const char *nodename = NULL;
	const char *servname = NULL;
	
	int i;
	for (i=1; i<argc; i++)
	{
		if (strcmp(argv[i], "-f4") == 0)
		{
			hints.ai_family = AF_INET;
		}
		else if (strcmp(argv[i], "-f6") == 0)
		{
			hints.ai_family = AF_INET6;
		}
		else if (strcmp(argv[i], "-f") == 0)
		{
			i++;
			if (i == argc)
			{
				usage();
				return 1;
			};
			
			if (strcmp(argv[i], "4") == 0)
			{
				hints.ai_family = AF_INET;
			}
			else if (strcmp(argv[i], "6") == 0)
			{
				hints.ai_family = AF_INET6;
			}
			else
			{
				usage();
				return 1;
			};
		}
		else if (memcmp(argv[i], "-p", 2) == 0)
		{
			const char *protostr = NULL;
			if (strcmp(argv[i], "-p") == 0)
			{
				i++;
				if (i == argc)
				{
					usage();
					return 1;
				};
				
				protostr = argv[i];
			}
			else
			{
				protostr = &argv[i][2];
			};
			
			hints.ai_protocol = atoi(protostr);
		}
		else if (memcmp(argv[i], "-t", 2) == 0)
		{
			const char *typestr = NULL;
			if (strcmp(argv[i], "-t") == 0)
			{
				i++;
				if (i == argc)
				{
					usage();
					return 1;
				};
				
				typestr = argv[i];
			}
			else
			{
				typestr = &argv[i][2];
			};
			
			if (strcmp(typestr, "STREAM") == 0)
			{
				hints.ai_socktype = SOCK_STREAM;
			}
			else if (strcmp(typestr, "DGRAM") == 0)
			{
				hints.ai_socktype = SOCK_DGRAM;
			}
			else
			{
				usage();
				return 1;
			};
		}
		else if (nodename == NULL)
		{
			nodename = argv[i];
		}
		else if (servname == NULL)
		{
			servname = argv[i];
		}
		else
		{
			usage();
			return 1;
		};
	};
	
	if ((nodename == NULL) || (servname == NULL))
	{
		usage();
		return 1;
	};
	
	struct addrinfo *result;
	int status = getaddrinfo(nodename, servname, &hints, &result);
	if (status != 0)
	{
		fprintf(stderr, "%s: getaddrinfo %s: %s\n", argv[0], nodename, gai_strerror(status));
		return 1;
	};
	
	printf("Family Type   Proto Address\n");
	struct addrinfo *ai;
	for (ai=result; ai!=NULL; ai=ai->ai_next)
	{
		int family;
		if (ai->ai_family == AF_INET6)
		{
			family = 6;
		}
		else
		{
			family = 4;
		};
		
		const char *typestr;
		if (ai->ai_socktype == SOCK_STREAM)
		{
			typestr = "STREAM";
		}
		else
		{
			typestr = "DGRAM ";
		};
		
		char protospec[5];
		sprintf(protospec, "%d", ai->ai_protocol);
		memset(&protospec[strlen(protospec)], ' ', 5-strlen(protospec));
		protospec[4] = 0;
		
		char addrspec[64];
		if (ai->ai_family == AF_INET)
		{
			struct sockaddr_in *inaddr = (struct sockaddr_in*) ai->ai_addr;
			char addrstr[INET_ADDRSTRLEN];
			sprintf(addrspec, "%s:%d", inet_ntop(AF_INET, &inaddr->sin_addr, addrstr, INET_ADDRSTRLEN), (int) ntohs(inaddr->sin_port));
		}
		else
		{
			struct sockaddr_in6 *inaddr = (struct sockaddr_in6*) ai->ai_addr;
			char addrstr[INET6_ADDRSTRLEN];
			sprintf(addrspec, "[%s]:%d", inet_ntop(AF_INET6, &inaddr->sin6_addr, addrstr, INET6_ADDRSTRLEN), (int) ntohs(inaddr->sin6_port));
		};
		
		printf("IPv%d   %s %s  %s\n", family, typestr, protospec, addrspec);
	};
	
	return 0;
};

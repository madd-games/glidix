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

#include <sys/glidix.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

void ip_to_string(struct in_addr *addr, struct in_addr *mask, char *str)
{
	uint8_t *fields = (uint8_t*) addr;
	sprintf(str, "%d.%d.%d.%d", fields[0], fields[1], fields[2], fields[3]);
	
	if (mask != NULL)
	{
		int bitCount = 0;
		uint8_t *scan = (uint8_t*) mask;
		int i;
		for (i=0; i<4; i++)
		{
			if (scan[i] == 0xFF)
			{
				bitCount += 8;
			}
			else
			{
				uint8_t part = scan[i];
				while (part & 0x80)
				{
					bitCount++;
					part <<= 1;
				};
				break;
			};
		};
		
		char suffix[8];
		sprintf(suffix, "/%d", bitCount);
		strcat(str, suffix);
	};
	
	memset(&str[strlen(str)], ' ', 19-strlen(str));
	str[19] = 0;
};

void ip6_to_string(struct in6_addr *addr, struct in6_addr *mask, char *str)
{
	uint16_t *groups = (uint16_t*) addr;
	sprintf(str, "%x:%x:%x:%x:%x:%x:%x:%x",
		__builtin_bswap16(groups[0]),
		__builtin_bswap16(groups[1]),
		__builtin_bswap16(groups[2]),
		__builtin_bswap16(groups[3]),
		__builtin_bswap16(groups[4]),
		__builtin_bswap16(groups[5]),
		__builtin_bswap16(groups[6]),
		__builtin_bswap16(groups[7])
	);
	
	if (mask != NULL)
	{
		int bitCount = 0;
		uint8_t *scan = (uint8_t*) mask;
		int i;
		for (i=0; i<16; i++)
		{
			if (scan[i] == 0xFF)
			{
				bitCount += 8;
			}
			else
			{
				uint8_t part = scan[i];
				while (part & 0x80)
				{
					bitCount++;
					part <<= 1;
				};
				break;
			};
		};
		
		char suffix[8];
		sprintf(suffix, "/%d", bitCount);
		strcat(str, suffix);
	};
	
	memset(&str[strlen(str)], ' ', 44-strlen(str));
	str[44] = 0;
};

int mainUtil(int argc, char *argv[])
{
	if (strcmp(argv[1], "add") == 0)
	{
		if (argc < 4)
		{
			fprintf(stderr, "USAGE:\t%s add subnet interface [-g gateway] [-i index]\n", argv[0]);
			fprintf(stderr, "\tAdds an entry for the specified subnet to the routing table. If the network size\n");
			fprintf(stderr, "\tis not given, adds just the specified address. You can also optionally specify a\n");
			fprintf(stderr, "\tgateway, as well as the position within the interface's routing table.\n");
			return 1;
		};
		
		char subnet[19];
		char gateway[16] = "0.0.0.0";
		char ifname[16];
		int pos = -1;
		
		if (strlen(argv[2]) > 18)
		{
			fprintf(stderr, "%s: invalid address or subnet: %s\n", argv[0], argv[2]);
			return 1;
		};
		
		if (strlen(argv[3]) > 15)
		{
			fprintf(stderr, "%s: invalid interface name: %s\n", argv[0], argv[3]);
			return 1;
		};
		
		strcpy(subnet, argv[2]);
		strcpy(ifname, argv[3]);
		
		int i;
		for (i=4; i<argc; i++)
		{
			if (strcmp(argv[i], "-g") == 0)
			{
				i++;
				if (strlen(argv[i]) > 15)
				{
					fprintf(stderr, "%s: invalid gateway: %s\n", argv[0], argv[i]);
					return 1;
				};
				strcpy(gateway, argv[i]);
			}
			else if (strcmp(argv[i], "-i") == 0)
			{
				i++;
				pos = atoi(argv[i]);
			}
			else
			{
				fprintf(stderr, "%s: unrecognised command-line option: %s\n", argv[0], argv[i]);
				return 1;
			};
		};
		
		_glidix_gen_route genroute;
		_glidix_in_route *inroute = (_glidix_in_route*) &genroute;
		strcpy(inroute->ifname, ifname);
		memset(&inroute->mask, 0xFF, 4);
		
		char *slashpos = strchr(subnet, '/');
		if (slashpos != NULL)
		{
			*slashpos = 0;
			int netsize = atoi(slashpos+1);
			
			if ((netsize < 0) || (netsize > 32))
			{
				fprintf(stderr, "%s: invalid subnet size: /%d\n", argv[0], netsize);
				return 1;
			};
			
			uint8_t *out = (uint8_t*) &inroute->mask;
			memset(out, 0, 4);
			
			while (netsize >= 8)
			{
				*out++ = 0xFF;
				netsize -= 8;
			};
			
			if (netsize > 0)
			{
				// number of bits at the end that should be 0
				int zeroBits = 8 - netsize;
				
				// make a number which has this many '1's at the end, and then NOT it
				*out = ~((1 << zeroBits)-1);
			};
		};
		
		if (inet_pton(AF_INET, subnet, &inroute->dest) != 1)
		{
			fprintf(stderr, "%s: failed to parse address %s: %s\n", argv[0], subnet, strerror(errno));
			return 1;
		};
		
		if (inet_pton(AF_INET, gateway, &inroute->gateway) != 1)
		{
			fprintf(stderr, "%s: failed to parse address %s: %s\n", argv[0], gateway, strerror(errno));
			return 1;
		};
		
		inroute->flags = 0;
		if (_glidix_route_add(AF_INET, pos, &genroute) != 0)
		{
			fprintf(stderr, "%s: failed to add the route: %s\n", argv[0], strerror(errno));
			return 1;
		};
	};
};

int main(int argc, char *argv[])
{
	if (argc > 1)
	{
		return mainUtil(argc, argv);
	};
	
	printf("Kernel IPv4 routing table:\n");
	printf("Network            Gateway            Interface\n");
	int fd = _glidix_routetab(AF_INET);
	
	_glidix_in_route route;
	while (1)
	{
		ssize_t count = read(fd, &route, sizeof(_glidix_in_route));
		if (count < sizeof(_glidix_in_route))
		{
			break;
		};
		
		char network[20];
		char gate[17];
		
		ip_to_string(&route.dest, &route.mask, network);
		ip_to_string(&route.gateway, NULL, gate);
		
		printf("%s%s%s\n", network, gate, route.ifname);
	};
	
	close(fd);
	
	printf("\nKernel IPv6 routing table:\n");
	fd = _glidix_routetab(AF_INET6);
	
	_glidix_in6_route route6;
	while (1)
	{
		ssize_t count = read(fd, &route6, sizeof(_glidix_in6_route));
		if (count < sizeof(_glidix_in6_route))
		{
			break;
		};
		
		char network[44];
		char gateway[44];
		
		ip6_to_string(&route6.dest, &route6.mask, network);
		ip6_to_string(&route6.gateway, NULL, gateway);
		
		printf("%s%s%s\n", network, gateway, route6.ifname);
	};
	
	close(fd);
	
	return 0;
};

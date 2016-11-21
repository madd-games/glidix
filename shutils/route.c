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

#include <sys/glidix.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <arpa/inet.h>

void getdomname(char *domname, int domain)
{
	if (domain == _GLIDIX_DOM_GLOBAL)
	{
		strcpy(domname, "Global");
	}
	else if (domain == _GLIDIX_DOM_LINK)
	{
		strcpy(domname, "Link-local");
	}
	else if (domain == _GLIDIX_DOM_LOOPBACK)
	{
		strcpy(domname, "Loopback");
	}
	else if (domain == _GLIDIX_DOM_SITE)
	{
		strcpy(domname, "Site-local");
	}
	else if (domain == _GLIDIX_DOM_MULTICAST)
	{
		strcpy(domname, "Multicast");
	}
	else if (domain == _GLIDIX_DOM_NODEFAULT)
	{
		strcpy(domname, "Non-default");
	}
	else
	{
		sprintf(domname, "Domain %d", domain);
	};
};

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
	char buf[INET6_ADDRSTRLEN];
	inet_ntop(AF_INET6, addr, buf, INET6_ADDRSTRLEN);
	
	if (strlen(buf) > 27)
	{
		memcpy(str, buf, 26);
		strcpy(&str[26], "...");
	}
	else
	{
		strcpy(str, buf);
	};
	
	if (mask != NULL)
	{
		//printf("MASK: %s\n", inet_ntop(AF_INET6, mask, buf, INET6_ADDRSTRLEN));
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
	
	memset(&str[strlen(str)], ' ', 34-strlen(str));
	str[34] = 0;
};

int mainUtil(int argc, char *argv[])
{
	if (strcmp(argv[1], "add") == 0)
	{
		if (argc < 4)
		{
			fprintf(stderr, "USAGE:\t%s add subnet interface [-g gateway] [-i index] [-6]\n", argv[0]);
			fprintf(stderr, "\tAdds an entry for the specified subnet to the routing table. If the\n");
			fprintf(stderr, "\tnetwork size is not given, adds just the specified address. You can\n");
			fprintf(stderr, "\talso optionally specify a gateway, as well as the position within the\n");
			fprintf(stderr, "\tinterface's routing table. The '-6' indicates that the route is IPv6.\n");
			return 1;
		};
		
		char subnet[INET6_ADDRSTRLEN+4];
		char gateway[INET6_ADDRSTRLEN] = "";
		char ifname[16];
		int pos = -1;
		int family = AF_INET;
		
		if (strlen(argv[2]) > (INET6_ADDRSTRLEN+4))
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
			else if (strcmp(argv[i], "-6") == 0)
			{
				family = AF_INET6;
			}
			else
			{
				fprintf(stderr, "%s: unrecognised command-line option: %s\n", argv[0], argv[i]);
				return 1;
			};
		};
		
		if (gateway[0] == 0)
		{
			if (family == AF_INET)
			{
				strcpy(gateway, "0.0.0.0");
			}
			else
			{
				strcpy(gateway, "::");
			};
		};
		
		_glidix_gen_route genroute;
		memset(&genroute, 0, sizeof(_glidix_gen_route));
		_glidix_in_route *inroute = (_glidix_in_route*) &genroute;
		_glidix_in6_route *in6route = (_glidix_in6_route*) &genroute;
		if (family == AF_INET) strcpy(inroute->ifname, ifname);
		else strcpy(in6route->ifname, ifname);
		
		if (family == AF_INET) memset(&inroute->mask, 0xFF, 4);
		else memset(&in6route->mask, 0xFF, 16);
		
		char *slashpos = strchr(subnet, '/');
		if (slashpos != NULL)
		{
			*slashpos = 0;
			int netsize = atoi(slashpos+1);
			
			int maxNetSize = 32;
			if (family == AF_INET6) maxNetSize = 128;
			if ((netsize < 0) || (netsize > maxNetSize))
			{
				fprintf(stderr, "%s: invalid subnet size: /%d\n", argv[0], netsize);
				return 1;
			};
			
			uint8_t *out;
			if (family == AF_INET) out = (uint8_t*) &inroute->mask;
			else out = (uint8_t*) &in6route->mask;
			
			if (family == AF_INET) memset(out, 0, 4);
			else memset(out, 0, 16);
			
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
		
		if (family == AF_INET)
		{
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
		}
		else
		{
			if (inet_pton(AF_INET6, subnet, &in6route->dest) != 1)
			{
				fprintf(stderr, "%s: failed to parse address %s: %s\n", argv[0], subnet, strerror(errno));
				return 1;
			};
		
			if (inet_pton(AF_INET6, gateway, &in6route->gateway) != 1)
			{
				fprintf(stderr, "%s: failed to parse address %s: %s\n", argv[0], gateway, strerror(errno));
				return 1;
			};
		};
		
		if (family == AF_INET) inroute->flags = 0;
		else in6route->flags = 0;
		
		if (_glidix_route_add(family, pos, &genroute) != 0)
		{
			fprintf(stderr, "%s: failed to add the route: %s\n", argv[0], strerror(errno));
			return 1;
		};
		
		return 0;
	}
	else if (strcmp(argv[1], "clear") == 0)
	{
		if (argc != 3)
		{
			fprintf(stderr, "USAGE:\t%s clear <interface>\n", argv[0]);
			fprintf(stderr, "\tDelete all IPv4 routes of an interface.\n");
			fprintf(stderr, "\tSee also: clear6\n");
			return 1;
		};
		
		if (_glidix_route_clear(AF_INET, argv[2]) != 0)
		{
			fprintf(stderr, "%s: cannot clear %s: %s\n", argv[0], argv[2], strerror(errno));
			return 1;
		};
		
		return 0;
	}
	else if (strcmp(argv[1], "clear6") == 0)
	{
		if (argc != 3)
		{
			fprintf(stderr, "USAGE:\t%s clear6 <interface>\n", argv[0]);
			fprintf(stderr, "\tDelete all IPv6 routes of an interface.\n");
			fprintf(stderr, "\tSee also: clear\n");
			return 1;
		};
		
		if (_glidix_route_clear(AF_INET6, argv[2]) != 0)
		{
			fprintf(stderr, "%s: cannot clear6 %s: %s\n", argv[0], argv[2], strerror(errno));
			return 1;
		};
		
		return 0;
	}
	else
	{
		fprintf(stderr, "%s: unknown command: %s\n", argv[0], argv[1]);
		return 1;
	};
};

int main(int argc, char *argv[])
{
	if (argc > 1)
	{
		return mainUtil(argc, argv);
	};
	
	char domname[256];
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
		
		getdomname(domname, route.domain);
		printf("%s%s%s (%s)\n", network, gate, route.ifname, domname);
	};
	
	close(fd);
	
	printf("\nKernel IPv6 routing table:\n");
	printf("Network                           Next hop                          Interface\n");
	fd = _glidix_routetab(AF_INET6);
	
	_glidix_in6_route route6;
	while (1)
	{
		ssize_t count = read(fd, &route6, sizeof(_glidix_in6_route));
		if (count < sizeof(_glidix_in6_route))
		{
			break;
		};
		
		char network[35];
		char gateway[35];
		
		ip6_to_string(&route6.dest, &route6.mask, network);
		ip6_to_string(&route6.gateway, NULL, gateway);
		
		getdomname(domname, route6.domain);
		printf("%s%s%s (%s)\n", network, gateway, route6.ifname, domname);
	};
	
	close(fd);
	
	return 0;
};

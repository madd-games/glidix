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
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

typedef struct
{
	struct in_addr			addr;
	struct in_addr			mask;
	int				domain;
} IPNetIfAddr4;

typedef struct
{
	struct in6_addr			addr;
	struct in6_addr			mask;
	int				domain;
} IPNetIfAddr6;

char *progName;

void usage()
{
	fprintf(stderr, "USAGE:\t%s <ifname>\n", progName);
	fprintf(stderr, "\tConfigure network interfaces.\n");
	exit(1);
};

const char *linkname(int type)
{
	switch (type)
	{
	case 0:				// IF_LOOPBACK
		return "Loopback";
	case 1:				// IF_ETHERNET
		return "Ethernet";
	case 2:				// IF_TUNNEL
		return "Software Tunnel";
	default:
		return "?";
	};
};

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

void printIntfInfo(const char *ifname)
{
	_glidix_netstat netstat;
	if (_glidix_netconf_stat(ifname, &netstat, sizeof(_glidix_netstat)) == -1)
	{
		fprintf(stderr, "%s: %s: %s\n", progName, ifname, strerror(errno));
		exit(1);
	};
	
	_glidix_ifaddr4 *addrs = (_glidix_ifaddr4*) malloc(sizeof(_glidix_ifaddr4) * netstat.numAddrs4);
	_glidix_netconf_getaddrs(ifname, AF_INET, addrs, sizeof(_glidix_ifaddr4) * netstat.numAddrs4);
	_glidix_ifaddr6 *addrs6 = (_glidix_ifaddr6*) malloc(sizeof(_glidix_ifaddr6) * netstat.numAddrs6);
	_glidix_netconf_getaddrs(ifname, AF_INET6, addrs6, sizeof(_glidix_ifaddr6) * netstat.numAddrs6);
	
	printf("Interface: %s\n", netstat.ifname);
	printf("\tPackets sent: %d\n", netstat.numTrans);
	printf("\tPackets received: %d\n", netstat.numRecv);
	printf("\tPacket errors: %d\n", netstat.numErrors);
	printf("\tPackets dropped: %d\n", netstat.numDropped);
	printf("\tScope ID: %u\n", netstat.scopeID);
	printf("\tLink: %s\n", linkname(netstat.ifconfig.type));
	if (netstat.ifconfig.type == 1)
	{
		// IF_ETHERNET
		printf("\t\tMAC: %02x:%02x:%02x:%02x:%02x:%02x\n", netstat.ifconfig.ethernet.mac[0], netstat.ifconfig.ethernet.mac[1],
			netstat.ifconfig.ethernet.mac[2], netstat.ifconfig.ethernet.mac[3], netstat.ifconfig.ethernet.mac[4],
			netstat.ifconfig.ethernet.mac[5]
		);
	};
	
	printf("\tAddresses:\n");
	char netaddr[INET6_ADDRSTRLEN];
	int netsize;
	int i;
	for (i=0; i<netstat.numAddrs4; i++)
	{
		inet_ntop(AF_INET, &addrs[i].addr, netaddr, INET_ADDRSTRLEN);
		netsize = 0;
		uint8_t *bytes = (uint8_t*) &addrs[i].mask;
		int j;
		netsize = 0;
		for (j=0; j<4; j++)
		{
			if (bytes[j] == 0xFF)
			{
				netsize += 8;
			}
			else
			{
				uint8_t byte = bytes[j];
				while (byte & 0x80)
				{
					netsize++;
					byte <<= 1;
				};
				break;
			};
		};
		
		char domname[256];
		getdomname(domname, addrs[i].domain);
		printf("\t\t%s/%d (%s)\n", netaddr, netsize, domname);
	};

	for (i=0; i<netstat.numAddrs6; i++)
	{
		inet_ntop(AF_INET6, &addrs6[i].addr, netaddr, INET6_ADDRSTRLEN);
		netsize = 0;
		uint8_t *bytes = (uint8_t*) &addrs6[i].mask;
		int j;
		netsize = 0;
		for (j=0; j<16; j++)
		{
			if (bytes[j] == 0xFF)
			{
				netsize += 8;
			}
			else
			{
				uint8_t byte = bytes[j];
				while (byte & 0x80)
				{
					netsize++;
					byte <<= 1;
				};
				break;
			};
		};
		
		char domname[256];
		getdomname(domname, addrs6[i].domain);
		printf("\t\t%s/%d (%s)\n", netaddr, netsize, domname);
	};
	
	printf("\n");
};

void parseAddr(int family, char *addrname, void *addrout, void *maskout)
{
	char *slashpos = strchr(addrname, '/');
	if (slashpos == NULL)
	{
		fprintf(stderr, "%s: %s is not a valid CIDR address\n", progName, addrname);
		exit(1);
	};
	
	*slashpos = 0;
	int netsize = atoi(slashpos+1);
	
	if (netsize < 0)
	{
		fprintf(stderr, "%s: invalid subnet size: /%d\n", progName, netsize);
		exit(1);
	};
	
	if ((family == AF_INET) && (netsize > 32))
	{
		fprintf(stderr, "%s: invalid subnet size for IPv4: /%d\n", progName, netsize);
		exit(1);
	};
	
	if ((family == AF_INET6) && (netsize > 128))
	{
		fprintf(stderr, "%s: invalid subnet size for IPv6: /%d\n", progName, netsize);
		exit(1);
	};
	
	if (!inet_pton(family, addrname, addrout))
	{
		fprintf(stderr, "%s: invalid address: %s\n", progName, addrname);
		exit(1);
	};
	
	uint8_t *out = (uint8_t*) maskout;
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

void doAddrConf(int family, const char *ifname, char **addrnames, int count)
{
	size_t addrsize = sizeof(IPNetIfAddr4);
	if (family == AF_INET6)
	{
		addrsize = sizeof(IPNetIfAddr6);
	};
	
	size_t maskoff = 4;
	if (family == AF_INET6)
	{
		maskoff = 16;
	};
	
	void *buffer = malloc(addrsize * count);
	memset(buffer, 0, addrsize * count);
	
	int i;
	for (i=0; i<count; i++)
	{
		size_t offset = addrsize * i;
		size_t offMask = offset + maskoff;
		
		parseAddr(family, addrnames[i], (char*) buffer + offset, (char*) buffer + offMask);
	};
	
	if (_glidix_netconf_addr(family, ifname, buffer, addrsize * count) != 0)
	{
		fprintf(stderr, "%s: cannot configure %s: %s\n", progName, ifname, strerror(errno));
		exit(1);
	};
};

int main(int argc, char *argv[])
{
	progName = argv[0];
	
	if (argc == 1)
	{
		_glidix_netstat netstat;
		unsigned int i;
		for (i=0;;i++)
		{
			if (_glidix_netconf_statidx(i, &netstat, sizeof(_glidix_netstat)) != -1)
			{
				printIntfInfo(netstat.ifname);
			}
			else
			{
				if (errno == ENOENT)
				{
					break;
				}
				else
				{
					fprintf(stderr, "%s: %u: %s\n", argv[0], i, strerror(errno));
				};
			};
		};
	}
	else if (argc == 2)
	{
		printIntfInfo(argv[1]);
	}
	else if (strcmp(argv[2], "addr") == 0)
	{
		doAddrConf(AF_INET, argv[1], &argv[3], argc-3);
	}
	else if (strcmp(argv[2], "addr6") == 0)
	{
		doAddrConf(AF_INET6, argv[1], &argv[3], argc-3);
	}
	else
	{
		usage();
		return 1;
	};
	
	return 0;
};

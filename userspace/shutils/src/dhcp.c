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
#error This program can only run on Glidix!
#endif

#define	VERBOSE			if (verbose) printf

#include <sys/glidix.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#define	GSO_RCVTIMEO			0
#define	GSO_SNDTIMEO			1
#define	GSO_SNDFLAGS			2
#define	GSO_COUNT			3

#define	PKT_HDRINC			(1 << 8)
#define	PKT_DONTROUTE			(1 << 9)
#define	PKT_DONTFRAG			(1 << 10)

char *progName;
int verbose = 0;

typedef struct
{
	struct in_addr			addr;
	struct in_addr			mask;
} IPNetIfAddr4;

typedef struct
{
	uint8_t				op;
	uint8_t				htype;
	uint8_t				hlen;
	uint8_t				hops;
	uint32_t			xid;
	uint16_t			secs;
	uint16_t			flags;
	uint32_t			ciaddr;
	uint32_t			yiaddr;
	uint32_t			siaddr;
	uint32_t			giaddr;
	uint8_t				chaddr[16];
	uint8_t				sname[64];
	char				file[128];
	char				magic[4];
	char				options[];
} __attribute__ ((packed)) DHCPHeader;

typedef struct
{
	DHCPHeader			header;
	uint8_t				opt53;			// set to 53
	uint8_t				opt53_len1;		// set to 1
	uint8_t				optDiscover;		// set to 1
	uint8_t				opt55;			// set to 55
	uint8_t				opt55_len2;		// set to 2
	uint8_t				optReqMask;		// set to 1 (request subnet mask)
	uint8_t				optReqRoute;		// set to 3 (request router)
	uint8_t				optend;			// set ot 255 (end of options)
} __attribute__ ((packed)) DHCPDiscover;

typedef struct
{
	DHCPHeader			header;
	uint8_t				opt53;			// set to 53
	uint8_t				opt53_len1;		// set to 1
	uint8_t				optRequest;		// set to 3
	uint8_t				opt50;			// set to 50
	uint8_t				opt50_len4;		// set to 4
	uint8_t				myip[4];		// the IP that was offered to me
	uint8_t				opt54;			// set to 54
	uint8_t				opt54_len4;		// set to 4
	uint8_t				server[4];		// DHCP server IP
	uint8_t				optend;			// set to 255
} __attribute__ ((packed)) DHCPRequest;

char buffer[4096];

char myip[4];
char addrmask[4];
char router[4];
char server[4];

void usage()
{
	fprintf(stderr, "USAGE:\t%s <interface> [-v]\n", progName);
	fprintf(stderr, "\tConfigure a network interface using DHCP. Use -v for verbose output.\n");
};

void parseOptions(uint8_t *scan)
{
	while (*scan != 255)
	{
		switch (*scan++)
		{
		case 0:
			break;
		case 1:
			memcpy(addrmask, scan+1, 4);
			scan += (*scan) + 1;
			break;
		case 3:
			memcpy(router, scan+1, 4);
			scan += (*scan) + 1;
			break;
		case 54:
			memcpy(server, scan+1, 4);
			scan += (*scan) + 1;
			break;
		default:
			scan += (*scan) + 1;
			break;
		};
	};
};

int main(int argc, char *argv[])
{
	progName = argv[0];
	
	if (argc < 2)
	{
		usage();
		return 1;
	};
	
	int i;
	const char *ifname = NULL;
	for (i=1; i<argc; i++)
	{
		if (argv[i][0] != '-')
		{
			if (ifname == NULL)
			{
				ifname = argv[i];
			}
			else
			{
				usage();
				return 1;
			};
		}
		else if (strcmp(argv[i], "-v") == 0)
		{
			verbose = 1;
		}
		else
		{
			usage();
			return 1;
		};
	};
	
	if (ifname == NULL)
	{
		usage();
		return 1;
	};
	
	VERBOSE("Determining MAC address of %s...\n", ifname);
	unsigned char mac[6];
	_glidix_netstat netstat;
	if (_glidix_netconf_stat(ifname, &netstat, sizeof(_glidix_netstat)) == -1)
	{
		fprintf(stderr, "%s: %s: %s\n", argv[0], ifname, strerror(errno));
		return 1;
	};
	
	if (netstat.ifconfig.type == 1)
	{
		// Ethernet
		memcpy(mac, netstat.ifconfig.ethernet.mac, 6);
	}
	else
	{
		fprintf(stderr, "%s: link of %s not supported by DHCP\n", argv[0], ifname);
		return 1;
	};
	
	VERBOSE("MAC: %02x:%02x:%02x:%02x:%02x:%02x\n", (int)mac[0], (int)mac[1], (int)mac[2], (int)mac[3], (int)mac[4], (int)mac[5]);
	
	VERBOSE("Clearing all routes of %s...\n", ifname);
	if (_glidix_route_clear(AF_INET, ifname) != 0)
	{
		fprintf(stderr, "%s: cannot clear IPv4 routes of %s: %s\n", argv[0], ifname, strerror(errno));
		return 1;
	};
	
	VERBOSE("Clearing addresses of %s...\n", ifname);
	if (_glidix_netconf_addr(AF_INET, ifname, NULL, 0) != 0)
	{
		fprintf(stderr, "%s: cannot clear address of %s: %s\n", argv[0], ifname, strerror(errno));
		return 1;
	};
	
	VERBOSE("Opening a socket on UDP port 68 on %s...\n", ifname);
	int sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (sockfd == -1)
	{
		fprintf(stderr, "%s: socket: %s\n", argv[0], strerror(errno));
		return 1;
	};
	
	if (_glidix_bindif(sockfd, ifname) != 0)
	{
		fprintf(stderr, "%s: cannot bind to %s: %s\n", argv[0], ifname, strerror(errno));
		return 1;
	};
	
	// prevent routing
	_glidix_setsockopt(sockfd, SOL_SOCKET, GSO_SNDFLAGS, PKT_DONTROUTE);
	
	struct sockaddr_in laddr;
	laddr.sin_family = AF_INET;
	laddr.sin_port = htons(68);
	memset(&laddr.sin_addr, 0, 4);
	
	if (bind(sockfd, (struct sockaddr*) &laddr, sizeof(struct sockaddr_in)) != 0)
	{
		fprintf(stderr, "%s: cannot bind to 0.0.0.0:68: %s\n", argv[0], ifname, strerror(errno));
		return 1;
	};
	
	VERBOSE("Generating XID...\n");
	uint32_t xid;
	int fd = open("/dev/random", O_RDONLY);
	if (fd == -1)
	{
		VERBOSE("WARNING: Cannot open /dev/random, using uninitialized XID\n");
		xid ^= 0xDEADBEEF;
	}
	else
	{
		read(fd, &xid, 4);
		close(fd);
	};
	
	VERBOSE("Sending DHCPDISCOVER...\n");
	DHCPDiscover disc;
	memset(&disc, 0, sizeof(DHCPDiscover));
	disc.header.op = 1;
	disc.header.htype = 1;
	disc.header.hlen = 6;
	disc.header.hops = 0;
	disc.header.xid = xid;
	memcpy(disc.header.chaddr, mac, 6);
	memcpy(disc.header.magic, "\x63\x82\x53\x63", 4);
	
	// filling in options
	disc.opt53 = 53;
	disc.opt53_len1 = 1;
	disc.optDiscover = 1;
	disc.opt55 = 55;
	disc.opt55_len2 = 2;
	disc.optReqMask = 1;
	disc.optReqRoute = 3;
	disc.optend = 255;
	
	struct sockaddr_in daddr;
	daddr.sin_family = AF_INET;
	daddr.sin_port = htons(67);
	memset(&daddr.sin_addr, 255, 4);
	
	if (sendto(sockfd, &disc, sizeof(DHCPDiscover), 0, (struct sockaddr*) &daddr, sizeof(struct sockaddr_in)) == -1)
	{
		fprintf(stderr, "%s: cannot send DHCPDISCOVER: %s\n", argv[0], strerror(errno));
		return 1;
	};
	
	VERBOSE("Waiting for DHCP offer...\n");
	DHCPHeader *packet = (DHCPHeader*) buffer;
	do
	{
		socklen_t discard = 0;
		recvfrom(sockfd, buffer, 4096, 0, NULL, &discard);
	} while ((packet->op != 2) || (packet->xid != xid));
	
	char namebuf[INET_ADDRSTRLEN];
	parseOptions(packet->options);
	memcpy(myip, &packet->yiaddr, 4);
	
	VERBOSE("Received offer: %s\n", inet_ntop(AF_INET, myip, namebuf, INET_ADDRSTRLEN));
	VERBOSE("DHCP Server: %s\n", inet_ntop(AF_INET, server, namebuf, INET_ADDRSTRLEN));
	VERBOSE("Accepting offer...\n");
	
	IPNetIfAddr4 addr;
	memcpy(&addr.addr, myip, 4);
	memcpy(&addr.mask, addrmask, 4);
	
	if (_glidix_netconf_addr(AF_INET, ifname, &addr, sizeof(IPNetIfAddr4)) != 0)
	{
		fprintf(stderr, "%s: failed to configure %s: %s\n", argv[0], ifname, strerror(errno));
		close(sockfd);
		return 1;
	};
	
	DHCPRequest req;
	memset(&req, 0, sizeof(DHCPRequest));
	req.header.op = 1;
	req.header.htype = 1;
	req.header.hlen = 6;
	req.header.hops = 0;
	req.header.xid = xid;
	req.header.siaddr = packet->siaddr;
	memcpy(req.header.chaddr, mac, 6);
	memcpy(req.header.magic, "\x63\x82\x53\x63", 4);

	// fill in the options
	req.opt53 = 53;
	req.opt53_len1 = 1;
	req.optRequest = 3;
	req.opt50 = 50;
	req.opt50_len4 = 4;
	memcpy(req.myip, myip, 4);
	req.opt54 = 54;
	req.opt54_len4 = 4;
	memcpy(req.server, &packet->siaddr, 4);
	req.optend = 255;
	
	// send it
	if (sendto(sockfd, &req, sizeof(DHCPRequest), 0, (struct sockaddr*) &daddr, sizeof(struct sockaddr_in)) == -1)
	{
		fprintf(stderr, "%s: cannot send DCHPREQUEST: %s\n", argv[0], strerror(errno));
		return 1;
	};
	
	VERBOSE("Waiting for acknowledgment...\n");
	do
	{
		socklen_t discard = 0;
		recvfrom(sockfd, buffer, 4096, 0, NULL, &discard);
	} while ((packet->op != 2) || (packet->xid != xid));
	
	parseOptions(packet->options);
	memcpy(myip, &packet->yiaddr, 4);

	close(sockfd);	
	
	VERBOSE("Detected configuration:\n");
	VERBOSE("\tMy IP address:    %s\n", inet_ntop(AF_INET, myip, namebuf, INET_ADDRSTRLEN));
	VERBOSE("\tSubnet mask:      %s\n", inet_ntop(AF_INET, addrmask, namebuf, INET_ADDRSTRLEN));
	VERBOSE("\tRouter:           %s\n", inet_ntop(AF_INET, router, namebuf, INET_ADDRSTRLEN));
	VERBOSE("Adding routes...\n");
	
	_glidix_gen_route genroute;
	_glidix_in_route *inroute = (_glidix_in_route*) &genroute;
	strcpy(inroute->ifname, ifname);
	uint32_t destnet = (*((uint32_t*)myip)) & (*((uint32_t*)addrmask));
	memcpy(&inroute->dest, &destnet, 4);
	memcpy(&inroute->mask, addrmask, 4);
	memset(&inroute->gateway, 0, 4);
	inroute->flags = 0;
	
	if (_glidix_route_add(AF_INET, -1, &genroute) != 0)
	{
		fprintf(stderr, "%s: cannot add local route: %s\n", argv[0], strerror(errno));
		return 1;
	};
	
	strcpy(inroute->ifname, ifname);
	memset(&inroute->dest, 0, 4);
	memset(&inroute->mask, 0, 4);
	memcpy(&inroute->gateway, router, 4);
	inroute->flags = 0;
	
	if (_glidix_route_add(AF_INET, -1, &genroute) != 0)
	{
		fprintf(stderr, "%s: cannot add internet route: %s\n", argv[0], strerror(errno));
		return 1;
	};
	
	VERBOSE("Configuration complete.\n");
	return 0;
};

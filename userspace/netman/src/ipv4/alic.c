/*
	Glidix Network Manager

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
#include <arpa/inet.h>
#include <gxnetman.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>

typedef struct
{
	struct in_addr			addr;
	struct in_addr			mask;
	int				domain;
} IPNetIfAddr4;

int netman_alic_init(NetmanIfConfig *config)
{
	config->data = NULL;
	return 0;
};

void netman_alic_line(NetmanIfConfig *config, int lineno, char *line)
{
	fprintf(stderr, "alic: if.conf line %d error: ALIC does not accept options\n", lineno);
	config->status = NETMAN_STATUS_ERROR;
};

void netman_alic_ifup(NetmanIfConfig *config)
{
	printf("[alic] attempting to configure using ALIC...\n");

	// delete DNS servers
	char dnspath[256];
	sprintf(dnspath, "/etc/dns/ipv4/%s.if", config->ifname);
	unlink(dnspath);
	
	// we need a socket to test if addresses are reachable
	char zeroes[8];
	memset(zeroes, 0, 8);
	int sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_UDP);
	if (sockfd == -1)
	{
		printf("[alic] cannot open socket: %s\n", strerror(errno));
		return;
	};
	
	if (_glidix_bindif(sockfd, config->ifname) != 0)
	{
		printf("[alic] failed to bind socket to interface %s: %s\n", config->ifname, strerror(errno));
		return;
	};
	
	// get MAC address (or random seed)
	unsigned char mac[6];
	_glidix_netstat netstat;
	if (_glidix_netconf_stat(config->ifname, &netstat, sizeof(_glidix_netstat)) == -1)
	{
		fprintf(stderr, "[alic] cannot stat %s: %s\n", config->ifname, strerror(errno));
		return;
	};
	
	if (netstat.ifconfig.type == 1)
	{
		// Ethernet
		memcpy(mac, netstat.ifconfig.ethernet.mac, 6);
	}
	else
	{
		int fd = open("/dev/random", O_RDONLY);
		if (fd != -1)
		{
			read(fd, mac, 6);
			close(fd);
		};
	};
	
	// first remove all current addresses
	if (_glidix_netconf_addr(AF_INET, config->ifname, NULL, 0) != 0)
	{
		fprintf(stderr, "[alic] cannot clear address of %s: %s\n", config->ifname, strerror(errno));
		return;
	};
	
	// remove all routes
	if (_glidix_route_clear(AF_INET, config->ifname) != 0)
	{
		fprintf(stderr, "[alic] cannot clear IPv4 routes of %s: %s\n", config->ifname, strerror(errno));
		return;
	};
	
	// add route to 169.254.0.0/16
	_glidix_gen_route genroute;
	_glidix_in_route *inroute = (_glidix_in_route*) &genroute;
	strcpy(inroute->ifname, config->ifname);
	char destnet[4] = {169, 254, 0, 0};
	char addrmask[4] = {255, 255, 0, 0};
	memcpy(&inroute->dest, destnet, 4);
	memcpy(&inroute->mask, addrmask, 4);
	memset(&inroute->gateway, 0, 4);
	inroute->domain = _GLIDIX_DOM_LINK;
	inroute->flags = 0;
	
	if (_glidix_route_add(AF_INET, -1, &genroute) != 0)
	{
		fprintf(stderr, "[alic] cannot add local route: %s\n", strerror(errno));
		return;
	};
	
	// random address generation
	static const unsigned __int128 p = 281474976710731UL;
	unsigned __int128 m = (unsigned __int128) mac[0]
		| ((unsigned __int128) mac[1] << 8)
		| ((unsigned __int128) mac[2] << 16)
		| ((unsigned __int128) mac[3] << 24)
		| ((unsigned __int128) mac[4] << 32)
		| ((unsigned __int128) mac[5] << 40);

	IPNetIfAddr4 addr;
	memcpy(&addr.mask, addrmask, 4);
	addr.domain = _GLIDIX_DOM_LINK;
	
	while (1)
	{
		unsigned __int128 r = (m*m)%p;
		uint64_t r64 = (uint64_t) r;
		
		uint16_t a = r64 & 0xFFFF;
		uint16_t b = (r64 >> 16) & 0xFFFF;
		uint16_t c = (r64 >> 32) & 0xFFFF;
		uint16_t d = (r64 >> 48) & 0xFFFF;
		
		uint16_t l = a ^ b ^ c ^ d;
		if (l & 0x8000) l ^= 0xFFFF;
		
		uint8_t laddr[4] = {169, 254, 0, 0};
		laddr[2] = (uint8_t) (l >> 8) + 1;
		laddr[3] = (uint8_t) l;
		
		memcpy(&addr.addr, laddr, 4);
		
		struct sockaddr_in sin;
		memset(&sin, 0, sizeof(struct sockaddr_in));
		sin.sin_family = AF_INET;
		memcpy(&sin.sin_addr, laddr, 4);
		
		printf("[alic] trying address 169.254.%d.%d\n", (int)laddr[2], (int)laddr[3]);
		m = r;			// for retrial
		
		if (sendto(sockfd, zeroes, 8, 0, (struct sockaddr*)&sin, sizeof(struct sockaddr_in)) == -1)
		{
			if (errno == EHOSTUNREACH)
			{
				printf("[alic] no ARP response - success\n");
				break;
			};
		};
	};
	
	if (_glidix_netconf_addr(AF_INET, config->ifname, &addr, sizeof(IPNetIfAddr4)) != 0)
	{
		fprintf(stderr, "[alic] failed to configure %s: %s\n", config->ifname, strerror(errno));
		_glidix_route_clear(AF_INET, config->ifname);
		return;
	};

	while (netman_running()) pause();
	
	printf("[alic] removing ALIC configuration\n");
	_glidix_netconf_addr(AF_INET, config->ifname, NULL, 0);
	_glidix_route_clear(AF_INET, config->ifname);
};

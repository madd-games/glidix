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

int main(int argc, char *argv[])
{
	if (argc != 2)
	{
		fprintf(stderr, "USAGE:\t%s <interface>\n", argv[0]);
		fprintf(stderr, "\tConfigure a network interface with a link-local IPv6\n");
		fprintf(stderr, "\taddress and route to fe80::/64\n");
		return 1;
	};
	
	const char *ifname = argv[1];
	
	// find the MAC address of the interface
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
		fprintf(stderr, "%s: link of %s not supported by linkslaac\n", argv[0], ifname);
		return 1;
	};
	
	mac[0] ^= 2;
	_glidix_ifaddr6 laddr;
	memset(&laddr.addr, 0, 16);
	laddr.addr.s6_addr[0] = 0xFE;
	laddr.addr.s6_addr[1] = 0x80;
	memcpy(&laddr.addr.s6_addr[8], mac, 3);
	laddr.addr.s6_addr[11] = 0xFF;
	laddr.addr.s6_addr[12] = 0xFE;
	memcpy(&laddr.addr.s6_addr[13], &mac[3], 3);
	
	memset(&laddr.mask, 0xFF, 8);
	memset(&laddr.mask.s6_addr[8], 0, 8);
	
	laddr.domain = _GLIDIX_DOM_LINK;
	
	if (_glidix_netconf_addr(AF_INET6, ifname, &laddr, sizeof(_glidix_ifaddr6)) != 0)
	{
		fprintf(stderr, "%s: cannot set IPv6 address of %s: %s\n", argv[0], ifname, strerror(errno));
		return 1;
	};
	
	// clear all routes
	if (_glidix_route_clear(AF_INET6, ifname) != 0)
	{
		fprintf(stderr, "%s: failed to clear routes of %s: %s\n", argv[0], ifname, strerror(errno));
		return 1;
	};
	
	// add a route to fe80::/64
	_glidix_gen_route genroute;
	_glidix_in6_route *inroute = (_glidix_in6_route*) &genroute;
	memset(inroute, 0, sizeof(_glidix_gen_route));
	strcpy(inroute->ifname, ifname);
	inroute->dest.s6_addr[0] = 0xFE;
	inroute->dest.s6_addr[1] = 0x80;
	memset(&inroute->mask, 0xFF, 8);
	inroute->domain = _GLIDIX_DOM_LINK;
	
	if (_glidix_route_add(AF_INET6, -1, &genroute) != 0)
	{
		fprintf(stderr, "%s: failed to add route fe80::/64 to %s: %s\n", argv[0], ifname, strerror(errno));
		return 1;
	};
	
	// add a route for the link-local multicast (ff02::/16) prefix, specifying the link-local
	// domain. this is required for NDP to work; it makes glidix select the link-local address
	// when sending NDP packets to ff02::1
	memset(inroute, 0, sizeof(_glidix_gen_route));
	strcpy(inroute->ifname, ifname);
	inroute->dest.s6_addr[0] = 0xFF;
	inroute->dest.s6_addr[1] = 0x02;
	memset(&inroute->mask, 0xFF, 2);
	inroute->domain = _GLIDIX_DOM_LINK;
	
	if (_glidix_route_add(AF_INET6, -1, &genroute) != 0)
	{
		fprintf(stderr, "%s: failed to add route ff02::/16 to %s: %s\n", argv[0], ifname, strerror(errno));
		return 1;
	};
	
	return 0;
};

/*
	Glidix Network Manager

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

#include <sys/glidix.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <gxnetman.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>

#define	SPACES				" \t"

/**
 * IPv6 State-Less Address Auto-Configuration (SLAAC)
 * TODO: test uniqueness of addresses
 * TODO: make routes expire
 */

#define	NUM_TEMP_ADDR				4

static uint8_t interfaceID[8];
static uint8_t mac[6];

typedef struct
{
	_glidix_ifaddr6				addrFixed;
	_glidix_ifaddr6				addrTemp[NUM_TEMP_ADDR];
} PrefixAddrList;

typedef struct
{
	uint8_t					prefix[8];
	clock_t					lifetimePref;
	clock_t					lifetimeValid;
	clock_t					rotateDeadline;
} PrefixInfo;

typedef struct
{
	_glidix_ifaddr6				addrLink;	// fixed link-local address
	PrefixAddrList				prefixes[];
} AutoAddrInfo;
static AutoAddrInfo *addrInfo = NULL;
static PrefixInfo *prefixInfo = NULL;
size_t numPrefixes = 0;

typedef struct
{
	uint8_t			type;			// 133 = router soliciation
	uint8_t			code;			// 0
	uint16_t		checksum;
	uint32_t		reserved;		// 0
	
	uint8_t			optMac;			// 1 = source link-layer address
	uint8_t			len;			// 1 = 8 bytes
	uint8_t			mac[6];
} RouterSolicit;

typedef struct
{
	uint8_t			src[16];
	uint8_t			dest[16];
	uint32_t		len;
	uint8_t			zeroes[3];
	uint8_t			proto;			// IPPROTO_ICMPV6
	uint8_t			payload[];
} PseudoHeader;

typedef struct
{
	PseudoHeader		pseudo;
	RouterSolicit		sol;
} SolicitEncap;

#define	ADV_MANAGED		(1 << 7)
#define	ADV_OTHER		(1 << 6)
typedef struct
{
	uint8_t			type;			// 134 = router advertisment
	uint8_t			code;			// 0
	uint16_t		checksum;
	uint8_t			hops;
	uint8_t			flags;
	uint16_t		routerLifetime;
	uint32_t		reachableTime;
	uint32_t		retransTime;
} AdvHeader;

#define	PREFIX_ONLINK		(1 << 7)
#define	PREFIX_AUTO		(1 << 6)
typedef struct
{
	uint8_t			preflen;		// prefix length
	uint8_t			flags;
	uint32_t		lifetimeValid;
	uint32_t		lifetimePref;
	uint32_t		reserved;		// 0
	uint8_t			prefix[16];
} __attribute__ ((packed)) AdvPrefixInfo;

typedef struct
{
	PseudoHeader		pseudo;
	uint8_t			payload[1280];
} InboundBuffer;

static uint16_t inet_checksum(void* vdata,size_t length)
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

int netman_slaac_init(NetmanIfConfig *config)
{
	config->data = NULL;
	return 0;
};

void netman_slaac_line(NetmanIfConfig *config, int lineno, char *line)
{
	fprintf(stderr, "slaac: if.conf line %d error: SLAAC does not accept options\n", lineno);
	config->status = NETMAN_STATUS_ERROR;
};

static void doAddrConf(const char *ifname)
{
	size_t addrCount = 1 + numPrefixes + NUM_TEMP_ADDR * numPrefixes;
	if (_glidix_netconf_addr(AF_INET6, ifname, (_glidix_ifaddr6*)addrInfo, sizeof(_glidix_ifaddr6)*addrCount) != 0)
	{
		fprintf(stderr, "[slaac] failed to configure IPv6 addresses of %s: %s\n", ifname, strerror(errno));
	};
};

static int isAddrUnique(struct in6_addr *addr)
{
	// TODO: perform DAD!
	return 1;
};

static int fdRandom;
static void genRandomAddr(uint8_t *prefix, struct in6_addr *addr)
{
	memcpy(addr, prefix, 8);
	do
	{
		do
		{
			read(fdRandom, &addr->s6_addr[8], 8);
			addr->s6_addr[8] &= ~2;			// mark the address "non-universal"
		} while ((addr->s6_addr[11] == 0xFF) && (addr->s6_addr[12] == 0xFE));
	} while (!isAddrUnique(addr));
};

void netman_slaac_ifup(NetmanIfConfig *config)
{
	fprintf(stderr, "[slaac] attempting configuration of %s\n", config->ifname);
	
	fdRandom = open("/dev/random", O_RDONLY);
	if (fdRandom == -1)
	{
		fprintf(stderr, "[slaac] failed to open /dev/random: aborting\n");
		return;
	};
	
	// delete the DNS servers
	char dnspath[256];
	sprintf(dnspath, "/etc/dns/ipv6/%s.if", config->ifname);
	unlink(dnspath);
	
	// remove all routes
	if (_glidix_route_clear(AF_INET6, config->ifname) != 0)
	{
		fprintf(stderr, "[slaac] cannot clear IPv6 routes of %s: %s\n", config->ifname, strerror(errno));
		close(fdRandom);
		return;
	};
	
	// remove all addresses
	if (_glidix_netconf_addr(AF_INET6, config->ifname, NULL, 0) != 0)
	{
		fprintf(stderr, "[slaac] failed to remove IPv6 addresses of %s: %s\n", config->ifname, strerror(errno));
		close(fdRandom);
		return;
	};
	
	_glidix_netstat info;
	if (_glidix_netconf_stat(config->ifname, &info, sizeof(_glidix_netstat)) == -1)
	{
		fprintf(stderr, "[slaac] failed to netconf stat %s: %s\n", config->ifname, strerror(errno));
		close(fdRandom);
		return;
	};

	if (info.ifconfig.type == IF_ETHERNET)
	{
		// interfaceID based on MAC
		memcpy(&interfaceID[0], info.ifconfig.ethernet.mac, 3);
		interfaceID[0] ^= 2;
		interfaceID[3] = 0xFF;
		interfaceID[4] = 0xFE;
		memcpy(&interfaceID[5], &info.ifconfig.ethernet.mac[3], 3);
		memcpy(mac, info.ifconfig.ethernet.mac, 6);
	}
	else
	{
		fprintf(stderr, "[slaac] interface type of %s is not supported\n", config->ifname);
		close(fdRandom);
		return;
	};
	
	// start by adding the link-local route
	_glidix_gen_route route;
	_glidix_in6_route *inroute = (_glidix_in6_route*) &route;
	memset(inroute, 0, sizeof(_glidix_in6_route));
	
	strcpy(inroute->ifname, config->ifname);
	inet_pton(AF_INET6, "fe80::", &inroute->dest);
	memset(&inroute->mask, 0xFF, 8);
	inroute->domain = _GLIDIX_DOM_LINK;
	
	if (_glidix_route_add(AF_INET6, -1, &route) != 0)
	{
		fprintf(stderr, "[slaac] failed to add link-local route: %s\n", strerror(errno));
		close(fdRandom);
		return;
	};
	
	// now the link-local multicast route
	memset(&inroute->mask, 0, 16);
	memset(&inroute->mask, 0xFF, 2);
	inet_pton(AF_INET6, "ff02::", &inroute->dest);
	
	if (_glidix_route_add(AF_INET6, -1, &route) != 0)
	{
		fprintf(stderr, "[slaac] failed to add link-local multicast route: %s\n", strerror(errno));
		_glidix_route_clear(AF_INET6, config->ifname);
		close(fdRandom);
		return;
	};
	
	// prepare the link-local address
	addrInfo = (AutoAddrInfo*) malloc(sizeof(AutoAddrInfo));
	inet_pton(AF_INET6, "fe80::", &addrInfo->addrLink.addr);
	memcpy(&addrInfo->addrLink.addr.s6_addr[8], interfaceID, 8);
	inet_pton(AF_INET6, "ffff:ffff:ffff:ffff::", &addrInfo->addrLink.mask);
	addrInfo->addrLink.domain = _GLIDIX_DOM_LINK;
	doAddrConf(config->ifname);
	
	// send a router solicitation
	int sockfd = socket(AF_INET6, SOCK_RAW, IPPROTO_ICMPV6);
	if (sockfd == -1)
	{
		fprintf(stderr, "[slaac] failed to open ICMPv6 socket: %s\n", strerror(errno));
		free(addrInfo);
		_glidix_route_clear(AF_INET6, config->ifname);
		_glidix_netconf_addr(AF_INET6, config->ifname, NULL, 0);
		close(fdRandom);
		return;
	};
	
	if (_glidix_bindif(sockfd, config->ifname) != 0)
	{
		fprintf(stderr, "[slaac] failed to bind ICMPv6 to %s: %s\n", config->ifname, strerror(errno));
		free(addrInfo);
		close(sockfd);
		_glidix_route_clear(AF_INET6, config->ifname);
		_glidix_netconf_addr(AF_INET6, config->ifname, NULL, 0);
		close(fdRandom);
		return;
	};
	
	struct timeval timeout;
	timeout.tv_sec = 5;
	timeout.tv_usec = 0;
	if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(struct timeval)) != 0)
	{
		fprintf(stderr, "[slaac] failed to set ICMPv6 timeout: %s\n", strerror(errno));
		free(addrInfo);
		close(sockfd);
		_glidix_route_clear(AF_INET6, config->ifname);
		_glidix_netconf_addr(AF_INET6, config->ifname, NULL, 0);
		close(fdRandom);
		return;
	};
	
	uint_t hops = 255;
	if (setsockopt(sockfd, IPPROTO_IPV6, IPV6_UNICAST_HOPS, &hops, sizeof(uint_t)) != 0)
	{
		fprintf(stderr, "[slaac] failed to set IPv6 unicast hop limit: %s\n", strerror(errno));
		free(addrInfo);
		close(sockfd);
		_glidix_route_clear(AF_INET6, config->ifname);
		_glidix_netconf_addr(AF_INET6, config->ifname, NULL, 0);
		close(fdRandom);
		return;
	};
	
	if (setsockopt(sockfd, IPPROTO_IPV6, IPV6_MULTICAST_HOPS, &hops, sizeof(uint_t)) != 0)
	{
		fprintf(stderr, "[slaac] failed to set IPv6 multicast hop limit: %s\n", strerror(errno));
		free(addrInfo);
		close(sockfd);
		_glidix_route_clear(AF_INET6, config->ifname);
		_glidix_netconf_addr(AF_INET6, config->ifname, NULL, 0);
		close(fdRandom);
		return;
	};
	
	SolicitEncap encap;
	memset(&encap, 0, sizeof(SolicitEncap));
	memcpy(encap.pseudo.src, &addrInfo->addrLink.addr, 16);
	inet_pton(AF_INET6, "ff02::2", encap.pseudo.dest);
	encap.pseudo.len = htonl(sizeof(RouterSolicit));
	encap.pseudo.proto = IPPROTO_ICMPV6;
	encap.sol.type = 133;
	encap.sol.code = 0;
	encap.sol.optMac = 1;
	encap.sol.len = 1;
	memcpy(encap.sol.mac, mac, 6);
	encap.sol.checksum = inet_checksum(&encap, sizeof(SolicitEncap));
	
	struct sockaddr_in6 allRouters;
	memset(&allRouters, 0, sizeof(struct sockaddr_in6));
	allRouters.sin6_family = AF_INET6;
	inet_pton(AF_INET6, "ff02::2", &allRouters.sin6_addr);
	allRouters.sin6_scope_id = info.scopeID;
	
	if (sendto(sockfd, &encap.sol, sizeof(RouterSolicit), 0, (struct sockaddr*)&allRouters, sizeof(struct sockaddr_in6)) == -1)
	{
		fprintf(stderr, "[slaac] failed to send router solicitation: %s\n", strerror(errno));
		free(addrInfo);
		close(sockfd);
		_glidix_route_clear(AF_INET6, config->ifname);
		_glidix_netconf_addr(AF_INET6, config->ifname, NULL, 0);
	};
	
	InboundBuffer inbuf;
	struct sockaddr_in6 saddr;
	while (netman_running())
	{
		size_t i;
		for (i=0; i<numPrefixes; i++)
		{
			if (clock() >= prefixInfo[i].rotateDeadline)
			{
				// rotate the temporary addresses
				fprintf(stderr, "[slaac] rotating temporary addresses\n");
				
				prefixInfo[i].rotateDeadline = clock() + 10UL * 60UL * CLOCKS_PER_SEC;
				
				size_t j;
				for (j=0; j<(NUM_TEMP_ADDR-1); j++)
				{
					memcpy(&addrInfo->prefixes[i].addrTemp[j+1].addr,
						&addrInfo->prefixes[i].addrTemp[j].addr, 16);
				};
				
				genRandomAddr(prefixInfo[i].prefix, &addrInfo->prefixes[i].addrTemp[0].addr);
				doAddrConf(config->ifname);
			};
		};
		
		socklen_t addrlen = sizeof(struct sockaddr_in6);
		ssize_t size = recvfrom(sockfd, inbuf.payload, 1280, 0, (struct sockaddr*)&saddr, &addrlen);
		
		if (size == -1)
		{
			continue;
		};
		
		size_t pktsz = (size_t) size;
		
		// fill in the pseudo-header
		memset(&inbuf.pseudo, 0, sizeof(PseudoHeader));
		memcpy(inbuf.pseudo.src, &saddr.sin6_addr, 16);
		memcpy(inbuf.pseudo.dest, &addrInfo->addrLink.addr, 16);
		inbuf.pseudo.len = htonl((uint32_t)pktsz);
		inbuf.pseudo.proto = IPPROTO_ICMPV6;
		
		// do the checksum
		if (inet_checksum(&inbuf.pseudo, sizeof(PseudoHeader)+pktsz) != 0)
		{
			// the packet may also have been sent to ff02::1
			inet_pton(AF_INET6, "ff02::1", inbuf.pseudo.dest);
			if (inet_checksum(&inbuf.pseudo, sizeof(PseudoHeader)+pktsz) != 0)
			{
				// invalid checksum
				continue;
			};
		};
		
		if (pktsz < sizeof(AdvHeader))
		{
			continue;
		};
		
		AdvHeader *header = (AdvHeader*) inbuf.payload;
		if ((header->type != 134) || (header->code != 0))
		{
			// not a router advertisment
			continue;
		};
		
		if (header->routerLifetime != 0)
		{
			// add a default route
			memset(&inroute->dest, 0, 16);
			memset(&inroute->mask, 0, 16);
			memcpy(&inroute->gateway, &saddr.sin6_addr, 16);
			inroute->domain = _GLIDIX_DOM_GLOBAL;
			
			if (_glidix_route_add(AF_INET6, -1, &route) != 0)
			{
				fprintf(stderr, "[slaac] WARNING: failed to add default route\n");
			};
		};
		
		uint8_t *scan = (uint8_t*) &header[1];
		uint8_t *end = (uint8_t*) inbuf.payload + pktsz;
		
		while (scan < end)
		{
			uint8_t opt = scan[0];
			uint8_t len = scan[1];
			uint8_t *valptr = &scan[2];
			
			scan += (size_t)len * 8;
			if ((opt == 3) && (len == 4))
			{
				// prefix information
				AdvPrefixInfo *info = (AdvPrefixInfo*) valptr;
				
				// ignore it if the prefix length is not 64
				if (info->preflen != 64)
				{
					continue;
				};
				
				// ignore off-link advertisments
				if ((info->flags & PREFIX_ONLINK) == 0)
				{
					continue;
				};
				
				// ignore advertisments that aren't meant for auto-configuration
				if ((info->flags & PREFIX_AUTO) == 0)
				{
					continue;
				};
				
				// check if we've already seen this prefix
				int found = 0;
				for (i=0; i<numPrefixes; i++)
				{
					if (memcmp(prefixInfo[i].prefix, info->prefix, 8) == 0)
					{
						// update lifetimes
						prefixInfo[i].lifetimeValid = clock() + (clock_t)ntohl(info->lifetimeValid);
						prefixInfo[i].lifetimePref = clock() + (clock_t)ntohl(info->lifetimePref);
						found = 1;
						break;
					};
				};
				
				if (!found)
				{
					// new prefix!
					// start by adding the route
					memcpy(&inroute->dest, info->prefix, 16);
					memset(&inroute->mask, 0, 16);
					memset(&inroute->mask, 0xFF, 8);
					memset(&inroute->gateway, 0, 16);
					inroute->domain = _GLIDIX_DOM_GLOBAL;
					
					// we add it at the very start (index 0) because the default route
					// must come later
					if (_glidix_route_add(AF_INET6, 0, &route) != 0)
					{
						fprintf(stderr, "[slaac] failed to add a route: %s\n", strerror(errno));
						continue;
					};
					
					size_t prefIndex = numPrefixes++;
					prefixInfo = (PrefixInfo*) realloc(prefixInfo, sizeof(PrefixInfo)*numPrefixes);
					addrInfo = (AutoAddrInfo*) realloc(addrInfo,
							sizeof(AutoAddrInfo)+sizeof(PrefixAddrList)*numPrefixes);
					
					memcpy(prefixInfo[prefIndex].prefix, info->prefix, 8);
					prefixInfo[prefIndex].lifetimeValid = clock() + (clock_t)ntohl(info->lifetimeValid);
					prefixInfo[prefIndex].lifetimePref = clock() + (clock_t)ntohl(info->lifetimePref);
					prefixInfo[prefIndex].rotateDeadline = clock() + 10UL * 60UL * CLOCKS_PER_SEC;
					
					// the fixed address is made from the interface ID and has the non-defualt domain
					memcpy(&addrInfo->prefixes[prefIndex].addrFixed.addr, info->prefix, 8);
					memcpy(&addrInfo->prefixes[prefIndex].addrFixed.addr.s6_addr[8], interfaceID, 8);
					
					if (!isAddrUnique(&addrInfo->prefixes[prefIndex].addrFixed.addr))
					{
						// this shouldn't happen but if it does then just generate a random
						// address as the fixed address
						genRandomAddr(info->prefix, &addrInfo->prefixes[prefIndex].addrFixed.addr);
					};
					
					uint8_t mask64[16];
					inet_pton(AF_INET6, "ffff:ffff:ffff:ffff::", mask64);
					memcpy(&addrInfo->prefixes[prefIndex].addrFixed.mask, mask64, 16);
					addrInfo->prefixes[prefIndex].addrFixed.domain = _GLIDIX_DOM_NODEFAULT;
					
					// temporary addresses have the global domain
					for (i=0; i<NUM_TEMP_ADDR; i++)
					{
						genRandomAddr(info->prefix, &addrInfo->prefixes[prefIndex].addrTemp[i].addr);
						memcpy(&addrInfo->prefixes[prefIndex].addrTemp[i].mask, mask64, 16);
						addrInfo->prefixes[prefIndex].addrTemp[i].domain = _GLIDIX_DOM_GLOBAL;
					};
					
					// submit the changes to the system
					doAddrConf(config->ifname);
				};
			};
		};
	};
	
	fprintf(stderr, "[slaac] terminating\n");
	free(addrInfo);
	close(sockfd);
	_glidix_route_clear(AF_INET6, config->ifname);
	_glidix_netconf_addr(AF_INET6, config->ifname, NULL, 0);
	close(fdRandom);
};

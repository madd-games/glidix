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

#define	GSO_RCVTIMEO			0
#define	GSO_SNDTIMEO			1
#define	GSO_SNDFLAGS			2
#define	GSO_COUNT			3

#define	PKT_HDRINC			(1 << 8)
#define	PKT_DONTROUTE			(1 << 9)
#define	PKT_DONTFRAG			(1 << 10)

typedef struct
{
	/**
	 * If nonzero, the default route shall be added on this interface.
	 * Default value is yes.
	 */
	int				defRoute;
	
	/**
	 * If nonzero, DNS servers should be accepted from the DHCP server.
	 * Default value is yes.
	 */
	int				dnsAccept;
	
	/**
	 * The client name. The default value is derived from the MAC address.
	 */
	char*				clientName;
} DHCPOptions;

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
} DHCPHeader;

typedef struct
{
	uint32_t			myip;
	uint32_t			server;
	uint32_t			router;
	uint32_t			mask;
} DHCPParams;

typedef struct
{
	size_t				currentSize;
	void*				data;
} PacketBuffer;

static PacketBuffer dhcpDiscover;
static PacketBuffer dhcpRequest;
static char receiveBuffer[65536];
static DHCPParams params;

static void pktAppend(PacketBuffer *buf, const void *src, size_t size)
{
	buf->data = realloc(buf->data, buf->currentSize+size);
	memcpy((char*)buf->data + buf->currentSize, src, size);
	buf->currentSize += size;
};

int netman_dhcp_init(NetmanIfConfig *config)
{
	_glidix_netstat info;
	if (_glidix_netconf_stat(config->ifname, &info, sizeof(_glidix_netstat)) == -1)
	{
		fprintf(stderr, "dhcp: the interface %s does not exist\n", config->ifname);
		return -1;
	};
	
	if (info.ifconfig.type != IF_ETHERNET)
	{
		fprintf(stderr, "dhcp: the interface %s is not an Ethernet interface\n", config->ifname);
		return -1;
	};
	
	DHCPOptions *options = (DHCPOptions*) malloc(sizeof(DHCPOptions));
	memset(options, 0, sizeof(DHCPOptions));
	options->defRoute = 1;
	options->dnsAccept = 1;
	
	char buffer[256];
	sprintf(buffer, "glidix%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx",
		info.ifconfig.ethernet.mac[0], info.ifconfig.ethernet.mac[1], info.ifconfig.ethernet.mac[2],
		info.ifconfig.ethernet.mac[3], info.ifconfig.ethernet.mac[4], info.ifconfig.ethernet.mac[5]
	);
	
	options->clientName = strdup(buffer);
	config->data = options;
	return 0;
};

void netman_dhcp_line(NetmanIfConfig *config, int lineno, char *line)
{
	DHCPOptions *options = (DHCPOptions*) config->data;
	
	char *cmd = strtok(line, SPACES);
	if (cmd == NULL)
	{
		return;
	};
	
	if (strcmp(cmd, "name") == 0)
	{
		char *name = strtok(NULL, SPACES);
		if (name == NULL)
		{
			fprintf(stderr, "dhcp: if.conf line %d error: expected client name\n", lineno);
			config->status = NETMAN_STATUS_ERROR;
			return;
		};
		
		if (strlen(name) > 256)
		{
			fprintf(stderr, "dhcp: if.conf line %d error: client name too long (256 chars max)\n", lineno);
			config->status = NETMAN_STATUS_ERROR;
			return;
		};
		
		free(options->clientName);
		options->clientName = strdup(name);
	}
	else if (strcmp(cmd, "default-route") == 0)
	{
		char *opt = strtok(NULL, SPACES);
		if (opt == NULL)
		{
			fprintf(stderr, "dhcp: if.conf line %d error: expected argument to 'default-route'\n", lineno);
			config->status = NETMAN_STATUS_ERROR;
			return;
		};
		
		if (strcmp(opt, "yes") == 0)
		{
			options->defRoute = 1;
		}
		else if (strcmp(opt, "no") == 0)
		{
			options->defRoute = 0;
		}
		else
		{
			fprintf(stderr, "dhcp: if.conf line %d error: expected 'yes' or 'no', not '%s'\n", lineno, opt);
			config->status = NETMAN_STATUS_ERROR;
			return;
		};
	}
	else if (strcmp(cmd, "accept-dns") == 0)
	{
		char *opt = strtok(NULL, SPACES);
		if (opt == NULL)
		{
			fprintf(stderr, "dhcp: if.conf line %d error: expected argument to 'accept-dns'\n", lineno);
			config->status = NETMAN_STATUS_ERROR;
			return;
		};
		
		if (strcmp(opt, "yes") == 0)
		{
			options->dnsAccept = 1;
		}
		else if (strcmp(opt, "no") == 0)
		{
			options->dnsAccept = 0;
		}
		else
		{
			fprintf(stderr, "dhcp: if.conf line %d error: expected 'yes' or 'no', not '%s'\n", lineno, opt);
			config->status = NETMAN_STATUS_ERROR;
			return;
		};
	}
	else
	{
		fprintf(stderr, "dhcp: if.conf line %d error: unknown command '%s'\n", lineno, cmd);
		config->status = NETMAN_STATUS_ERROR;
		return;
	};
};

void netman_dhcp_ifup(NetmanIfConfig *config)
{
	DHCPOptions *options = (DHCPOptions*) config->data;
	printf("[dhcp] attempting to configure using DHCP...\n");

	// delete DNS servers
	char dnspath[256];
	sprintf(dnspath, "/etc/dns/ipv4/%s.if", config->ifname);
	unlink(dnspath);
	
	// first remove all current addresses
	if (_glidix_netconf_addr(AF_INET, config->ifname, NULL, 0) != 0)
	{
		fprintf(stderr, "[dhcp] cannot clear address of %s: %s\n", config->ifname, strerror(errno));
		return;
	};
	
	// remove all routes
	if (_glidix_route_clear(AF_INET, config->ifname) != 0)
	{
		fprintf(stderr, "[dhcp] cannot clear IPv4 routes of %s: %s\n", config->ifname, strerror(errno));
		return;
	};
	
	uint8_t mac[6];
	_glidix_netstat info;
	if (_glidix_netconf_stat(config->ifname, &info, sizeof(_glidix_netstat)) == -1)
	{
		fprintf(stderr, "[dhcp] failed to netconf stat %s: %s\n", config->ifname, strerror(errno));
		return;
	};
	
	if (info.ifconfig.type != IF_ETHERNET)
	{
		fprintf(stderr, "[dhcp] failed to get MAC address of %s: not ethernet\n", config->ifname);
		return;
	};
	
	memcpy(mac, &info.ifconfig.ethernet.mac, 6);

	fprintf(stderr, "[dhcp] generating XID...\n");
	uint32_t xid;
	int fd = open("/dev/random", O_RDONLY);
	if (fd == -1)
	{
		fprintf(stderr, "[dhcp] WARNING: Cannot open /dev/random, using uninitialized XID\n");
		xid ^= 0xDEADBEEF;
	}
	else
	{
		read(fd, &xid, 4);
		close(fd);
	};

	int sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (sockfd == -1)
	{
		fprintf(stderr, "[dhcp] failed to open UDP socket: %s\n", strerror(errno));
		return;
	};
	
	if (_glidix_bindif(sockfd, config->ifname) != 0)
	{
		fprintf(stderr, "[dhcp] failed to bind to %s: %s\n", config->ifname, strerror(errno));
		close(sockfd);
		return;
	};
	
	// there is no routing table so we must disable routing on the socket
	_glidix_setsockopt(sockfd, SOL_SOCKET, GSO_SNDFLAGS, PKT_DONTROUTE);
	
	struct timeval rcvtimeo;
	rcvtimeo.tv_sec = 3;
	rcvtimeo.tv_usec = 0;
	if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &rcvtimeo, sizeof(struct timeval)) != 0)
	{
		fprintf(stderr, "[dhcp] failed to set receive timeout: %s\n", strerror(errno));
		close(sockfd);
		return;
	};
	
	// broadcast address (destination)
	struct sockaddr_in bcast;
	memset(&bcast, 0, sizeof(struct sockaddr_in));
	bcast.sin_family = AF_INET;
	bcast.sin_addr.s_addr = INADDR_BROADCAST;
	bcast.sin_port = htons(67);
	
	// bind to INADDR_ANY
	struct sockaddr_in laddr;
	memset(&laddr, 0, sizeof(struct sockaddr_in));
	laddr.sin_family = AF_INET;
	laddr.sin_addr.s_addr = INADDR_ANY;
	laddr.sin_port = htons(68);
	
	if (bind(sockfd, (struct sockaddr*)&laddr, sizeof(struct sockaddr_in)) != 0)
	{
		fprintf(stderr, "[dhcp] failed to bind to UDP port 68: %s\n", strerror(errno));
		close(sockfd);
		return;
	};
	
	fprintf(stderr, "[dhcp] sending DHCPDISCOVER\n");
	DHCPHeader header;
	memset(&header, 0, sizeof(DHCPHeader));
	header.op = 1;
	header.htype = 1;
	header.hlen = 6;
	header.hops = 0;
	header.xid = xid;
	header.flags = htons(0x8000);				// tell server to broadcast messages
	memcpy(header.chaddr, mac, 6);
	memcpy(header.magic, "\x63\x82\x53\x63", 4);
	
	pktAppend(&dhcpDiscover, &header, sizeof(DHCPHeader));
	
	// add the "message type" option (53), for DHCPDISCOVER (1)
	uint8_t optDiscover[3] = {53, 1, 1};
	pktAppend(&dhcpDiscover, optDiscover, 3);

	// parameter request: subnet mask, router, name server
	uint8_t optParReq[5] = {55, 3, 1, 3, 6};
	pktAppend(&dhcpDiscover, optParReq, 5);
	
	// client name option
	uint8_t optClientNameHdr[2];
	optClientNameHdr[0] = 12;
	optClientNameHdr[1] = (uint8_t) strlen(options->clientName);
	pktAppend(&dhcpDiscover, optClientNameHdr, 2);
	pktAppend(&dhcpDiscover, options->clientName, strlen(options->clientName));
	
	// end of options
	pktAppend(&dhcpDiscover, "\xFF", 1);
	
	// send it
	int numRetries = 3;
	int gotOffer = 0;
	while ((numRetries--) && (!gotOffer))
	{
		if (sendto(sockfd, dhcpDiscover.data, dhcpDiscover.currentSize, 0,
			(struct sockaddr*)&bcast, sizeof(struct sockaddr_in)) == -1)
		{
			fprintf(stderr, "[dhcp] failed to send DHCPDISCOVER: %s\n", strerror(errno));
			close(sockfd);
			return;
		};
		
		clock_t timeoutClock = clock() + 3UL * CLOCKS_PER_SEC;
		while (1)
		{
			if (clock() >= timeoutClock)
			{
				fprintf(stderr, "[dhcp] timeout: trying again\n");
				break;
			};
			
			ssize_t size = read(sockfd, receiveBuffer, 65536);
			if (size == -1)
			{
				if (errno == EINTR)
				{
					if (!netman_running())
					{
						close(sockfd);
						return;
					};
				}
				else if (errno == ETIMEDOUT)
				{
					fprintf(stderr, "[dhcp] timeout: trying again\n");
					break;
				}
				else
				{
					fprintf(stderr, "[dhcp] failed to receive: %s\n", strerror(errno));
					close(sockfd);
					return;
				};
			};
			
			if (size < sizeof(DHCPHeader))
			{
				continue;
			};
			
			DHCPHeader *hdr = (DHCPHeader*) receiveBuffer;
			if ((hdr->op == 2) && (hdr->xid == xid))
			{
				gotOffer = 1;
				break;
			};
		};
	};
	
	if (!gotOffer)
	{
		fprintf(stderr, "[dhcp] exhausted attempts and no offer received; giving up\n");
		close(sockfd);
		return;
	};
	
	DHCPHeader *hdr = (DHCPHeader*) receiveBuffer;
	memcpy(&params.myip, &hdr->yiaddr, 4);
	
	uint8_t *scan = (uint8_t*) &hdr[1];
	while ((*scan) != 0xFF)
	{
		uint8_t len = scan[1];
		uint8_t opt = scan[0];
		uint8_t *optptr = &scan[2];
		
		scan += 2;
		scan += len;
		
		if ((opt == 54) && (len == 4))
		{
			memcpy(&params.server, optptr, 4);
		};
	};
	
	if (params.server == 0)
	{
		fprintf(stderr, "[dhcp] DHCP server specified no IPv4 address\n");
		close(sockfd);
		return;
	};
	
	char myip[INET_ADDRSTRLEN];
	char serverip[INET_ADDRSTRLEN];
	inet_ntop(AF_INET, &params.myip, myip, INET_ADDRSTRLEN);
	inet_ntop(AF_INET, &params.server, serverip, INET_ADDRSTRLEN);
	
	fprintf(stderr, "[dhcp] got DHCPOFFER for %s from %s\n", myip, serverip);
	fprintf(stderr, "[dhcp] sending DHCPREQUEST\n");
	
	xid += 0x67;
	memset(&header, 0, sizeof(DHCPHeader));
	header.op = 1;
	header.htype = 1;
	header.hlen = 6;
	header.hops = 0;
	header.xid = xid;
	header.siaddr = params.server;
	memcpy(header.chaddr, mac, 6);
	memcpy(header.magic, "\x63\x82\x53\x63", 4);
	pktAppend(&dhcpRequest, &header, sizeof(DHCPHeader));
	
	// request type option "53"; 3 = DHCPREQUEST
	uint8_t optRequest[3] = {53, 1, 3};
	pktAppend(&dhcpRequest, optRequest, 3);
	
	// requested IPv4 address
	uint8_t optReqAddr[6] = {50, 4, 0, 0, 0, 0};
	memcpy(&optReqAddr[2], &params.myip, 4);
	pktAppend(&dhcpRequest, optReqAddr, 6);
	
	// DHCP server IPv4 address
	uint8_t optServerAddr[6] = {54, 4, 0, 0, 0, 0};
	memcpy(&optServerAddr[2], &params.server, 4);
	pktAppend(&dhcpRequest, optServerAddr, 6);
	
	// parameter request and client name
	pktAppend(&dhcpRequest, optParReq, 5);
	pktAppend(&dhcpRequest, optClientNameHdr, 2);
	pktAppend(&dhcpRequest, options->clientName, strlen(options->clientName));
	
	// end of options
	pktAppend(&dhcpRequest, "\xFF", 1);
	
	// send it
	numRetries = 3;
	int gotAck = 0;
	while ((numRetries--) && (!gotAck))
	{
		if (sendto(sockfd, dhcpRequest.data, dhcpRequest.currentSize, 0,
			(struct sockaddr*)&bcast, sizeof(struct sockaddr_in)) == -1)
		{
			fprintf(stderr, "[dhcp] failed to send DHCPREQUEST: %s\n", strerror(errno));
			close(sockfd);
			return;
		};
		
		clock_t timeoutClock = clock() + 3UL * CLOCKS_PER_SEC;
		while (1)
		{
			if (clock() >= timeoutClock)
			{
				fprintf(stderr, "[dhcp] timeout: trying again\n");
				break;
			};
			
			ssize_t size = read(sockfd, receiveBuffer, 65536);
			if (size == -1)
			{
				if (errno == EINTR)
				{
					if (!netman_running())
					{
						close(sockfd);
						return;
					};
				}
				else if (errno == ETIMEDOUT)
				{
					fprintf(stderr, "[dhcp] timeout: trying again\n");
					break;
				}
				else
				{
					fprintf(stderr, "[dhcp] failed to receive: %s\n", strerror(errno));
					close(sockfd);
					return;
				};
			};
			
			if (size < sizeof(DHCPHeader))
			{
				continue;
			};
			
			DHCPHeader *hdr = (DHCPHeader*) receiveBuffer;
			if ((hdr->op == 2) && (hdr->xid == xid))
			{
				gotAck = 1;
				break;
			};
		};
	};
	
	if (!gotAck)
	{
		fprintf(stderr, "[dhcp] failed to receive initial DHCPACK; giving up\n");
		close(sockfd);
		return;
	};
	
	fprintf(stderr, "[dhcp] Received DHCPACK, beginning configuration\n");
	struct in_addr *nameServers;
	size_t numNameServers = 0;
	unsigned leaseTime = 0;
	
	scan = (uint8_t*) &hdr[1];
	while ((*scan) != 0xFF)
	{
		uint8_t len = scan[1];
		uint8_t opt = scan[0];
		uint8_t *optptr = &scan[2];
		
		scan += 2;
		scan += len;
		
		if ((opt == 53) && (len == 1))
		{
			if ((*optptr) != 5)
			{
				fprintf(stderr, "[dhcp] Invalid DHCPACK! (Type is not 5)\n");
				close(sockfd);
				return;
			};
		}
		else if ((opt == 1) && (len == 4))
		{
			memcpy(&params.mask, optptr, 4);
		}
		else if ((opt == 3) && (len == 4))
		{
			memcpy(&params.router, optptr, 4);
		}
		else if ((opt == 51) && (len == 4))
		{
			leaseTime = ntohl(*((uint32_t*)optptr));
		}
		else if (opt == 6)
		{
			nameServers = (struct in_addr*) optptr;
			numNameServers = len / 4;
		};
	};
	
	if (params.mask == 0)
	{
		fprintf(stderr, "[dhcp] DHCPACK did not specify an address mask!\n");
		close(sockfd);
		return;
	};
	
	// add routes
	int addDefaultRoute = (options->defRoute) && (params.router != 0);
	
	// we use the router here such that if the mask is 255.255.255.255, then the route
	// will indeed point to the router.
	// except if the router is 0
	uint32_t localroute = params.router & params.mask;
	if (params.router == 0) localroute = params.myip & params.mask;
	
	_glidix_gen_route genroute;
	_glidix_in_route *inroute = (_glidix_in_route*) &genroute;
	strcpy(inroute->ifname, config->ifname);
	memcpy(&inroute->dest, &localroute, 4);
	memcpy(&inroute->mask, &params.mask, 4);
	memset(&inroute->gateway, 0, 4);
	inroute->domain = 0;
	inroute->flags = 0;
	
	if (_glidix_route_add(AF_INET, -1, &genroute) != 0)
	{
		fprintf(stderr, "[dhcp] cannot add local route: %s\n", strerror(errno));
		close(sockfd);
		return;
	};
	
	if (addDefaultRoute)
	{
		memset(&inroute->dest, 0, 4);
		memset(&inroute->mask, 0, 4);
		memcpy(&inroute->gateway, &params.router, 4);
		
		if (_glidix_route_add(AF_INET, -1, &genroute) != 0)
		{
			fprintf(stderr, "[dhcp] cannot add default route: %s\n", strerror(errno));
			close(sockfd);
			return;
		};
	};
	
	// configure IPv4 address
	_glidix_ifaddr4 ifaddr;
	memcpy(&ifaddr.addr, &params.myip, 4);
	memcpy(&ifaddr.mask, &params.mask, 4);
	ifaddr.domain = 0;
	
	if (_glidix_netconf_addr(AF_INET, config->ifname, &ifaddr, sizeof(_glidix_ifaddr4)) != 0)
	{
		fprintf(stderr, "[dhcp] failed to configure IPv4 address: %s\n", strerror(errno));
		close(sockfd);
		_glidix_route_clear(AF_INET, config->ifname);
		return;
	};
	
	// DNS servers
	if (options->dnsAccept)
	{
		FILE *fp = fopen(dnspath, "w");
		if (fp == NULL)
		{
			fprintf(stderr, "[dhcp] failed to open %s: %s\n", dnspath, strerror(errno));
			close(sockfd);
			_glidix_route_clear(AF_INET, config->ifname);
			_glidix_netconf_addr(AF_INET, config->ifname, NULL, 0);
			return;
		};
		
		fprintf(fp, "# DNS servers configured by DHCP\n");
		size_t i;
		for (i=0; i<numNameServers; i++)
		{
			char buffer[INET_ADDRSTRLEN];
			inet_ntop(AF_INET, &nameServers[i], buffer, INET_ADDRSTRLEN);
			fprintf(fp, "%s\n", buffer);
		};
		
		fclose(fp);
	};
	
	unsigned leftToSleep;
	fprintf(stderr, "[dhcp] configuration complete; lease=%u seconds\n", leaseTime);
	if (leaseTime == 0)
	{
		while (netman_running()) pause();
	}
	else
	{
		while (1)
		{
			leftToSleep = leaseTime/2;
			while (leftToSleep > 0)
			{
				leftToSleep = sleep(leftToSleep);
				
				if (!netman_running())
				{
					goto down;
				};
			};
			
			fprintf(stderr, "[dhcp] lease ending; attempting to renew\n");
			
			numRetries = 3;
			gotAck = 0;
			while ((numRetries--) && (!gotAck))
			{
				if (sendto(sockfd, dhcpRequest.data, dhcpRequest.currentSize, 0,
					(struct sockaddr*)&bcast, sizeof(struct sockaddr_in)) == -1)
				{
					fprintf(stderr, "[dhcp] failed to send DHCPREQUEST: %s\n", strerror(errno));
					close(sockfd);
					return;
				};
		
				clock_t timeoutClock = clock() + 3UL * CLOCKS_PER_SEC;
				while (1)
				{
					if (clock() >= timeoutClock)
					{
						fprintf(stderr, "[dhcp] timeout: trying again\n");
						break;
					};
			
					ssize_t size = read(sockfd, receiveBuffer, 65536);
					if (size == -1)
					{
						if (errno == EINTR)
						{
							if (!netman_running())
							{
								close(sockfd);
								return;
							};
						}
						else if (errno == ETIMEDOUT)
						{
							fprintf(stderr, "[dhcp] timeout: trying again\n");
							break;
						}
						else
						{
							fprintf(stderr, "[dhcp] failed to receive: %s\n", strerror(errno));
							close(sockfd);
							return;
						};
					};
			
					if (size < sizeof(DHCPHeader))
					{
						continue;
					};
			
					DHCPHeader *hdr = (DHCPHeader*) receiveBuffer;
					if ((hdr->op == 2) && (hdr->xid == xid))
					{
						gotAck = 1;
						break;
					};
				};
			};
			
			if (!gotAck)
			{
				fprintf(stderr, "[dhcp] the DHCP server did not acknowledge my IP; giving up\n");
				goto down;
			};
			
			scan = (uint8_t*) &hdr[1];
			while ((*scan) != 0xFF)
			{
				uint8_t len = scan[1];
				uint8_t opt = scan[0];
				uint8_t *optptr = &scan[2];
		
				scan += 2;
				scan += len;
		
				if ((opt == 53) && (len == 1))
				{
					if ((*optptr) != 5)
					{
						fprintf(stderr, "[dhcp] Invalid DHCPACK! (Type is not 5)\n");
						goto down;
					};
				};
			};
		};
	};
	
	down:
	fprintf(stderr, "[dhcp] bringing interface down\n");
	close(sockfd);
	_glidix_route_clear(AF_INET, config->ifname);
	_glidix_netconf_addr(AF_INET, config->ifname, NULL, 0);
};

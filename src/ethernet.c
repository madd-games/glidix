/*
	Glidix kernel

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

#include <glidix/ethernet.h>
#include <glidix/netif.h>
#include <glidix/memory.h>
#include <glidix/errno.h>
#include <glidix/string.h>
#include <glidix/console.h>
#include <glidix/icmp.h>
#include <glidix/socket.h>

#define CRCPOLY2 0xEDB88320UL  /* left-right reversal */

uint32_t ether_checksum(const void *data, size_t n)
{
	const uint8_t *c = (const uint8_t*) data;
	int i, j;
	uint32_t r;

	r = 0xFFFFFFFFUL;
	for (i = 0; i < n; i++)
	{
		r ^= c[i];
		for (j = 0; j < 8; j++)
			if (r & 1) r = (r >> 1) ^ CRCPOLY2;
			else       r >>= 1;
	}
	return r ^ 0xFFFFFFFFUL;
}

static void etherSendRaw(NetIf *netif, const void *frame, size_t framelen)
{
	// send the frame to capture sockets
	struct sockaddr_cap caddr;
	memset(&caddr, 0, sizeof(struct sockaddr_cap));
	caddr.scap_family = AF_CAPTURE;
	strcpy(caddr.scap_ifname, netif->name);
	onTransportPacket((struct sockaddr*)&caddr, (struct sockaddr*)&caddr, sizeof(struct sockaddr_cap),
		frame, framelen-4, IF_ETHERNET);	// without CRC

	netif->ifconfig.ethernet.send(netif, frame, framelen);
};

static MacResolution *getMacResolution(NetIf *netif, int family, uint8_t *ip)
{
	size_t size = 16;
	if (family == AF_INET) size = 4;
	
	spinlockAcquire(&netif->ifconfig.ethernet.resLock);
	MacResolution *scan;
	MacResolution *last = NULL;
	for (scan=netif->ifconfig.ethernet.res; scan!=NULL; scan=scan->next)
	{
		last = scan;
		if (scan->family == family)
		{
			if (memcmp(scan->ip, ip, size) == 0)
			{
				spinlockRelease(&netif->ifconfig.ethernet.resLock);
				return scan;
			};
		};
	};
	
	// no resolution yet, create a new one
	MacResolution *macres = NEW(MacResolution);
	macres->family = family;
	memset(macres->ip, 0, 16);
	memcpy(macres->ip, ip, size);
	memset(&macres->mac, 0, 6);
	cvInit(&macres->cond);
	macres->next = NULL;
	
	if (last == NULL)
	{
		netif->ifconfig.ethernet.res = macres;
	}
	else
	{
		last->next = macres;
	};
	
	spinlockRelease(&netif->ifconfig.ethernet.resLock);
	return macres;
};

static int resolveAddress(NetIf *netif, int family, uint8_t *ip, MacAddress *mac, uint64_t nanotimeout)
{
	// trivial resolutions
	if (family == AF_INET)
	{
		uint32_t broadcast = 0xFFFFFFFF;
		if (memcmp(ip, &broadcast, 4) == 0)
		{
			memset(mac, 0xFF, 6);
			return 0;
		};
		
		int i;
		for (i=0; i<netif->ipv4.numAddrs; i++)
		{
			uint32_t laddr = *((uint32_t*)&netif->ipv4.addrs[i].addr);
			uint32_t mask = *((uint32_t*)&netif->ipv4.addrs[i].mask);
			uint32_t addr = *((uint32_t*)ip);
			
			if ((laddr | (~mask)) == addr)
			{
				// local broadcast
				memset(mac, 0xFF, 6);
				return 0;
			};
		};
	}
	else if (family == AF_INET6)
	{
		if (ip[0] == 0xFF)
		{
			// IPv6 multicast; take bottom 32 bits and map onto 33:33:x:x:x:x
			mac->addr[0] = mac->addr[1] = 0x33;
			memcpy(&mac->addr[2], &ip[12], 4);
			return 0;
		};
	};
	
	MacResolution *res = getMacResolution(netif, family, ip);
	if (!res->cond.value)
	{
		if (family == AF_INET)
		{
			// before waiting, send an ARP request
			ARPPacket arp;
			memset(&arp, 0, sizeof(ARPPacket));
			memset(&arp.header.dest, 0xFF, 6);
			memcpy(&arp.header.src, &netif->ifconfig.ethernet.mac, 6);
			arp.header.type = __builtin_bswap16(ETHER_TYPE_ARP);
			arp.htype = __builtin_bswap16(1);
			arp.ptype = __builtin_bswap16(ETHER_TYPE_IP);
			arp.hlen = 6;
			arp.plen = 4;
			arp.oper = __builtin_bswap16(1);		// request
			memcpy(&arp.sha, &netif->ifconfig.ethernet.mac, 6);
			if (netif->ipv4.numAddrs == 0)
			{
				memset(arp.spa, 0, 4);
			}
			else
			{
				memcpy(&arp.spa, &netif->ipv4.addrs[0].addr, 4);
			};
			memset(&arp.tha, 0, 6);
			memcpy(&arp.tpa, res->ip, 4);
			arp.crc = ether_checksum(&arp, sizeof(ARPPacket)-4);
			etherSendRaw(netif, &arp, sizeof(ARPPacket));
		}
		else if (family == AF_INET6)
		{
			// before waiting, send an NDP neighbor solicitation
			static uint8_t solicitedNodePrefix[16] =
				{0xFF, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0xFF, 0x00, 0x00, 0x00};
			struct sockaddr_in6 ndp_src;
			memset(&ndp_src, 0, sizeof(struct sockaddr_in6));
			ndp_src.sin6_family = AF_UNSPEC;
			
			struct sockaddr_in6 ndp_dest;
			memset(&ndp_dest, 0, sizeof(struct sockaddr_in6));
			ndp_dest.sin6_family = AF_INET6;
			memcpy(&ndp_dest.sin6_addr, solicitedNodePrefix, 13);
			memcpy(&ndp_dest.sin6_addr.s6_addr[13], &ip[13], 3);
			ndp_dest.sin6_scope_id = netif->scopeID;
			
			// find out what IPv6 address we should use as the source
			int status = sendPacket((struct sockaddr*)&ndp_src, (const struct sockaddr*)&ndp_dest,
				NULL, 0, 0, 0, NULL);
			if (status == 0)
			{
				PseudoHeaderICMPv6 *phead = (PseudoHeaderICMPv6*)
					kalloca(sizeof(PseudoHeaderICMPv6) + sizeof(NDPNeighborSolicit));
				memcpy(phead->src, &ndp_src.sin6_addr, 16);
				memcpy(phead->dest, &ndp_dest.sin6_addr, 16);
				phead->len = __builtin_bswap32((uint32_t) sizeof(NDPNeighborSolicit));
				memset(phead->zeroes, 0, 3);
				phead->proto = IPPROTO_ICMPV6;
				
				NDPNeighborSolicit *sol = (NDPNeighborSolicit*) phead->payload;
				sol->type = 135;
				sol->code = 0;
				sol->checksum = 0;
				sol->resv = 0;
				memcpy(sol->addr, ip, 16);
				sol->opt1 = 1;
				sol->len1 = 1;
				memcpy(sol->mac, &netif->ifconfig.ethernet.mac, 6);
				sol->checksum = ipv4_checksum(phead, sizeof(PseudoHeaderICMPv6) + sizeof(NDPNeighborSolicit));
				
				sendPacket((struct sockaddr*)&ndp_src, (const struct sockaddr*)&ndp_dest,
					sol, sizeof(NDPNeighborSolicit), IPPROTO_ICMPV6 | PKT_DONTROUTE, nanotimeout, NULL);
			};
		};
	};
	
	if ((nanotimeout == 0) || (nanotimeout > NT_SECS(1)))
	{
		nanotimeout = NT_SECS(1);
	};
	
	int status = cvWait(&res->cond, nanotimeout);
	if (status == 0)
	{
		memcpy(mac, &res->mac, 6);
		return 0;
	};
	
	return -EHOSTUNREACH;
};

static int sendPacketToEthernet4(NetIf *netif, const struct sockaddr_in *gateway, const void *packet, size_t packetlen, uint64_t nanotimeout)
{
	MacAddress mac;
	int status = resolveAddress(netif, AF_INET, (uint8_t*) &gateway->sin_addr, &mac, nanotimeout);
	if (status != 0)
	{
		return status;
	};
	
	size_t sendlen = packetlen;
	if (sendlen < 46)
	{
		sendlen = 46;
	};
	
	// we allocate 2 extra bytes to allow alignments within the driver
	char *etherPacket = (char*) kmalloc(sizeof(EthernetHeader) + sendlen + 6);
	memset(etherPacket, 0, sizeof(EthernetHeader) + sendlen + 6);			// sending uninitialised data is dangerous
	EthernetHeader *head = (EthernetHeader*) etherPacket;
	
	memcpy(&head->dest, &mac, 6);
	memcpy(&head->src, &netif->ifconfig.ethernet.mac, 6);
	head->type = __builtin_bswap16(ETHER_TYPE_IP);
	
	memcpy(&head[1], packet, packetlen);
	
	uint32_t *crcPtr = (uint32_t*) &etherPacket[sizeof(EthernetHeader) + sendlen];
	*crcPtr = ether_checksum(etherPacket, sizeof(EthernetHeader) + sendlen);
	
	etherSendRaw(netif, etherPacket, sizeof(EthernetHeader) + sendlen + 4);
	kfree(etherPacket);
	return 0;
};

static int sendPacketToEthernet6(NetIf *netif, const struct sockaddr_in6 *gateway, const void *packet, size_t packetlen, uint64_t nanotimeout)
{
	MacAddress mac;
	int status = resolveAddress(netif, AF_INET6, (uint8_t*) &gateway->sin6_addr, &mac, nanotimeout);
	if (status != 0)
	{
		return status;
	};

	size_t sendlen = packetlen;
	if (sendlen < 46)
	{
		sendlen = 46;
	};
	
	char *etherPacket = (char*) kmalloc(sizeof(EthernetHeader) + sendlen + 6);
	memset(etherPacket, 0, sizeof(EthernetHeader) + sendlen + 6);
	EthernetHeader *head = (EthernetHeader*) etherPacket;
	
	memcpy(&head->dest, &mac, 6);
	memcpy(&head->src, &netif->ifconfig.ethernet.mac, 6);
	head->type = __builtin_bswap16(ETHER_TYPE_IPV6);
	
	memcpy(&head[1], packet, packetlen);
	
	uint32_t *crcPtr = (uint32_t*) &etherPacket[sizeof(EthernetHeader) + sendlen];
	*crcPtr = ether_checksum(etherPacket, sizeof(EthernetHeader)+sendlen);
	
	etherSendRaw(netif, etherPacket, sizeof(EthernetHeader) + sendlen + 4);
	kfree(etherPacket);
	return 0;
};

int sendPacketToEthernet(NetIf *netif, const struct sockaddr *gateway, const void *packet, size_t packetlen, uint64_t nanotimeout)
{
	if (gateway->sa_family == AF_INET)
	{
		return sendPacketToEthernet4(netif, (const struct sockaddr_in*) gateway, packet, packetlen, nanotimeout);
	}
	else if (gateway->sa_family == AF_INET6)
	{
		return sendPacketToEthernet6(netif, (const struct sockaddr_in6*) gateway, packet, packetlen, nanotimeout);
	}
	else
	{
		return -ENETUNREACH;
	};
};

static void onARPPacket(NetIf *netif, ARPPacket *arp)
{
	uint16_t op = __builtin_bswap16(arp->oper);
	if (op == 1)
	{
		// ARP request - see if they are asking about our IP address
		if ((arp->hlen != 6) || (arp->plen != 4))
		{
			return;
		};
		
		int i;
		int found = 0;
		for (i=0; i<netif->ipv4.numAddrs; i++)
		{
			if (memcmp(&netif->ipv4.addrs[i].addr, arp->tpa, 4) == 0)
			{
				found = 1;
				break;
			};
		};
		
		if (!found)
		{
			return;
		};
		
		ARPPacket reply;
		memset(&reply, 0, sizeof(ARPPacket));
		memcpy(&reply.header.dest, &arp->header.src, 6);
		memcpy(&reply.header.src, &netif->ifconfig.ethernet.mac, 6);
		reply.header.type = __builtin_bswap16(ETHER_TYPE_ARP);
		reply.htype = __builtin_bswap16(1);
		reply.ptype = __builtin_bswap16(ETHER_TYPE_IP);
		reply.hlen = 6;
		reply.plen = 4;
		reply.oper = __builtin_bswap16(2);
		memcpy(&reply.sha, &netif->ifconfig.ethernet.mac, 6);
		memcpy(reply.spa, arp->tpa, 4);
		memcpy(&reply.tha, &arp->sha, 6);
		memcpy(reply.tpa, arp->spa, 4);
		
		etherSendRaw(netif, &reply, sizeof(ARPPacket));
	}
	else if (op == 2)
	{
		// ARP reply - add mapping
		if ((arp->hlen != 6) || (arp->plen != 4))
		{
			return;
		};
		
		kprintf_debug("Found ARP mapping: %x:%x:%x:%x:%x:%x -> %d.%d.%d.%d\n",
			arp->sha.addr[0], arp->sha.addr[1], arp->sha.addr[2], arp->sha.addr[3], arp->sha.addr[4], arp->sha.addr[5],
			arp->spa[0], arp->spa[1], arp->spa[2], arp->spa[3]
		);
		
		MacResolution *res;
		for (res=netif->ifconfig.ethernet.res; res!=NULL; res=res->next)
		{
			if (res->family == AF_INET)
			{
				if (memcmp(res->ip, arp->spa, 4) == 0)
				{
					memcpy(&res->mac, &arp->sha, 6);
					cvSignal(&res->cond);
					break;
				};
			};
		};
	};
};

void etherAddResolution(struct NetIf_ *netif, int family, const void *ip, const MacAddress *mac)
{	
	MacResolution *res = getMacResolution(netif, family, (uint8_t*)ip);
	memcpy(&res->mac, mac, 6);
	cvSignal(&res->cond);
};

void onEtherFrame(NetIf *netif, const void *frame, size_t framelen, int flags)
{
	static uint8_t broadcastMac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
	static uint8_t multicastIPv6[2] = {0x33, 0x33};
	
	if (framelen < 64)
	{
		// drop
		return;
	};
	
	if (netif->ifconfig.type != IF_ETHERNET)
	{
		// not even ethernet
		return;
	};
	
	if ((flags & ETHER_IGNORE_CRC) == 0)
	{
		uint32_t crc = ether_checksum(frame, framelen);
		kprintf_debug("RECEIVED CRC: %a\n", (uint64_t)crc);
		// TODO: is the CRC correct???
	};
	
	
	// send the frame to capture sockets
	struct sockaddr_cap caddr;
	memset(&caddr, 0, sizeof(struct sockaddr_cap));
	caddr.scap_family = AF_CAPTURE;
	strcpy(caddr.scap_ifname, netif->name);
	onTransportPacket((struct sockaddr*)&caddr, (struct sockaddr*)&caddr, sizeof(struct sockaddr_cap),
		frame, framelen-4, IF_ETHERNET);	// without CRC
	
	size_t overheadSize = sizeof(EthernetHeader) + 4;
	EthernetHeader *head = (EthernetHeader*) frame;

	uint16_t type = __builtin_bswap16(head->type);
	if (memcmp(&head->dest, broadcastMac, 6) != 0)
	{
		// not broadcast, so make sure this frame is targetted at us
		if (memcmp(&head->dest, &netif->ifconfig.ethernet.mac, 6) != 0)
		{
			// it might also be IPv6 multicast
			if ((memcmp(&head->dest, multicastIPv6, 2) != 0) || (type != ETHER_TYPE_IPV6))
			{
				// destined to someone else; drop
				return;
			};
		};
	};
	
	switch (type)
	{
	case ETHER_TYPE_ARP:
		onARPPacket(netif, (ARPPacket*) frame);
		break;
	case ETHER_TYPE_IP:
	case ETHER_TYPE_IPV6:
		onPacket(netif, &head[1], framelen-overheadSize);
		break;
	};
};

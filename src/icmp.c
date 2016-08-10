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

#include <glidix/icmp.h>
#include <glidix/errno.h>
#include <glidix/string.h>
#include <glidix/socket.h>
#include <glidix/memory.h>
#include <glidix/console.h>
#include <glidix/ethernet.h>

static int sendErrorPacket4(struct sockaddr *src, const struct sockaddr *dest, int errnum, const void *packet, size_t packetlen)
{
	if (packetlen < 28)
	{
		// don't bother replying to extremely short packets made of less than 8 bytes
		// after the IPv4 header.
		return 0;
	};
	
	/**
	 * We must not respond to ICMP messages 3, 11 and 12 as they are error messages,
	 * and we should not reply to an error message with an error messages.
	 */
	IPHeader4 *header = (IPHeader4*) packet;
	if (header->proto == (uint8_t)IPPROTO_ICMP)
	{
		ErrorPacket4 *icmp = (ErrorPacket4*) &header[1];
		if ((icmp->type == 3) || (icmp->type == 11) || (icmp->type == 12))
		{
			return 0;
		};
	};
	
	if (packetlen > 28)
	{
		packetlen = 28;
	};
	
	ErrorPacket4 errpack;

	switch (errnum)
	{
	case ETIMEDOUT:
		errpack.type = 11;
		errpack.code = 0;
		break;
	case ENETUNREACH:
		errpack.type = 3;
		errpack.code = 0;
		break;
	case EHOSTUNREACH:
		errpack.type = 3;
		errpack.code = 1;
		break;
	default:
		return -EINVAL;
	};
	
	errpack.checksum = 0;
	errpack.zero = 0;
	memcpy(errpack.payload, packet, packetlen);
	
	errpack.checksum = ipv4_checksum(&errpack, sizeof(ErrorPacket4));
	
	return sendPacket(src, dest, &errpack, sizeof(ErrorPacket4), IPPROTO_ICMP | PKT_DONTFRAG, NT_SECS(1), NULL);
};

int sendErrorPacket(struct sockaddr *src, const struct sockaddr *dest, int errnum, const void *packet, size_t packetlen)
{
	if (dest->sa_family == AF_INET)
	{
		return sendErrorPacket4(src, dest, errnum, packet, packetlen);
	}
	else
	{
		// TODO: IPv6
		return 0;
	};
};

void onICMPPacket(const struct sockaddr *src, const struct sockaddr *dest, const void *msg, size_t size)
{
	// only accept ICMPv4 (IPPROTO_ICMP) packets from IPv4
	if (src->sa_family == AF_INET)
	{
		if (size < sizeof(PingPongPacket)) return;
	
		const PingPongPacket *ping = (const PingPongPacket*) msg;
		if ((ping->type == 8) && (ping->code == 0))
		{
			// this really is a ping
			if (ipv4_checksum(ping, size) == 0)
			{
				// actually valid
				PingPongPacket *pong = (PingPongPacket*) kmalloc(size);
				memcpy(pong, ping, size);
				pong->type = 0;
				pong->checksum = 0;
				pong->checksum = ipv4_checksum(pong, size);
			
				struct sockaddr_in srcPong;
				struct sockaddr_in dstPong;
				memcpy(&srcPong, dest, sizeof(struct sockaddr_in));
				memcpy(&dstPong, src, sizeof(struct sockaddr_in));
				sendPacket((struct sockaddr*)&srcPong, (struct sockaddr*)&dstPong,
					pong, size, IPPROTO_ICMP | PKT_DONTFRAG, 0, NULL);
				kfree(pong);
			};
		};
	};
};

static void handleSolicit(const struct sockaddr_in6 *src, NDPNeighborSolicit *sol)
{
	if (src->sin6_scope_id == 0) return;
	
	// we expect the "source MAC" option
	if ((sol->opt1 != 1) || (sol->len1 != 1))
	{
		return;
	};
	
	// queried address
	struct sockaddr_in6 qaddr;
	memset(&qaddr, 0, sizeof(struct sockaddr_in6));
	qaddr.sin6_family = AF_INET6;
	memcpy(&qaddr.sin6_addr, sol->addr, 16);
	qaddr.sin6_scope_id = src->sin6_scope_id;
	
	if (!isLocalAddr((struct sockaddr*)&qaddr))
	{
		// it's not a local address
		return;
	};
	
	struct sockaddr_in6 adv_src;
	struct sockaddr_in6 adv_dst;
	memcpy(&adv_dst, src, sizeof(struct sockaddr_in6));
	memset(&adv_src, 0, sizeof(struct sockaddr_in6));
	adv_src.sin6_family = AF_INET6;
	memcpy(&adv_src.sin6_addr, sol->addr, 16);
	adv_src.sin6_scope_id = src->sin6_scope_id;
	
	// find the Ethernet device in question, add a resolution of the sender's address,
	// and get our own MAC.
	acquireNetIfLock();
	MacAddress myMAC;
	NetIf *netif = netIfByScope(src->sin6_scope_id);
	if (netif == NULL)
	{
		releaseNetIfLock();
		return;
	};
	if (netif->ifconfig.type != IF_ETHERNET)
	{
		releaseNetIfLock();
		return;
	};
	etherAddResolution(netif, AF_INET6, &src->sin6_addr, (MacAddress*) sol->mac);
	memcpy(&myMAC, &netif->ifconfig.ethernet.mac, 6);
	releaseNetIfLock();
	
	PseudoHeaderICMPv6 *phead = (PseudoHeaderICMPv6*) kalloca(sizeof(PseudoHeaderICMPv6) + sizeof(NDPNeighborAdvert));
	memcpy(phead->src, &adv_src.sin6_addr, 16);
	memcpy(phead->dest, &adv_dst.sin6_addr, 16);
	phead->len = __builtin_bswap32((uint32_t) sizeof(NDPNeighborAdvert));
	memset(phead->zeroes, 0, 3);
	phead->proto = IPPROTO_ICMPV6;
	
	NDPNeighborAdvert *adv = (NDPNeighborAdvert*) phead->payload;
	adv->type = 136;
	adv->code = 0;
	adv->checksum = 0;
	adv->flags = NDP_NADV_SOLICIT;
	memcpy(adv->addr, &adv_src.sin6_addr, 16);
	adv->opt2 = 2;
	adv->len1 = 1;
	memcpy(adv->mac, &myMAC, 6);
	
	adv->checksum = ipv4_checksum(phead, sizeof(PseudoHeaderICMPv6) + sizeof(NDPNeighborAdvert));
	sendPacket((struct sockaddr*)&adv_src, (struct sockaddr*)&adv_dst, adv, sizeof(NDPNeighborAdvert),
			IPPROTO_ICMPV6 | PKT_DONTROUTE, NT_SECS(1), NULL);
};

static void handleAdvert(const struct sockaddr_in6 *src, NDPNeighborAdvert *adv)
{
	if (src->sin6_scope_id == 0) return;
	
	if ((adv->opt2 != 2) || (adv->len1 != 1))
	{
		return;
	};
	
	
	acquireNetIfLock();
	NetIf *netif = netIfByScope(src->sin6_scope_id);
	if (netif == NULL)
	{
		releaseNetIfLock();
		return;
	};
	if (netif->ifconfig.type != IF_ETHERNET)
	{
		releaseNetIfLock();
		return;
	};
	etherAddResolution(netif, AF_INET6, adv->addr, (MacAddress*) adv->mac);
	releaseNetIfLock();
};

void onICMP6Packet(const struct sockaddr *src, const struct sockaddr *dest, const void *msg, size_t size)
{
	// only accept ICMPv6 (IPPROTO_ICMPV6) packets from IPv6
	if (src->sa_family == AF_INET6)
	{
		if (size < sizeof(PingPong6Packet)) return;
		
		const struct sockaddr_in6 *insrc = (const struct sockaddr_in6*) src;
		const struct sockaddr_in6 *indst = (const struct sockaddr_in6*) dest;
		
		PseudoHeaderICMPv6 *phead = (PseudoHeaderICMPv6*) kmalloc(size + sizeof(PseudoHeaderICMPv6));
		memcpy(phead->src, &insrc->sin6_addr, 16);
		memcpy(phead->dest, &indst->sin6_addr, 16);
		phead->len = __builtin_bswap32((uint32_t)size);
		memset(phead->zeroes, 0, 3);
		phead->proto = IPPROTO_ICMPV6;
		memcpy(phead->payload, msg, size);
		
		if (ipv4_checksum(phead, size + sizeof(PseudoHeaderICMPv6)) == 0)
		{
			const PingPong6Packet *ping = (const PingPong6Packet*) msg;
			if ((ping->type == 128) && (ping->code == 0))
			{
				PingPong6Packet *pong = (PingPong6Packet*) phead->payload;
				pong->type = 129;
				pong->checksum = 0;
				pong->checksum = ipv4_checksum(phead, size + sizeof(PseudoHeaderICMPv6));
			
				struct sockaddr_in6 srcPong;
				struct sockaddr_in6 dstPong;
				memcpy(&srcPong, indst, sizeof(struct sockaddr_in6));
				memcpy(&dstPong, insrc, sizeof(struct sockaddr_in6));
			
				sendPacket((struct sockaddr*)&srcPong, (struct sockaddr*)&dstPong,
					pong, size, IPPROTO_ICMPV6 | PKT_DONTFRAG, 0, NULL);
			}
			else if ((ping->type == 135) && (ping->code == 0))
			{
				// neighbor solicitation; make sure it's the expected size (so that it has the source
				// MAC option only).
				if (size == sizeof(NDPNeighborSolicit))
				{
					NDPNeighborSolicit *sol = (NDPNeighborSolicit*) msg;
					handleSolicit((const struct sockaddr_in6*)src, sol);
				};
			}
			else if ((ping->type == 136) && (ping->code == 0))
			{
				// neighbor advertisment; make sure it's the expected size (so that it has the target
				// MAC option only).
				if (size == sizeof(NDPNeighborAdvert))
				{
					NDPNeighborAdvert *adv = (NDPNeighborAdvert*) msg;
					handleAdvert((const struct sockaddr_in6*)src, adv);
				};
			};
		};

		kfree(phead);
	};
};

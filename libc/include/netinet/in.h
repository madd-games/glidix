/*
	Glidix Runtime

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

#ifndef _NETINET_IN_H
#define _NETINET_IN_H

#include <sys/socket.h>
#include <string.h>

/* special IPv4 addresses */
#define	INADDR_ANY			0
#define	INADDR_BROADCAST		0xFFFFFFFF

/* IP protocols */
#define	IPPROTO_IP			0		/* Dummy protocol for TCP.  */
#define	IPPROTO_ICMP			1		/* Internet Control Message Protocol.  */
#define	IPPROTO_IGMP			2		/* Internet Group Management Protocol. */
#define	IPPROTO_IPIP			4		/* IPIP tunnels (older KA9Q tunnels use 94).  */
#define	IPPROTO_TCP			6		/* Transmission Control Protocol.  */
#define	IPPROTO_EGP			8		/* Exterior Gateway Protocol.  */
#define	IPPROTO_PUP			12		/* PUP protocol.  */
#define	IPPROTO_UDP			17		/* User Datagram Protocol.  */
#define	IPPROTO_IDP			22		/* XNS IDP protocol.  */
#define	IPPROTO_TP			29		/* SO Transport Protocol Class 4.  */
#define	IPPROTO_DCCP			33		/* Datagram Congestion Control Protocol.  */
#define	IPPROTO_IPV6			41		/* IPv6 header.  */
#define	IPPROTO_RSVP			46		/* Reservation Protocol.  */
#define	IPPROTO_GRE			47		/* General Routing Encapsulation.  */
#define	IPPROTO_ESP			50		/* encapsulating security payload.  */
#define	IPPROTO_AH			51		/* authentication header.  */
#define	IPPROTO_ICMPV6			58		/* Internet Control Message Protocol for IPv6 */
#define	IPPROTO_MTP			92		/* Multicast Transport Protocol.  */
#define	IPPROTO_BEETPH			94		/* IP option pseudo header for BEET.  */
#define	IPPROTO_ENCAP			98		/* Encapsulation Header.  */
#define	IPPROTO_PIM			103		/* Protocol Independent Multicast.  */
#define	IPPROTO_COMP			108		/* Compression Header Protocol.  */
#define	IPPROTO_SCTP			132		/* Stream Control Transmission Protocol.  */
#define	IPPROTO_UDPLITE			136		/* UDP-Lite protocol.  */
#define	IPPROTO_RAW			255		/* Raw IP packets.  */
#define	IPPROTO_MAX			256

/* byte order manipulation */
#define	ntohs				__builtin_bswap16
#define	htons				__builtin_bswap16
#define	ntohl				__builtin_bswap32
#define	htonl				__builtin_bswap32

/* macros to initialize "struct in6_addr" variable to special values */
#define	IN6ADDR_ANY_INIT		{{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}}
#define	IN6ADDR_LOOPBACK_INIT		{{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1}}

/* macros to test for special IPv6 addresses */
#define	IN6_IS_ADDR_UNSPECIFIED(a)	(memcmp((a), &in6addr_any, 16) == 0)
#define	IN6_IS_ADDR_LOOPBACK(a)		(memcmp((a), &in6addr_loopback, 16) == 0)
#define	IN6_IS_ADDR_MULTICAST(a)	((a)->s6_addr[0] == 0xFF)
#define	IN6_IS_ADDR_LINKLOCAL(a)	(((a)->s6_addr[0] == 0xFE) && ((a)->s6_addr[1] == 0x80))
#define	IN6_IS_ADDR_SITELOCAL(a)	((a)->s6_addr[0] == 0xFD)
#define	IN6_IS_ADDR_V4MAPPED(a)		(memcmp((a), &__prefix_v4mapped, 12) == 0)
#define	IN6_IS_ADDR_V4COMPAT(a)		(memcmp((a), &in6addr_any, 12) == 0)
#define	IN6_IS_ADDR_MC_NODELOCAL(a)	(((a)->s6_addr[0] == 0xFF) && ((a)->s6_addr[1] == 0x01))
#define	IN6_IS_ADDR_MC_LINKLOCAL(a)	(((a)->s6_addr[0] == 0xFF) && ((a)->s6_addr[1] == 0x02))
#define	IN6_IS_ADDR_MC_SITELOCAL(a)	(((a)->s6_addr[0] == 0xFF) && ((a)->s6_addr[1] == 0x05))
#define	IN6_IS_ADDR_MC_ORGLOCAL(a)	(((a)->s6_addr[0] == 0xFF) && ((a)->s6_addr[1] == 0x08))
#define IN6_IS_ADDR_MC_GLOBAL(a)	(((a)->s6_addr[0] == 0xFF) && ((a)->s6_addr[1] == 0x0E))

/* socket options for the IPPROTO_IPV6 level */
#define	IPV6_JOIN_GROUP			0
#define	IPV6_LEAVE_GROUP		1
#define	IPV6_MULTICAST_HOPS		2
#define	IPV6_UNICAST_HOPS		3
#define	IPV6_V6ONLY			4

/* synonyms with some of the above */
#define	IPV6_ADD_MEMBERSHIP		IPV6_JOIN_GROUP
#define	IPV6_DROP_MEMBERSHIP		IPV6_LEAVE_GROUP

struct in_addr
{
	in_addr_t			s_addr;
};

struct sockaddr_in
{
	sa_family_t			sin_family;		/* AF_INET */
	in_port_t			sin_port;
	struct in_addr			sin_addr;
	char				sin_zero[20];
};

struct in6_addr
{
	unsigned char			s6_addr[16];
};

struct sockaddr_in6
{
	sa_family_t			sin6_family;		/* AF_INET6 */
	in_port_t			sin6_port;
	uint32_t			sin6_flowinfo;
	struct in6_addr			sin6_addr;
	uint32_t			sin6_scope_id;
};

struct ipv6_mreq
{
	struct in6_addr			ipv6mr_multiaddr;
	uint32_t			ipv6mr_interface;
};

/* defined in src/netinet/in.c */
extern const struct in6_addr in6addr_any;
extern const struct in6_addr in6addr_loopback;
extern const struct in6_addr __prefix_v4mapped;

#endif

/*
	Glidix kernel

	Copyright (c) 2014-2015, Madd Games.
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

#ifndef __glidix_netif_h
#define __glidix_netif_h

#include <glidix/common.h>

/**
 * This header file defines structures and routines that describe glidix network interfaces.
 */

/* address families */
#define	AF_UNSPEC			0
#define	AF_LOCAL			1
#define	AF_UNIX				AF_LOCAL
#define	AF_INET				2
#define	AF_INET6			3

/* IP protocols */
#define	IPPROTO_IP	0		/* Dummy protocol for TCP.  */
#define	IPPROTO_ICMP	1		/* Internet Control Message Protocol.  */
#define	IPPROTO_IGMP	2		/* Internet Group Management Protocol. */
#define	IPPROTO_IPIP	4		/* IPIP tunnels (older KA9Q tunnels use 94).  */
#define	IPPROTO_TCP	6		/* Transmission Control Protocol.  */
#define	IPPROTO_EGP	8		/* Exterior Gateway Protocol.  */
#define	IPPROTO_PUP	12		/* PUP protocol.  */
#define	IPPROTO_UDP	17		/* User Datagram Protocol.  */
#define	IPPROTO_IDP	22		/* XNS IDP protocol.  */
#define	IPPROTO_TP	29		/* SO Transport Protocol Class 4.  */
#define	IPPROTO_DCCP	33		/* Datagram Congestion Control Protocol.  */
#define	IPPROTO_IPV6	41		/* IPv6 header.  */
#define	IPPROTO_RSVP	46		/* Reservation Protocol.  */
#define	IPPROTO_GRE	47		/* General Routing Encapsulation.  */
#define	IPPROTO_ESP	50		/* encapsulating security payload.  */
#define	IPPROTO_AH	51		/* authentication header.  */
#define	IPPROTO_MTP	92		/* Multicast Transport Protocol.  */
#define	IPPROTO_BEETPH	94		/* IP option pseudo header for BEET.  */
#define	IPPROTO_ENCAP	98		/* Encapsulation Header.  */
#define	IPPROTO_PIM	103		/* Protocol Independent Multicast.  */
#define	IPPROTO_COMP	108		/* Compression Header Protocol.  */
#define	IPPROTO_SCTP	132		/* Stream Control Transmission Protocol.  */
#define	IPPROTO_UDPLITE	136		/* UDP-Lite protocol.  */
#define	IPPROTO_RAW	255		/* Raw IP packets.  */
#define	IPPROTO_MAX	256

struct sockaddr
{
	uint16_t			sa_family;		/* AF_* */
	char				sa_data[26];
};

/**
 * AF_INET (IPv4)
 */
struct in_addr
{
	uint32_t			s_addr;
};

struct sockaddr_in
{
	uint16_t			sin_family;		/* AF_INET */
	uint16_t			sin_port;
	struct in_addr			sin_addr;
	/* without the padding, sizeof(sockaddr_in)=8. Pad to 28 by adding 20 */
	char				sin_zero[20];
};

typedef struct
{
	uint16_t			optInfo;
	char				optData[];
} PACKED IPOptions4;

/**
 * Please note that this structure is in network byte order! Use the utility functions to
 * read/write such structures.
 */
typedef struct
{
	uint8_t				versionIHL;
	uint8_t				dscpECN;
	uint16_t			len;
	uint16_t			id;
	uint16_t			flagsFragOff;
	uint8_t				hop;
	uint8_t				proto;			/* IPPROTO_* */
	uint16_t			checksum;
	uint32_t			saddr;
	uint32_t			daddr;
	IPOptions4			options[];
} PACKED IPHeader4;

/**
 * AF_INET6 (IPv6)
 */
struct in6_addr
{
	unsigned char			s6_addr[16];
};

/* sizeof(sockaddr_in6) = 28 */
struct sockaddr_in6
{
	uint16_t			sin6_family;		/* AF_INET6 */
	uint16_t			sin6_port;
	uint32_t			sin6_flowinfo;
	struct in6_addr			sin6_addr;
	uint32_t			sin6_scope_id;
};

typedef struct
{
	uint32_t			versionClassFlow;
	uint16_t			payloadLen;
	uint8_t				nextHeader;
	uint8_t				hop;
	uint8_t				saddr[16];
	uint8_t				daddr[16];
} PACKED IPHeader6;

/**
 * IPv4 handling functions.
 * PacketInfo4 is a "decomposed" version of an IPv4 packet that can be read/written directly.
 * ipv4_info2header() and ipv4_header2info() can convert back-and-forth between the PacketInfo4
 * format and the network header format (IPHeader4).
 * ipv4_header2info() returns 0 on success, -1 if there is a problem with the header.
 */
typedef struct
{
	struct in_addr			saddr;			/* source address */
	struct in_addr			daddr;			/* destination address */
	int				proto;			/* protocol (IPPROTO_* unless invalid) */
	uint64_t			dataOffset;		/* offset to data (returned by ipv4_header2info) */
	uint64_t			size;			/* size of the payload */
	uint64_t			id;			/* datagram ID for fragmented datagrams */
	uint64_t			fragOff;		/* offset into the fragment that the payload should be loaded into */
	uint8_t				hop;			/* hop count, discard when it reaches 0 */
	int				moreFrags;		/* 1 if more fragments incoming, 0 otherwise (returned by ipv4_header2info) */
} PacketInfo4;

void	ipv4_info2header(PacketInfo4 *info, IPHeader4 *head);
int	ipv4_header2info(IPHeader4 *head, PacketInfo4 *info);

/**
 * IPv6 handling functions. Works the same way as the IPv4 ones.
 * TODO: Additional IPv6 headers not yet supported.
 */
typedef struct
{
	struct in6_addr			saddr;			/* source address */
	struct in6_addr			daddr;			/* destination address */
	int				proto;			/* protocol (IPPROTO_* unless invalid) */
	uint64_t			dataOffset;		/* offset to the data (returned by ipv6_header2info) */
	uint64_t			size;			/* size of the payload */
	uint8_t				hop;			/* hop count, discard when it reaches 0 */
	uint32_t			flowinfo;
} PacketInfo6;

void	ipv6_info2header(PacketInfo6 *info, IPHeader6 *head);
int	ipv6_header2info(IPHeader6 *head, PacketInfo6 *info);

/**
 * Specifies a scope of IPv4 addresses available to an interface.
 */
typedef struct
{
	/**
	 * The default address (this is used for example by connect() without an explicit bind()).
	 */
	struct in_addr			addr;
	
	/**
	 * The subnet mask. All addresses in the subnet of 'addr' with this mask are available for use
	 * by bind() on the given interface.
	 */
	struct in_addr			mask;
} IPNetIfAddr4;

/**
 * Specifies an IPv4 route, used when sending a packet.
 */
typedef struct
{
	/**
	 * Destination network address.
	 */
	struct in_addr			dest;
	
	/**
	 * Destination network mask.
	 */
	struct in_addr			mask;
	
	/**
	 * Gateway. If this is "0.0.0.0", then no gateway is used, and instead glidix will
	 * attempt to transmit directly to the requested IP address.
	 *
	 * If, however, a gateway is specified, then IP packets directed to this network will
	 * be sent to the machine which identifies with this IP addresses.
	 */
	struct in_addr			gateway;
} IPRoute4;

/**
 * Encapsulation of all IPv4 options.
 */
typedef struct
{
	IPNetIfAddr4*			addrs;
	int				numAddrs;
	IPRoute4*			routes;
	int				numRoutes;
	struct in_addr*			dnsServers;
	int				numDNSServers;
} IPConfig4;

/**
 * Same for IPv6.
 */
typedef struct
{
	struct in6_addr			addr;
	struct in6_addr			mask;
} IPNetIfAddr6;

typedef struct
{
	struct in6_addr			dest;
	struct in6_addr			mask;
	struct in6_addr			gateway;
} IPRoute6;

typedef struct
{
	IPNetIfAddr6*			addrs;
	int				numAddrs;
	IPRoute6*			routes;
	int				numRoutes;
	struct in6_addr*		dnsServers;
	int				numDNSServers;
} IPConfig6;

/**
 * Describes a network interface.
 */
typedef struct NetIf_
{
	/**
	 * Driver-specific private data.
	 */
	void*				drvdata;
	
	/**
	 * Name of the interface.
	 */
	char				name[16];

	/**
	 * IPv4 and IPv6 configuration of this interface.
	 */
	IPConfig4			ipv4;
	IPConfig6			ipv6;
	
	/**
	 * Interface statistics. Number of successfully transmitted and recevied packets,
	 * number of dropped packets and number of errors.
	 */
	int				numTrans;
	int				numRecv;
	int				numDropped;
	int				numErrors;
	
	/**
	 * Send a packet through this link. 'addr' and 'addrlen' are the IP address and its size,
	 * respectively (this will be 4 for IPv4 and 16 for IPv6). 'packet' and 'packetlen' are a
	 * pointer to the packet, and its size, respectively. The packet is an IP packet, which the
	 * interface may need to encapsulate in a link layer packet.
	 *
	 * The interface shall look at its ARP cache to determine the target MAC address, or, if not
	 * found, run another ARP discovery. If the IP address still cannot be resolved, a failure is
	 * reported. If the interface does not use the ARP protocol, then it shall use whatever other
	 * protocol is necessary to understand the address.
	 *
	 * Upon success, numTrans shall be incremented. Upon failure, numErrors shall be incremented.
	 */
	void (*send)(struct NetIf_ *netif, const void *addr, size_t addrlen, const void *packet, size_t packetlen);
	
	struct NetIf_ *prev;
	struct NetIf_ *next;
} NetIf;

/**
 * Initialize the network interface.
 */
void initNetIf();

/**
 * This function is called by network drivers when a packet is received. The arguments specify the source interface
 * (from which configuration such as responsiveness to ping requests is read), the pointer to an IP packet, and its
 * length (including the payload).
 */
void onPacket(NetIf *netif, const void *packet, size_t packetlen);

typedef struct
{
	char				ifname[16];
	struct in_addr			dest;
	struct in_addr			mask;
	struct in_addr			gateway;
	uint64_t			flags;
} in_route;

typedef struct
{
	char				ifname[16];
	struct in6_addr			dest;
	struct in6_addr			mask;
	struct in6_addr			gateway;
	uint64_t			flags;
} in6_route;

/**
 * A system call that returns a file descriptor from which one may read the routing table for a particular address family.
 * Each read() call returns a single entry. If the given read size cannot encompass the full structure, only part of the
 * structure is returned. Only the families AF_INET and AF_INET6 are currently supported, and each returns a different
 * structure (_glidix_in_route for AF_INET and _glidix_in6_route for AF_INET6). Returns a positive file descriptor on
 * success, or -1 on error and sets errno to EINVAL.
 */
int sysRouteTable(uint64_t family);

/**
 * Send a packet over the network, selecting the interface based on the specified address (which must be AF_INET or AF_INET6).
 * Port numbers are ignored. The address may be swapped for a gateway address of necessary. Returns 0 on success; -1 if the
 * packet could not be delivered.
 */
int sendPacket(const struct sockaddr *addr, const void *packet, size_t len);

/**
 * Load an IPv4 or IPv6 address which is the default source address for the interface that "dest" goes to.
 */
void getDefaultAddr4(struct in_addr *src, const struct in_addr *dest);
void getDefaultAddr6(struct in6_addr *src, const struct in6_addr *dest);

uint16_t ipv4_checksum(void *data, size_t size);

#endif

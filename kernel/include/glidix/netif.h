/*
	Glidix kernel

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

#ifndef __glidix_netif_h
#define __glidix_netif_h

#include <glidix/common.h>
#include <glidix/ethernet.h>

/**
 * This header file defines structures and routines that describe glidix network interfaces.
 */

/* address families */
#define	AF_UNSPEC			0
#define	AF_LOCAL			1
#define	AF_UNIX				AF_LOCAL
#define	AF_INET				2
#define	AF_INET6			3
#define	AF_CAPTURE			4

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

/* sendPacket() flags (start with bit 8 because the bottom 8 bits are the protocol number) */
#define	PKT_HDRINC			(1 << 8)
#define	PKT_DONTROUTE			(1 << 9)
#define	PKT_DONTFRAG			(1 << 10)
#define	PKT_MASK			(PKT_HDRINC|PKT_DONTROUTE|PKT_DONTFRAG)

/* types of interfaces */
#define	IF_LOOPBACK			0		/* loopback interface (localhost) */
#define	IF_ETHERNET			1		/* ethernet controller */
#define	IF_TUNNEL			2		/* software tunnel */

/* system-defined address/route domains; numbers 0-15 are reserved for the system; 16+ may be used by user */
#define	DOM_GLOBAL			0		/* global (internet) */
#define	DOM_LINK			1		/* link-local (LAN only) */
#define	DOM_LOOPBACK			2		/* loopback (host only) */
#define	DOM_SITE			3		/* site-local (organization scope only) */
#define	DOM_MULTICAST			4		/* multicast (used for addresses and NEVER routes) */
#define	DOM_NODEFAULT			5		/* non-default address (never selected for any route) */

/**
 * Default hop limits for unicast and multicast packets.
 */
#define	DEFAULT_UNICAST_HOPS		64
#define	DEFAULT_MULTICAST_HOPS		1

/**
 * The size of an internet-domain socket address (struct sockaddr_in and struct sockaddr_in6).
 * This can be fixed at 28 because it will not change due to ABI compatiblity. This is here so
 * that bind(), connect(), etc interfaces on internet sockets can check 'addrlen' against this,
 * without making it look like it's checking for IPv4 or IPv6 specifically; previously,
 * sizeof(struct sockaddr) was used for this, but now it is expanded above 28 bytes because of
 * UNIX sockets and stuff, so we have this convenient macro instead.
 */
#define	INET_SOCKADDR_LEN		28

/**
 * Type-specific network interface options.
 */
struct NetIf_;
typedef union
{
	/**
	 * Type of connection - this is one of the IF_* constants. It also determines which
	 * of the structures inside this union is valid.
	 */
	int				type;
	
	/**
	 * IF_LOOPBACK has no options (reserved).
	 */
	struct
	{
		int			type;			// IF_LOOPBACK
	} loopback;
	
	/**
	 * IF_ETHERNET (Ethernet) options.
	 */
	struct
	{
		int			type;			// IF_ETHERNET
		
		/**
		 * The interface's MAC address (used as source address of transmitted frames).
		 */
		MacAddress		mac;
		
		/**
		 * Send an ethernet frame through the interface. 'frame' points to the ethernet frame,
		 * which already has an EtherHeader at the front. 'framelen' is the length, in bytes,
		 * of that frame. The destination and source MAC addresses are in the EthernetHeader.
		 */
		void (*send)(struct NetIf_ *netif, const void *frame, size_t framelen);
		
		/**
		 * The head of the currently-resolved address list, and the list lock.
		 */
		MacResolution*		res;
		Semaphore		resLock;
	} ethernet;
	
	/**
	 * IF_TUNNEL (Software tunnel) options.
	 */
	struct
	{
		int			type;			// IF_TUNNEL
	} tunnel;
} NetIfConfig;

struct sockaddr
{
	uint16_t			sa_family;		/* AF_* */
	char				sa_data[108];
};

/**
 * AF_LOCAL/AF_UNIX (local sockets)
 */
struct sockaddr_un
{
	uint16_t			sun_family;		/* AF_UNIX/AF_LOCAL */
	char				sun_path[108];
};

/**
 * AF_CAPTURE (capture sockets)
 */
struct sockaddr_cap
{
	uint16_t			scap_family;		/* AF_CAPTURE */
	char				scap_ifname[16];
	int				scap_proto_flags;
	char				scap_pad[4];
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

typedef struct
{
	uint8_t				nextHeader;
	uint8_t				zero;
	uint16_t			fragResM;
	uint32_t			id;
} PACKED IPFragmentHeader6;

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
	uint64_t			fragOff;		/* offset into the fragment that the payload should be loaded into */
	uint32_t			id;			/* ID for the particular datagram if it is fragmented */
	int				moreFrags;		/* 1 if more fragments incoming, 0 otherwise */
} PacketInfo6;

void	ipv6_info2header(PacketInfo6 *info, IPHeader6 *head);
int	ipv6_header2info(IPHeader6 *head, PacketInfo6 *info);

/**
 * Specifies IPv4 addresses available to an interface.
 */
typedef struct
{
	/**
	 * The default address (this is used for example by connect() without an explicit bind()).
	 */
	struct in_addr			addr;
	
	/**
	 * The subnet mask.
	 */
	struct in_addr			mask;
	
	/**
	 * Domain of the address.
	 */
	int				domain;
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
	
	/**
	 * Domain of the route.
	 */
	int				domain;
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
} IPConfig4;

/**
 * Same for IPv6.
 */
typedef struct
{
	struct in6_addr			addr;
	struct in6_addr			mask;
	int				domain;
} IPNetIfAddr6;

typedef struct
{
	struct in6_addr			dest;
	struct in6_addr			mask;
	struct in6_addr			gateway;
	int				domain;
} IPRoute6;

typedef struct
{
	IPNetIfAddr6*			addrs;
	int				numAddrs;
	IPRoute6*			routes;
	int				numRoutes;
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
	 * Interface configuration (described above, near "connection types").
	 */
	NetIfConfig			ifconfig;

	/**
	 * Maximum Transmission Unit (MTU). This is the maximum possible packet size
	 * passed to the driver's send() implementation. If bigger packets are to be
	 * transferred, they are fragmented.
	 */
	uint64_t			mtu;

	/**
	 * Interface statistics. Number of successfully transmitted and received packets,
	 * number of dropped packets and number of errors. Should be updated using atomic
	 * operations.
	 */
	int				numTrans;
	int				numRecv;
	int				numDropped;
	int				numErrors;
	
	/**
	 * Scope ID for this interface. This is used for IPv6 routing.
	 */
	uint32_t			scopeID;
	
	struct NetIf_ *prev;
	struct NetIf_ *next;
} NetIf;

typedef struct
{
	char				ifname[16];
	int				numTrans;
	int				numRecv;
	int				numDropped;
	int				numErrors;
	int				numAddrs4;
	int				numAddrs6;
	/* current offset: 6*4 + 16 = 24 + 16 = 40 */
	
	NetIfConfig			ifconfig;
	
	/**
	 * Put the remaining fields at a constant offset of 1024 bytes.
	 */
	char				resv_[1024-(40+sizeof(NetIfConfig))];
	
	uint32_t			scopeID;
} NetStat;

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
	int				domain;
	uint64_t			flags;
} in_route;

typedef struct
{
	char				ifname[16];
	struct in6_addr			dest;
	struct in6_addr			mask;
	struct in6_addr			gateway;
	int				domain;
	uint64_t			flags;
} in6_route;

typedef struct
{
	char				ifname[16];
	char				data[3*16];
	uint64_t			flags;
} gen_route;

/**
 * A system call that returns a file descriptor from which one may read the routing table for a particular address family.
 * Each read() call returns a single entry. If the given read size cannot encompass the full structure, only part of the
 * structure is returned. Only the families AF_INET and AF_INET6 are currently supported, and each returns a different
 * structure (_glidix_in_route for AF_INET and _glidix_in6_route for AF_INET6). Returns a positive file descriptor on
 * success, or -1 on error and sets errno to EINVAL.
 */
int sysRouteTable(uint64_t family);

int sendPacket(struct sockaddr *src, const struct sockaddr *dest, const void *packet, size_t packetlen, int flags,
		uint64_t nanotimeout, const char *ifname);

/**
 * A better version of sendPacket() basically.
 */
int sendPacketEx(struct sockaddr *src, const struct sockaddr *dest, const void *packet, size_t packetlen,
			int proto, uint64_t *sockopts, const char *ifname);

/**
 * Load an IPv4 or IPv6 address which is the default source address for the interface that "dest" goes to.
 */
void getDefaultAddr4(struct in_addr *src, const struct in_addr *dest);
void getDefaultAddr6(struct in6_addr *src, const struct in6_addr *dest);

/**
 * Calculate the "internet checksum" of the data specified.
 */
uint16_t ipv4_checksum(const void *data, size_t size);

/**
 * System call to add a new route. Only 'root' is allowed to do this (effective UID must be 0).
 * The family is either AF_INET or AF_INET6. 'route' is a pointer to an address structure, which
 * is cast to either '_glidix_in_route' or '_glidix_in6_route' depending on the family. The route
 * specifies the network address, mask, and optionally a gateway (a zero gateway means the lack
 * thereof). 'pos' indicates the position within the list to place the entry (e.g. pos=0 means it
 * comes to the very beginning). A 'pos' of -1 indicates addition to the end.
 * Returns 0 on success, -1 on error, setting errno appropriately:
 * - ENOENT	The specified network interface does not exist.
 * - EINVAL	Invalid address family.
 * - EACCES	Access denied (you are not root).
 */
int route_add(int family, int pos, gen_route *route);

/**
 * System call to clear an interface's routing table for a given protocol.
 */
int route_clear(int family, const char *ifname);

/**
 * Return the statistics for a particular network interface. The statistics include the number of
 * successfully transmitted and received packets, as well as dropped and errored packets.
 * The 'buffer' points to a NetStat ('_glidix_netstat') structure to be filled with information,
 * and 'size' is the maximum number of bytes that structure can hold. This function returns the
 * actual number of bytes loaded into the buffer on success, or -1 on error, setting errno appropriately:
 * - ENOENT	The specified network interface does not exist.
 */
ssize_t netconf_stat(const char *ifname, NetStat *buffer, size_t size);

/**
 * Like netconf_stat() but takes an index instead of a name. Fails with ENOENT if the index is out of bounds.
 */
ssize_t netconf_statidx(unsigned int index, NetStat *buffer, size_t size);

/**
 * Return the list of addresses for a particular network interface with the name 'ifname'. 'family' specifies
 * which address protocol shall be queried: AF_INET or AF_INET6. 'buffer' points to an area of size 'bufsiz'
 * which is an array of either IPNetIfAddr4 ('_glidix_ifaddr4') or IPNetIfAddr6 ('_glidix_ifaddr6'). The number
 * of addresses for each protocol is given by netconf_stat(). Returns the actual size of the data returned on
 * success, or -1 on error, setting errno appropriately:
 *  - ENOENT	The specified network interface does not exist.
 *  - EINVAL	The 'family' is invalid (neither AF_INET nor AF_INET6).
 */
ssize_t netconf_getaddrs(const char *ifname, int family, void *buffer, size_t bufsiz);

/**
 * These 2 functions are used by network device drivers. The first one adds a new network interface that a driver has detected,
 * and returns a pointer to a new NetIf structure that refers to it. This new structure can then be passed to DeleteNetworkInterface()
 * when the driver is being unloaded. 2 parameters are needed to define such an interface: driver-specific data (assigned to the
 * NetIf structure directly) and an interface configuration, as described in the NetIf structure. Details of the
 * creation of the new interface depends on the value of ifconfig->type:
 *
 *	IF_LOOPBACK
 *		Results in an error (NULL is returned); you cannot add another loopback interface.
 *
 *	IF_ETHERNET
 *		The new interface is assigned the name "ethX", where X is a number, indicating it to be an ethernet connection.
 *		You are not allowed to rename the interface. The given MAC address and MTU must be valid. The interface is initialized
 *		with no routing information or addresses, but depending on the network settings, the network manager may run a DHCP
 *		session on the interface to get addresses.
 *
 *	IF_TUNNEL
 *		The new interface is assigned the name "tnlX", where X is a number, indicating it be a software tunnel. No DHCP or
 *		other automatic configuration methods will be tried by the system. The tunneling software must configure the tunnel
 *		in its own way.
 *
 * Upon error, NULL is returned; otherwise, the new interface handle is returned.
 */
NetIf *CreateNetworkInterface(void *drvdata, NetIfConfig *ifconfig);
void DeleteNetworkInterface(NetIf *netif);

/**
 * Returns true if the specified address belongs to one of the interfaces on this system.
 */
int isLocalAddr(const struct sockaddr *addr);

/**
 * Userspace function to configure an interface's address(es).
 * @param family The address family (AF_INET or AF_INET6).
 * @param ifname Name of the interface to configure.
 * @param buffer A buffer containing an array of IPNetIfAddr4 or IPNetIfAddr6 structures.
 * @param size   Size of the buffer, used to extrapolate the number of addresses.
 * @return 0 on success, -1 on error, 'errno' set appropriately:
 *
 * ENOENT	The named interface does not exist.
 * EINVAL	The family is invalid or 'size' is not divisible by the size of an address structure.
 * EPERM	You are not root.
 */
int netconf_addr(int family, const char *ifname, const void *buffer, size_t size);

/**
 * Get a network interface by scope ID. Only call this when the network interface lock is acquired.
 * Returns NULL on failure.
 */
NetIf *netIfByScope(uint32_t scopeID);

int isMappedAddress46(const struct sockaddr *addr);
void remapAddress64(const struct sockaddr_in6 *addr, struct sockaddr_in *result);
void remapAddress46(const struct sockaddr_in *addr, struct sockaddr_in6 *result);

void releaseNetIfLock();
void acquireNetIfLock();
#endif

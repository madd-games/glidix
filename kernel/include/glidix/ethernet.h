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

#ifndef __glidix_ethernet_h
#define __glidix_ethernet_h

#include <glidix/common.h>
#include <glidix/condvar.h>

/**
 * Ethernet stuff. This is used by <glidix/netif.h>.
 */

#define	ETHER_TYPE_ARP			0x0806
#define	ETHER_TYPE_IP			0x0800
#define	ETHER_TYPE_IPV6			0x86DD

/**
 * Flags for onEtherFrame().
 */
#define	ETHER_IGNORE_CRC		(1 << 0)

/**
 * Flags for NDP Router Advertisments.
 */
#define	NDP_NADV_ROUTER			(1 << 7)
#define	NDP_NADV_SOLICIT		(1 << 6)
#define	NDP_NADV_OVERRIDE		(1 << 5)

struct NetIf_;
struct sockaddr;

typedef struct
{
	uint8_t				addr[6];
} MacAddress;

typedef struct
{
	MacAddress			dest;
	MacAddress			src;
	uint16_t			type;
} PACKED EthernetHeader;

typedef struct
{
	EthernetHeader			header;
	uint16_t			htype;
	uint16_t			ptype;
	uint8_t				hlen;
	uint8_t				plen;
	uint16_t			oper;
	MacAddress			sha;
	uint8_t				spa[4];
	MacAddress			tha;
	uint8_t				tpa[4];
	uint8_t				padding[18];		// brings the size up to 64 bytes
	uint32_t			crc;
} PACKED ARPPacket;

typedef struct
{
	uint8_t				type;			// 135 - neighbor solicitation
	uint8_t				code;			// 0
	uint16_t			checksum;
	uint32_t			resv;			// set to 0
	uint8_t				addr[16];		// unicast address of target
	
	/* source link-layer address option */
	uint8_t				opt1;			// set this 1
	uint8_t				len1;			// set this to 1 (= size is 8 bytes)
	uint8_t				mac[6];			// our MAC address
} NDPNeighborSolicit;

typedef struct
{
	uint8_t				type;			// 136 - neighbor advertisment
	uint8_t				code;			// 0
	uint16_t			checksum;
	uint32_t			flags;
	uint8_t				addr[16];		// my IPv6 address
	
	/* my MAC address option */
	uint8_t				opt2;			// set this to 2
	uint8_t				len1;			// set this to 1 (=size is 8 bytes)
	uint8_t				mac[6];
} NDPNeighborAdvert;

/**
 * Describes a resolution of an IP address to a MAC address.
 */
typedef struct MacResolution_
{
	/**
	 * Which version of IP this address is for - AF_INET for IPv4 or AF_INET6 for IPv6.
	 */
	int				family;
	
	/**
	 * The IP address; for IPv4, only the first 4 bytes are used.
	 */
	uint8_t				ip[16];
	
	/**
	 * The physical (MAC) address. All zeroes means the address is unresolved.
	 */
	MacAddress			mac;
	
	/**
	 * This condition variable is signalled once this resolution actually becomes valid.
	 */
	CondVar				cond;

	struct MacResolution_*		next;
} MacResolution;

/**
 * Calculate the Ethernet CRC32.
 */
uint32_t ether_checksum(const void *data, size_t size);

/**
 * Send an IP packet through an Ethernet device. This may use ARP or NDP to resolve to a MAC address, and
 * the time allowed for that is limited by 'nanotimeout'.
 */
int sendPacketToEthernet(struct NetIf_ *netif, const struct sockaddr *gateway, const void *packet, size_t packetlen, uint64_t nanotimeout);

/**
 * Called by drivers upon receiving an Ethernet frame.
 * Flags:
 *	ETHER_IGNORE_CRC - This frame had its CRC checked by the hardware and might not be valid
 *	                   in the received buffer.
 */
void onEtherFrame(struct NetIf_ *netif, const void *frame, size_t framelen, int flags);

/**
 * Add an address resolution to an Ethernet device.
 */
void etherAddResolution(struct NetIf_ *netif, int family, const void *ip, const MacAddress *mac);

#endif

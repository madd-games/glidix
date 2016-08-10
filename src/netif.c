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

#include <glidix/netif.h>
#include <glidix/string.h>
#include <glidix/semaphore.h>
#include <glidix/vfs.h>
#include <glidix/sched.h>
#include <glidix/errno.h>
#include <glidix/qfile.h>
#include <glidix/memory.h>
#include <glidix/socket.h>
#include <glidix/console.h>
#include <glidix/mutex.h>

static Mutex iflistLock;
static NetIf iflist;

uint16_t ipv4_checksum(const void* vdata,size_t length)
{
	// Cast the data pointer to one that can be indexed.
	const char* data=(const char*)vdata;

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
}

void ipv4_info2header(PacketInfo4 *info, IPHeader4 *head)
{
	uint16_t flags = 0;
	if (info->moreFrags)
	{
		flags |= (1 << 13);
	};
	
	head->versionIHL = 0x45;				// IPv4, 5 dwords (20 bytes) header
	head->dscpECN = 0;					// XXX ??
	head->len = __builtin_bswap16(20 + info->size);
	head->id = info->id;
	head->flagsFragOff = __builtin_bswap16((uint16_t) info->fragOff | flags);
	head->hop = info->hop;
	head->proto = (uint8_t) info->proto;
	head->checksum = 0;
	head->saddr = *((uint32_t*)&info->saddr);
	head->daddr = *((uint32_t*)&info->daddr);
	head->checksum = ipv4_checksum(head, 20);
};

int ipv4_header2info(IPHeader4 *head, PacketInfo4 *info)
{
	if (head->versionIHL != 0x45)
	{
		// we do not handle extra options or non-IPv4 packets here
		return -1;
	};
	
	if (ipv4_checksum(head, 20) != 0)
	{
		// invalid checksum
		return -1;
	};

	info->size = __builtin_bswap16(head->len) - 20;
	info->id = head->id;
	uint16_t flagsFragOff = __builtin_bswap16(head->flagsFragOff);
	uint16_t flags = (flagsFragOff >> 13) & 0x7;
	info->fragOff = flagsFragOff & ((1 << 13)-1);
	info->moreFrags = flags & 1;
	info->dataOffset = 20;

	info->hop = head->hop;
	info->proto = (int) head->proto;

	memcpy(&info->saddr, &head->saddr, 4);
	memcpy(&info->daddr, &head->daddr, 4);
	return 0;
};

void ipv6_info2header(PacketInfo6 *info, IPHeader6 *head)
{
	head->versionClassFlow = 0x60 | (info->flowinfo & ~0xF);		/* low-order 6 gets loaded into lowest byte */
	head->payloadLen = __builtin_bswap16((uint16_t) info->size);
	head->nextHeader = (uint8_t) info->proto;
	head->hop = info->hop;
	memcpy(&head->saddr, &info->saddr, 16);
	memcpy(&head->daddr, &info->daddr, 16);
	
	if ((info->fragOff > 0) || (info->moreFrags))
	{
		// sending a fragmented packet; embed fragment header
		head->nextHeader = 44;
		IPFragmentHeader6 *fraghead = (IPFragmentHeader6*) &head[1];
		fraghead->nextHeader = (uint8_t) info->proto;
		fraghead->zero = 0;
		uint16_t flags = 0;
		if (info->moreFrags) flags |= 1;
		fraghead->fragResM = __builtin_bswap16(info->fragOff | flags);	// fragOff being divisible by 8
		fraghead->id = info->id;
		head->payloadLen = __builtin_bswap16(info->size+sizeof(IPFragmentHeader6));
	}; 
};

int ipv6_header2info(IPHeader6 *head, PacketInfo6 *info)
{
	if ((head->versionClassFlow & 0xF0) != 0x60)
	{
		// not IPv6
		return -1;
	};
	
	info->size = __builtin_bswap16(head->payloadLen);
	info->proto = (int) head->nextHeader;
	info->hop = head->hop;
	info->dataOffset = 40;
	info->flowinfo = head->versionClassFlow & ~0xF;
	info->fragOff = 0;
	info->moreFrags = 0;
	
	memcpy(&info->saddr, &head->saddr, 16);
	memcpy(&info->daddr, &head->daddr, 16);
	
	if (head->nextHeader == 44)
	{
		// fragment header
		IPFragmentHeader6 *fraghead = (IPFragmentHeader6*) &head[1];
		info->proto = (int) fraghead->nextHeader;
		uint16_t fragResM = __builtin_bswap16(fraghead->fragResM);
		info->fragOff = fragResM >> 3;
		info->moreFrags = fragResM & 1;
		info->id = fraghead->id;
		info->dataOffset += 8;
		info->size -= sizeof(IPFragmentHeader6);
	};

	return 0;
};

static Semaphore loLock;
static Semaphore loPacketCounter;

typedef struct lopacket_
{
	struct lopacket_* next;
	size_t size;
	char data[];
} lopacket;

static lopacket *loQueue = NULL;

//static void loopbackSend(NetIf *netif, const void *addr, size_t addrlen, const void *packet, size_t packetlen)
static void loopbackSend(NetIf *netif, const void *packet, size_t packetlen)
{
	// send the frame to capture sockets
	struct sockaddr_cap caddr;
	memset(&caddr, 0, sizeof(struct sockaddr_cap));
	caddr.scap_family = AF_CAPTURE;
	strcpy(caddr.scap_ifname, netif->name);
	onTransportPacket((struct sockaddr*)&caddr, (struct sockaddr*)&caddr, sizeof(struct sockaddr_cap),
		packet, packetlen, IF_LOOPBACK);
		
	__sync_fetch_and_add(&netif->numTrans, 1);
	lopacket *newPacket = (lopacket*) kmalloc(sizeof(lopacket) + packetlen);
	newPacket->next = NULL;
	newPacket->size = packetlen;
	memcpy(newPacket->data, packet, packetlen);
	
	semWait(&loLock);
	if (loQueue == NULL)
	{
		loQueue = newPacket;
	}
	else
	{
		lopacket *queue = loQueue;
		while (queue->next != NULL) queue = queue->next;
		queue->next = newPacket;
	};
	semSignal(&loLock);
	
	semSignal(&loPacketCounter);
};

static void loopbackThread(void *ignore)
{
	(void)ignore;
	while (1)
	{
		semWait(&loPacketCounter);
		
		semWait(&loLock);
		lopacket *packet = loQueue;
		loQueue = loQueue->next;
		semSignal(&loLock);
		
		// 'iflist' always starts with "lo"
		onPacket(&iflist, packet->data, packet->size);
		kfree(packet);
	};
};

void initSocket();		/* socket.c */
void initNetIf()
{
	mutexInit(&iflistLock);
	semInit(&loLock);
	semInit2(&loPacketCounter, 0);
	
	// the loopback thread shall call onPacket() on all packets waiting on the loopback interface.
	KernelThreadParams pars;
	memset(&pars, 0, sizeof(KernelThreadParams));
	pars.stackSize = DEFAULT_STACK_SIZE;
	pars.name = "inet_loopback";
	CreateKernelThread(loopbackThread, &pars, NULL);

	// the head of the interface list shall be the local loopback.
	static IPNetIfAddr4 loaddr4 = {{0x0100007F}, {0x000000FF}, DOM_LOOPBACK};
	static IPRoute4 lortab4 = {{0x0000007F}, {0x000000FF}, {0}, DOM_LOOPBACK};
	static IPNetIfAddr6 loaddr6[3] = {
		{{{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1}},
		{{255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255}}, DOM_LOOPBACK},
		
		{{{0xFF, 0x01, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1}},
		{{255, 255, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}}, DOM_MULTICAST},

		{{{0xFF, 0x01, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2}},
		{{255, 255, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}}, DOM_MULTICAST}
	};
	
	static IPRoute6 lortab6[2] = {
		{{{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1}},
		{{255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255}},
		{{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}}, DOM_LOOPBACK},
		
		{{{0xFF, 0x01, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
		{{255, 255, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
		{{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}}, DOM_LOOPBACK}
	};

	iflist.drvdata = NULL;
	strcpy(iflist.name, "lo");
	
	//iflist.ipv4.addrs = &loaddr4;
	iflist.ipv4.addrs = (IPNetIfAddr4*) kmalloc(sizeof(IPNetIfAddr4));
	memcpy(iflist.ipv4.addrs, &loaddr4, sizeof(IPNetIfAddr4));
	iflist.ipv4.numAddrs = 1;
	//iflist.ipv4.routes = &lortab4;
	iflist.ipv4.routes = (IPRoute4*) kmalloc(sizeof(IPRoute4));
	memcpy(iflist.ipv4.routes, &lortab4, sizeof(IPRoute4));
	iflist.ipv4.numRoutes = 1;
	
	//iflist.ipv6.addrs = &loaddr6;
	iflist.ipv6.addrs = (IPNetIfAddr6*) kmalloc(sizeof(IPNetIfAddr6)*3);
	memcpy(iflist.ipv6.addrs, loaddr6, sizeof(IPNetIfAddr6)*3);
	iflist.ipv6.numAddrs = 3;
	//iflist.ipv6.routes = &lortab6;
	iflist.ipv6.routes = (IPRoute6*) kmalloc(sizeof(IPRoute6)*2);
	memcpy(iflist.ipv6.routes, lortab6, sizeof(IPRoute6)*2);
	iflist.ipv6.numRoutes = 2;
	
	iflist.numTrans = 0;
	iflist.numRecv = 0;
	iflist.numDropped = 0;
	iflist.numErrors = 0;
	iflist.scopeID = 1;
	
	//iflist.send = loopbackSend;
	iflist.prev = NULL;
	iflist.next = NULL;
	
	initSocket();
};

void packetDump(const void *packet, size_t packetlen)
{
	const uint8_t *scan = (const uint8_t*) packet;
	size_t offset;
	
	static const char hexdigits[16] = "0123456789abcdef";
	for (offset=0; offset<packetlen; offset+=16)
	{
		kprintf_debug("%p ", offset);
		
		size_t fullOffset;
		for (fullOffset=offset; fullOffset<(offset+16) && (fullOffset<packetlen); fullOffset++)
		{
			char hi = hexdigits[scan[fullOffset] >> 4];
			char lo = hexdigits[scan[fullOffset] & 0xF];
			kprintf_debug("%c%c ", hi, lo);
		};
		
		kprintf_debug("\n");
	};
	kprintf_debug("---\n");
};

void onPacket(NetIf *netif, const void *packet, size_t packetlen)
{
	if (packetlen < 1)
	{
		__sync_fetch_and_add(&netif->numDropped, 1);
		return;
	};
	
	// first we send the packet over to capture sockets
	struct sockaddr_cap caddr;
	memset(&caddr, 0, sizeof(struct sockaddr_cap));
	caddr.scap_family = AF_CAPTURE;
	strcpy(caddr.scap_ifname, netif->name);
	onTransportPacket((struct sockaddr*)&caddr, (struct sockaddr*)&caddr, sizeof(struct sockaddr_cap),
		packet, packetlen, IPPROTO_IP);

	uint8_t ipver = ((*((const uint8_t*)packet)) >> 4) & 0xF;
	if (ipver == 4)
	{
		if (packetlen < 20)
		{
			__sync_fetch_and_add(&netif->numDropped, 1);
			return;
		};
		
		PacketInfo4 info;
		if (ipv4_header2info((IPHeader4*) packet, &info) != 0)
		{
			//kprintf("failed to parse header\n");
			__sync_fetch_and_add(&netif->numDropped, 1);
			return;
		};
		
		if (info.hop != 0) info.hop--;
		((IPHeader4*)packet)->hop = (uint8_t) info.hop;
		((IPHeader4*)packet)->checksum = 0;
		((IPHeader4*)packet)->checksum = ipv4_checksum(packet, 20);
		
		__sync_fetch_and_add(&netif->numRecv, 1);
		
		// find out the source and destination address of the packet
		// the ports shall be set to zero; those are filled out by passPacketToSocket() according
		// to protocol.
		struct sockaddr_in addr_src;
		addr_src.sin_family = AF_INET;
		addr_src.sin_port = 0;
		memcpy(&addr_src.sin_addr, &info.saddr, 4);
		
		struct sockaddr_in addr_dest;
		addr_dest.sin_family = AF_INET;
		addr_dest.sin_port = 0;
		memcpy(&addr_dest.sin_addr, &info.daddr, 4);
		
		passPacketToSocket((struct sockaddr*) &addr_src, (struct sockaddr*) &addr_dest, sizeof(struct sockaddr_in),
					packet, packetlen, info.proto, info.dataOffset);
	}
	else if (ipver == 6)
	{
		if (packetlen < 40)
		{
			__sync_fetch_and_add(&netif->numDropped, 1);
			return;
		};
		
		PacketInfo6 info;
		if (ipv6_header2info((IPHeader6*) packet, &info) != 0)
		{
			__sync_fetch_and_add(&netif->numDropped, 1);
			return;
		};
		
		if (info.hop != 0) info.hop--;
		((IPHeader6*)packet)->hop = (uint8_t) info.hop;

		__sync_fetch_and_add(&netif->numRecv, 1);
		
		// find out the source and destination address of the packet
		// the ports shall be set to zero; those are filled out by passPacketToSocket() according
		// to protocol.
		struct sockaddr_in6 src_addr;
		src_addr.sin6_family = AF_INET6;
		src_addr.sin6_port = 0;
		src_addr.sin6_flowinfo = info.flowinfo;
		memcpy(&src_addr.sin6_addr, &info.saddr, 16);
		src_addr.sin6_scope_id = netif->scopeID;
		
		struct sockaddr_in6 dest_addr;
		dest_addr.sin6_family = AF_INET6;
		dest_addr.sin6_port = 0;
		dest_addr.sin6_flowinfo = info.flowinfo;
		memcpy(&dest_addr.sin6_addr, &info.daddr, 16);
		dest_addr.sin6_scope_id = netif->scopeID;
		
		passPacketToSocket((struct sockaddr*) &src_addr, (struct sockaddr*) &dest_addr, sizeof(struct sockaddr_in),
					packet, packetlen, info.proto, info.dataOffset);
	}
	else
	{
		__sync_fetch_and_add(&netif->numDropped, 1);
	};
};

QFileEntry *getRoutes4()
{
	mutexLock(&iflistLock);
	QFileEntry *first = NULL;
	QFileEntry *last = NULL;
	
	NetIf *netif = &iflist;
	while (netif != NULL)
	{
		int i;
		for (i=0; i<netif->ipv4.numRoutes; i++)
		{
			IPRoute4 *route = &netif->ipv4.routes[i];
			QFileEntry *entry = (QFileEntry*) kmalloc(sizeof(QFileEntry) + sizeof(in_route));
			in_route *rinfo = (in_route*) entry->data;
			entry->next = NULL;
			entry->size = sizeof(in_route);
			
			strcpy(rinfo->ifname, netif->name);
			rinfo->dest = route->dest;
			rinfo->mask = route->mask;
			rinfo->gateway = route->gateway;
			rinfo->domain = route->domain;
			rinfo->flags = 0;
			
			if (first == NULL)
			{
				first = last = entry;
			}
			else
			{
				last->next = entry;
				last = entry;
			};
		};
		
		netif = netif->next;
	};
	
	mutexUnlock(&iflistLock);
	return first;
};

QFileEntry *getRoutes6()
{
	mutexLock(&iflistLock);
	QFileEntry *first = NULL;
	QFileEntry *last = NULL;
	
	NetIf *netif = &iflist;
	while (netif != NULL)
	{
		int i;
		for (i=0; i<netif->ipv6.numRoutes; i++)
		{
			IPRoute6 *route = &netif->ipv6.routes[i];
			QFileEntry *entry = (QFileEntry*) kmalloc(sizeof(QFileEntry) + sizeof(in6_route));
			in6_route *rinfo = (in6_route*) entry->data;
			entry->next = NULL;
			entry->size = sizeof(in6_route);
			
			strcpy(rinfo->ifname, netif->name);
			rinfo->dest = route->dest;
			rinfo->mask = route->mask;
			rinfo->gateway = route->gateway;
			rinfo->domain = route->domain;
			rinfo->flags = 0;
			
			if (first == NULL)
			{
				first = last = entry;
			}
			else
			{
				last->next = entry;
				last = entry;
			};
		};
		
		netif = netif->next;
	};
	
	mutexUnlock(&iflistLock);
	return first;
};

int sysRouteTable(uint64_t family)
{
	switch (family)
	{
	case AF_INET:
	case AF_INET6:
		break;
	default:
		getCurrentThread()->therrno = EINVAL;
		return -1;
	};
	
	FileTable *ftab = getCurrentThread()->ftab;
	spinlockAcquire(&ftab->spinlock);

	int i;
	for (i=0; i<MAX_OPEN_FILES; i++)
	{
		if (ftab->entries[i] == NULL)
		{
			break;
		};
	};

	if (i == MAX_OPEN_FILES)
	{
		spinlockRelease(&ftab->spinlock);
		getCurrentThread()->therrno = EMFILE;
		return -1;
	};
	
	QFileEntry *head;
	if (family == AF_INET)
	{
		head = getRoutes4();
	}
	else
	{
		head = getRoutes6();
	};
	
	ftab->entries[i] = qfileCreate(head);
	spinlockRelease(&ftab->spinlock);
	
	return i;
};

int isMatchingMask(const void *a_, const void *b_, const void *mask_, size_t count)
{
	const uint8_t *a = (const uint8_t*) a_;
	const uint8_t *b = (const uint8_t*) b_;
	const uint8_t *mask = (const uint8_t*) mask_;
	
	size_t i;
	for (i=0; i<count; i++)
	{
		if ((a[i] & mask[i]) != (b[i] & mask[i]))
		{
			return 0;
		};
	};
	
	return 1;
};

int isMappedAddress46(const struct sockaddr *addr)
{
	static uint8_t mappedNetwork46[16] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xFF, 0xFF, 0, 0, 0, 0};
	static uint8_t mappedMask46[16] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0, 0, 0, 0};
	
	if (addr->sa_family == AF_INET6)
	{
		const struct sockaddr_in6 *inaddr = (const struct sockaddr_in6*) addr;
		if (isMatchingMask(&inaddr->sin6_addr, mappedNetwork46, mappedMask46, 16))
		{
			return 1;
		};
	};
	
	return 0;
};

void remapAddress64(const struct sockaddr_in6 *addr, struct sockaddr_in *result)
{
	result->sin_family = AF_INET;
	memcpy(&result->sin_addr, &addr->sin6_addr.s6_addr[12], 4);
};

void remapAddress46(const struct sockaddr_in *addr, struct sockaddr_in6 *result)
{
	static uint8_t mappedNetwork46[16] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xFF, 0xFF, 0, 0, 0, 0};
	result->sin6_family = AF_INET6;
	memcpy(&result->sin6_addr, mappedNetwork46, 16);
	memcpy(&result->sin6_addr.s6_addr[12], &addr->sin_addr, 4);
	result->sin6_scope_id = 0;
};

uint32_t getNextPacketID()
{
	static uint32_t nextPacketID = 1;
	return nextPacketID++;
};

static int sendPacketToInterface(NetIf *netif, const struct sockaddr *gateway, const void *packet, size_t packetlen, uint64_t nanotimeout)
{
	// send the packet to capture sockets
	struct sockaddr_cap caddr;
	memset(&caddr, 0, sizeof(struct sockaddr_cap));
	caddr.scap_family = AF_CAPTURE;
	strcpy(caddr.scap_ifname, netif->name);
	onTransportPacket((struct sockaddr*)&caddr, (struct sockaddr*)&caddr, sizeof(struct sockaddr_cap),
		packet, packetlen, IPPROTO_IP);

	// this is called when iflistLock is locked, and 'gateway' is supposedly reacheable directly.
	// depending on the type of interface, different address-resolution methods may be used.
	switch (netif->ifconfig.type)
	{
	case IF_LOOPBACK:
		loopbackSend(netif, packet, packetlen);
		return 0;
	case IF_ETHERNET:
		return sendPacketToEthernet(netif, gateway, packet, packetlen, nanotimeout);
	default:
		return -EHOSTUNREACH;
	};
};

int sendPacket(struct sockaddr *src, const struct sockaddr *dest, const void *packet, size_t packetlen, int flags,
		uint64_t nanotimeout, const char *ifname)
{
	static uint8_t zeroes[16] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
	if (ifname != NULL)
	{
		if (ifname[0] == 0)
		{
			ifname = NULL;
		};
	};
	
	if (isMappedAddress46(dest))
	{
		struct sockaddr_in src4;
		struct sockaddr_in dest4;
		src4.sin_family = AF_UNSPEC;
		
		if (src->sa_family != AF_UNSPEC)
		{
			if (!isMappedAddress46(src))
			{
				// destination is IPv4 but source is IPv6
				return -EINVAL;
			};
			
			remapAddress64((struct sockaddr_in6*) src, (struct sockaddr_in*)&src4);
		};
		
		remapAddress64((struct sockaddr_in6*) dest, (struct sockaddr_in*) &dest4);
		
		// send the packet over IPv4
		int status = sendPacket((struct sockaddr*) &src4, (const struct sockaddr*) &dest4, packet, packetlen, flags,
						nanotimeout, ifname);
		
		// if the IPv4 source address was just determined, map it onto an IPv6 address.
		if (src->sa_family == AF_UNSPEC)
		{
			remapAddress46((struct sockaddr_in*) &src4, (struct sockaddr_in6*) src);
		};
		
		return status;
	};
	
	if (isMappedAddress46(src))
	{
		// source is IPv4 but destination is IPv6
		return -EINVAL;
	};
	
	if (src->sa_family == AF_UNSPEC)
	{
		if (dest->sa_family == AF_INET)
		{
			struct sockaddr_in *insrc = (struct sockaddr_in*) src;
			struct sockaddr_in *indst = (struct sockaddr_in*) dest;
			insrc->sin_family = AF_INET;
			getDefaultAddr4(&insrc->sin_addr, &indst->sin_addr);
		}
		else if (dest->sa_family == AF_INET6)
		{
			struct sockaddr_in6 *insrc = (struct sockaddr_in6*) src;
			struct sockaddr_in6 *indst = (struct sockaddr_in6*) dest;
			insrc->sin6_family = AF_INET6;
			insrc->sin6_scope_id = indst->sin6_scope_id;
			getDefaultAddr6(&insrc->sin6_addr, &indst->sin6_addr);
		}
		else
		{
			return -EINVAL;
		};
	};
	
	if (src->sa_family != dest->sa_family)
	{
		// family mismatch
		return -EINVAL;
	};
	
	if (src->sa_family == AF_INET)
	{
		struct sockaddr_in *insrc = (struct sockaddr_in*) src;
		const struct sockaddr_in *indst = (const struct sockaddr_in*) dest;
		
		if (memcmp(&insrc->sin_addr, zeroes, 4) == 0)
		{
			getDefaultAddr4(&insrc->sin_addr, &indst->sin_addr);
		};
	}
	else
	{
		struct sockaddr_in6 *insrc = (struct sockaddr_in6*) src;
		const struct sockaddr_in6 *indst = (const struct sockaddr_in6*) dest;
		
		if (memcmp(&insrc->sin6_addr, zeroes, 16) == 0)
		{
			getDefaultAddr6(&insrc->sin6_addr, &indst->sin6_addr);
		};
	};
	
	if (packetlen == 0)
	{
		// drop zero-sized packets secretly
		return 0;
	};
	
	int proto = flags & 0xFF;
	if (proto == IPPROTO_RAW)
	{
		flags |= PKT_HDRINC | PKT_DONTFRAG;
	};
	
	if (flags & PKT_HDRINC)
	{
		flags |= PKT_DONTFRAG;
	};
	
	// TODO: proper path MTU discovery!
	size_t mtu = 1280;
	if (dest->sa_family == AF_INET)
	{
		mtu = 576;
	};
	
	if ((flags & PKT_HDRINC) == 0)
	{
		if (dest->sa_family == AF_INET)
		{
			mtu -= 20;			// IPv4 header is 20 bytes
		}
		else
		{
			mtu -= 40;			// IPv6 header is 40 bytes
		};
	};
	
	if (packetlen > mtu)
	{
		if (flags & PKT_DONTFRAG)
		{
			return -E2BIG;
		};
		
		// we're fragmenting, so for IPv6 we must also append a fragment extension header!
		if (dest->sa_family == AF_INET6)
		{
			mtu -= 8;
			
			// it must also be divisible by 8 (fun fact)
			mtu &= ~7;
		};
		
		size_t numFullDatagrams = packetlen / mtu;
		size_t sizeLeft = packetlen % mtu;
		if (sizeLeft > 0) numFullDatagrams++;
		
		uint32_t packetID = getNextPacketID();
		
		size_t i;
		for (i=0; i<numFullDatagrams; i++)
		{
			uint8_t *encapPacket;
			size_t encapSize;
			if (dest->sa_family == AF_INET)
			{
				PacketInfo4 info;
				struct sockaddr_in *insrc = (struct sockaddr_in*) src;
				const struct sockaddr_in *indst = (const struct sockaddr_in*) dest;
				memcpy(&info.saddr, &insrc->sin_addr, 4);
				memcpy(&info.daddr, &indst->sin_addr, 4);
				info.proto = proto;
				info.size = mtu;
				info.id = packetID;
				info.fragOff = i * mtu;
				info.hop = DEFAULT_HOP_LIMIT;
				info.moreFrags = (i != (numFullDatagrams-1));
				
				if (!info.moreFrags)
				{
					info.size = sizeLeft;
				};
				
				//kprintf("SENDING: fragOff=%d, size=%d, moreFrags=%d\n", (int)info.fragOff, (int)info.size, info.moreFrags);
				encapSize = 20+info.size;
				encapPacket = (uint8_t*) kmalloc(20+info.size);
				ipv4_info2header(&info, (IPHeader4*)encapPacket);
				memcpy(&encapPacket[20], ((uint8_t*)packet+(i*mtu)), info.size);
			}
			else
			{
				// IPv6
				PacketInfo6 info;
				struct sockaddr_in6 *insrc = (struct sockaddr_in6*) src;
				const struct sockaddr_in6 *indst = (const struct sockaddr_in6*) dest;
				memcpy(&info.saddr, &insrc->sin6_addr, 16);
				memcpy(&info.daddr, &indst->sin6_addr, 16);
				info.flowinfo = 0;
				info.proto = proto;
				info.size = mtu;
				info.id = packetID;
				info.fragOff = i * mtu;
				info.hop = DEFAULT_HOP_LIMIT;
				info.moreFrags = (i != (numFullDatagrams-1));
				
				if (!info.moreFrags)
				{
					info.size = sizeLeft;
				};
				
				encapSize = 48+info.size;
				encapPacket = (uint8_t*) kmalloc(48+info.size);
				ipv6_info2header(&info, (IPHeader6*)encapPacket);
				memcpy(&encapPacket[48], ((uint8_t*)packet+(i*mtu)), info.size);
			};
			
			int status = sendPacket(src, dest, encapPacket, encapSize, flags | PKT_HDRINC, nanotimeout, ifname);
			kfree(encapPacket);
			
			if (status != 0) return status;
		};
		
		return 0;
	};
	
	// add an IP header if needed
	if ((flags & PKT_HDRINC) == 0)
	{
		uint8_t *encapPacket;
		size_t encapSize;
		
		if (dest->sa_family == AF_INET)
		{
			PacketInfo4 info;
			struct sockaddr_in *insrc = (struct sockaddr_in*) src;
			const struct sockaddr_in *indst = (const struct sockaddr_in*) dest;
			memcpy(&info.saddr, &insrc->sin_addr, 4);
			memcpy(&info.daddr, &indst->sin_addr, 4);
			info.proto = proto;
			info.size = packetlen;
			info.id = getNextPacketID();
			info.fragOff = 0;
			info.hop = DEFAULT_HOP_LIMIT;
			info.moreFrags = 0;
			
			encapSize = 20+info.size;
			encapPacket = (uint8_t*) kmalloc(20+info.size);
			ipv4_info2header(&info, (IPHeader4*)encapPacket);
			memcpy(&encapPacket[20], packet, packetlen);
		}
		else
		{
			// IPv6
			PacketInfo6 info;
			struct sockaddr_in6 *insrc = (struct sockaddr_in6*) src;
			const struct sockaddr_in6 *indst = (const struct sockaddr_in6*) dest;
			memcpy(&info.saddr, &insrc->sin6_addr, 16);
			memcpy(&info.daddr, &indst->sin6_addr, 16);
			info.flowinfo = 0;
			info.proto = proto;
			info.size = packetlen;
			info.id = getNextPacketID();
			info.fragOff = 0;
			info.hop = DEFAULT_HOP_LIMIT;
			info.moreFrags = 0;
			
			encapSize = 40+info.size;
			encapPacket = (uint8_t*) kmalloc(40+info.size);
			ipv6_info2header(&info, (IPHeader6*)encapPacket);
			memcpy(&encapPacket[40], packet, packetlen);
		};
		
		int status = sendPacket(src, dest, encapPacket, encapSize, flags | PKT_HDRINC, nanotimeout, ifname);
		kfree(encapPacket);
		return status;
	};
	
	// at this point, we have an IP packet with a complete header, which we just have to send to an interface.
	// so we look through the routing tables, select an interface, and attempt to transmit by using address
	// resolution.

	mutexLock(&iflistLock);
	
	NetIf *netif;
	for (netif=&iflist; netif!=NULL; netif=netif->next)
	{
		int useThisOne = 1;
		
		if (ifname != NULL)
		{
			if (strcmp(ifname, netif->name) != 0)
			{
				useThisOne = 0;
			};
		};
		
		if (dest->sa_family == AF_INET6)
		{
			const struct sockaddr_in6 *indst = (const struct sockaddr_in6*) dest;
			if (indst->sin6_scope_id != 0)
			{
				if (indst->sin6_scope_id != netif->scopeID)
				{
					useThisOne = 0;
				};
			};
		};
		
		if (useThisOne)
		{
			if (dest->sa_family == AF_INET)
			{
				const struct sockaddr_in *indst = (const struct sockaddr_in*) dest;
				struct sockaddr_in gateway;
				gateway.sin_family = AF_INET;
				
				int gatewayFound = 0;
				int i;
				
				if (flags & PKT_DONTROUTE)
				{
					gatewayFound = 1;
					memcpy(&gateway, indst, sizeof(struct sockaddr_in));
				}
				else
				{
					for (i=0; i<netif->ipv4.numRoutes; i++)
					{
						IPRoute4 *route = &netif->ipv4.routes[i];
						if (isMatchingMask(&route->dest, &indst->sin_addr, &route->mask, 4))
						{
							if (memcmp(&route->gateway, zeroes, 4) == 0)
							{
								memcpy(&gateway.sin_addr, &indst->sin_addr, 4);
							}
							else
							{
								memcpy(&gateway.sin_addr, &route->gateway, 4);
							};
						
							gatewayFound = 1;
							break;
						};
					};
				};
				
				if (gatewayFound)
				{
					int status = sendPacketToInterface(netif, (struct sockaddr*) &gateway,
										packet, packetlen, nanotimeout);
					mutexUnlock(&iflistLock);
					return status;
				};
			}
			else
			{
				const struct sockaddr_in6 *indst = (const struct sockaddr_in6*) dest;
				struct sockaddr_in6 gateway;
				gateway.sin6_family = AF_INET6;
				
				int gatewayFound = 0;
				int i;
				
				if (flags & PKT_DONTROUTE)
				{
					gatewayFound = 1;
					memcpy(&gateway, indst, sizeof(struct sockaddr_in6));
				}
				else
				{
					for (i=0; i<netif->ipv6.numRoutes; i++)
					{
						IPRoute6 *route = &netif->ipv6.routes[i];
						if (isMatchingMask(&route->dest, &indst->sin6_addr, &route->mask, 16))
						{
							if (memcmp(&route->gateway, zeroes, 16) == 0)
							{
								memcpy(&gateway.sin6_addr, &indst->sin6_addr, 16);
							}
							else
							{
								memcpy(&gateway.sin6_addr, &route->gateway, 16);
							};
						
							gatewayFound = 1;
							break;
						};
					};
				};
				
				if (gatewayFound)
				{
					int status = sendPacketToInterface(netif, (struct sockaddr*) &gateway,
										packet, packetlen, nanotimeout);
					mutexUnlock(&iflistLock);
					return status;
				};
			};
		};
	};
	
	mutexUnlock(&iflistLock);
	return -ENETUNREACH;
};

void getDefaultAddr4(struct in_addr *src, const struct in_addr *dest)
{
	uint32_t addr = *((uint32_t*)dest);

	mutexLock(&iflistLock);
	
	NetIf *netif = &iflist;
	while (netif != NULL)
	{
		int i;
		for (i=0; i<netif->ipv4.numRoutes; i++)
		{
			IPRoute4 *route = &netif->ipv4.routes[i];

			uint32_t dest = *((uint32_t*)&route->dest);
			uint32_t mask = *((uint32_t*)&route->mask);
			
			if ((addr & mask) == (dest & mask))
			{	
				int j;
				for (j=0; j<netif->ipv4.numAddrs; j++)
				{
					if (netif->ipv4.addrs[j].domain == route->domain)
					{
						memcpy(src, &netif->ipv4.addrs[j].addr, 4);
						mutexUnlock(&iflistLock);
						return;
					};
				};
				
				memset(src, 0, 4);
				mutexUnlock(&iflistLock);
				return;
			};
		};
		
		netif = netif->next;
	};
	
	memset(src, 0, 4);
	mutexUnlock(&iflistLock);
};

void getDefaultAddr6(struct in6_addr *src, const struct in6_addr *dest)
{
	uint64_t *addr = ((uint64_t*)dest);

	mutexLock(&iflistLock);
	
	NetIf *netif = &iflist;
	while (netif != NULL)
	{
		int i;
		for (i=0; i<netif->ipv6.numRoutes; i++)
		{
			IPRoute6 *route = &netif->ipv6.routes[i];

			uint64_t *dest = ((uint64_t*)&route->dest);
			uint64_t *mask = ((uint64_t*)&route->mask);

			if (((addr[0] & mask[0]) == (dest[0] & mask[0])) && ((addr[1] & mask[1]) == (dest[1] & mask[1])))
			{	
				int j;
				for (j=0; j<netif->ipv6.numAddrs; j++)
				{
					if (netif->ipv6.addrs[j].domain == route->domain)
					{
						memcpy(src, &netif->ipv6.addrs[j].addr, 16);
						mutexUnlock(&iflistLock);
						return;
					};
				};
				
				memset(src, 0, 16);
				mutexUnlock(&iflistLock);
				return;
			};
		};
		
		netif = netif->next;
	};
	
	memset(src, 0, 16);
	mutexUnlock(&iflistLock);
};

int route_add(int family, int pos, gen_route *route)
{
	if ((family != AF_INET) && (family != AF_INET6))
	{
		getCurrentThread()->therrno = EINVAL;
		return -1;
	};
	
	if (getCurrentThread()->creds->euid != 0)
	{
		getCurrentThread()->therrno = EACCES;
		return -1;
	};
	
	mutexLock(&iflistLock);
	
	NetIf *netif = &iflist;
	while (netif != NULL)
	{
		if (strcmp(route->ifname, netif->name) == 0)
		{
			if (family == AF_INET)
			{
				in_route *inroute = (in_route*) route;
				netif->ipv4.numRoutes++;
				IPRoute4 *oldRoutes = netif->ipv4.routes;
				netif->ipv4.routes = (IPRoute4*) kmalloc(sizeof(IPRoute4)*netif->ipv4.numRoutes);
				if (pos == -1)
				{
					pos = netif->ipv4.numRoutes - 1;
				};
				if (pos > (netif->ipv4.numRoutes - 1))
				{
					pos = netif->ipv4.numRoutes - 1;
				};
				
				memcpy(netif->ipv4.routes, oldRoutes, sizeof(IPRoute4)*pos);
				memcpy(&netif->ipv4.routes[pos+1], &oldRoutes[pos], sizeof(IPRoute4)*(netif->ipv4.numRoutes-1-pos));
				memcpy(&netif->ipv4.routes[pos].dest, &inroute->dest, 4);
				memcpy(&netif->ipv4.routes[pos].mask, &inroute->mask, 4);
				memcpy(&netif->ipv4.routes[pos].gateway, &inroute->gateway, 4);
				netif->ipv4.routes[pos].domain = inroute->domain;
				kfree(oldRoutes);
				mutexUnlock(&iflistLock);
				return 0;
			}
			else if (family == AF_INET6)
			{
				in6_route *inroute = (in6_route*) route;
				netif->ipv6.numRoutes++;
				IPRoute6 *oldRoutes = netif->ipv6.routes;
				netif->ipv6.routes = (IPRoute6*) kmalloc(sizeof(IPRoute6)*netif->ipv6.numRoutes);
				if (pos == -1)
				{
					pos = netif->ipv6.numRoutes - 1;
				};
				if (pos > (netif->ipv6.numRoutes - 1))
				{
					pos = netif->ipv6.numRoutes - 1;
				};
				
				memcpy(netif->ipv6.routes, oldRoutes, sizeof(IPRoute6)*pos);
				memcpy(&netif->ipv6.routes[pos+1], &oldRoutes[pos], sizeof(IPRoute6)*(netif->ipv6.numRoutes-1-pos));
				memcpy(&netif->ipv6.routes[pos].dest, &inroute->dest, 16);
				memcpy(&netif->ipv6.routes[pos].mask, &inroute->mask, 16);
				memcpy(&netif->ipv6.routes[pos].gateway, &inroute->gateway, 16);
				netif->ipv6.routes[pos].domain = inroute->domain;
				kfree(oldRoutes);
				mutexUnlock(&iflistLock);
				return 0;
			};
		};
		
		netif = netif->next;
	};
	
	mutexUnlock(&iflistLock);
	
	// the interface was not found!
	getCurrentThread()->therrno = ENOENT;
	return -1;
};

int route_clear(int family, const char *ifname)
{
	if ((family != AF_INET) && (family != AF_INET6))
	{
		getCurrentThread()->therrno = EINVAL;
		return -1;
	};
	
	if (getCurrentThread()->creds->euid != 0)
	{
		getCurrentThread()->therrno = EACCES;
		return -1;
	};
	
	mutexLock(&iflistLock);
	
	NetIf *netif = &iflist;
	while (netif != NULL)
	{
		if (strcmp(ifname, netif->name) == 0)
		{
			if (family == AF_INET)
			{	
				if (netif->ipv4.numRoutes != 0)
				{
					kfree(netif->ipv4.routes);
				};
				
				netif->ipv4.numRoutes = 0;
				netif->ipv4.routes = NULL;
				mutexUnlock(&iflistLock);
				return 0;
			}
			else if (family == AF_INET6)
			{
				if (netif->ipv6.numRoutes != 0)
				{
					kfree(netif->ipv6.routes);
				};
				
				netif->ipv6.numRoutes = 0;
				netif->ipv6.routes = NULL;
				mutexUnlock(&iflistLock);
				return 0;
			};
		};
		
		netif = netif->next;
	};
	
	mutexUnlock(&iflistLock);
	
	// the interface was not found!
	getCurrentThread()->therrno = ENOENT;
	return -1;
};

ssize_t netconf_stat(const char *ifname, NetStat *buffer, size_t size)
{
	NetStat netstat;
	mutexLock(&iflistLock);
	
	NetIf *netif = &iflist;
	while (netif != NULL)
	{
		if (strcmp(ifname, netif->name) == 0)
		{
			strcpy(netstat.ifname, netif->name);
			netstat.numTrans = netif->numTrans;
			netstat.numErrors = netif->numErrors;
			netstat.numRecv = netif->numRecv;
			netstat.numDropped = netif->numDropped;
			netstat.numAddrs4 = netif->ipv4.numAddrs;
			netstat.numAddrs6 = netif->ipv6.numAddrs;
			netstat.scopeID = netif->scopeID;
			memcpy(&netstat.ifconfig, &netif->ifconfig, sizeof(NetIfConfig));
			
			if (size > sizeof(NetStat))
			{
				size = sizeof(NetStat);
			};
			
			memcpy(buffer, &netstat, size);
			mutexUnlock(&iflistLock);
			return (ssize_t) size;
		};
		
		netif = netif->next;
	};
	
	mutexUnlock(&iflistLock);
	
	// the interface was not found!
	getCurrentThread()->therrno = ENOENT;
	return -1;
};

ssize_t netconf_statidx(unsigned int index, NetStat *buffer, size_t size)
{
	NetStat netstat;
	mutexLock(&iflistLock);
	
	NetIf *netif = &iflist;
	while (netif != NULL)
	{
		if ((index--) == 0)
		{
			strcpy(netstat.ifname, netif->name);
			netstat.numTrans = netif->numTrans;
			netstat.numErrors = netif->numErrors;
			netstat.numRecv = netif->numRecv;
			netstat.numDropped = netif->numDropped;
			netstat.numAddrs4 = netif->ipv4.numAddrs;
			netstat.numAddrs6 = netif->ipv6.numAddrs;
			memcpy(&netstat.ifconfig, &netif->ifconfig, sizeof(NetIfConfig));
			
			if (size > sizeof(NetStat))
			{
				size = sizeof(NetStat);
			};
			
			memcpy(buffer, &netstat, size);
			mutexUnlock(&iflistLock);
			return (ssize_t) size;
		};
		
		netif = netif->next;
	};
	
	mutexUnlock(&iflistLock);
	
	// the interface was not found!
	getCurrentThread()->therrno = ENOENT;
	return -1;
};

ssize_t netconf_getaddrs(const char *ifname, int family, void *buffer, size_t bufsiz)
{
	if ((family != AF_INET) && (family != AF_INET6))
	{
		getCurrentThread()->therrno = EINVAL;
		return -1;
	};
	
	mutexLock(&iflistLock);
	
	NetIf *netif = &iflist;
	while (netif != NULL)
	{
		if (strcmp(ifname, netif->name) == 0)
		{
			if (family == AF_INET)
			{
				size_t totalsz = sizeof(IPNetIfAddr4) * netif->ipv4.numAddrs;
				if (bufsiz > totalsz) bufsiz = totalsz;
				memcpy(buffer, netif->ipv4.addrs, bufsiz);
			}
			else if (family == AF_INET6)
			{
				size_t totalsz = sizeof(IPNetIfAddr6) * netif->ipv6.numAddrs;
				if (bufsiz > totalsz) bufsiz = totalsz;
				memcpy(buffer, netif->ipv6.addrs, bufsiz);
			};
			
			mutexUnlock(&iflistLock);
			return (ssize_t) bufsiz;
		};
		
		netif = netif->next;
	};
	
	mutexUnlock(&iflistLock);
	
	// the interface was not found!
	getCurrentThread()->therrno = ENOENT;
	return -1;
};

static int nextEthNumber = 0;
static int nextTnlNumber = 0;
static uint32_t nextScopeID = 2;		/* 0 = global scope, 1 = scope of "lo" interface */

NetIf *CreateNetworkInterface(void *drvdata, NetIfConfig *ifconfig)
{
	mutexLock(&iflistLock);
	char ifname[16];
	if (ifconfig->type == IF_LOOPBACK)
	{
		mutexUnlock(&iflistLock);
		return NULL;
	}
	else if (ifconfig->type == IF_ETHERNET)
	{
		if (nextEthNumber == 100)
		{
			mutexUnlock(&iflistLock);
			return NULL;
		};
		
		// validate mac address: it must not be broadcast (ff:ff:ff:ff:ff:ff), or multicast
		// (bit 0 of first byte set).
		if (ifconfig->ethernet.mac.addr[0] & 1)
		{
			mutexUnlock(&iflistLock);
			return NULL;
		};
		
		static uint8_t macBroadcast[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
		if (memcmp(&ifconfig->ethernet.mac, macBroadcast, 6) == 0)
		{
			mutexUnlock(&iflistLock);
			return NULL;
		};
		
		memcpy(ifname, "eth", 3);
		if (nextEthNumber >= 10)
		{
			ifname[3] = '0' + (nextEthNumber/10);
			ifname[4] = '0' + (nextEthNumber%10);
			nextEthNumber++;
			ifname[5] = 0;
		}
		else
		{
			ifname[3] = '0' + nextEthNumber;
			nextEthNumber++;
			ifname[4] = 0;
		};
	}
	else if (ifconfig->type == IF_TUNNEL)
	{
		if (nextTnlNumber == 100)
		{
			mutexUnlock(&iflistLock);
			return NULL;
		};
		
		memcpy(ifname, "tnl", 3);
		if (nextTnlNumber >= 10)
		{
			ifname[3] = '0' + (nextTnlNumber/10);
			ifname[4] = '0' + (nextTnlNumber%10);
			nextTnlNumber++;
			ifname[5] = 0;
		}
		else
		{
			ifname[3] = '0' + nextTnlNumber;
			nextTnlNumber++;
		};
	}
	else
	{
		mutexUnlock(&iflistLock);
		return NULL;
	};
	
	NetIf *netif = (NetIf*) kmalloc(sizeof(NetIf));
	memset(netif, 0, sizeof(NetIf));
	netif->drvdata = drvdata;
	strcpy(netif->name, ifname);
	memcpy(&netif->ifconfig, ifconfig, sizeof(NetIfConfig));
	netif->scopeID = __sync_fetch_and_add(&nextScopeID, 1);
	
	NetIf *last = &iflist;
	while (last->next != NULL)
	{
		last = last->next;
	};
	
	last->next = netif;
	netif->prev = last;
	
	mutexUnlock(&iflistLock);
	return netif;
};

void DeleteNetworkInterface(NetIf *netif)
{
	mutexLock(&iflistLock);
	if (netif->prev != NULL) netif->prev->next = netif->next;
	if (netif->next != NULL) netif->next->prev = netif->prev;
	mutexUnlock(&iflistLock);
	
	kfree(netif);
};

int isLocalAddr(const struct sockaddr *addr)
{
	if (addr->sa_family == AF_INET)
	{
		const struct sockaddr_in *inaddr = (const struct sockaddr_in*) addr;
		if (inaddr->sin_addr.s_addr == 0xFFFFFFFF)
		{
			return 1;
		};
	};
	
	mutexLock(&iflistLock);
	
	NetIf *netif;
	for (netif=&iflist; netif!=NULL; netif=netif->next)
	{
		if (addr->sa_family == AF_INET)
		{
			const struct sockaddr_in *inaddr = (const struct sockaddr_in*) addr;

			int i;
			for (i=0; i<netif->ipv4.numAddrs; i++)
			{
				IPNetIfAddr4 *ifaddr = &netif->ipv4.addrs[i];
				if (memcmp(&ifaddr->addr, &inaddr->sin_addr, 4) == 0)
				{
					mutexUnlock(&iflistLock);
					return 1;
				};
				
				// also count the broadcast address as a local address!
				uint32_t laddr = *((uint32_t*)&ifaddr->addr);
				uint32_t mask = *((uint32_t*)&ifaddr->mask);
				uint32_t thisaddr = *((uint32_t*)&inaddr->sin_addr);
				uint32_t broadcast = (laddr & mask) | (~mask);
				
				if (thisaddr == broadcast)
				{
					mutexUnlock(&iflistLock);
					return 1;
				};
			};
		}
		else if (addr->sa_family == AF_INET6)
		{
			// IPv6
			const struct sockaddr_in6 *inaddr = (const struct sockaddr_in6*) addr;
			
			if (inaddr->sin6_addr.s6_addr[0] == 0xFF)
			{
				// IPv6 multicast, we don't want to forward this by accident
				mutexUnlock(&iflistLock);
				return 1;
			};
			
			int i;
			for (i=0; i<netif->ipv6.numAddrs; i++)
			{
				IPNetIfAddr6 *ifaddr = &netif->ipv6.addrs[i];
				if (memcmp(&ifaddr->addr, &inaddr->sin6_addr, 16) == 0)
				{
					if ((inaddr->sin6_scope_id == 0) || (inaddr->sin6_scope_id == netif->scopeID))
					{
						mutexUnlock(&iflistLock);
						return 1;
					};
				};
			};
		};
	};
	
	mutexUnlock(&iflistLock);
	return 0;
};

int netconf_addr(int family, const char *ifname, const void *buffer, size_t size)
{
	if (getCurrentThread()->creds->euid != 0)
	{
		ERRNO = EPERM;
		return -1;
	};
	
	if ((family != AF_INET) && (family != AF_INET6))
	{
		ERRNO = EINVAL;
		return -1;
	};
	
	if (family == AF_INET)
	{
		if ((size % sizeof(IPNetIfAddr4)) != 0)
		{
			ERRNO = EINVAL;
			return -1;
		};
	}
	else
	{
		if ((size % sizeof(IPNetIfAddr6)) != 0)
		{
			ERRNO = EINVAL;
			return -1;
		};
	};
	
	mutexLock(&iflistLock);
	
	NetIf *netif;
	for (netif=&iflist; netif!=NULL; netif=netif->next)
	{
		if (strcmp(netif->name, ifname) == 0)
		{
			if (family == AF_INET)
			{
				if (netif->ipv4.numAddrs != 0)
				{
					kfree(netif->ipv4.addrs);
				};
				
				netif->ipv4.numAddrs = (int) (size / sizeof(IPNetIfAddr4));
				netif->ipv4.addrs = (IPNetIfAddr4*) kmalloc(size);
				memcpy(netif->ipv4.addrs, buffer, size);
				
				mutexUnlock(&iflistLock);
				return 0;
			}
			else
			{
				// IPv6 (AF_INET6)
				if (netif->ipv6.numAddrs != 0)
				{
					kfree(netif->ipv6.addrs);
				};
				
				netif->ipv6.numAddrs = (int) (size / sizeof(IPNetIfAddr6));
				netif->ipv6.addrs = (IPNetIfAddr6*) kmalloc(size);
				memcpy(netif->ipv6.addrs, buffer, size);
				
				mutexUnlock(&iflistLock);
				return 0;
			};
		};
	};
	
	mutexUnlock(&iflistLock);
	ERRNO = ENOENT;
	return -1;
};

NetIf *netIfByScope(uint32_t scopeID)
{
	NetIf *netif;
	for (netif=&iflist; netif!=NULL; netif=netif->next)
	{
		if (netif->scopeID == scopeID)
		{
			return netif;
		};
	};
	
	return NULL;
};

void releaseNetIfLock()
{
	mutexUnlock(&iflistLock);
};

void acquireNetIfLock()
{
	mutexLock(&iflistLock);
};

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

static Semaphore iflistLock;
static NetIf iflist;

uint16_t ipv4_checksum(void* vdata,size_t length)
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
}

void ipv4_info2header(PacketInfo4 *info, IPHeader4 *head)
{
	head->versionIHL = 0x45;				// IPv4, 5 dwords (20 bytes) header
	head->dscpECN = 0;					// XXX ??
	head->len = 20 + info->size;
	head->id = info->id;
	head->flagsFragOff = 0;
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

	info->size = head->len - 20;
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
	head->versionClassFlow = 6 | (info->flowinfo & ~0xF);		/* low-order 6 gets loaded into lowest byte */
	head->payloadLen = (uint16_t) info->size;
	head->nextHeader = (uint8_t) info->proto;
	head->hop = info->hop;
	memcpy(&head->saddr, &info->saddr, 16);
	memcpy(&head->daddr, &info->daddr, 16);
};

int ipv6_header2info(IPHeader6 *head, PacketInfo6 *info)
{
	if ((head->versionClassFlow & 0xF) != 6)
	{
		// not IPv6
		return -1;
	};
	
	info->size = head->payloadLen;
	info->proto = (int) head->nextHeader;
	info->hop = head->hop;
	info->dataOffset = 40;
	info->flowinfo = head->versionClassFlow & ~0xF;
	
	memcpy(&info->saddr, &head->saddr, 16);
	memcpy(&info->daddr, &head->daddr, 16);
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

static void loopbackSend(NetIf *netif, const void *addr, size_t addrlen, const void *packet, size_t packetlen)
{
	__sync_fetch_and_add(&netif->numTrans, 1);
	//onPacket(netif, packet, packetlen);
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
	semInit(&iflistLock);
	semInit(&loLock);
	semInit2(&loPacketCounter, 0);
	
	// the loopback thread shall call onPacket() on all packets waiting on the loopback interface.
	KernelThreadParams pars;
	pars.stackSize = DEFAULT_STACK_SIZE;
	pars.name = "inet_loopback";
	CreateKernelThread(loopbackThread, &pars, NULL);

	// the head of the interface list shall be the local loopback.
	static IPNetIfAddr4 loaddr4 = {{0x0100007F}, {0x000000FF}};
	static IPRoute4 lortab4 = {{0x0000007F}, {0x000000FF}, {0}};
	static IPNetIfAddr6 loaddr6 = {{{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1}},
						{{255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255}}};
	static IPRoute6 lortab6 = {{{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1}},
						{{255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255}},
						{{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}}};

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
	iflist.ipv4.dnsServers = NULL;
	iflist.ipv4.numDNSServers = 0;
	
	//iflist.ipv6.addrs = &loaddr6;
	iflist.ipv6.addrs = (IPNetIfAddr6*) kmalloc(sizeof(IPNetIfAddr6));
	memcpy(iflist.ipv6.addrs, &loaddr6, sizeof(IPNetIfAddr6));
	iflist.ipv6.numAddrs = 1;
	//iflist.ipv6.routes = &lortab6;
	iflist.ipv6.routes = (IPRoute6*) kmalloc(sizeof(IPRoute6));
	memcpy(iflist.ipv6.routes, &lortab6, sizeof(IPRoute6));
	iflist.ipv6.numRoutes = 1;
	iflist.ipv6.dnsServers = NULL;
	iflist.ipv6.numDNSServers = 0;
	
	iflist.numTrans = 0;
	iflist.numRecv = 0;
	iflist.numDropped = 0;
	iflist.numErrors = 0;
	
	iflist.send = loopbackSend;
	iflist.prev = NULL;
	iflist.next = NULL;
	
	initSocket();
};

void onPacket(NetIf *netif, const void *packet, size_t packetlen)
{
	if (packetlen < 1)
	{
		__sync_fetch_and_add(&netif->numDropped, 1);
		return;
	};
	
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
			__sync_fetch_and_add(&netif->numDropped, 1);
			return;
		};
		
		if (info.hop != 0) info.hop--;
		if (info.hop == 0)
		{
			// TODO: send a timeout packet when echo is enabled on the interface
			__sync_fetch_and_add(&netif->numDropped, 1);
			return;
		};
		
		if ((info.moreFrags) || (info.fragOff != 0))
		{
			// TODO: packet reassembler
			__sync_fetch_and_add(&netif->numDropped, 1);
			return;
		};
		
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
		if (info.hop == 0)
		{
			// TODO: send a timeout packet when echo is enabled on the interface
			__sync_fetch_and_add(&netif->numDropped, 1);
			return;
		};

		// TODO: reassembly! (handle protocol 44 for fragmented packets)
		__sync_fetch_and_add(&netif->numRecv, 1);
		
		// find out the source and destination address of the packet
		// the ports shall be set to zero; those are filled out by passPacketToSocket() according
		// to protocol.
		struct sockaddr_in6 src_addr;
		src_addr.sin6_family = AF_INET6;
		src_addr.sin6_port = 0;
		src_addr.sin6_flowinfo = info.flowinfo;
		memcpy(&src_addr.sin6_addr, &info.saddr, 16);
		src_addr.sin6_scope_id = 0xE;
		
		struct sockaddr_in6 dest_addr;
		dest_addr.sin6_family = AF_INET6;
		dest_addr.sin6_port = 0;
		dest_addr.sin6_flowinfo = info.flowinfo;
		memcpy(&dest_addr.sin6_addr, &info.daddr, 16);
		dest_addr.sin6_scope_id = 0xE;
		
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
	semWait(&iflistLock);
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
	
	semSignal(&iflistLock);
	return first;
};

QFileEntry *getRoutes6()
{
	semWait(&iflistLock);
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
	
	semSignal(&iflistLock);
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

int sendPacket4(const struct in_addr *addr_, const void *packet, size_t len)
{
	uint32_t addr = *((uint32_t*)addr_);

	semWait(&iflistLock);
	
	NetIf *netif = &iflist;
	while (netif != NULL)
	{
		int i;
		for (i=0; i<netif->ipv4.numRoutes; i++)
		{
			IPRoute4 *route = &netif->ipv4.routes[i];

			uint32_t dest = *((uint32_t*)&route->dest);
			uint32_t mask = *((uint32_t*)&route->mask);
			uint32_t gateway = *((uint32_t*)&route->gateway);
			
			if ((addr & mask) == (dest & mask))
			{
				if (gateway != 0)
				{
					addr = gateway;
				};
				
				if (netif->send == NULL)
				{
					semSignal(&iflistLock);
					return -1;
				};

				netif->send(netif, &addr, 4, packet, len);
				semSignal(&iflistLock);
				return 0;
			};
		};
		
		netif = netif->next;
	};
	
	semSignal(&iflistLock);
	return -1;
};

int sendPacket6(const struct in6_addr *addr_, const void *packet, size_t len)
{
	uint64_t *addr = ((uint64_t*)addr_);

	semWait(&iflistLock);
	
	NetIf *netif = &iflist;
	while (netif != NULL)
	{
		int i;
		for (i=0; i<netif->ipv6.numRoutes; i++)
		{
			IPRoute6 *route = &netif->ipv6.routes[i];

			uint64_t *dest = ((uint64_t*)&route->dest);
			uint64_t *mask = ((uint64_t*)&route->mask);
			uint64_t *gateway = ((uint64_t*)&route->gateway);

			if (((addr[0] & mask[0]) == (dest[0] & mask[0])) && ((addr[1] & mask[1]) == (dest[1] & mask[1])))
			{
				if ((gateway[0] != 0) || (gateway[1] != 0))
				{
					addr = gateway;
				};
				
				if (netif->send == NULL)
				{
					semSignal(&iflistLock);
					return -1;
				};

				netif->send(netif, addr, 16, packet, len);
				semSignal(&iflistLock);
				return 0;
			};
		};
		
		netif = netif->next;
	};
	
	semSignal(&iflistLock);
	return -1;
};

int sendPacket(const struct sockaddr *addr, const void *packet, size_t len)
{
	if (addr->sa_family == AF_INET)
	{
		const struct sockaddr_in *addr4 = (const struct sockaddr_in*) addr;
		return sendPacket4(&addr4->sin_addr, packet, len);
	}
	else if (addr->sa_family == AF_INET6)
	{
		const struct sockaddr_in6 *addr6 = (const struct sockaddr_in6*) addr;
		return sendPacket6(&addr6->sin6_addr, packet, len);
	}
	else
	{
		return -1;
	};
};

void getDefaultAddr4(struct in_addr *src, const struct in_addr *dest)
{
	uint32_t addr = *((uint32_t*)dest);

	semWait(&iflistLock);
	
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
				if (netif->ipv4.numAddrs == 0)
				{
					memset(src, 0, 4);
					semSignal(&iflistLock);
					return;
				};
				
				memcpy(src, &netif->ipv4.addrs[0].addr, 4);
				semSignal(&iflistLock);
				return;
			};
		};
		
		netif = netif->next;
	};
	
	memset(src, 0, 4);
	semSignal(&iflistLock);
};

void getDefaultAddr6(struct in6_addr *src, const struct in6_addr *dest)
{
	uint64_t *addr = ((uint64_t*)dest);

	semWait(&iflistLock);
	
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
				if (netif->ipv6.numAddrs == 0)
				{
					semSignal(&iflistLock);
					memset(src, 0, 16);
					return;
				};
				
				memcpy(src, &netif->ipv6.addrs[0].addr, 16);
				semSignal(&iflistLock);
				return;
			};
		};
		
		netif = netif->next;
	};
	
	memset(src, 0, 16);
	semSignal(&iflistLock);
};

int route_add(int family, int pos, gen_route *route)
{
	if ((family != AF_INET) && (family != AF_INET6))
	{
		getCurrentThread()->therrno = EINVAL;
		return -1;
	};
	
	if (getCurrentThread()->euid != 0)
	{
		getCurrentThread()->therrno = EACCES;
		return -1;
	};
	
	semWait(&iflistLock);
	
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
				kfree(oldRoutes);
				semSignal(&iflistLock);
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
				kfree(oldRoutes);
				semSignal(&iflistLock);
				return 0;
			};
		};
		
		netif = netif->next;
	};
	
	semSignal(&iflistLock);
	
	// the interface was not found!
	getCurrentThread()->therrno = ENOENT;
	return -1;
};

ssize_t netconf_stat(const char *ifname, NetStat *buffer, size_t size)
{
	NetStat netstat;
	semWait(&iflistLock);
	
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
			
			if (size > sizeof(NetStat))
			{
				size = sizeof(NetStat);
			};
			
			memcpy(buffer, &netstat, size);
			semSignal(&iflistLock);
			return (ssize_t) size;
		};
		
		netif = netif->next;
	};
	
	semSignal(&iflistLock);
	
	// the interface was not found!
	getCurrentThread()->therrno = ENOENT;
	return -1;
};

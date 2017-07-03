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

#include <sys/glidix.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>
#include <errno.h>
#include <time.h>
#include <dirent.h>
#include <unistd.h>
#include <fcntl.h>

#define	SPACES	" \t"

typedef struct NameResolution_
{
	struct NameResolution_*			next;
	char					nodename[256];
	struct in6_addr*			addrList6;
	size_t					addrCount6;
	struct in_addr*				addrList4;
	size_t					addrCount4;
	clock_t					expires;		// 0 = never, 1 = must be resolved now
} NameResolution;

typedef struct
{
	uint16_t				id;
	uint16_t				flags;
	uint16_t				numQuestions;
	uint16_t				numAnswers;
	uint16_t				numServers;
	uint16_t				numAdditional;
} DNSHeader;

/**
 * NOTE: This strucutre MUST be packed, since its size is not divisible by 4 but
 * the longest element is 4 bytes long; this, without packing, causes the compiler
 * to do weird alignment stuff and causes all returned addresses to be off by 2 bytes.
 */
typedef struct
{
	uint16_t				type;
	uint16_t				qclass;
	uint32_t				ttl;
	uint16_t				rdlen;
} __attribute__ ((packed)) RRBody;

struct addr_list
{
	struct addrinfo*			head;
	struct addrinfo*			tail;
};

typedef struct
{
	void*					data;
	size_t					size;
} PacketBuffer;

static pthread_mutex_t dnsLock = PTHREAD_MUTEX_INITIALIZER;
static int dnsInitDone = 0;
static NameResolution *dnsFirst = NULL;
static NameResolution *dnsLast = NULL;
static struct in6_addr *dnsServers = NULL;
static size_t dnsNumServers = 0;
static uint16_t dnsNextID = 0x100;

static void pktInit(PacketBuffer *buffer)
{
	buffer->data = NULL;
	buffer->size = 0;
};

static void pktAppend(PacketBuffer *buffer, const void *data, size_t size)
{
	buffer->data = realloc(buffer->data, buffer->size+size);
	memcpy((char*)buffer->data + buffer->size, data, size);
	buffer->size += size;
};

static void dnsPerformResolution(NameResolution *res)
{
	free(res->addrList4);
	free(res->addrList6);
	res->addrCount4 = 0;
	res->addrCount6 = 0;
	res->addrList4 = NULL;
	res->addrList6 = NULL;
	res->expires = 1;
	
	int sockfd = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);
	if (sockfd == -1)
	{
		fprintf(stderr, "***LIBC ERROR***: DNS resolver ciritical error: failed to create socket: %s\n", strerror(errno));
		return;
	};
	
	// set the timeout to 5 seconds
	struct timeval tv;
	tv.tv_sec = 5;
	tv.tv_usec = 0;
	if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(struct timeval)) != 0)
	{
		fprintf(stderr, "***LIBC ERROR***: DNS resolver critical error: failed to set socket timeout: %s\n",
			strerror(errno));
		return;
	};
	
	// since we're talking to both IPv4 and IPv6 nameservers, we want to force the socket to accept
	// IPv6 traffic. if the first server happens to be IPv4 for some weird reason, the socket will
	// be bound to an IPv4 address and refuse IPv6 traffic; so what we do is we bind it the IPv6
	// unspecified address (::), and to port 0, so that it binds to an arbitrary port but is still
	// IPv6-capable
	struct sockaddr_in6 laddr;
	memset(&laddr, 0, sizeof(struct sockaddr_in6));
	laddr.sin6_family = AF_INET6;
	
	if (bind(sockfd, (struct sockaddr*)&laddr, sizeof(struct sockaddr_in6)) != 0)
	{
		fprintf(stderr, "***LIBC ERROR***: DNS resolver critical error: failed to bind socket: %s\n", strerror(errno));
		close(sockfd);
		return;
	};
	
	// form a query for AAAA records and also a query for A records
	PacketBuffer query4, query6;
	pktInit(&query4);
	pktInit(&query6);
	
	// the header is the same for both except the ID
	uint16_t id = __sync_fetch_and_add(&dnsNextID, 1);
	uint16_t id4 = (id << 1);
	uint16_t id6 = id4 | 1;
	DNSHeader header;
	header.id = id4;					// IPv4
	header.flags = htons(0x0100);				// standard, recursive query
	header.numQuestions = htons(1);
	header.numAnswers = 0;
	header.numServers = 0;
	header.numAdditional = 0;
	
	pktAppend(&query4, &header, sizeof(DNSHeader));
	header.id = id6;					// IPv6
	pktAppend(&query6, &header, sizeof(DNSHeader));
	
	// the QNAME is the same for both
	const char *scan = res->nodename;
	while (strchr(scan, '.') != NULL)
	{
		char *dot = strchr(scan, '.');
		uint8_t index = (uint8_t) (dot-scan);
		if (index == 0)
		{
			// not a valid domain name
			close(sockfd);
			return;
		};
		
		pktAppend(&query4, &index, 1);
		pktAppend(&query6, &index, 1);
		
		pktAppend(&query4, scan, (size_t)index);
		pktAppend(&query6, scan, (size_t)index);
		
		scan = dot + 1;
	};
	
	uint8_t lastSize = (uint8_t) strlen(scan);
	pktAppend(&query4, &lastSize, 1);
	pktAppend(&query6, &lastSize, 1);
	
	pktAppend(&query4, scan, strlen(scan));
	pktAppend(&query6, scan, strlen(scan));
	
	// end of labels
	pktAppend(&query4, "", 1);
	pktAppend(&query6, "", 1);
	
	// type = 28 (AAAA) for the IPv6 query, and 1 (A) for the IPv4 query
	uint16_t type = htons(28);
	pktAppend(&query6, &type, 2);
	type = htons(1);
	pktAppend(&query4, &type, 2);
	
	// class is IN (1) for both
	uint16_t qclass = htons(1);
	pktAppend(&query4, &qclass, 2);
	pktAppend(&query6, &qclass, 2);
	
	// send the query to all DNS servers
	struct sockaddr_in6 daddr;
	memset(&daddr, 0, sizeof(struct sockaddr_in6));
	daddr.sin6_family = AF_INET6;
	daddr.sin6_port = htons(53);
	
	size_t i;
	for (i=0; i<dnsNumServers; i++)
	{
		memcpy(&daddr.sin6_addr, &dnsServers[i], 16);
		sendto(sockfd, query4.data, query4.size, 0, (struct sockaddr*)&daddr, sizeof(struct sockaddr_in6));
		sendto(sockfd, query6.data, query6.size, 0, (struct sockaddr*)&daddr, sizeof(struct sockaddr_in6));
	};
	
	// wait for responses
	struct sockaddr_in6 saddr;
	socklen_t addrlen;
	void *rcvbuf = malloc(65536);
	
	clock_t timeout = clock() + 5UL * CLOCKS_PER_SEC;
	int gotBack4 = 0;
	int gotBack6 = 0;
	uint32_t ttlMin = 0xFFFFFFFF;
	while ((clock() < timeout) && ((!gotBack4) || (!gotBack6)))
	{
		addrlen = sizeof(struct sockaddr_in6);
		ssize_t size = recvfrom(sockfd, rcvbuf, 65536, 0, (struct sockaddr*)&saddr, &addrlen);
		if (size == -1)
		{
			continue;
		};
		
		// make sure the response came from one of the configured DNS servers
		int ok = 0;
		for (i=0; i<dnsNumServers; i++)
		{
			if (memcmp(&saddr.sin6_addr, &dnsServers[i], 16) == 0)
			{
				ok = 1;
				break;
			};
		};
		
		if (!ok)
		{
			continue;
		};
		
		// make sure there is at least a DNS header
		if (size < sizeof(DNSHeader))
		{
			continue;
		};
		
		// make sure it is a response to one of our queries
		DNSHeader *hdr = (DNSHeader*) rcvbuf;
		if ((hdr->id != id4) && (hdr->id != id6))
		{
			continue;
		};
		
		// ensure that this is a response
		uint16_t flags = ntohs(hdr->flags);
		if ((flags & (1 << 15)) == 0)
		{
			// not a response
			continue;
		};
		
		// make sure we haven't received either query yet
		if (hdr->id == id4)
		{
			if (gotBack4) continue;
			gotBack4 = 1;
		}
		else
		{
			if (gotBack6) continue;
			gotBack6 = 1;
		};
		
		uint16_t numQuestions = ntohs(hdr->numQuestions);
		uint16_t numAnswers = ntohs(hdr->numAnswers);
		
		// skip the questions section, to get to the answer question
		uint8_t *scan = (uint8_t*) &hdr[1];
		size_t sizeLeft = (size_t)size - sizeof(DNSHeader);
		while (numQuestions--)
		{
			if (sizeLeft == 0)
			{
				// the response is invalid
				break;
			};
			
			while (*scan != 0)
			{
				uint8_t skip = (*scan) + 1;
				if ((skip & 0xC0) == 0xC0)
				{
					// "compressed domain name"
					if (sizeLeft < 2)
					{
						sizeLeft = 0;
						break;
					};
					
					sizeLeft -= 2;
					scan += 2;
					break;
				}
				else
				{
					if ((size_t)skip > sizeLeft)
					{
						sizeLeft = 0;
						break;
					};
				
					scan += skip;
					sizeLeft -= skip;
				};
			};
			
			if (sizeLeft < 5)
			{
				sizeLeft = 0;
				break;
			};
			
			sizeLeft -= 5;
			scan += 5;
		};
		
		if (sizeLeft == 0)
		{
			continue;
		};
		
		// that's it; we're at the answer section!
		// parse it
		while (numAnswers--)
		{
			char ansname[256];
			memset(ansname, 0, 256);
			
			uint8_t *domptr = scan;
			if (((*domptr) & 0xC0) == 0xC0)
			{
				uint16_t offset = ntohs(*((uint16_t*)scan)) & 0x3F;
				scan += 2;
				domptr = (uint8_t*) rcvbuf + offset;
			}
			else
			{
				while (*scan != 0)
				{
					uint8_t skip = (*scan) + 1;
					if ((size_t)skip > sizeLeft)
					{
						sizeLeft = 0;
						break;
					};
			
					scan += skip;
					sizeLeft -= skip;
				};
				
				if (sizeLeft < 1)
				{
					sizeLeft = 0;
					break;
				};
				
				scan++;
				sizeLeft--;
			};
			
			// parse the domain name
			char *put = ansname;
			if (*domptr == 0)
			{
				// invalid name; ignore the response
				break;
			};
			
			while (1)
			{
				uint8_t nextLen = *domptr;
				if (((size_t)nextLen + strlen(ansname)) > 255)
				{
					sizeLeft = 0;
					break;
				};
				
				domptr++;
				memcpy(put, domptr, (size_t)nextLen);
				domptr += nextLen;
				put += nextLen;
				
				if (*domptr == 0)
				{
					break;
				}
				else
				{
					if (strlen(ansname) >= 254)
					{
						sizeLeft = 0;
						break;
					};
					*put++ = '.';
				};
			};
			
			if (sizeLeft < sizeof(RRBody))
			{
				sizeLeft = 0;
				break;
			};
			
			RRBody *body = (RRBody*)scan;
			
			if (sizeLeft < (sizeof(RRBody)+ntohs(body->rdlen)))
			{
				sizeLeft = 0;
				break;
			};
			
			uint8_t *rdata = scan + sizeof(RRBody);
			scan += sizeof(RRBody) + ntohs(body->rdlen);
			sizeLeft -= (sizeof(RRBody) + ntohs(body->rdlen));
			
			if (strcmp(ansname, res->nodename) == 0)
			{
				if (ntohs(body->qclass) != 1)
				{
					// not IN
					continue;
				};
			
				if ((ntohs(body->type) == 1) && (ntohs(body->rdlen) == 4))
				{
					// add the IPv4 address
					res->addrList4 = (struct in_addr*) realloc(res->addrList4, 4*(res->addrCount4+1));
					memcpy(&res->addrList4[res->addrCount4], rdata, 4);
					res->addrCount4++;
				
					if (ntohs(body->ttl) < ttlMin)
					{
						ttlMin = ntohs(body->ttl);
					};
				}
				else if ((ntohs(body->type) == 28) && (ntohs(body->rdlen) == 16))
				{
					// add the IPv6 address
					res->addrList6 = (struct in6_addr*) realloc(res->addrList6, 16*(res->addrCount6+1));
					memcpy(&res->addrList6[res->addrCount6], rdata, 16);
					res->addrCount6++;
				
					if (ntohs(body->ttl) < ttlMin)
					{
						ttlMin = ntohs(body->ttl);
					};
				};
			};
		};
	};
	
	if (ttlMin == 0xFFFFFFFF)
	{
		// must immediately re-resolve on next call
		res->expires = 1;
	}
	else
	{
		// we can cache
		res->expires = clock() + (clock_t)ttlMin * CLOCKS_PER_SEC;
	};
	
	free(rcvbuf);
	close(sockfd);
};

static NameResolution *dnsCreateResolution(const char *nodename)
{
	NameResolution *res = (NameResolution*) malloc(sizeof(NameResolution));
	memset(res, 0, sizeof(NameResolution));
	res->expires = 1;
	strcpy(res->nodename, nodename);
	return res;
};

static NameResolution *dnsGetResolution(const char *nodename, int make)
{
	if (dnsFirst == NULL)
	{
		if (make)
		{
			dnsFirst = dnsLast = dnsCreateResolution(nodename);
			return dnsFirst;
		}
		else
		{
			return NULL;
		};
	}
	else
	{
		NameResolution *res;
		for (res=dnsFirst; res!=NULL; res=res->next)
		{
			if (strcmp(res->nodename, nodename) == 0)
			{
				return res;
			};
		};
		
		if (make)
		{
			NameResolution *res = dnsCreateResolution(nodename);
			dnsLast->next = res;
			dnsLast = res;
			return res;
		}
		else
		{
			return NULL;
		};
	};
};

static void dnsAddResolution(int family, const char *nodename, const void *addrbuf)
{
	NameResolution *res = dnsGetResolution(nodename, 1);
	res->expires = 0;
	
	if (family == AF_INET)
	{
		res->addrList4 = (struct in_addr*) realloc(res->addrList4, 4*(res->addrCount4+1));
		memcpy(&res->addrList4[res->addrCount4], addrbuf, 4);
		res->addrCount4++;
	}
	else
	{
		res->addrList6 = (struct in6_addr*) realloc(res->addrList6, 16*(res->addrCount6+1));
		memcpy(&res->addrList6[res->addrCount6], addrbuf, 16);
		res->addrCount6++;
	};
};

static void dnsReadServerFile(const char *filename)
{
	FILE *fp = fopen(filename, "r");
	if (fp == NULL)
	{
		fprintf(stderr, "***LIBC ERROR***: Failed to open %s for read: %s\n", filename, strerror(errno));
		return;
	};
	
	char line[64];
	while (fgets(line, 64, fp) != NULL)
	{
		if (line[0] == '#')
		{
			continue;
		};
		
		char *endline = strchr(line, '\n');
		if (endline == NULL)
		{
			continue;
		};
		
		*endline = 0;
		
		struct in6_addr addr;
		if (!inet_pton(AF_INET6, line, &addr))
		{
			fprintf(stderr, "***LIBC ERROR***: Invalid DNS server in %s: %s\n", filename, line);
			continue;
		};
		
		dnsServers = realloc(dnsServers, 16*(dnsNumServers+1));
		memcpy(&dnsServers[dnsNumServers], &addr, 16);
		dnsNumServers++;
	};
	
	fclose(fp);
};

static void dnsReadServers(const char *dirname)
{
	char filename[256];
	
	DIR *dirp = opendir(dirname);
	if (dirp == NULL) return;
	
	struct dirent *ent;
	while ((ent = readdir(dirp)) != NULL)
	{
		if (ent->d_name[0] != '.')
		{
			sprintf(filename, "%s/%s", dirname, ent->d_name);
			dnsReadServerFile(filename);
		};
	};
	
	closedir(dirp);
};

static void dnsInit()
{
	dnsInitDone = 1;
	
	// first try to get permanent resolutions from /etc/hosts
	FILE *fp = fopen("/etc/hosts", "r");
	if (fp != NULL)
	{
		char *line = (char*) malloc(4096);
		char *saveptr;
		while (fgets(line, 4096, fp) != NULL)
		{
			char *endline = strchr(line, '\n');
			if (endline == NULL)
			{
				// line too long; ignore
				continue;
			};
			
			*endline = 0;
			
			char *addrstr = strtok_r(line, SPACES, &saveptr);
			if (addrstr == NULL)
			{
				// empty line, it's OK
				continue;
			};
			
			if (addrstr[0] == '#')
			{
				// comment
				continue;
			};
			
			int family;
			char addrbuf[16];
			
			if (inet_pton(AF_INET, addrstr, addrbuf))
			{
				family = AF_INET;
			}
			else if (inet_pton(AF_INET6, addrstr, addrbuf))
			{
				family = AF_INET6;
			}
			else
			{
				fprintf(stderr, "***LIBC ERROR***: invalid address '%s' in /etc/hosts\n", addrstr);
				continue;
			};
			
			char *nodename;
			while ((nodename = strtok_r(NULL, SPACES, &saveptr)) != NULL)
			{
				if (strlen(nodename) < 256)
				{
					dnsAddResolution(family, nodename, addrbuf);
				};
			};
		};
		
		free(line);
		fclose(fp);
	};
	
	// load nameservers from all necessary directories
	// we read the IPv6 ones before the IPv4 ones
	dnsReadServers("/run/dns/ipv6");
	dnsReadServers("/run/dns/ipv4");
};

static int __getsrvport(const char *servname, int *resp)
{
	int result = 0;
	for (; *servname!=0; servname++)
	{
		char c = *servname;
		if ((c < '0') || (c > '9'))
		{
			return -1;
		};
		
		result = result * 10 + (c-'0');
	};
	
	*resp = result;
	return 0;
};

void __add_addr(struct addr_list *list, const struct addrinfo *hints, struct addrinfo *info)
{
	info->ai_addr = (struct sockaddr*) &info->__ai_storage;
	
	if (hints != NULL)
	{
		if ((hints->ai_family != AF_UNSPEC) && (hints->ai_family != info->ai_family))
		{
			return;
		};
		
		if ((hints->ai_socktype != 0) && (hints->ai_socktype != info->ai_socktype))
		{
			return;
		};
		
		info->ai_protocol = hints->ai_protocol;
		
		if ((hints->ai_flags & AI_V4MAPPED) && (info->ai_addr->sa_family == AF_INET))
		{
			char addr4[4];
			memcpy(addr4, &(((struct sockaddr_in*)info->ai_addr)->sin_addr), 4);
			unsigned short portno = ((struct sockaddr_in*)info->ai_addr)->sin_port;
			
			struct sockaddr_in6 *inaddr = (struct sockaddr_in6*) info->ai_addr;
			memset(inaddr, 0, sizeof(struct sockaddr_in6));
			inaddr->sin6_family = AF_INET6;
			inaddr->sin6_port = htons(portno);
			inaddr->sin6_flowinfo = 0;
			
			memset(&inaddr->sin6_addr.s6_addr[0], 0, 10);
			memset(&inaddr->sin6_addr.s6_addr[10], 0xFF, 2);
			memcpy(&inaddr->sin6_addr.s6_addr[12], addr4, 4);
			
			inaddr->sin6_scope_id = 0;
			info->ai_addrlen = sizeof(struct sockaddr_in6);
		};
	};
	
	struct addrinfo *ai = (struct addrinfo*) malloc(sizeof(struct addrinfo));
	memcpy(ai, info, sizeof(struct addrinfo));
	ai->ai_addr = (struct sockaddr*) &ai->__ai_storage;
	ai->ai_next = NULL;
	
	if (list->head == NULL)
	{
		list->head = list->tail = ai;
	}
	else
	{
		list->tail->ai_next = ai;
		list->tail = ai;
	};
};

static int __parse_zone_name(const char *zonename, uint32_t *out)
{
	/* "0" is not a valid zone */
	if (strcmp(zonename, "0") == 0)
	{
		return -1;
	};
	
	if (sscanf(zonename, "%u", out) == 1)
	{
		return 0;
	};
	
	_glidix_netstat netstat;
	if (_glidix_netconf_stat(zonename, &netstat, sizeof(_glidix_netstat)) == -1)
	{
		return -1;
	};
	
	*out = netstat.scopeID;
	return 0;
};

int getaddrinfo(const char *nodename, const char *servname, const struct addrinfo *hints, struct addrinfo **out)
{
	int portno = 0;
	
	if (servname != NULL)
	{
		if (__getsrvport(servname, &portno) != 0)
		{
			return EAI_SERVICE;
		};
	};
	
	struct addr_list list = {NULL, NULL};
	
	if (nodename == NULL)
	{
		struct addrinfo wildcard_v4, wildcard_v6;
		memset(&wildcard_v4, 0, sizeof(struct addrinfo));
		memset(&wildcard_v6, 0, sizeof(struct addrinfo));
		
		wildcard_v6.ai_family = AF_INET6;
		wildcard_v6.ai_socktype = SOCK_STREAM;
		wildcard_v6.ai_addrlen = sizeof(struct sockaddr_in6);

		struct sockaddr_in6 *inaddr6 = (struct sockaddr_in6*) &wildcard_v6.__ai_storage;
		inaddr6->sin6_family = AF_INET6;
		inaddr6->sin6_port = htons((unsigned short) portno);
		
		__add_addr(&list, hints, &wildcard_v6);
		wildcard_v6.ai_socktype = SOCK_DGRAM;
		__add_addr(&list, hints, &wildcard_v6);
		
		wildcard_v4.ai_family = AF_INET;
		wildcard_v4.ai_socktype = SOCK_STREAM;
		wildcard_v4.ai_addrlen = sizeof(struct sockaddr_in);
		
		struct sockaddr_in *inaddr = (struct sockaddr_in*) &wildcard_v4.__ai_storage;
		inaddr->sin_family = AF_INET;
		inaddr->sin_port = htons((unsigned short) portno);
		
		__add_addr(&list, hints, &wildcard_v4);
		wildcard_v4.ai_socktype = SOCK_DGRAM;
		__add_addr(&list, hints, &wildcard_v4);
	}
	else
	{
		char addrbuf[16];
		if (inet_pton(AF_INET, nodename, addrbuf))
		{
			// we have an IPv4 address
			struct addrinfo info;
			memset(&info, 0, sizeof(struct addrinfo));
			
			info.ai_family = AF_INET;
			info.ai_socktype = SOCK_STREAM;
			info.ai_addrlen = sizeof(struct sockaddr_in);
			
			struct sockaddr_in *inaddr = (struct sockaddr_in*) &info.__ai_storage;
			inaddr->sin_family = AF_INET;
			memcpy(&inaddr->sin_addr, addrbuf, 4);
			inaddr->sin_port = htons((unsigned short) portno);
			
			__add_addr(&list, hints, &info);
			info.ai_socktype = SOCK_DGRAM;
			__add_addr(&list, hints, &info);
		}
		else if (inet_pton(AF_INET6, nodename, addrbuf))
		{
			// we have an IPv6 address
			struct addrinfo info;
			memset(&info, 0, sizeof(struct addrinfo));
			
			info.ai_family = AF_INET6;
			info.ai_socktype = SOCK_STREAM;
			info.ai_addrlen = sizeof(struct sockaddr_in6);
			
			struct sockaddr_in6 *inaddr = (struct sockaddr_in6*) &info.__ai_storage;
			inaddr->sin6_family = AF_INET6;
			memcpy(&inaddr->sin6_addr, addrbuf, 16);
			inaddr->sin6_port = htons((unsigned short) portno);
			__add_addr(&list, hints, &info);
			info.ai_socktype = SOCK_DGRAM;
			__add_addr(&list, hints, &info);
		}
		else if (strchr(nodename, '%') != NULL)
		{
			// IPv6 address with zone index
			char *zoneptr = strchr(nodename, '%');
			off_t pos = zoneptr - nodename;
			
			if (pos < INET6_ADDRSTRLEN)
			{
				char v6addr[INET6_ADDRSTRLEN];
				memcpy(v6addr, nodename, (size_t)pos);
				v6addr[pos] = 0;
				const char *zonename = zoneptr + 1;
				
				if (inet_pton(AF_INET6, v6addr, addrbuf))
				{
					struct addrinfo info;
					memset(&info, 0, sizeof(struct addrinfo));
			
					info.ai_family = AF_INET6;
					info.ai_socktype = SOCK_STREAM;
					info.ai_addrlen = sizeof(struct sockaddr_in6);
					
					struct sockaddr_in6 *inaddr = (struct sockaddr_in6*) &info.__ai_storage;
					inaddr->sin6_family = AF_INET6;
					memcpy(&inaddr->sin6_addr, addrbuf, 16);
					inaddr->sin6_port = htons((unsigned short) portno);
					
					if (__parse_zone_name(zonename, &inaddr->sin6_scope_id) == 0)
					{
						__add_addr(&list, hints, &info);
						info.ai_socktype = SOCK_DGRAM;
						__add_addr(&list, hints, &info);
					};
				};
			};
		}
		else
		{
			pthread_mutex_lock(&dnsLock);
			if (!dnsInitDone)
			{
				dnsInit();
			};
			
			NameResolution *res = dnsGetResolution(nodename, 1);
			if (res->expires != 0)
			{
				if (res->expires <= clock())
				{
					dnsPerformResolution(res);
				};
			};
			
			size_t i;
			for (i=0; i<res->addrCount6; i++)
			{
				struct addrinfo info;
				memset(&info, 0, sizeof(struct addrinfo));
				
				info.ai_family = AF_INET6;
				info.ai_socktype = SOCK_STREAM;
				info.ai_addrlen = sizeof(struct sockaddr_in6);
				
				struct sockaddr_in6 *inaddr = (struct sockaddr_in6*) &info.__ai_storage;
				inaddr->sin6_family = AF_INET6;
				memcpy(&inaddr->sin6_addr, &res->addrList6[i], 16);
				inaddr->sin6_port = htons((unsigned short) portno);
				__add_addr(&list, hints, &info);
				info.ai_socktype = SOCK_DGRAM;
				__add_addr(&list, hints, &info);
			};
			
			for (i=0; i<res->addrCount4; i++)
			{
				struct addrinfo info;
				memset(&info, 0, sizeof(struct addrinfo));
				
				info.ai_family = AF_INET;
				info.ai_socktype = SOCK_STREAM;
				info.ai_addrlen = sizeof(struct sockaddr_in);
				
				struct sockaddr_in *inaddr = (struct sockaddr_in*) &info.__ai_storage;
				inaddr->sin_family = AF_INET;
				memcpy(&inaddr->sin_addr, &res->addrList4[i], 4);
				inaddr->sin_port = htons((unsigned short) portno);
				__add_addr(&list, hints, &info);
				info.ai_socktype = SOCK_DGRAM;
				__add_addr(&list, hints, &info);
			};
			
			pthread_mutex_unlock(&dnsLock);
		};
	};
	
	if (list.head == NULL)
	{
		return EAI_NONAME;
	};
	
	*out = list.head;
	return 0;
};

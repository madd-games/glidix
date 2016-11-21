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

#include <glidix/socket.h>
#include <glidix/memory.h>
#include <glidix/string.h>
#include <glidix/sched.h>
#include <glidix/errno.h>
#include <glidix/semaphore.h>
#include <glidix/netif.h>
#include <glidix/console.h>
#include <glidix/common.h>
#include <glidix/random.h>
#include <glidix/time.h>

/**
 * TCP socket states.
 */
#define	TCP_CLOSED				0
#define	TCP_LISTEN				1
#define	TCP_CONNECTING				2
#define	TCP_ESTABLISHED				3
#define	TCP_TERMINATED				4

/**
 * TCP segment flags
 */
#define	TCP_FIN					(1 << 0)
#define	TCP_SYN					(1 << 1)
#define	TCP_RST					(1 << 2)
#define	TCP_PSH					(1 << 3)
#define	TCP_ACK					(1 << 4)
#define	TCP_URG					(1 << 5)
#define	TCP_ECE					(1 << 6)
#define	TCP_CWR					(1 << 7)

/**
 * TCP timeouts.
 */
#define	TCP_RETRANS_TIMEOUT			NT_MILLI(200)

/**
 * Size of TCP buffers (the size of the receive buffer, and of the send buffer).
 */
#define	TCP_BUFFER_SIZE				0xFFFF

typedef struct
{
	uint16_t				srcport;
	uint16_t				dstport;
	uint32_t				seqno;
	uint32_t				ackno;
	uint8_t					dataOffsetNS;
	uint8_t					flags;
	uint16_t				winsz;
	uint16_t				checksum;
	uint16_t				urgptr;
} TCPSegment;

typedef struct
{
	uint32_t				saddr;
	uint32_t				daddr;
	uint8_t					zero;
	uint8_t					proto;
	uint16_t				len;
	TCPSegment				seg;
} TCPEncap4;

typedef struct
{
	uint8_t					saddr[16];
	uint8_t					daddr[16];
	uint32_t				len;
	uint8_t					zero[3];
	uint8_t					proto;
	TCPSegment				seg;
} TCPEncap6;

/**
 * Represents a segment in the outbound queue.
 */
typedef struct
{
	/**
	 * Size of the segment.
	 */
	size_t					size;
	
	/**
	 * Size of the segment + the pseudo-header.
	 */
	size_t					pseudoSize;
	
	/**
	 * Points to the start of the actual segment within 'data'.
	 */
	TCPSegment*				segment;
	
	/**
	 * The data; this is prefixed with the pseudo-header which is NOT sent! The
	 * pseudo-header is not taken into account when computing 'size'. The 'segment'
	 * field points to the aqctual data to be sent.
	 */
	char					data[];
} TCPOutbound;

/**
 * TCP SOCKET
 *
 * Implementation of the TCP protocol in Glidix.
 */
typedef struct
{
	Socket					header_;
	struct sockaddr				sockname;
	struct sockaddr				peername;
	int					shutflags;
	uint32_t				nextSeqNo;	// next sequence number to send
	uint32_t				nextAckNo;	// next sequence number to receive = next ACK to send
	int					state;
	Semaphore				lock;
	
	/**
	 * This semaphore is signalled once; when the socket becomes connected.
	 */
	Semaphore				semConnected;
	
	/**
	 * Current error on the socket (0 = no error).
	 */
	int					sockErr;
	
	/**
	 * The current packet waiting to be acknowledged; the handler thread will re-send it every 200ms
	 * until an ACK with number expectedAck is received; when this happens, the semAck semaphore
	 * is signalled. Please access the expectedAck field atomically. A value of (1 << 32) in the
	 * expectedAck field means we're not expecting anything to be acknowledged yet.
	 */
	Semaphore				semAck;
	TCPOutbound*				currentOut;
	uint64_t				expectedAck;
	
	/**
	 * The handler thread.
	 */
	Thread*					thread;
	
	/**
	 * This semaphore is signalled to tell the handler thread to terminate.
	 */
	Semaphore				semStop;
	
	/**
	 * This semaphore is signalled every time a segment is received and must be ACK-ed.
	 * More specifically, every time the nextAckNo changes.
	 */
	Semaphore				semAckOut;
	
	/**
	 * The send buffer, the semaphore that counts the number of bytes that can still be put
	 * in, a semaphore that counts the number of bytes that may be fetched, and the put/fetch
	 * pointers. This is a ring buffer.
	 */
	uint8_t					bufSend[TCP_BUFFER_SIZE];
	Semaphore				semSendPut;
	Semaphore				semSendFetch;
	size_t					idxSendPut;
	size_t					idxSendFetch;
	
	/**
	 * Same as above but for receive; we use a counter instead of a semaphore for counting bytes
	 * to be put in, because we never block.
	 */
	uint8_t					bufRecv[TCP_BUFFER_SIZE];
	size_t					cntRecvPut;
	Semaphore				semRecvFetch;
	size_t					idxRecvPut;
	size_t					idxRecvFetch;
} TCPSocket;

static int tcpsock_bind(Socket *sock, const struct sockaddr *addr, size_t addrlen)
{
	TCPSocket *tcpsock = (TCPSocket*) sock;
	if (addrlen < sizeof(struct sockaddr))
	{
		ERRNO = EAFNOSUPPORT;
		return -1;
	};
	
	if (tcpsock->state != TCP_CLOSED)
	{
		ERRNO = EAFNOSUPPORT;
		return -1;
	};
	
	if (tcpsock->sockname.sa_family != AF_UNSPEC)
	{
		ERRNO = EINVAL;
		return -1;
	};
	
	if (addr->sa_family != sock->domain)
	{
		ERRNO = EAFNOSUPPORT;
		return -1;
	};
	
	uint16_t port;
	if (addr->sa_family == AF_INET)
	{
		const struct sockaddr_in *inaddr = (const struct sockaddr_in*) addr;
		port = inaddr->sin_port;
	}
	else
	{
		const struct sockaddr_in6 *inaddr = (const struct sockaddr_in6*) addr;
		port = inaddr->sin6_port;
	};
	
	// convert port to machine order and then ensure we have permission to bind to it
	port = __builtin_bswap16(port);
	if (port < 1024)
	{
		if (getCurrentThread()->creds->euid != 0)
		{
			// only root can bind to ports below 1024
			ERRNO = EACCES;
			return -1;
		};
	};
	
	if (port >= 49152)
	{
		// nobody can bind to ephemeral ports explicitly
		ERRNO = EACCES;
		return -1;
	};
	
	if (ClaimSocketAddr(addr, &tcpsock->sockname) != 0)
	{
		ERRNO = EADDRINUSE;
		return -1;
	};
	
	return 0;
};

static TCPOutbound* CreateOutbound(const struct sockaddr *src, const struct sockaddr *dest, size_t dataSize)
{
	size_t segmentSize = sizeof(TCPSegment) + dataSize;
	if (src->sa_family == AF_INET)
	{
		const struct sockaddr_in *insrc = (const struct sockaddr_in*) src;
		const struct sockaddr_in *indst = (const struct sockaddr_in*) dest;
		
		TCPOutbound *ob = (TCPOutbound*) kmalloc(sizeof(TCPOutbound) + sizeof(TCPEncap4) + dataSize);
		ob->size = segmentSize;
		ob->pseudoSize = sizeof(TCPEncap4) + dataSize;
		
		TCPEncap4 *encap = (TCPEncap4*) ob->data;
		memset(encap, 0, sizeof(TCPEncap4)+dataSize);
		
		ob->segment = &encap->seg;
		encap->saddr = insrc->sin_addr.s_addr;
		encap->daddr = indst->sin_addr.s_addr;
		encap->proto = IPPROTO_TCP;
		encap->len = htons((uint16_t) segmentSize);
		
		return ob;
	}
	else if (src->sa_family == AF_INET6)
	{
		const struct sockaddr_in6 *insrc = (const struct sockaddr_in6*) src;
		const struct sockaddr_in6 *indst = (const struct sockaddr_in6*) dest;
		
		TCPOutbound *ob = (TCPOutbound*) kmalloc(sizeof(TCPOutbound) + sizeof(TCPEncap6) + dataSize);
		ob->size = segmentSize;
		ob->pseudoSize = sizeof(TCPEncap6) + dataSize;
		
		TCPEncap6 *encap = (TCPEncap6*) ob->data;
		memset(encap, 0, sizeof(TCPEncap6*)+dataSize);
		
		ob->segment = &encap->seg;
		memcpy(encap->saddr, &insrc->sin6_addr, 16);
		memcpy(encap->daddr, &indst->sin6_addr, 16);
		encap->len = htonl((uint32_t)segmentSize);
		encap->proto = IPPROTO_TCP;
		
		return ob;
	}
	else
	{
		panic("CreateUnbound() called with invalid address family (this is a bug)");
		return NULL;
	};
};

static void ChecksumOutbound(TCPOutbound *ob)
{
	ob->segment->checksum = 0;
	ob->segment->checksum = ipv4_checksum(ob->data, ob->pseudoSize);
};

static uint16_t ValidateChecksum(const struct sockaddr *src, const struct sockaddr *dest, const void *packet, size_t packetSize)
{
	if (src->sa_family == AF_INET)
	{
		const struct sockaddr_in *insrc = (const struct sockaddr_in*) src;
		const struct sockaddr_in *indst = (const struct sockaddr_in*) dest;
		
		TCPEncap4 *encap = (TCPEncap4*) kalloca(sizeof(TCPEncap4) + packetSize - sizeof(TCPSegment));
		memset(encap, 0, sizeof(TCPEncap4));
		memcpy(&encap->seg, packet, packetSize);
		encap->saddr = insrc->sin_addr.s_addr;
		encap->daddr = indst->sin_addr.s_addr;
		encap->proto = IPPROTO_TCP;
		encap->len = htons((uint16_t) packetSize);
		
		return ipv4_checksum(encap, sizeof(TCPEncap4) + packetSize - sizeof(TCPSegment));
	}
	else if (src->sa_family == AF_INET6)
	{
		const struct sockaddr_in6 *insrc = (const struct sockaddr_in6*) src;
		const struct sockaddr_in6 *indst = (const struct sockaddr_in6*) dest;
		
		TCPEncap6 *encap = (TCPEncap6*) kalloca(sizeof(TCPEncap6) + packetSize - sizeof(TCPSegment));
		memset(encap, 0, sizeof(TCPEncap6));
		memcpy(&encap->seg, packet, packetSize);
		memcpy(encap->saddr, &insrc->sin6_addr, 16);
		memcpy(encap->daddr, &indst->sin6_addr, 16);
		encap->proto = IPPROTO_TCP;
		encap->len = htonl((uint32_t) packetSize);
		
		return ipv4_checksum(encap, sizeof(TCPEncap6) + packetSize - sizeof(TCPSegment));
	}
	else
	{
		panic("invalid address family passed to ValidateChecksum()");
		return 0;
	};
};

static void tcpThread(void *context)
{
	Socket *sock = (Socket*) context;
	TCPSocket *tcpsock = (TCPSocket*) context;
	Semaphore *sems[4] = {&tcpsock->semConnected, &tcpsock->semStop, &tcpsock->semAck, &tcpsock->semAckOut};
	Semaphore *semsOut[3] = {&tcpsock->semStop, &tcpsock->semAckOut, &tcpsock->semSendFetch};

	uint16_t srcport, dstport;
	if (sock->domain == AF_INET)
	{
		const struct sockaddr_in *inaddr = (const struct sockaddr_in*) &tcpsock->peername;
		struct sockaddr_in *inname = (struct sockaddr_in*) &tcpsock->sockname;
		srcport = inname->sin_port;
		dstport = inaddr->sin_port;
	}
	else
	{
		const struct sockaddr_in6 *inaddr = (const struct sockaddr_in6*) &tcpsock->peername;
		struct sockaddr_in6 *inname = (struct sockaddr_in6*) &tcpsock->sockname;
		inname->sin6_family = AF_INET6;
		srcport = inname->sin6_port;
		dstport = inaddr->sin6_port;
	};

	int connectDone = 0;
	while (1)
	{
		uint64_t deadline = getNanotime() + sock->options[GSO_SNDTIMEO];
		if (sock->options[GSO_SNDTIMEO] == 0)
		{
			deadline = 0;
		};
		
		int sendOK = 0;
		while ((getNanotime() < deadline) || (deadline == 0))
		{
			tcpsock->currentOut->segment->ackno = htonl(tcpsock->nextAckNo);
			ChecksumOutbound(tcpsock->currentOut);
			
			int status = sendPacketEx(&tcpsock->sockname, &tcpsock->peername,
							tcpsock->currentOut->segment, tcpsock->currentOut->size,
							IPPROTO_TCP, sock->options, sock->ifname);

			if (status != 0)
			{
				tcpsock->sockErr = -status;

				if (tcpsock->state == TCP_CONNECTING)
				{
					semSignal(&tcpsock->semConnected);
				};
			
				tcpsock->state = TCP_TERMINATED;
				kfree(tcpsock->currentOut);
				return;
			};
			
			uint8_t bitmap = 0;
			if (semPoll(4, sems, &bitmap, 0, TCP_RETRANS_TIMEOUT) == 0)
			{
				continue;
			};
			
			if (bitmap & (1 << 1))
			{
				kfree(tcpsock->currentOut);
				tcpsock->state = TCP_TERMINATED;
				return;
			};
			
			if (bitmap & (1 << 2))
			{
				semWait(&tcpsock->semAck);
				sendOK = 1;
				break;
			};
			
			if (bitmap & (1 << 3))
			{
				semWaitGen(&tcpsock->semAckOut, -1, 0, 0);
			};
		};
		
		kfree(tcpsock->currentOut);
		
		if (!sendOK)
		{
			tcpsock->sockErr = ETIMEDOUT;
			
			if (tcpsock->state == TCP_CONNECTING)
			{
				semSignal(&tcpsock->semConnected);
			};
			
			tcpsock->state = TCP_TERMINATED;
			return;
		};
		
		if (!connectDone)
		{
			while ((getNanotime() < deadline) || (deadline == 0))
			{
				uint8_t bitmap = 0;
				semPoll(3, sems, &bitmap, 0, sock->options[GSO_SNDTIMEO]);
			
				if (bitmap & (1 << 1))
				{
					tcpsock->state = TCP_TERMINATED;
					return;
				};
				
				if (bitmap & (1 << 0))
				{
					connectDone = 1;
					break;
				};
			};

			if (!connectDone)
			{
				tcpsock->sockErr = ETIMEDOUT;
				semSignal(&tcpsock->semConnected);
				tcpsock->state = TCP_TERMINATED;
				return;
			};

			sems[0] = NULL;
			tcpsock->state = TCP_ESTABLISHED;
		};
		
		while (1)
		{
			uint8_t bitmap = 0;
			semPoll(3, semsOut, &bitmap, 0, 0);
		
			if (bitmap & (1 << 0))
			{
				tcpsock->state = TCP_TERMINATED;
				return;
			};
		
			if (bitmap & (1 << 2))
			{
				if (bitmap & (1 << 1))
				{
					semWaitGen(&tcpsock->semAckOut, -1, 0, 0);
				};
				
				int count = semWaitGen(&tcpsock->semSendFetch, 512, 0, 0);
				
				TCPOutbound *ob = CreateOutbound(&tcpsock->sockname, &tcpsock->peername, (size_t)count);
				ob->segment->srcport = srcport;
				ob->segment->dstport = dstport;
				ob->segment->seqno = htonl(tcpsock->nextSeqNo);
				// ackno filled in at the start of the loop iteration
				ob->segment->dataOffsetNS = 0x50;
				ob->segment->flags = TCP_PSH | TCP_ACK;
				ob->segment->winsz = 0xFFFF;
				uint8_t *put = (uint8_t*) &ob->segment[1];
				uint32_t size = (uint32_t)count;
				while (count--)
				{
					*put++ = tcpsock->bufSend[tcpsock->idxSendFetch];
					tcpsock->idxSendFetch = (tcpsock->idxSendFetch+1) % TCP_BUFFER_SIZE;
				};

				tcpsock->nextSeqNo += size;
				tcpsock->expectedAck = tcpsock->nextSeqNo;
				tcpsock->currentOut = ob;
				semSignal2(&tcpsock->semSendPut, (int)size);
				break;		// continues the outer loop
			};
			
			if (bitmap & (1 << 1))
			{
				semWaitGen(&tcpsock->semAckOut, -1, 0, 0);
			
				TCPOutbound *ob = CreateOutbound(&tcpsock->sockname, &tcpsock->peername, 0);
				TCPSegment *ack = ob->segment;
				
				ack->srcport = srcport;
				ack->dstport = dstport;
				ack->seqno = htonl(tcpsock->nextSeqNo);
				ack->ackno = htonl(tcpsock->nextAckNo);
				ack->dataOffsetNS = 0x50;
				ack->flags = TCP_ACK;
				ack->winsz = 0xFFFF;
				ChecksumOutbound(ob);
				
				sendPacketEx(&tcpsock->sockname, &tcpsock->peername,
						ob->segment, ob->size,
						IPPROTO_TCP, sock->options, sock->ifname);
				
				kfree(ob);
			};
		};
	};
};

static int tcpsock_connect(Socket *sock, const struct sockaddr *addr, size_t size)
{
	TCPSocket *tcpsock = (TCPSocket*) sock;
	if (addr->sa_family != sock->domain)
	{
		ERRNO = EAFNOSUPPORT;
		return -1;
	};
	
	if (size != sizeof(struct sockaddr))
	{
		ERRNO = EAFNOSUPPORT;
		return -1;
	};
	
	semWait(&tcpsock->lock);
	if (tcpsock->state != TCP_CLOSED)
	{
		semSignal(&tcpsock->lock);
		ERRNO = EALREADY;
		return -1;
	};
	
	uint16_t srcport, dstport;
	if (sock->domain == AF_INET)
	{
		const struct sockaddr_in *inaddr = (const struct sockaddr_in*) addr;
		struct sockaddr_in *inname = (struct sockaddr_in*) &tcpsock->sockname;
		inname->sin_family = AF_INET;
		getDefaultAddr4(&inname->sin_addr, &inaddr->sin_addr);
		inname->sin_port = AllocPort();
		srcport = inname->sin_port;
		dstport = inaddr->sin_port;
	}
	else
	{
		const struct sockaddr_in6 *inaddr = (const struct sockaddr_in6*) addr;
		struct sockaddr_in6 *inname = (struct sockaddr_in6*) &tcpsock->sockname;
		inname->sin6_family = AF_INET6;
		getDefaultAddr6(&inname->sin6_addr, &inaddr->sin6_addr);
		inname->sin6_port = AllocPort();
		srcport = inname->sin6_port;
		dstport = inaddr->sin6_port;
	};
	
	memcpy(&tcpsock->peername, addr, sizeof(struct sockaddr));
	tcpsock->state = TCP_CONNECTING;
	
	tcpsock->nextSeqNo = (uint32_t) getRandom();
	
	tcpsock->currentOut = CreateOutbound(&tcpsock->sockname, &tcpsock->peername, 0);
	tcpsock->expectedAck = tcpsock->nextSeqNo;
	TCPSegment *syn = tcpsock->currentOut->segment;
	syn->srcport = srcport;
	syn->dstport = dstport;
	syn->seqno = htonl(tcpsock->nextSeqNo-1);
	syn->dataOffsetNS = 0x50;
	syn->flags = TCP_SYN;
	syn->winsz = 0xFFFF;
	ChecksumOutbound(tcpsock->currentOut);
	
	KernelThreadParams pars;
	memset(&pars, 0, sizeof(KernelThreadParams));
	pars.stackSize = DEFAULT_STACK_SIZE;
	pars.name = "TCP Thread";
	tcpsock->thread = CreateKernelThread(tcpThread, &pars, tcpsock);
	
	semSignal(&tcpsock->lock);
	
	uint8_t bitmap = 0;
	Semaphore *semConn = &tcpsock->semConnected;
	if (semPoll(1, &semConn, &bitmap, SEM_W_FILE(sock->fp->oflag), 0) == 0)
	{
		// we caught an interrupt before the connection was completed
		ERRNO = EINPROGRESS;
		return -1;
	}
	else
	{
		// report the error synchronously if there is one; otherwise success
		if (tcpsock->sockErr == 0)
		{
			return 0;
		}
		else
		{
			ERRNO = tcpsock->sockErr;
			return -1;
		};
	};
};

static void tcpsock_close(Socket *sock)
{
	TCPSocket *tcpsock = (TCPSocket*) sock;
	if (tcpsock->thread != NULL)
	{
		uint16_t srcport, dstport;
		if (sock->domain == AF_INET)
		{
			const struct sockaddr_in *inaddr = (const struct sockaddr_in*) &tcpsock->peername;
			struct sockaddr_in *inname = (struct sockaddr_in*) &tcpsock->sockname;
			srcport = inname->sin_port;
			dstport = inaddr->sin_port;
		}
		else
		{
			const struct sockaddr_in6 *inaddr = (const struct sockaddr_in6*) &tcpsock->peername;
			struct sockaddr_in6 *inname = (struct sockaddr_in6*) &tcpsock->sockname;
			inname->sin6_family = AF_INET6;
			srcport = inname->sin6_port;
			dstport = inaddr->sin6_port;
		};
		
		semSignal(&tcpsock->semStop);
		ReleaseKernelThread(tcpsock->thread);

		TCPOutbound *ob = CreateOutbound(&tcpsock->sockname, &tcpsock->peername, 0);
		ob->segment->srcport = srcport;
		ob->segment->dstport = dstport;
		ob->segment->seqno = htonl(tcpsock->nextSeqNo);
		ob->segment->ackno = htonl(tcpsock->nextAckNo);
		ob->segment->dataOffsetNS = 0x50;
		ob->segment->flags = TCP_FIN | TCP_ACK;
		ChecksumOutbound(ob);
		
		sendPacketEx(&tcpsock->sockname, &tcpsock->peername, ob->segment, ob->size, IPPROTO_TCP,
			sock->options, sock->ifname);
		
		kfree(ob);
		
		FreePort(srcport);
	};
};

static void tcpsock_packet(Socket *sock, const struct sockaddr *src, const struct sockaddr *dest, size_t addrlen,
			const void *packet, size_t size, int proto)
{
	// TODO: 4-mapped-6 addresses
	if (src->sa_family != sock->domain)
	{
		return;
	};
	
	if (size < sizeof(TCPSegment))
	{
		return;
	};
	
	TCPSocket *tcpsock = (TCPSocket*) sock;
	if (tcpsock->state == TCP_CLOSED)
	{
		return;
	};
	
	if (proto == IPPROTO_TCP)
	{	
		if (ValidateChecksum(src, dest, packet, size) != 0)
		{
			return;
		};
		
		uint16_t localPort, remotePort;
		if (sock->domain == AF_INET)
		{
			const struct sockaddr_in *inname = (const struct sockaddr_in*) &tcpsock->sockname;
			const struct sockaddr_in *inpeer = (const struct sockaddr_in*) &tcpsock->peername;
			
			localPort = inname->sin_port;
			remotePort = inpeer->sin_port;
			
			const struct sockaddr_in *insrc = (const struct sockaddr_in*) src;
			const struct sockaddr_in *indst = (const struct sockaddr_in*) dest;
			
			if (memcmp(&insrc->sin_addr, &inpeer->sin_addr, 4) != 0)
			{
				return;
			};
			
			if (memcmp(&indst->sin_addr, &inname->sin_addr, 4) != 0)
			{
				return;
			};
		}
		else
		{
			const struct sockaddr_in6 *inname = (const struct sockaddr_in6*) &tcpsock->sockname;
			const struct sockaddr_in6 *inpeer = (const struct sockaddr_in6*) &tcpsock->peername;
			
			localPort = inname->sin6_port;
			remotePort = inpeer->sin6_port;

			const struct sockaddr_in6 *insrc = (const struct sockaddr_in6*) src;
			const struct sockaddr_in6 *indst = (const struct sockaddr_in6*) dest;
			
			if (memcmp(&insrc->sin6_addr, &inpeer->sin6_addr, 16) != 0)
			{
				return;
			};
			
			if (memcmp(&indst->sin6_addr, &inname->sin6_addr, 16) != 0)
			{
				return;
			};
		};
		
		TCPSegment *seg = (TCPSegment*) packet;

		if ((seg->srcport != remotePort) || (seg->dstport != localPort))
		{
			return;
		};
		
		if (seg->flags & TCP_ACK)
		{
			uint64_t ackno = (uint64_t) ntohl(seg->ackno);
			//kprintf("Received ACK number: %p; expecting %p\n", ackno, tcpsock->expectedAck);
			if (atomic_compare_and_swap64(&tcpsock->expectedAck, ackno, (1UL << 32)) == ackno)
			{
				semSignal(&tcpsock->semAck);
			};
		};
		
		if (seg->flags & TCP_RST)
		{
			semWait(&tcpsock->lock);
			if (ntohl(seg->seqno) == tcpsock->nextAckNo)
			{
				tcpsock->sockErr = ECONNRESET;
				tcpsock->shutflags |= SHUT_WR | SHUT_RD;
				tcpsock->state = TCP_TERMINATED;
				semSignal(&tcpsock->semStop);
			};
			semSignal(&tcpsock->lock);
		};
		
		if (tcpsock->state == TCP_TERMINATED)
		{
			semSignal(&tcpsock->semAckOut);
			return;
		};
		
		size_t headerSize = (size_t)(seg->dataOffsetNS >> 4) * 4;
		if (seg->flags & TCP_SYN)
		{
			Semaphore *semConn = &tcpsock->semConnected;
			uint8_t bitmap = 0;
			semPoll(1, &semConn, &bitmap, SEM_W_NONBLOCK, 0);
			
			if (bitmap == 0)
			{
				tcpsock->nextAckNo = ntohl(seg->seqno)+1;
				semSignal(&tcpsock->semConnected);
			};

			semSignal(&tcpsock->semAckOut);
		}
		else if (seg->flags & TCP_FIN)
		{
			uint32_t seqno = ntohl(seg->seqno);
			semWait(&tcpsock->lock);
			
			if (seqno == tcpsock->nextAckNo)
			{
				tcpsock->shutflags |= SHUT_RD;
				tcpsock->nextAckNo = seqno+1;
				semTerminate(&tcpsock->semRecvFetch);
				semSignal(&tcpsock->semAckOut);
			};
			
			semSignal(&tcpsock->lock);
		}
		else if (size > headerSize)
		{
			size_t payloadSize = size - headerSize;
			const uint8_t *scan = (const uint8_t*)seg + headerSize;
			
			uint32_t seqno = ntohl(seg->seqno);
			semWait(&tcpsock->lock);
			if (seqno == tcpsock->nextAckNo)
			{
				if (tcpsock->cntRecvPut < payloadSize)
				{
					// we can't fit this payload in our buffer! drop it as it will
					// be re-transmitted soon
					semSignal(&tcpsock->lock);
					return;
				};
				
				tcpsock->cntRecvPut -= payloadSize;
				
				size_t count = payloadSize;
				while (count--)
				{
					tcpsock->bufRecv[tcpsock->idxRecvPut] = *scan++;
					tcpsock->idxRecvPut = (tcpsock->idxRecvPut+1) % TCP_BUFFER_SIZE;
				};
				
				semSignal2(&tcpsock->semRecvFetch, (int)payloadSize);
				
				tcpsock->nextAckNo += (uint32_t)payloadSize;
				semSignal(&tcpsock->semAckOut);
			};
			semSignal(&tcpsock->lock);
		};
	};
	// TODO: ICMP messages relating to TCP
};

static ssize_t tcpsock_sendto(Socket *sock, const void *buffer, size_t size, int flags, const struct sockaddr *addr, size_t addrlen)
{
	TCPSocket *tcpsock = (TCPSocket*) sock;
	
	if (flags != 0)
	{
		ERRNO = EOPNOTSUPP;
		return -1;
	};
	
	uint8_t bitmap = 0;
	Semaphore *semConn = &tcpsock->semConnected;
	
	int status = semPoll(1, &semConn, &bitmap, SEM_W_FILE(sock->fp->oflag), sock->options[GSO_SNDTIMEO]);
	
	if (status < 0)
	{
		ERRNO = -status;
		return -1;
	};
	
	if (status == 0)
	{
		ERRNO = EAGAIN;
		return -1;
	};
	
	ssize_t sizeWritten = 0;
	const uint8_t *scan = (const uint8_t*) buffer;
	while (size > 0)
	{
		int count = (int) size;
		if (size > TCP_BUFFER_SIZE)
		{
			count = TCP_BUFFER_SIZE;
		};
		
		status = semWaitGen(&tcpsock->semSendPut, count, SEM_W_FILE(sock->fp->oflag), sock->options[GSO_SNDTIMEO]);
		if (status < 0)
		{
			if (sizeWritten == 0)
			{
				ERRNO = EAGAIN;
				return -1;
			}
			else
			{
				break;
			};
		};
		
		if (status == 0)
		{
			break;
		};
		
		int gotCount = status;
		semWait(&tcpsock->lock);
		while (status--)
		{
			tcpsock->bufSend[tcpsock->idxSendPut] = *scan++;
			tcpsock->idxSendPut = (tcpsock->idxSendPut + 1) % TCP_BUFFER_SIZE;
			
			sizeWritten++;
			size--;
		};
		semSignal(&tcpsock->lock);
		semSignal2(&tcpsock->semSendFetch, gotCount);
	};
	
	if (sizeWritten == 0)
	{
		int error = (int) atomic_swap32(&tcpsock->sockErr, 0);
		if (error != 0)
		{
			ERRNO = error;
			return -1;
		};
	};
	
	return sizeWritten;
};

static ssize_t tcpsock_recvfrom(Socket *sock, void *buffer, size_t len, int flags, struct sockaddr *addr, size_t *addrlen)
{
	if (flags != 0)
	{
		ERRNO = EOPNOTSUPP;
		return -1;
	};
	
	TCPSocket *tcpsock = (TCPSocket*) sock;
	if (addrlen != NULL) *addrlen = sizeof(struct sockaddr);
	if (addr != NULL) memcpy(addr, &tcpsock->peername, sizeof(struct sockaddr));
	
	int size = semWaitGen(&tcpsock->semRecvFetch, (int)len, SEM_W_FILE(sock->fp->oflag), sock->options[GSO_RCVTIMEO]);
	if (size < 0)
	{
		if (size == -ETIMEDOUT)
		{
			ERRNO = EAGAIN;
		}
		else
		{
			ERRNO = -size;
		};
		
		return -1;
	};
	
	if (size == 0)
	{
		// remote peer closed connection
		int error = (int) atomic_swap32(&tcpsock->sockErr, 0);
		if (error != 0)
		{
			ERRNO = error;
			return -1;
		};
		
		return 0;
	};
	
	semWait(&tcpsock->lock);
	ssize_t sizeRead = 0;
	uint8_t *put = (uint8_t*) buffer;
	
	while (size--)
	{
		*put++ = tcpsock->bufRecv[tcpsock->idxRecvFetch];
		tcpsock->idxRecvFetch = (tcpsock->idxRecvFetch+1) % TCP_BUFFER_SIZE;
		sizeRead++;
	};
	
	tcpsock->cntRecvPut += sizeRead;
	semSignal(&tcpsock->lock);
	
	return sizeRead;
};

void tcpsock_shutdown(Socket *sock, int shutflags)
{
	TCPSocket *tcpsock = (TCPSocket*) sock;
	
	semWait(&tcpsock->lock);
	
	if (shutflags & SHUT_RD)
	{
		if ((tcpsock->shutflags & SHUT_RD) == 0)
		{
			tcpsock->shutflags |= SHUT_RD;
			semTerminate(&tcpsock->semRecvFetch);
		};
	};
	
	if (shutflags & SHUT_WR)
	{
		if ((tcpsock->shutflags & SHUT_WR) == 0)
		{
			tcpsock->shutflags |= SHUT_WR;
			semTerminate(&tcpsock->semSendPut);
		};
	};
	
	semSignal(&tcpsock->lock);
};

static void tcpsock_pollinfo(Socket *sock, Semaphore **sems)
{
	TCPSocket *tcpsock = (TCPSocket*) sock;
	sems[PEI_READ] = &tcpsock->semRecvFetch;
	sems[PEI_WRITE] = &tcpsock->semConnected;
};

static int tcpsock_getsockname(Socket *sock, struct sockaddr *addr, size_t *addrlenptr)
{
	size_t addrlen = *addrlenptr;
	if (addrlen > sizeof(struct sockaddr))
	{
		addrlen = (*addrlenptr) = sizeof(struct sockaddr);
	};
	
	TCPSocket *tcpsock = (TCPSocket*) sock;
	memcpy(addr, &tcpsock->sockname, addrlen);
	return 0;
};

static int tcpsock_getpeername(Socket *sock, struct sockaddr *addr, size_t *addrlenptr)
{
	size_t addrlen = *addrlenptr;
	if (addrlen > sizeof(struct sockaddr))
	{
		addrlen = (*addrlenptr) = sizeof(struct sockaddr);
	};
	
	TCPSocket *tcpsock = (TCPSocket*) sock;
	memcpy(addr, &tcpsock->peername, addrlen);
	return 0;
};

Socket *CreateTCPSocket()
{
	TCPSocket *tcpsock = NEW(TCPSocket);
	memset(tcpsock, 0, sizeof(TCPSocket));
	semInit(&tcpsock->lock);
	semInit2(&tcpsock->semConnected, 0);
	semInit2(&tcpsock->semAck, 0);
	semInit2(&tcpsock->semStop, 0);
	semInit2(&tcpsock->semAckOut, 0);
	semInit2(&tcpsock->semSendPut, TCP_BUFFER_SIZE);
	semInit2(&tcpsock->semSendFetch, 0);
	semInit2(&tcpsock->semRecvFetch, 0);
	tcpsock->cntRecvPut = TCP_BUFFER_SIZE;
	Socket *sock = (Socket*) tcpsock;
	
	sock->bind = tcpsock_bind;
	sock->connect = tcpsock_connect;
	sock->close = tcpsock_close;
	sock->shutdown = tcpsock_shutdown;
	sock->packet = tcpsock_packet;
	sock->sendto = tcpsock_sendto;
	sock->recvfrom = tcpsock_recvfrom;
	sock->pollinfo = tcpsock_pollinfo;
	sock->getsockname = tcpsock_getsockname;
	sock->getpeername = tcpsock_getpeername;
	
	return sock;
};

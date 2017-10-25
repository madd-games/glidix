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
#define	TCP_LISTENING				5

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
#define	TCP_RETRANS_TIMEOUT			NT_MILLI(500)

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
	 * field points to the actual data to be sent.
	 */
	char					data[];
} TCPOutbound;

/**
 * Represents the addresses of a pending TCP connection.
 */
typedef struct TCPPending_
{
	struct TCPPending_*			next;
	struct sockaddr				local;
	struct sockaddr				peer;
	uint32_t				ackno;		// the ACK number that we must use in our SYN+ACK
} TCPPending;

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
	
	/**
	 * (Listening sockets) counts the number of connections waiting to be accept()ed,
	 * and the list of them.
	 */
	Semaphore				semConnWaiting;
	TCPPending*				firstPending;
} TCPSocket;

Socket *CreateTCPSocket();

static int tcpsock_bind(Socket *sock, const struct sockaddr *addr, size_t addrlen)
{
	TCPSocket *tcpsock = (TCPSocket*) sock;
	if (addrlen < INET_SOCKADDR_LEN)
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
	
	if (ClaimSocketAddr(addr, &tcpsock->sockname, sock->ifname) != 0)
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
		memset(encap, 0, sizeof(TCPEncap6)+dataSize);
		
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

	detachMe();
	
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
	if (tcpsock->state == TCP_ESTABLISHED)
	{
		sems[0] = NULL;
		connectDone = 1;
	};
	int wantExit = 0;
	while (1)
	{
		if (wantExit) break;
		
		uint64_t deadline = getNanotime() + sock->options[GSO_SNDTIMEO];
		if (sock->options[GSO_SNDTIMEO] == 0)
		{
			deadline = 0;
		};
		
		int sendOK = 0;
		int retransCount = 16;
		while (((getNanotime() < deadline) || (deadline == 0)) && (retransCount--))
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
				wantExit = 1;
				break;
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
				wantExit = 1;
				break;
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
		
		if (wantExit) break;
		
		int wasFin = tcpsock->currentOut->segment->flags & TCP_FIN;
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
		
		if (wasFin) break;
		
		while (1)
		{
			uint8_t bitmap = 0;
			semPoll(3, semsOut, &bitmap, 0, 0);
		
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
				// a count of zero means end of data, so send FIN.
				if (count == 0)
				{
					ob->segment->flags = TCP_FIN | TCP_ACK;
					tcpsock->nextSeqNo++;
				};
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

			if (bitmap & (1 << 0))
			{
				tcpsock->state = TCP_TERMINATED;
				wantExit = 1;
				break;
			};
		};
	};

	// wait up to 4 minutes, acknowledging any packets if necessary (during a clean exit)
	uint64_t deadline = getNanotime() + 4UL * 60UL * 1000000000UL;
	uint64_t currentTime;
	Semaphore *semsClosing[2] = {&tcpsock->semStop, &tcpsock->semAckOut};
	while ((currentTime = getNanotime()) < deadline)
	{
		uint8_t bitmap = 0;
		semPoll(2, semsClosing, &bitmap, 0, deadline - currentTime);

		if (bitmap & (1 << 0))
		{
			wantExit = 1;
			semsClosing[0] = NULL;
		};
		
		if (bitmap & (1 << 1))
		{
			if (!wantExit)
			{
				semWaitGen(&tcpsock->semAckOut, -1, 0, 0);
	
				TCPOutbound *ob = CreateOutbound(&tcpsock->sockname, &tcpsock->peername, 0);
				TCPSegment *ack = ob->segment;
		
				ack->srcport = srcport;
				ack->dstport = dstport;
				ack->seqno = htonl(tcpsock->nextSeqNo);
				ack->ackno = htonl(tcpsock->nextAckNo);
				ack->dataOffsetNS = 0x50;
				ack->flags = TCP_FIN | TCP_ACK;
				ack->winsz = 0xFFFF;
				ChecksumOutbound(ob);
		
				sendPacketEx(&tcpsock->sockname, &tcpsock->peername,
						ob->segment, ob->size,
						IPPROTO_TCP, sock->options, sock->ifname);
		
				kfree(ob);
			};
		};
	};

	// wait for the socket to actually be closed by the application (in case it wasn't already)
	while (semWaitGen(&tcpsock->semSendFetch, 512, 0, 0) != 0);
	
	// free the port and socket
	FreePort(srcport);
	FreeSocket(sock);
};

static int tcpsock_connect(Socket *sock, const struct sockaddr *addr, size_t size)
{
	TCPSocket *tcpsock = (TCPSocket*) sock;
	if (addr->sa_family != sock->domain)
	{
		ERRNO = EAFNOSUPPORT;
		return -1;
	};
	
	if (size != INET_SOCKADDR_LEN)
	{
		ERRNO = EAFNOSUPPORT;
		return -1;
	};
	
	semWait(&tcpsock->lock);
	if (tcpsock->state == TCP_CONNECTING)
	{
		semSignal(&tcpsock->lock);
		ERRNO = EALREADY;
		return -1;
	};
	
	if (tcpsock->state != TCP_CLOSED)
	{
		semSignal(&tcpsock->lock);
		ERRNO = EISCONN;
		return -1;
	};
	
	uint16_t srcport, dstport;
	if (sock->domain == AF_INET)
	{
		const struct sockaddr_in *inaddr = (const struct sockaddr_in*) addr;
		struct sockaddr_in *inname = (struct sockaddr_in*) &tcpsock->sockname;
		if (inname->sin_family == AF_UNSPEC)
		{
			inname->sin_family = AF_INET;
			getDefaultAddr4(&inname->sin_addr, &inaddr->sin_addr, sock->ifname);
			inname->sin_port = AllocPort();
			srcport = inname->sin_port;
		}
		else
		{
			srcport = inname->sin_port;
		};
		
		dstport = inaddr->sin_port;
	}
	else
	{
		const struct sockaddr_in6 *inaddr = (const struct sockaddr_in6*) addr;
		struct sockaddr_in6 *inname = (struct sockaddr_in6*) &tcpsock->sockname;
		if (inname->sin6_family == AF_UNSPEC)
		{
			inname->sin6_family = AF_INET6;
			getDefaultAddr6(&inname->sin6_addr, &inaddr->sin6_addr, sock->ifname);
			inname->sin6_port = AllocPort();
			srcport = inname->sin6_port;
		}
		else
		{
			srcport = inname->sin6_port;
		}
		dstport = inaddr->sin6_port;
	};
	
	memcpy(&tcpsock->peername, addr, INET_SOCKADDR_LEN);
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

		// terminate the semSendFetch semaphore, causing the handler thread to send a FIN and
		// later terminate.
		semTerminate(&tcpsock->semSendFetch);
	}
	else
	{
		FreeSocket(sock);
	};
};

static int tcpsock_packet(Socket *sock, const struct sockaddr *src, const struct sockaddr *dest, size_t addrlen,
			const void *packet, size_t size, int proto)
{
	// TODO: 4-mapped-6 addresses
	if (src->sa_family != sock->domain)
	{
		return SOCK_CONT;
	};
	
	if (size < sizeof(TCPSegment))
	{
		return SOCK_CONT;
	};
	
	TCPSocket *tcpsock = (TCPSocket*) sock;
	if (tcpsock->state == TCP_CLOSED)
	{
		return SOCK_CONT;
	};
	
	if (proto == IPPROTO_TCP)
	{
		if (tcpsock->state == TCP_LISTENING)
		{
			TCPSegment *seg = (TCPSegment*) packet;
			uint16_t listenPort;
			static uint64_t zeroAddr[2] = {0, 0};
		
			if (sock->domain == AF_INET)
			{
				const struct sockaddr_in *inname = (const struct sockaddr_in*) &tcpsock->sockname;
			
				listenPort = inname->sin_port;
			
				const struct sockaddr_in *indst = (const struct sockaddr_in*) dest;
			
				if (memcmp(&indst->sin_addr, &inname->sin_addr, 4) != 0 && memcmp(&inname->sin_addr, zeroAddr, 4) != 0)
				{
					return SOCK_CONT;
				};
			}
			else
			{
				const struct sockaddr_in6 *inname = (const struct sockaddr_in6*) &tcpsock->sockname;
			
				listenPort = inname->sin6_port;

				const struct sockaddr_in6 *indst = (const struct sockaddr_in6*) dest;
			
				if (memcmp(&indst->sin6_addr, &inname->sin6_addr, 16) != 0 && memcmp(&inname->sin6_addr, zeroAddr, 16) != 0)
				{
					return SOCK_CONT;
				};
			};
		
			if (seg->dstport != listenPort)
			{
				return SOCK_CONT;
			};
		
			if ((seg->flags & TCP_SYN) == 0)
			{
				return SOCK_CONT;
			};
			
			struct sockaddr local;
			memset(&local, 0, sizeof(struct sockaddr));
			struct sockaddr peer;
			memset(&peer, 0, sizeof(struct sockaddr));
			memcpy(&local, dest, addrlen);
			memcpy(&peer, src, addrlen);
		
			if (sock->domain == AF_INET)
			{
				((struct sockaddr_in*)&local)->sin_port = seg->dstport;
				((struct sockaddr_in*)&peer)->sin_port = seg->srcport;
			}
			else
			{
				((struct sockaddr_in6*)&local)->sin6_port = seg->dstport;
				((struct sockaddr_in6*)&peer)->sin6_port = seg->srcport;
				((struct sockaddr_in6*)&local)->sin6_flowinfo = 0;
				((struct sockaddr_in6*)&peer)->sin6_flowinfo = 0;
			};
		
			// we just received a connection
			semWait(&tcpsock->lock);
		
			// first check if the connection is already pending
			int found = 0;
			TCPPending *pend;
			for (pend=tcpsock->firstPending; pend!=NULL; pend=pend->next)
			{
				if (memcmp(&pend->local, &local, sizeof(struct sockaddr)) == 0
					&& memcmp(&pend->peer, &peer, sizeof(struct sockaddr)) == 0)
				{
					found = 1;
					break;
				};
			};
		
			// if not yet on the pending list, add it
			if (!found)
			{
				pend = NEW(TCPPending);
				pend->next = NULL;
				memcpy(&pend->local, &local, sizeof(struct sockaddr));
				memcpy(&pend->peer, &peer, sizeof(struct sockaddr));
				pend->ackno = ntohl(seg->seqno)+1;
				
				if (tcpsock->firstPending == NULL)
				{
					tcpsock->firstPending = pend;
				}
				else
				{
					TCPPending *last = tcpsock->firstPending;
					while (last->next != NULL) last = last->next;
					last->next = pend;
				};
			};
		
			semSignal(&tcpsock->lock);
			semSignal(&tcpsock->semConnWaiting);
			return SOCK_STOP;
		};
	
		if (ValidateChecksum(src, dest, packet, size) != 0)
		{
			return SOCK_CONT;
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
				return SOCK_CONT;
			};
			
			if (memcmp(&indst->sin_addr, &inname->sin_addr, 4) != 0)
			{
				return SOCK_CONT;
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
				return SOCK_CONT;
			};
			
			if (memcmp(&indst->sin6_addr, &inname->sin6_addr, 16) != 0)
			{
				return SOCK_CONT;
			};
		};
		
		TCPSegment *seg = (TCPSegment*) packet;

		if ((seg->srcport != remotePort) || (seg->dstport != localPort))
		{
			return SOCK_CONT;
		};
		
		// at this point, we know that this packet is destined to this socket, so from now on return SOCK_STOP only,
		// to avoid it arriving at other sockets
		
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
			return SOCK_STOP;
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
					return SOCK_STOP;
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
		
		return SOCK_STOP;
	};
	// TODO: ICMP messages relating to TCP
	
	return SOCK_CONT;
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
	if (addrlen != NULL) *addrlen = INET_SOCKADDR_LEN;
	if (addr != NULL) memcpy(addr, &tcpsock->peername, INET_SOCKADDR_LEN);
	
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
			// do not terminate the receive semaphore. this should only happen
			// if we actually receive a FIN.
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
	if (tcpsock->state == TCP_LISTENING) sems[PEI_READ] = &tcpsock->semConnWaiting;
	sems[PEI_WRITE] = &tcpsock->semConnected;
};

static int tcpsock_getsockname(Socket *sock, struct sockaddr *addr, size_t *addrlenptr)
{
	size_t addrlen = *addrlenptr;
	if (addrlen > INET_SOCKADDR_LEN)
	{
		addrlen = (*addrlenptr) = INET_SOCKADDR_LEN;
	};
	
	TCPSocket *tcpsock = (TCPSocket*) sock;
	memcpy(addr, &tcpsock->sockname, addrlen);
	return 0;
};

static int tcpsock_getpeername(Socket *sock, struct sockaddr *addr, size_t *addrlenptr)
{
	size_t addrlen = *addrlenptr;
	if (addrlen > INET_SOCKADDR_LEN)
	{
		addrlen = (*addrlenptr) = INET_SOCKADDR_LEN;
	};
	
	TCPSocket *tcpsock = (TCPSocket*) sock;
	memcpy(addr, &tcpsock->peername, addrlen);
	return 0;
};

static int tcpsock_listen(Socket *sock, int backlog)
{
	TCPSocket *tcpsock = (TCPSocket*) sock;
	semWait(&tcpsock->lock);
	if (tcpsock->state != TCP_CLOSED)
	{
		semSignal(&tcpsock->lock);
		ERRNO = EALREADY;
		return -1;
	};

	if (sock->domain == AF_INET)
	{
		struct sockaddr_in *inname = (struct sockaddr_in*) &tcpsock->sockname;
		if (inname->sin_family == AF_UNSPEC)
		{
			inname->sin_family = AF_INET;
			memset(&inname->sin_addr, 0, 4);
			inname->sin_port = AllocPort();
		};
	}
	else
	{
		struct sockaddr_in6 *inname = (struct sockaddr_in6*) &tcpsock->sockname;
		if (inname->sin6_family == AF_UNSPEC)
		{
			inname->sin6_family = AF_INET6;
			memset(&inname->sin6_addr, 0, 16);
			inname->sin6_port = AllocPort();
		};
	};
	
	tcpsock->state = TCP_LISTENING;
	semSignal(&tcpsock->lock);
	return 0;
};

static Socket* tcpsock_accept(Socket *sock, struct sockaddr *addr, size_t *addrlenptr)
{
	TCPSocket *tcpsock = (TCPSocket*) sock;
	if (tcpsock->state != TCP_LISTENING)
	{
		ERRNO = EINVAL;
		return NULL;
	};
	
	if (addrlenptr != NULL)
	{
		if ((*addrlenptr) < INET_SOCKADDR_LEN)
		{
			ERRNO = EINVAL;
			return NULL;
		};
	};
	
	int count = semWaitGen(&tcpsock->semConnWaiting, 1, SEM_W_FILE(sock->fp->oflag), sock->options[GSO_RCVTIMEO]);
	if (count < 0)
	{
		ERRNO = -count;
		return NULL;
	};
	
	// a connection is pending, create the socket.
	// TODO: there is a race condition where we remove the pending connection from the list,
	// and another SYN arrives before we add the socket to the list, causing another connection
	// request to be created. We must work on that somehow.
	Socket *client = CreateTCPSocket();
	client->domain = sock->domain;
	client->type = SOCK_STREAM;
	client->proto = IPPROTO_TCP;
	memset(client->options, 0, 8*GSO_COUNT);
	
	TCPSocket *tcpclient = (TCPSocket*) client;
	
	semWait(&tcpsock->lock);
	TCPPending *pend = tcpsock->firstPending;
	tcpsock->firstPending = pend->next;
	semSignal(&tcpsock->lock);
	
	semWait(&tcpclient->lock);
	memcpy(&tcpclient->sockname, &pend->local, sizeof(struct sockaddr));
	memcpy(&tcpclient->peername, &pend->peer, sizeof(struct sockaddr));
	
	tcpclient->state = TCP_ESTABLISHED;
	tcpclient->nextSeqNo = (uint32_t) getRandom();
	tcpclient->nextAckNo = pend->ackno;
	
	tcpclient->currentOut = CreateOutbound(&tcpclient->sockname, &tcpclient->peername, 0);
	tcpclient->expectedAck = tcpclient->nextSeqNo;
	TCPSegment *syn = tcpclient->currentOut->segment;
	if (sock->domain == AF_INET)
	{
		syn->srcport = ((struct sockaddr_in*)&pend->local)->sin_port;
		syn->dstport = ((struct sockaddr_in*)&pend->peer)->sin_port;
	}
	else
	{
		syn->srcport = ((struct sockaddr_in6*)&pend->local)->sin6_port;
		syn->dstport = ((struct sockaddr_in6*)&pend->peer)->sin6_port;
	};
	syn->seqno = htonl(tcpclient->nextSeqNo-1);
	syn->ackno = htonl(pend->ackno);
	syn->dataOffsetNS = 0x50;
	syn->flags = TCP_SYN | TCP_ACK;
	syn->winsz = 0xFFFF;
	ChecksumOutbound(tcpclient->currentOut);
	
	KernelThreadParams pars;
	memset(&pars, 0, sizeof(KernelThreadParams));
	pars.stackSize = DEFAULT_STACK_SIZE;
	pars.name = "TCP Thread";
	tcpclient->thread = CreateKernelThread(tcpThread, &pars, tcpclient);
	
	semSignal(&tcpclient->lock);
	semSignal(&tcpclient->semConnected);
	
	if (addrlenptr != NULL) *addrlenptr = INET_SOCKADDR_LEN;
	if (addr != NULL)
	{
		memcpy(addr, &pend->peer, INET_SOCKADDR_LEN);
	};
	
	kfree(pend);
	return client;
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
	semInit2(&tcpsock->semConnWaiting, 0);
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
	sock->listen = tcpsock_listen;
	sock->accept = tcpsock_accept;
	
	return sock;
};

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
#include <glidix/vfs.h>

#define	STATE_CLOSED				0
#define	STATE_LISTENING				1
#define	STATE_CONNECTED				2

/**
 * Represents a message in the queue.
 */
typedef struct Message_
{
	struct Message_*			next;
	struct sockaddr_un			src;
	size_t					size;
	uint8_t					payload[];
} Message;

/**
 * Represents a waiting connection.
 */
typedef struct ConnWaiting_
{
	struct ConnWaiting_*			next;
	Socket*					peer;
} ConnWaiting;

typedef union
{
	Socket					header_;
	
	/**
	 * If header_.type == SOCK_SEQPACKET.
	 */
	struct
	{
		Socket				header_;
		
		/**
		 * The inode representing the socket.
		 */
		Inode*				sysobj;
		
		/**
		 * Socket state (see above).
		 */
		int				state;
		
		/**
		 * The lock.
		 */
		Semaphore			lock;
		
		/**
		 * (Only initialized in listening state)
		 * Counts the number of connections waiting.
		 */
		Semaphore			semConnWaiting;
		ConnWaiting*			connFirst;
		ConnWaiting*			connLast;
		
		/**
		 * Socket name (what address it's bound to).
		 */
		struct sockaddr			name;
		
		/**
		 * List of incoming packets, and the counter.
		 */
		Semaphore			semMsgIn;
		Message*			msgFirst;
		Message*			msgLast;
		
		/**
		 * Inode for the other side of the connection.
		 */
		Inode*				peer;
	} seq;
} UnixSocket;

static int unixsock_bind(Socket *sock, const struct sockaddr *addr, size_t addrlen)
{
	UnixSocket *unixsock = (UnixSocket*) sock;
	
	if (addrlen < 2)
	{
		ERRNO = EAFNOSUPPORT;
		return -1;
	};
	
	if (addr->sa_family != AF_UNIX)
	{
		ERRNO = EAFNOSUPPORT;
		return -1;
	};
	
	if (addrlen > sizeof(struct sockaddr_un))
	{
		ERRNO = EAFNOSUPPORT;
		return -1;
	};
	
	const struct sockaddr_un *unaddr = (const struct sockaddr_un*) addr;
	
	// validate
	int i;
	int ok = 0;
	for (i=0; i<(addrlen-2); i++)
	{
		if (unaddr->sun_path[i] == 0)
		{
			ok = 1;
			break;
		};
	};
	
	if (!ok)
	{
		ERRNO = EAFNOSUPPORT;
		return -1;
	};
	
	// we can bind to it now
	if (sock->type == SOCK_SEQPACKET)
	{
		if (unixsock->seq.name.sa_family != AF_UNSPEC)
		{
			ERRNO = EINVAL;
			return -1;
		};
		
		int error;
		DentryRef dref = vfsGetDentry(VFS_NULL_IREF, unaddr->sun_path, 1, &error);
		if (dref.dent == NULL)
		{
			vfsUnrefDentry(dref);
			ERRNO = EADDRNOTAVAIL;
			return -1;
		};
		
		if (dref.dent->ino != 0)
		{
			vfsUnrefDentry(dref);
			ERRNO = EADDRINUSE;
			return -1;
		};
		
		vfsBindInode(dref, unixsock->seq.sysobj);
	};
	
	ERRNO = EINVAL;
	return -1;
};

static ssize_t unixsock_sendto(Socket *sock, const void *message, size_t size, int flags, const struct sockaddr *addr, size_t addrlen)
{
	UnixSocket *unixsock = (UnixSocket*) sock;
	
	if (sock->type == SOCK_SEQPACKET)
	{
		semWait(&unixsock->seq.lock);
		if (unixsock->seq.state != STATE_CONNECTED)
		{
			semSignal(&unixsock->seq.lock);
			
			ERRNO = ENOTCONN;
			return -1;
		};
		
		Inode *sysobj = unixsock->seq.peer;
		vfsUprefInode(sysobj);
		
		struct sockaddr_un myName;
		memcpy(&myName, &unixsock->seq.name, sizeof(struct sockaddr_un));
		semSignal(&unixsock->seq.lock);
		
		UnixSocket *peer = (UnixSocket*) sysobj->fsdata;
		semWait(&peer->seq.lock);
		
		if (peer->seq.state != STATE_CONNECTED)
		{
			// it dropped the connection
			semSignal(&peer->seq.lock);
			vfsDownrefInode(sysobj);
			ERRNO = ECONNRESET;
			return -1;
		};
		
		Message *msg = (Message*) kmalloc(sizeof(Message) + size);
		msg->next = NULL;
		memcpy(&msg->src, &myName, sizeof(struct sockaddr_un));
		msg->size = size;
		memcpy(msg->payload, message, size);
		
		if (peer->seq.msgLast == NULL)
		{
			peer->seq.msgFirst = peer->seq.msgLast = msg;
		}
		else
		{
			peer->seq.msgLast->next = msg;
			peer->seq.msgLast = msg;
		};
		
		semSignal2(&peer->seq.semMsgIn, 1);
		semSignal(&peer->seq.lock);
		
		vfsDownrefInode(sysobj);
		return (ssize_t) size;
	};
	
	ERRNO = EINVAL;
	return -1;
};

static ssize_t unixsock_recvfrom(Socket *sock, void *buffer, size_t len, int flags, struct sockaddr *addr, size_t *addrlen)
{
	UnixSocket *unixsock = (UnixSocket*) sock;
	
	if (sock->type == SOCK_SEQPACKET)
	{
		if (unixsock->seq.state != STATE_CONNECTED)
		{
			ERRNO = ENOTCONN;
			return -1;
		};
		
		int status = semWaitGen(&unixsock->seq.semMsgIn, 1, SEM_W_FILE(sock->fp->oflags), sock->options[GSO_RCVTIMEO]);
		if (status < 0)
		{
			ERRNO = -status;
			return -1;
		};
		
		if (status == 0)
		{
			// EOF
			return 0;
		};
		
		semWait(&unixsock->seq.lock);
		Message *msg = unixsock->seq.msgFirst;
				
		if (len > msg->size)
		{
			len = msg->size;
		};
		
		size_t realAddrLen = 3 + strlen(msg->src.sun_path);
		if (addrlen != NULL)
		{
			if ((*addrlen) < realAddrLen)
			{
				realAddrLen = *addrlen;
			}
			else
			{
				*addrlen = realAddrLen;
			};
		}
		else
		{
			realAddrLen = 0;
		};
		
		if (addr != NULL)
		{
			memcpy(addr, &msg->src, realAddrLen);
		};
		
		memcpy(buffer, msg->payload, len);
		
		if ((flags & MSG_PEEK) == 0)
		{
			unixsock->seq.msgFirst = msg->next;
			if (unixsock->seq.msgFirst == NULL)
			{
				unixsock->seq.msgLast = NULL;
			};
			
			kfree(msg);
		}
		else
		{
			// just peeking; return the message
			semSignal2(&unixsock->seq.semMsgIn, 1);
		};
		
		semSignal(&unixsock->seq.lock);
		return (ssize_t) len;
	}
	else
	{
		ERRNO = EINVAL;
		return -1;
	};
};

static void unixsock_free(Inode *sysobj)
{
	// we can safely free the socket now
	// (the file descriptor for it is definitely closed, because otherwise the reference
	// count of the sysobj would not drop to zero)
	// TODO: clean up the queue and stuff
	Socket *sock = (Socket*) sysobj->fsdata;
	FreeSocket(sock);
};

static void unixsock_close(Socket *sock)
{
	UnixSocket *unixsock = (UnixSocket*) sock;
	if (sock->type == SOCK_SEQPACKET)
	{
		if (unixsock->seq.state == STATE_CONNECTED)
		{
			semWait(&unixsock->seq.lock);
			unixsock->seq.state = STATE_CLOSED;
			
			Inode *sysobj = unixsock->seq.peer;
			// don't incref: we're destroying the refrence
			
			semSignal(&unixsock->seq.lock);
			
			UnixSocket *peer = (UnixSocket*) sysobj->fsdata;
			semWait(&peer->seq.lock);
			semTerminate(&peer->seq.semMsgIn);
			semSignal(&peer->seq.lock);
			
			vfsDownrefInode(sysobj);
		};
		
		vfsDownrefInode(unixsock->seq.sysobj);
	};
};

static int unixsock_getsockname(Socket *sock, struct sockaddr *addr, size_t *addrlenptr)
{
	size_t addrlen = *addrlenptr;
	if (addrlen > sizeof(struct sockaddr_un))
	{
		addrlen = (*addrlenptr) = sizeof(struct sockaddr_un);
	};
	
	UnixSocket *unixsock = (UnixSocket*) sock;
	memcpy(addr, &unixsock->seq.name, addrlen);
	return 0;
};

static int unixsock_getpeername(Socket *sock, struct sockaddr *addr, size_t *addrlenptr)
{
	size_t addrlen = *addrlenptr;
	if (addrlen > sizeof(struct sockaddr_un))
	{
		addrlen = (*addrlenptr) = sizeof(struct sockaddr_un);
	};
	
	UnixSocket *unixsock = (UnixSocket*) sock;
	
	if (sock->type == SOCK_SEQPACKET)
	{
		semWait(&unixsock->seq.lock);
		if (unixsock->seq.state != STATE_CONNECTED)
		{
			semSignal(&unixsock->seq.lock);
			
			ERRNO = ENOTCONN;
			return -1;
		};
		
		// 'peer' is never NULL if the state is STATE_CONNECTED
		Inode *sysobj = unixsock->seq.peer;
		vfsUprefInode(sysobj);
		
		semSignal(&unixsock->seq.lock);
		
		UnixSocket *peer = (UnixSocket*) sysobj->fsdata;
		semWait(&peer->seq.lock);
		
		memcpy(addr, &peer->seq.name, addrlen);
		semSignal(&peer->seq.lock);
		
		vfsDownrefInode(sysobj);
		return 0;
	};
	
	ERRNO = EINVAL;
	return -1;
};

static int unixsock_listen(Socket *sock, int backlog)
{
	UnixSocket *unixsock = (UnixSocket*) sock;
	
	if (sock->type == SOCK_SEQPACKET)
	{
		semWait(&unixsock->seq.lock);
		if (unixsock->seq.state != STATE_CLOSED)
		{
			semSignal(&unixsock->seq.lock);
			ERRNO = EADDRINUSE;
			return -1;
		};
		
		semInit2(&unixsock->seq.semConnWaiting, 0);
		unixsock->seq.state = STATE_LISTENING;
		semSignal(&unixsock->seq.lock);
		return 0;
	};
	
	ERRNO = EINVAL;
	return -1;
};

static void unixsock_pollinfo(Socket *sock, Semaphore **sems)
{
	UnixSocket *unixsock = (UnixSocket*) sock;
	
	if (sock->type == SOCK_SEQPACKET)
	{
		sems[PEI_WRITE] = vfsGetConstSem();
	
		semWait(&unixsock->seq.lock);
		
		switch (unixsock->seq.state)
		{
		case STATE_CLOSED:
			sems[PEI_READ] = vfsGetConstSem();
			break;
		case STATE_LISTENING:
			sems[PEI_READ] = &unixsock->seq.semConnWaiting;
			break;
		case STATE_CONNECTED:
			sems[PEI_READ] = &unixsock->seq.semMsgIn;
			break;
		};
		
		semSignal(&unixsock->seq.lock);
	};
};

UnixSocket* CreateUnixSocket(int type);
static int unixsock_connect(Socket *sock, const struct sockaddr *addr, size_t addrlen)
{
	UnixSocket *unixsock = (UnixSocket*) sock;
	
	if (sock->type == SOCK_SEQPACKET)
	{
		if (addr->sa_family != AF_UNIX)
		{
			ERRNO = EAFNOSUPPORT;
			return -1;
		};
		
		if (addrlen > sizeof(struct sockaddr_un))
		{
			ERRNO = EAFNOSUPPORT;
			return -1;
		};
	
		const struct sockaddr_un *unaddr = (const struct sockaddr_un*) addr;
	
		// validate
		int i;
		int ok = 0;
		for (i=0; i<(addrlen-2); i++)
		{
			if (unaddr->sun_path[i] == 0)
			{
				ok = 1;
				break;
			};
		};
	
		if (!ok)
		{
			ERRNO = EAFNOSUPPORT;
			return -1;
		};
		
		int error;
		File *fp = vfsOpen(VFS_NULL_IREF, unaddr->sun_path, O_RDWR, 0644, &error);
		if (fp == NULL)
		{
			ERRNO = ECONNREFUSED;
			return -1;
		};
		
		Inode *sysobj = fp->iref.inode;
		vfsUprefInode(sysobj);
		vfsClose(fp);
		
		if ((sysobj->mode & 0xF000) != VFS_MODE_SOCKET)
		{
			vfsDownrefInode(sysobj);
			ERRNO = ECONNREFUSED;
			return -1;
		};
		
		Socket *listensock = (Socket*) sysobj->fsdata;
		if ((listensock->domain != AF_UNIX) || (listensock->type != SOCK_SEQPACKET))
		{
			vfsDownrefInode(sysobj);
			ERRNO = ECONNREFUSED;
			return -1;
		};
		
		UnixSocket *unixlisten = (UnixSocket*) listensock;
		semWait(&unixlisten->seq.lock);
		
		if (unixlisten->seq.state != STATE_LISTENING)
		{
			semSignal(&unixlisten->seq.lock);
			vfsDownrefInode(sysobj);
			ERRNO = ECONNREFUSED;
			return -1;
		};
		
		// make a new socket and connect it to us
		UnixSocket *newunix = CreateUnixSocket(SOCK_SEQPACKET);
		newunix->seq.state = STATE_CONNECTED;
		newunix->seq.peer = unixsock->seq.sysobj;
		vfsUprefInode(unixsock->seq.sysobj);
		vfsUprefInode(newunix->seq.sysobj);	// it's now in the queue and connected to us (refcount 2)
		
		Socket *newsock = (Socket*) newunix;
		newsock->domain = AF_UNIX;
		newsock->type = SOCK_SEQPACKET;
		
		ConnWaiting *queue = NEW(ConnWaiting);
		queue->next = NULL;
		queue->peer = newsock;
		
		if (unixlisten->seq.connLast == NULL)
		{
			unixlisten->seq.connFirst = unixlisten->seq.connLast = queue;
		}
		else
		{
			unixlisten->seq.connLast->next = queue;
			unixlisten->seq.connLast = queue;
		};
		
		semSignal2(&unixlisten->seq.semConnWaiting, 1);
		semSignal(&unixlisten->seq.lock);
		
		semWait(&unixsock->seq.lock);
		unixsock->seq.state = STATE_CONNECTED;
		unixsock->seq.peer = newunix->seq.sysobj;
		semSignal(&unixsock->seq.lock);
		
		return 0;
	};
	
	ERRNO = EINVAL;
	return -1;
};

Socket* unixsock_accept(Socket *sock, struct sockaddr *addr, size_t *addrlenptr)
{
	UnixSocket *unixsock = (UnixSocket*) sock;
	
	if (sock->type == SOCK_SEQPACKET)
	{
		if (unixsock->seq.state != STATE_LISTENING)
		{
			ERRNO = EINVAL;
			return NULL;
		};
		
		if (addrlenptr != NULL)
		{
			if ((*addrlenptr) < 3)
			{
				ERRNO = EINVAL;
				return NULL;
			};
			
			*addrlenptr = 3;
			
			struct sockaddr_un *unaddr = (struct sockaddr_un*) addr;
			unaddr->sun_family = AF_UNIX;
			unaddr->sun_path[0] = 0;
		};
		
		int status = semWaitGen(&unixsock->seq.semConnWaiting, 1, SEM_W_FILE(sock->fp->oflags),
						sock->options[GSO_RCVTIMEO]);
		if (status < 0)
		{
			ERRNO = -status;
			return NULL;
		};
		
		semWait(&unixsock->seq.lock);
		ConnWaiting *conn = unixsock->seq.connFirst;
		unixsock->seq.connFirst = conn->next;
		if (unixsock->seq.connFirst == NULL) unixsock->seq.connLast = NULL;
		semSignal(&unixsock->seq.lock);
		
		// connect() that created this socket set the refcount of sysobj correctly so that we do not
		// incref it here
		Socket *result = conn->peer;
		kfree(conn);
		return result;
	};
	
	ERRNO = EINVAL;
	return NULL;
};

UnixSocket* CreateUnixSocket(int type)
{
	if (type != SOCK_SEQPACKET)
	{
		ERRNO = EPROTONOSUPPORT;
		return NULL;
	};
	
	UnixSocket *unixsock = NEW(UnixSocket);
	memset(unixsock, 0, sizeof(UnixSocket));
	
	if (type == SOCK_SEQPACKET)
	{
		unixsock->seq.sysobj = vfsCreateInode(NULL, VFS_MODE_SOCKET | 0644);
		unixsock->seq.sysobj->fsdata = unixsock;
		unixsock->seq.sysobj->free = unixsock_free;
		unixsock->seq.state = STATE_CLOSED;
		semInit(&unixsock->seq.lock);
		semInit2(&unixsock->seq.semMsgIn, 0);
	};
	
	Socket *sock = (Socket*) unixsock;
	sock->bind = unixsock_bind;
	sock->sendto = unixsock_sendto;
	sock->recvfrom = unixsock_recvfrom;
	sock->close = unixsock_close;
	sock->getsockname = unixsock_getsockname;
	sock->getpeername = unixsock_getpeername;
	sock->listen = unixsock_listen;
	sock->pollinfo = unixsock_pollinfo;
	sock->connect = unixsock_connect;
	sock->accept = unixsock_accept;
	
	return unixsock;
};

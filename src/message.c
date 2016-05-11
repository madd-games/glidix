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

#include <glidix/message.h>
#include <glidix/vfs.h>
#include <glidix/sched.h>
#include <glidix/semaphore.h>
#include <glidix/errno.h>
#include <glidix/memory.h>
#include <glidix/string.h>
#include <glidix/console.h>
#include <glidix/syscall.h>

#define	MAX_MSG_SIZE				65536

typedef struct BufferedMessage_
{
	struct BufferedMessage_*		next;
	MessageInfo				info;
	size_t					size;
	char					data[];
} BufferedMessage;

typedef struct
{
	/**
	 * Semaphore counting the number of messages waiting.
	 */
	Semaphore				counter;
	
	/**
	 * Semaphore controlling access to the queue.
	 */
	Semaphore				sem;
	
	/**
	 * Messages waiting to be read.
	 */
	BufferedMessage*			first;
	BufferedMessage*			last;
	
	/**
	 * Creator identity.
	 */
	int					pid;
	int					fd;
	uid_t					uid;
	gid_t					gid;
	
	/**
	 * remotePid is 0 for server queues. Otherwise, this is the pid-fd of the
	 * remote server to which we are talking.
	 */
	int					remotePid;
	int					remoteFD;
} MessageQueue;

static MessageQueue* findQueue(int pid, int fd);

static void mq_close(File *fp)
{
	MessageQueue *queue = (MessageQueue*) fp->fsdata;
	semWait(&queue->sem);
	if (queue->remotePid != 0)
	{
		MessageQueue *server = findQueue(queue->remotePid, queue->remoteFD);
		if (server != NULL)
		{
			semWait(&server->sem);
			BufferedMessage *msg = NEW(BufferedMessage);
			msg->info.type = MQ_HANGUP;
			msg->info.pid = queue->pid;
			msg->info.fd = queue->fd;
			msg->info.uid = queue->uid;
			msg->info.gid = queue->gid;
			msg->next = NULL;
			msg->size = 0;
			if (server->first == NULL)
			{
				server->first = server->last = msg;
			}
			else
			{
				server->last->next = msg;
				server->last = msg;
			};
			semSignal2(&server->counter, 1);
			semSignal(&server->sem);
		};
	};
	kfree(queue);
};

int sys_mqserver()
{
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
		ERRNO = EMFILE;
		return -1;
	};
	
	File *fp = NEW(File);
	memset(fp, 0, sizeof(File));
	
	MessageQueue *queue = NEW(MessageQueue);
	semInit2(&queue->counter, 0);
	semInit(&queue->sem);
	
	queue->first = NULL;
	queue->last = NULL;
	
	queue->pid = getCurrentThread()->creds->pid;
	queue->fd = i;
	queue->uid = getCurrentThread()->creds->euid;
	queue->gid = getCurrentThread()->creds->egid;
	queue->remotePid = 0;
	
	fp->fsdata = queue;
	fp->oflag = O_MSGQ | O_CLOEXEC;
	fp->close = mq_close;
	fp->refcount = 1;
	ftab->entries[i] = fp;
	spinlockRelease(&ftab->spinlock);
	return i;
};

static MessageQueue *findQueue(int pid, int fd)
{
	if ((fd < 0) || (fd > MAX_OPEN_FILES))
	{
		return NULL;
	};
	
	if (pid < 1)
	{
		return NULL;
	};
	
	cli();
	lockSched();
	Thread *thread = getCurrentThread();
	
	do
	{
		thread = thread->next;
		if (thread->creds->pid == pid) break;
	} while (thread != getCurrentThread());
	
	if (thread->creds->pid != pid)
	{
		unlockSched();
		sti();
		return NULL;
	};
	
	unlockSched();
	sti();
	
	spinlockAcquire(&thread->ftab->spinlock);
	File *fp = thread->ftab->entries[fd];
	if (fp == NULL)
	{
		spinlockRelease(&thread->ftab->spinlock);
		return NULL;
	};
	
	if ((fp->oflag & O_MSGQ) == 0)
	{
		spinlockRelease(&thread->ftab->spinlock);
		return NULL;
	};
	
	MessageQueue *queue = (MessageQueue*) fp->fsdata;
	
	spinlockRelease(&thread->ftab->spinlock);
	
	return queue;
};

int sys_mqclient(int pid, int fd)
{
	if (pid == getCurrentThread()->creds->pid)
	{
		ERRNO = EINVAL;
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
		ERRNO = EMFILE;
		return -1;
	};
	
	MessageQueue *server = findQueue(pid, fd);
	if (server == NULL)
	{
		spinlockRelease(&ftab->spinlock);
		ERRNO = ENOENT;
		return -1;
	};
	
	// tell the server we are connecting
	semWait(&server->sem);
	if (server->remotePid != 0)
	{
		// that queue is a client not a server
		semSignal(&server->sem);
		spinlockRelease(&ftab->spinlock);
		ERRNO = ENOENT;
		return -1;
	};
	
	BufferedMessage *msg = NEW(BufferedMessage);
	msg->info.type = MQ_CONNECT;
	msg->info.pid = getCurrentThread()->creds->pid;
	msg->info.fd = i;
	msg->info.uid = getCurrentThread()->creds->euid;
	msg->info.gid = getCurrentThread()->creds->egid;
	msg->next = NULL;
	msg->size = 0;
	if (server->first == NULL)
	{
		server->first = server->last = msg;
	}
	else
	{
		server->last->next = msg;
		server->last = msg;
	};
	semSignal2(&server->counter, 1);
	semSignal(&server->sem);
	
	File *fp = NEW(File);
	memset(fp, 0, sizeof(File));
	
	MessageQueue *queue = NEW(MessageQueue);
	semInit2(&queue->counter, 0);
	semInit(&queue->sem);
	
	queue->first = NULL;
	queue->last = NULL;
	
	queue->pid = getCurrentThread()->creds->pid;
	queue->fd = i;
	queue->uid = getCurrentThread()->creds->euid;
	queue->gid = getCurrentThread()->creds->egid;
	queue->remotePid = pid;
	queue->remoteFD = fd;
	
	fp->fsdata = queue;
	fp->oflag = O_MSGQ | O_CLOEXEC;
	fp->close = mq_close;
	fp->refcount = 1;
	
	ftab->entries[i] = fp;
	spinlockRelease(&ftab->spinlock);
	return i;
};

int sys_mqsend(int fd, int targetPid, int targetFD, const void *msg, size_t msgsize)
{
	if (!isPointerValid((uint64_t)msg, msgsize, PROT_READ))
	{
		ERRNO = EFAULT;
		return -1;
	};
	
	if ((fd < 0) || (fd > MAX_OPEN_FILES))
	{
		ERRNO = EBADF;
		return -1;
	};
	
	if (msgsize > MAX_MSG_SIZE)
	{
		ERRNO = EINVAL;
		return -1;
	};
	
	spinlockAcquire(&getCurrentThread()->ftab->spinlock);
	File *fp = getCurrentThread()->ftab->entries[fd];
	if (fp == NULL)
	{
		spinlockRelease(&getCurrentThread()->ftab->spinlock);
		ERRNO = EBADF;
		return -1;
	};
	
	if ((fp->oflag & O_MSGQ) == 0)
	{
		spinlockRelease(&getCurrentThread()->ftab->spinlock);
		ERRNO = EBADF;
		return -1;
	};
	
	MessageQueue *queue = (MessageQueue*) fp->fsdata;
	vfsDup(fp);
	spinlockRelease(&getCurrentThread()->ftab->spinlock);
	
	semWait(&queue->sem);
	if (queue->remotePid != 0)
	{
		if ((queue->remotePid != targetPid) || (queue->remoteFD != targetFD))
		{
			semSignal(&queue->sem);
			ERRNO = EINVAL;
			return -1;
		};
	};
	
	MessageQueue *dest = findQueue(targetPid, targetFD);
	if (dest == NULL)
	{
		semSignal(&queue->sem);
		vfsClose(fp);
		ERRNO = ENOENT;
		return -1;
	};
	
	semWait(&dest->sem);
	BufferedMessage *buf = (BufferedMessage*) kmalloc(sizeof(BufferedMessage) + msgsize);
	buf->info.type = MQ_INCOMING;
	buf->info.pid = queue->pid;
	buf->info.fd = queue->fd;
	buf->info.uid = queue->uid;
	buf->info.gid = queue->gid;
	buf->next = NULL;
	buf->size = msgsize;
	memcpy(buf->data, msg, msgsize);
	
	if (dest->first == NULL)
	{
		dest->first = dest->last = buf;
	}
	else
	{
		dest->last->next = buf;
		dest->last = buf;
	};
	
	semSignal2(&dest->counter, 1);
	semSignal(&dest->sem);
	semSignal(&queue->sem);
	vfsClose(fp);
	
	return 0;
};

ssize_t sys_mqrecv(int fd, MessageInfo *info, void *buffer, size_t bufsize)
{
	if (!isPointerValid((uint64_t)info, sizeof(MessageInfo), PROT_WRITE))
	{
		ERRNO = EFAULT;
		return -1;
	};
	
	if (!isPointerValid((uint64_t)buffer, bufsize, PROT_WRITE))
	{
		ERRNO = EFAULT;
		return -1;
	};
	
	if ((fd < 0) || (fd > MAX_OPEN_FILES))
	{
		ERRNO = EBADF;
		return -1;
	};
	
	spinlockAcquire(&getCurrentThread()->ftab->spinlock);
	File *fp = getCurrentThread()->ftab->entries[fd];
	if (fp == NULL)
	{
		spinlockRelease(&getCurrentThread()->ftab->spinlock);
		ERRNO = EBADF;
		return -1;
	};
	
	if ((fp->oflag & O_MSGQ) == 0)
	{
		spinlockRelease(&getCurrentThread()->ftab->spinlock);
		ERRNO = EBADF;
		return -1;
	};
	
	MessageQueue *queue = (MessageQueue*) fp->fsdata;
	vfsDup(fp);
	spinlockRelease(&getCurrentThread()->ftab->spinlock);
	
	if (semWaitTimeout(&queue->counter, 1, 0) < 0)
	{
		ERRNO = EINTR;
		vfsClose(fp);
		return -1;
	};
	
	semWait(&queue->sem);
	BufferedMessage *msg = queue->first;
	queue->first = msg->next;
	semSignal(&queue->sem);
	
	memcpy(info, &msg->info, sizeof(MessageInfo));
	if (bufsize > msg->size) bufsize = msg->size;
	memcpy(buffer, msg->data, bufsize);
	kfree(msg);
	vfsClose(fp);
	
	return (ssize_t) bufsize;
};

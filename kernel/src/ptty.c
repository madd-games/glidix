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

#include <glidix/ptty.h>
#include <glidix/devfs.h>
#include <glidix/console.h>
#include <glidix/string.h>
#include <glidix/memory.h>
#include <glidix/errno.h>
#include <glidix/sched.h>

static int ptsNextNo = 0;

static void ptm_close(Inode *inode, void *data)
{
	PseudoTerm *ptty = (PseudoTerm*) data;
	signalPid(-ptty->pgid, SIGHUP);
	devfsRemove(ptty->slaveName);
	vfsDownrefInode(ptty->devSlave);
	
	if (__sync_and_and_fetch(&ptty->sides, ~PTTY_MASTER) == 0)
	{
		kfree(ptty);
	};
};

static void pts_free(Inode *inode)
{
	PseudoTerm *ptty = (PseudoTerm*) inode->fsdata;
	
	if (__sync_and_and_fetch(&ptty->sides, ~PTTY_SLAVE) == 0)
	{
		kfree(ptty);
	};
};

static ssize_t pts_write(Inode *inode, File *fp, const void *buffer, size_t size, off_t off)
{
	PseudoTerm *ptty = (PseudoTerm*) inode->fsdata;
	semWait(&ptty->sem);
	
	const char *scan = (const char*) buffer;
	ssize_t outsz = (ssize_t) size;
	while (size--)
	{
		ptty->masterBuffer[ptty->masterPut] = *scan++;
		ptty->masterPut = (ptty->masterPut + 1) % TERM_BUFFER_SIZE;
	};
	
	semSignal2(&ptty->masterCounter, (int) outsz);
	semSignal(&ptty->sem);
	return outsz;
};

static int pts_ioctl(Inode *inode, File *fp, uint64_t cmd, void *argp)
{
	PseudoTerm *ptty = (PseudoTerm*) inode->fsdata;
	semWait(&ptty->sem);

	int pgid;
	struct termios *tc = (struct termios*) argp;
	Thread *scan;
	Thread *target = NULL;
	
	switch (cmd)
	{
	case IOCTL_TTY_GETATTR:
		memcpy(tc, &ptty->state, sizeof(struct termios));
		semSignal(&ptty->sem);
		return 0;
	case IOCTL_TTY_SETATTR:
		ptty->state.c_iflag = tc->c_iflag;
		ptty->state.c_oflag = tc->c_oflag;
		ptty->state.c_cflag = tc->c_cflag;
		ptty->state.c_lflag = tc->c_lflag;
		semSignal(&ptty->sem);
		return 0;
	case IOCTL_TTY_GETPGID:
		*((int*)argp) = ptty->pgid;
		semSignal(&ptty->sem);
		return 0;
	case IOCTL_TTY_SETPGID:
		if (getCurrentThread()->creds->sid != ptty->sid)
		{
			ERRNO = ENOTTY;
			return -1;
		};
		pgid = *((int*)argp);
		cli();
		lockSched();
		scan = getCurrentThread();
		do
		{
			if (scan->creds != NULL)
			{
				if (scan->creds->pgid == pgid)
				{
					target = scan;
					break;
				};
			};
			
			scan = scan->next;
		} while (scan != getCurrentThread());
		if (target == NULL)
		{
			unlockSched();
			sti();
			ERRNO = EPERM;
			semSignal(&ptty->sem);
			return -1;
		};
		if (target->creds->sid != ptty->sid)
		{
			unlockSched();
			sti();
			ERRNO = EPERM;
			semSignal(&ptty->sem);
			return -1;
		};
		unlockSched();
		sti();
		ptty->pgid = pgid;
		semSignal(&ptty->sem);
		return 0;
	default:
		ERRNO = EINVAL;
		semSignal(&ptty->sem);
		return -1;
	};
};

static ssize_t pts_read(Inode *inode, File *fp, void *buffer, size_t size, off_t off)
{
	PseudoTerm *ptty = (PseudoTerm*) inode->fsdata;
	int count = semWaitGen(&ptty->slaveCounter, (int)size, SEM_W_FILE(fp->oflags), 0);
	if (count < 0)
	{
		ERRNO = -count;
		return -1;
	};
	
	if (count == 0) return 0;
	
	semWait(&ptty->sem);
	ssize_t outsz = 0;
	char *put = (char*) buffer;
	while (count--)
	{
		*put++ = ptty->slaveBuffer[ptty->slaveFetch];
		ptty->slaveFetch = (ptty->slaveFetch + 1) % TERM_BUFFER_SIZE;
		outsz++;
	};
	semSignal(&ptty->sem);
	
	return outsz;
};

static ssize_t ptm_read(Inode *inode, File *fp, void *buffer, size_t size, off_t offset)
{
	PseudoTerm *ptty = (PseudoTerm*) fp->filedata;
	int count = semWaitGen(&ptty->masterCounter, (int)size, SEM_W_FILE(fp->oflags), 0);
	if (count < 0)
	{
		ERRNO = -count;
		return -1;
	};
	
	if (count == 0) return 0;
	
	semWait(&ptty->sem);
	ssize_t outsz = 0;
	char *put = (char*) buffer;
	while (count--)
	{
		*put++ = ptty->masterBuffer[ptty->masterFetch];
		ptty->masterFetch = (ptty->masterFetch + 1) % TERM_BUFFER_SIZE;
		outsz++;
	};
	semSignal(&ptty->sem);
	
	return outsz;
};

static ssize_t ptm_write(Inode *inode, File *fp, const void *buffer, size_t size, off_t offset)
{
	PseudoTerm *ptty = (PseudoTerm*) fp->filedata;
	semWait(&ptty->sem);
	
	int numPutMaster = 0;
	int numPutSlave = 0;
	ssize_t wroteTotal = (ssize_t) size;
	
	const char *scan = (const char*) buffer;
	while (size--)
	{
		char c = *scan++;
		if (ptty->state.c_lflag & ISIG)
		{
			char sigChar = 0;
			if ((unsigned char)c == CC_VINTR)
			{
				sigChar = 'C';
				ptty->lineSize = 0;
				siginfo_t si;
				memset(&si, 0, sizeof(siginfo_t));
				si.si_signo = SIGINT;
				signalPidEx(-ptty->pgid, &si, SP_NOPERM);
			}
			else if ((unsigned char)c == CC_VKILL)
			{
				sigChar = 'K';
				ptty->lineSize = 0;
				siginfo_t si;
				memset(&si, 0, sizeof(siginfo_t));
				si.si_signo = SIGKILL;
				signalPidEx(-ptty->pgid, &si, SP_NOPERM);
			};
			
			if (sigChar != 0)
			{
				if (ptty->state.c_lflag & ECHO)
				{
					numPutMaster += 2;
					ptty->masterBuffer[ptty->masterPut] = '^';
					ptty->masterPut = (ptty->masterPut + 1) % TERM_BUFFER_SIZE;
					ptty->masterBuffer[ptty->masterPut] = sigChar;
					ptty->masterPut = (ptty->masterPut + 1) % TERM_BUFFER_SIZE;
				};
				
				continue;
			};
		};
		
		if (ptty->state.c_lflag & ICANON)
		{
			if (c == '\b')
			{
				if (ptty->lineSize != 0)
				{
					ptty->lineSize--;
					if (ptty->state.c_lflag & ECHO)
					{
						ptty->masterBuffer[ptty->masterPut] = '\b';
						ptty->masterPut = (ptty->masterPut+1) % TERM_BUFFER_SIZE;
						numPutMaster++;
					};
				};
			}
			else if ((c == '\n') || (c == '\r'))
			{
				ptty->lineBuffer[ptty->lineSize++] = '\n';
				
				// put the line into the slave buffer
				numPutSlave += ptty->lineSize;
				
				int i;
				for (i=0; i<ptty->lineSize; i++)
				{
					ptty->slaveBuffer[ptty->slavePut] = ptty->lineBuffer[i];
					ptty->slavePut = (ptty->slavePut + 1) % TERM_BUFFER_SIZE;
				};
				
				ptty->lineSize = 0;
				
				// echo the newline to the master if necessary
				if (ptty->state.c_lflag & ECHONL)
				{
					ptty->masterBuffer[ptty->masterPut] = '\n';
					ptty->masterPut = (ptty->masterPut + 1) % TERM_BUFFER_SIZE;
					numPutMaster++;
				};
			}
			else
			{
				// put the character in the line buffer
				if (ptty->lineSize < (TERM_BUFFER_SIZE-1))
				{
					ptty->lineBuffer[ptty->lineSize++] = c;
				};
				
				// echo to master if necessary
				if (ptty->state.c_lflag & ECHO)
				{
					ptty->masterBuffer[ptty->masterPut] = c;
					ptty->masterPut = (ptty->masterPut + 1) % TERM_BUFFER_SIZE;
					numPutMaster++;
				};
			};
		}
		else
		{
			// non-canonical: simply put the character in the slave buffer
			ptty->slaveBuffer[ptty->slavePut] = c;
			ptty->slavePut = (ptty->slavePut + 1) % TERM_BUFFER_SIZE;
			numPutSlave++;
			
			if (c == '\n')
			{
				if (ptty->state.c_lflag & ECHONL)
				{
					ptty->masterBuffer[ptty->masterPut] = c;
					ptty->masterPut = (ptty->masterPut + 1) % TERM_BUFFER_SIZE;
					numPutMaster++;
				};
			}
			else if (ptty->state.c_lflag & ECHO)
			{
				ptty->masterBuffer[ptty->masterPut] = c;
				ptty->masterPut = (ptty->masterPut + 1) % TERM_BUFFER_SIZE;
				numPutMaster++;
			};
		};
	};
	
	semSignal2(&ptty->masterCounter, numPutMaster);
	semSignal2(&ptty->slaveCounter, numPutSlave);
	
	semSignal(&ptty->sem);
	return wroteTotal;
};

static int ptm_ioctl(Inode *inode, File *fp, uint64_t cmd, void *argp)
{
	PseudoTerm *ptty = (PseudoTerm*) fp->filedata;
	
	switch (cmd)
	{
	case IOCTL_TTY_GRANTPT:
		//SetDeviceCreds(ptty->devSlave, getCurrentThread()->creds->ruid, getCurrentThread()->creds->rgid);
		semWait(&ptty->devSlave->lock);
		ptty->devSlave->uid = getCurrentThread()->creds->ruid;
		ptty->devSlave->gid = getCurrentThread()->creds->rgid;
		semSignal(&ptty->devSlave->lock);
		return 0;
	case IOCTL_TTY_UNLOCKPT:
		// i see no point
		return 0;
	case IOCTL_TTY_PTSNAME:
		strformat((char*)argp, 256, "/dev/%s", ptty->slaveName);
		return 0;
	default:
		ERRNO = EINVAL;
		return -1;
	};
};

static void* ptmx_open(Inode *inode, int oflags)
{
	PseudoTerm *ptty = NEW(PseudoTerm);
	semInit(&ptty->sem);
	
	semInit2(&ptty->masterCounter, 0);
	semInit2(&ptty->slaveCounter, 0);
	
	ptty->state.c_iflag = ICRNL;
	ptty->state.c_oflag = 0;
	ptty->state.c_cflag = 0;
	ptty->state.c_lflag = ECHO | ECHOE | ECHOK | ECHONL | ICANON | ISIG;

	int i;
	for (i=0; i<NCCS; i++)
	{
		ptty->state.c_cc[i] = i+0x80;
	};
	
	ptty->masterPut = 0;
	ptty->masterFetch = 0;
	
	ptty->slavePut = 0;
	ptty->slaveFetch = 0;
	
	ptty->lineSize = 0;
	
	ptty->sid = 0;
	ptty->pgid = 0;
	ptty->sides = PTTY_MASTER | PTTY_SLAVE;
	
	strformat(ptty->slaveName, 256, "pts%d", __sync_fetch_and_add(&ptsNextNo, 1));
	
	Inode *slave = vfsCreateInode(NULL, VFS_MODE_CHARDEV | 0600);
	slave->fsdata = ptty;
	slave->pread = pts_read;
	slave->pwrite = pts_write;
	slave->ioctl = pts_ioctl;
	slave->free = pts_free;
	
	vfsUprefInode(slave);
	ptty->devSlave = slave;
	
	if (devfsAdd(ptty->slaveName, slave) != 0)
	{
		panic("unexpected failure in adding pseudoterminal slave");
	};
	
	return ptty;
};

void pttyInit()
{
	Inode *ptmx = vfsCreateInode(NULL, VFS_MODE_CHARDEV | 0666);
	ptmx->open = ptmx_open;
	ptmx->pread = ptm_read;
	ptmx->pwrite = ptm_write;
	ptmx->ioctl = ptm_ioctl;
	ptmx->close = ptm_close;
	
	if (devfsAdd("ptmx", ptmx) != 0)
	{
		FAILED();
		panic("failed to create /dev/ptmx");
	};
};

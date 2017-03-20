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

/**
 * DeleteDevice() frees associated data so we wrap the terminal description inside
 * a temporary container that can be safely freed by the device being destroyed.
 */
typedef struct
{
	PseudoTerm			*handle;
} PttyDevice;

static void ptty_downref(PseudoTerm *ptty)
{
	if (__sync_fetch_and_add(&ptty->refcount, -1) == 1)
	{
		semWait(&ptty->sem);
		kfree(ptty);
	};
};

static void ptm_close(File *fp)
{
	PseudoTerm *ptty = (PseudoTerm*) fp->fsdata;
	signalPid(-ptty->pgid, SIGHUP);
	DeleteDevice(ptty->devSlave);
	ptty_downref(ptty);
};

static void pts_close(File *fp)
{
	ptty_downref((PseudoTerm*)fp->fsdata);
};

static ssize_t pts_write(File *fp, const void *buffer, size_t size)
{
	PseudoTerm *ptty = (PseudoTerm*) fp->fsdata;
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

static int pts_ioctl(File *fp, uint64_t cmd, void *argp)
{
	PseudoTerm *ptty = (PseudoTerm*) fp->fsdata;
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

static ssize_t pts_read(File *fp, void *buffer, size_t size)
{
	PseudoTerm *ptty = (PseudoTerm*) fp->fsdata;
	int count = semWaitGen(&ptty->slaveCounter, (int)size, SEM_W_FILE(fp->oflag), 0);
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

static int pts_open(void *data, File *fp, size_t szFile)
{
	PttyDevice *devinfo = (PttyDevice*) data;
	PseudoTerm *ptty = devinfo->handle;
	
	semWait(&ptty->sem);
	if ((ptty->sid != 0) && (ptty->sid != getCurrentThread()->creds->sid))
	{
		semSignal(&ptty->sem);
		return VFS_BUSY;
	};
	
	__sync_fetch_and_add(&ptty->refcount, 1);
	if (ptty->sid == 0)
	{
		ptty->sid = getCurrentThread()->creds->sid;
		ptty->pgid = getCurrentThread()->creds->pgid;
	};
	semSignal(&ptty->sem);
	
	memset(fp, 0, szFile);
	fp->fsdata = ptty;
	fp->oflag = O_TERMINAL;
	fp->write = pts_write;
	fp->read = pts_read;
	fp->ioctl = pts_ioctl;
	fp->close = pts_close;
	
	return 0;
};

static ssize_t ptm_read(File *fp, void *buffer, size_t size)
{
	PseudoTerm *ptty = (PseudoTerm*) fp->fsdata;
	int count = semWaitGen(&ptty->masterCounter, (int)size, SEM_W_FILE(fp->oflag), 0);
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

static ssize_t ptm_write(File *fp, const void *buffer, size_t size)
{
	PseudoTerm *ptty = (PseudoTerm*) fp->fsdata;
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
		};
	};
	
	semSignal2(&ptty->masterCounter, numPutMaster);
	semSignal2(&ptty->slaveCounter, numPutSlave);
	
	semSignal(&ptty->sem);
	return wroteTotal;
};

static int ptm_ioctl(File *fp, uint64_t cmd, void *argp)
{
	PseudoTerm *ptty = (PseudoTerm*) fp->fsdata;
	
	switch (cmd)
	{
	case IOCTL_TTY_GRANTPT:
		SetDeviceCreds(ptty->devSlave, getCurrentThread()->creds->ruid, getCurrentThread()->creds->rgid);
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

static int ptmx_open(void *ignore, File *fp, size_t szFile)
{
	(void)ignore;
	memset(fp, 0, szFile);
	
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
	ptty->refcount = 1;
	
	semWait(&ptty->sem);
	strformat(ptty->slaveName, 256, "pts%d", __sync_fetch_and_add(&ptsNextNo, 1));
	PttyDevice *devinfo = NEW(PttyDevice);
	devinfo->handle = ptty;
	ptty->devSlave = AddDevice(ptty->slaveName, devinfo, pts_open, 0600);
	semSignal(&ptty->sem);
	
	fp->fsdata = ptty;
	fp->read = ptm_read;
	fp->write = ptm_write;
	fp->ioctl = ptm_ioctl;
	fp->close = ptm_close;
	
	return 0;
};

void pttyInit()
{
	Device ptmx = AddDevice("ptmx", NULL, ptmx_open, 0666);
	if (ptmx == NULL)
	{
		FAILED();
		panic("failed to create /dev/ptmx");
	};
};

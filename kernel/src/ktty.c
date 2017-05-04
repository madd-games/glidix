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

#include <glidix/ktty.h>
#include <glidix/memory.h>
#include <glidix/vfs.h>
#include <glidix/string.h>
#include <glidix/console.h>
#include <glidix/semaphore.h>
#include <glidix/common.h>
#include <glidix/errno.h>
#include <glidix/waitcnt.h>
#include <glidix/term.h>

#define	INPUT_BUFFER_SIZE			4096

static char					inputBuffer[INPUT_BUFFER_SIZE];
static volatile int				inputRead;
static volatile int				inputWrite;
static Semaphore				semCount;
static Semaphore				semInput;
static Semaphore				semLineBuffer;
static volatile int				lineBufferSize;
static struct termios				termState;
static int					termGroup = 1;

static ssize_t termWrite(File *file, const void *buffer, size_t size)
{
	if (getCurrentThread()->creds->pgid != termGroup)
	{
		cli();
		lockSched();
		siginfo_t si;
		si.si_signo = SIGTTOU;
		sendSignal(getCurrentThread(), &si);
		unlockSched();
		sti();
		ERRNO = ENOTTY;
		return -1;
	};

	kputbuf((const char*) buffer, size);
	return size;
};

static ssize_t termRead(File *fp, void *buffer, size_t size)
{
	if (getCurrentThread()->creds->pgid != termGroup)
	{
		cli();
		lockSched();
		siginfo_t si;
		si.si_signo = SIGTTIN;
		sendSignal(getCurrentThread(), &si);
		unlockSched();
		sti();
		ERRNO = ENOTTY;
		return -1;
	};
	
	int count = semWaitGen(&semCount, (int) size, SEM_W_INTR, 0);
	if (count < 0)
	{
		ERRNO = -count;
		return -1;
	};

	semWait(&semInput);

	if (size > (size_t) count) size = (size_t) count;
	ssize_t out = 0;
	while (size > 0)
	{
		if (inputRead == INPUT_BUFFER_SIZE) inputRead = 0;
		size_t max = INPUT_BUFFER_SIZE - inputRead;
		if (max > size) max = size;
		memcpy(buffer, &inputBuffer[inputRead], max);
		size -= max;
		out += max;
		inputRead += max;
		buffer = (void*)((uint64_t)buffer + max);
	};
	semSignal(&semInput);
	return out;
};

static int termDup(File *old, File *new, size_t szfile)
{
	memcpy(new, old, szfile);
	return 0;
};

int termIoctl(File *fp, uint64_t cmd, void *argp)
{
	Thread *target = NULL;
	Thread *scan;
	int pgid;
	struct termios *tc = (struct termios*) argp;

	switch (cmd)
	{
	case IOCTL_TTY_GETATTR:
		memcpy(tc, &termState, sizeof(struct termios));
		return 0;
	case IOCTL_TTY_SETATTR:
		termState.c_iflag = tc->c_iflag;
		termState.c_oflag = tc->c_oflag;
		termState.c_cflag = tc->c_cflag;
		termState.c_lflag = tc->c_lflag;
		return 0;
	case IOCTL_TTY_GETPGID:
		*((int*)argp) = termGroup;
		return 0;
	case IOCTL_TTY_SETPGID:
		if (getCurrentThread()->creds->sid != 1)
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
			return -1;
		};
		if (target->creds->sid != 1)
		{
			unlockSched();
			sti();
			ERRNO = EPERM;
			return -1;
		};
		unlockSched();
		sti();
		termGroup = pgid;
		return 0;
	default:
		getCurrentThread()->therrno = EINVAL;
		return -1;
	};
};

void termPutChar(char c)
{
	semWait(&semLineBuffer);
	if ((c == '\b') && (termState.c_lflag & ICANON))
	{
		if (lineBufferSize != 0)
		{
			if (inputWrite == 0) inputWrite = INPUT_BUFFER_SIZE;
			else inputWrite--;
			lineBufferSize--;
			if (termState.c_lflag & ECHO) kprintf("\b");
		};

		semSignal(&semLineBuffer);
		return;
	}
	else if (c < 0x80)
	{
		inputBuffer[inputWrite++] = c;
		if (inputWrite == INPUT_BUFFER_SIZE) inputWrite = 0;
		lineBufferSize++;
	};

	__sync_synchronize();
	if (((c & 0x80) == 0) && (termState.c_lflag & ECHO))
	{
		kprintf("%c", c);
	}
	else if ((c == '\n') && (termState.c_lflag & ECHONL))
	{
		kprintf("\n");
	};

	if ((termState.c_lflag & ICANON) == 0)
	{
		semSignal2(&semCount, lineBufferSize);
		lineBufferSize = 0;
	}
	else if (c == '\n')
	{
		semSignal2(&semCount, lineBufferSize);
		lineBufferSize = 0;
	};

	if ((unsigned char)c == CC_VINTR)
	{
		if (termState.c_lflag & ECHO) kprintf("^C");
		if (termState.c_lflag & ICANON)
		{
			inputWrite = inputRead;
			while (semWaitGen(&semCount, 1024, SEM_W_NONBLOCK, 0) > 0);
			lineBufferSize = 0;
		};
		if (termGroup != 0) signalPid(-termGroup, SIGINT);
	};

	semSignal(&semLineBuffer);
};

void setupTerminal(FileTable *ftab)
{
	File *termout = (File*) kmalloc(sizeof(File));
	memset(termout, 0, sizeof(File));

	termout->write = &termWrite;
	termout->dup = &termDup;
	termout->oflag = O_WRONLY | O_TERMINAL;
	termout->ioctl = &termIoctl;
	termout->refcount = 1;

	inputWrite = 0;
	inputRead = 0;
	lineBufferSize = 0;

	File *termin = (File*) kmalloc(sizeof(File));
	memset(termin, 0, sizeof(File));
	termin->oflag = O_RDONLY | O_TERMINAL;
	termin->read = &termRead;
	termin->dup = &termDup;
	termin->ioctl = &termIoctl;
	termin->refcount = 1;

	//ftab->entries[0] = termin;
	//ftab->entries[1] = termout;
	File *termerr = (File*) kmalloc(sizeof(File));
	termDup(termout, termerr, sizeof(File));
	//ftab->entries[2] = termerr;
	
	if (ftabAlloc(ftab) != 0) panic("ftabAlloc did not return 0!");
	if (ftabAlloc(ftab) != 1) panic("ftabAlloc did not return 1!");
	if (ftabAlloc(ftab) != 2) panic("ftabAlloc did not return 2!");

	ftabSet(ftab, 0, termin);
	ftabSet(ftab, 1, termout);
	ftabSet(ftab, 2, termerr);

	termState.c_iflag = ICRNL;
	termState.c_oflag = 0;
	termState.c_cflag = 0;
	termState.c_lflag = ECHO | ECHOE | ECHOK | ECHONL | ICANON | ISIG;

	int i;
	for (i=0; i<NCCS; i++)
	{
		termState.c_cc[i] = i+0x80;
	};

	semInit2(&semCount, 0);
	semInit(&semInput);
	semInit(&semLineBuffer);
};

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

#include <glidix/ktty.h>
#include <glidix/memory.h>
#include <glidix/vfs.h>
#include <glidix/string.h>
#include <glidix/console.h>
#include <glidix/semaphore.h>
#include <glidix/common.h>
#include <glidix/errno.h>

#define	INPUT_BUFFER_SIZE	256

static volatile ATOMIC(char)	inputBuffer[256];
static volatile ATOMIC(int)	inputPut;
static volatile ATOMIC(int)	inputRead;
static volatile ATOMIC(int)	lineCount;
static volatile ATOMIC(int)	lineCharCount;
static Semaphore		inputLock;

static ssize_t termWrite(File *file, const void *buffer, size_t size)
{
	kputbuf((const char*) buffer, size);
	return size;
};

static char getChar()
{
	char c;
	if (inputPut == inputRead) c = 0;
	else
	{
		c = inputBuffer[inputRead];
		inputRead++;
		if (inputRead == INPUT_BUFFER_SIZE) inputRead = 0;
	};

	return c;
};

static ssize_t termRead(File *fp, void *buffer, size_t size)
{
	int sigcntOrig = getCurrentThread()->sigcnt;

	ssize_t count = 0;
	char *out = (char*) buffer;
	while (size)
	{
		semWait(&inputLock);
		if (lineCount == 0)
		{
			semSignal(&inputLock);
			if (getCurrentThread()->sigcnt > sigcntOrig)
			{
				getCurrentThread()->therrno = EINTR;
				return -1;
			};
			continue;
		};
		char c;
		if ((c = getChar()) == 0)
		{
			semSignal(&inputLock);
			return count;
		};
		*out++ = c;
		count++;
		if (c == '\n') lineCount--;
		size--;
		semSignal(&inputLock);
	};

	return count;
};

static int termDup(File *old, File *new, size_t szfile)
{
	memcpy(new, old, szfile);
	return 0;
};

void termPutChar(char c)
{
	//kprintf_debug("PUT CHAR '%c'\n", c);

	semWait(&inputLock);
	if (c == '\b')
	{
		if (lineCharCount != 0)
		{
			lineCharCount--;
			if (inputPut == 0)
			{
				inputPut = INPUT_BUFFER_SIZE - 1;
			}
			else
			{
				inputPut--;
			};
			kprintf("\b");
		};

		semSignal(&inputLock);
		//kprintf_debug("end put char\n");
		return;
	};

	kprintf("%c", c);
	int newInputPut = inputPut + 1;
	if (newInputPut == INPUT_BUFFER_SIZE) newInputPut = 0;
	inputBuffer[inputPut] = c;
	inputPut = newInputPut;
	if (c == '\n')
	{
		lineCount++;
		lineCharCount = 0;
	}
	else
	{
		lineCharCount++;
	};
	semSignal(&inputLock);
	//kprintf_debug("end put char\n");
};

void setupTerminal(FileTable *ftab)
{
	File *termout = (File*) kmalloc(sizeof(File));
	memset(termout, 0, sizeof(File));

	termout->write = &termWrite;
	termout->dup = &termDup;
	termout->oflag = O_WRONLY;

	inputPut = 0;
	inputRead = 0;
	lineCount = 0;
	lineCharCount = 0;

	File *termin = (File*) kmalloc(sizeof(File));
	memset(termin, 0, sizeof(File));
	termin->oflag = O_RDONLY;
	termin->read = &termRead;
	termin->dup = &termDup;

	ftab->entries[0] = termin;
	ftab->entries[1] = termout;
	File *termerr = (File*) kmalloc(sizeof(File));
	termDup(termout, termerr, sizeof(File));
	ftab->entries[2] = termerr;

	semInit(&inputLock);
};

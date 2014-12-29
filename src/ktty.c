/*
	Glidix kernel

	Copyright (c) 2014, Madd Games.
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
#include <glidix/spinlock.h>

#define	INPUT_BUFFER_SIZE	256

static volatile char	inputBuffer[256];
static volatile int	inputPut;
static volatile int	inputRead;
static volatile int	lineCount;
static volatile int	lineCharCount;
static Spinlock		inputLock;

static ssize_t termWrite(File *file, const void *buffer, size_t size)
{
	kputbuf((const char*) buffer, size);
	return size;
};

static char getChar()
{
	spinlockAcquire(&inputLock);
	char c;
	if (inputPut == inputRead) c = 0;
	else
	{
		c = inputBuffer[inputRead];
		inputRead++;
		if (inputRead == INPUT_BUFFER_SIZE) inputRead = 0;
	};
	spinlockRelease(&inputLock);

	return c;
};

static ssize_t termRead(File *fp, void *buffer, size_t size)
{
	ssize_t count = 0;
	char *out = (char*) buffer;
	while (size--)
	{
		while (lineCount == 0);
		char c;
		while ((c = getChar()) == 0);
		*out++ = c;
		count++;
		if (c == '\n') lineCount--;
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
	spinlockAcquire(&inputLock);
	if (c == '\b')
	{
		if (lineCharCount != 0)
		{
			lineCharCount--;
			if (inputPut == 0)
			{
				inputPut = INPUT_BUFFER_SIZE - inputPut;
			}
			else
			{
				inputPut--;
			};
			kprintf("\b");
		};

		spinlockRelease(&inputLock);
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
	spinlockRelease(&inputLock);
};

void setupTerminal(FileTable *ftab)
{
	File *termout = (File*) kmalloc(sizeof(File));
	memset(termout, 0, sizeof(File));

	termout->write = &termWrite;
	termout->dup = &termDup;

	inputPut = 0;
	inputRead = 0;
	lineCount = 0;
	lineCharCount = 0;

	File *termin = (File*) kmalloc(sizeof(File));
	memset(termin, 0, sizeof(File));
	termin->read = &termRead;
	termin->dup = &termDup;

	ftab->entries[0] = termin;
	ftab->entries[1] = termout;
	File *termerr = (File*) kmalloc(sizeof(File));
	termDup(termout, termerr, sizeof(File));
	ftab->entries[2] = termerr;

	spinlockRelease(&inputLock);
};

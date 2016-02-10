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

#include <glidix/module.h>
#include <glidix/console.h>
#include <glidix/vfs.h>
#include <glidix/devfs.h>
#include <glidix/string.h>

static Device devRandom;
static Device devZero;
static Device devNull;

static unsigned int lastRand = 56;

static char randByte()
{
	unsigned int nextRand = (1103515245 * lastRand + 12345);
	lastRand = nextRand;
	return (char) nextRand;
};

static ssize_t readRandom(File *fp, void *buffer, size_t size)
{
	char *put = (char*) buffer;
	ssize_t ret = 0;

	while (size--)
	{
		*put++ = randByte();
		ret++;
	};

	return ret;
};

static ssize_t readZero(File *fp, void *buffer, size_t size)
{
	memset(buffer, 0, size);
	return (ssize_t) size;
};

static ssize_t readNull(File *fp, void *buffer, size_t size)
{
	return (ssize_t) 0;
};

static ssize_t writeNull(File *fp, const void *buffer, size_t size)
{
	return (ssize_t) size;
};

static int openRandom(void *data, File *fp, size_t szFile)
{
	fp->read = readRandom;
	fp->write = writeNull;
	return 0;
};

static int openZero(void *data, File *fp, size_t szFile)
{
	fp->read = readZero;
	fp->write = writeNull;
	return 0;
};

static int openNull(void *data, File *fp, size_t szFile)
{
	fp->read = readNull;
	fp->write = writeNull;
	return 0;
};

MODULE_INIT(const char *opt)
{
	devRandom = AddDevice("random", NULL, openRandom, 0);
	if (devRandom == NULL)
	{
		kprintf("specdevs: could not add the random device!\n");
		return 1;
	};
	
	devZero = AddDevice("zero", NULL, openZero, 0);
	if (devZero == NULL)
	{
		kprintf("specdevs: could not add the zero device!\n");
		return 1;
	};
	
	devNull = AddDevice("null", NULL, openNull, 0);
	if (devNull == NULL)
	{
		kprintf("specdevs: could not add the null device!\n");
		return 1;
	};
	
	return 0;
};

MODULE_FINI()
{
	DeleteDevice(devRandom);
	kprintf("Unloaded mod_random\n");
	return 0;
};

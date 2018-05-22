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

#include <glidix/util/memory.h>
#include <glidix/thread/mutex.h>
#include <glidix/display/console.h>
#include <glidix/hw/pagetab.h>
#include <glidix/util/string.h>
#include <glidix/hw/physmem.h>
#include <glidix/util/isp.h>
#include <glidix/storage/storage.h>
#include <glidix/util/random.h>
#include <stdint.h>

static uint64_t placement;
int readyForDynamic;
uint64_t placementEnd;
static Mutex heapLock;

void initMemoryPhase1(uint64_t pc, uint64_t size)
{
	placement = pc;
	placementEnd = pc + size;
	mutexInit(&heapLock);
	readyForDynamic = 0;
};

void *_kxmalloc(size_t size, int flags, const char *aid, int lineno)
{	
	if (size == 0) return NULL;
	if (size >= 0x40000000)
	{
		// do not even attempt allocations of size 1GB (the heap maximum) or more
		return NULL;
	};
	
	if (readyForDynamic)
	{
		return kxmallocDynamic(size, flags, aid, lineno);
	};

	mutexLock(&heapLock);

	// align the placement addr on a page boundary if neccessary
	if (flags & MEM_PAGEALIGN)
	{
		if ((placement & 0xFFF) != 0)
		{
			placement &= ~0xFFF;
			placement += 0x1000;
		};
	}
	else
	{
		// 16-byte alignment
		placement = (placement+1) & ~0xF;
	};

	void *ret = (void*) placement;
	placement += size;
	if (placement >= placementEnd)
	{
		panic("placement allocation exhausted!");
	};
	mutexUnlock(&heapLock);
	return ret;
};

void *_kmalloc(size_t size, const char *aid, int lineno)
{
	return _kxmalloc(size, 0, aid, lineno);
};

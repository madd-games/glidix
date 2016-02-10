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

#include <glidix/mp.h>
#include <glidix/console.h>
#include <glidix/common.h>
#include <glidix/string.h>
#include <glidix/isp.h>
#include <glidix/memory.h>

uint8_t mpChecksum(const void *data_, size_t n)
{
	const uint8_t *data = (const uint8_t*) data_;
	size_t i;
	uint8_t sum = 0;
	for (i=0; i<n; i++)
	{
		sum += data[i];
	};
	
	return sum;
};

void mpParse(void *table)
{
	MPConfigHeader *head = (MPConfigHeader*) table;
	
	uint8_t *scan = (uint8_t*) &head[1];
	size_t entryCount = (size_t) head->entryCount;
	
	while (entryCount--)
	{
		if ((*scan) == 1)
		{
			MPBusEntry *bus = (MPBusEntry*) scan;
			kprintf("BUS: %d [%s]\n", bus->busid, bus->bustype);
		};
		
		if ((*scan) == 3)
		{
			MPIntAssignment *intr = (MPIntAssignment*) scan;
			kprintf("INT: bus %d, int %d -> %d\n", intr->bus, intr->irq, intr->intpin);
		};
		
		if ((*scan) == 0)
		{
			scan += 20;
		}
		else
		{
			scan += 8;
		};
	};
	
	panic("stop");
};

void mpFound(MPFloatPtr *fptr)
{
	MPConfigHeader chead;
	pmem_read(&chead, (uint64_t) fptr->conftabPhys, sizeof(MPConfigHeader));

	size_t size = (size_t) chead.length;
	void *table = kmalloc(size);
	pmem_read(table, (uint64_t) fptr->conftabPhys, size);
	mpParse(table);
};

void mpInit()
{
	kprintf("Searching for MP tables... ");
	
	uint64_t addr = 0xFFFF8000000E0000;
	for (addr=0xFFFF8000000E0000; addr<0xFFFF8000000FFFFF; addr+=0x10)
	{
		MPFloatPtr *fptr = (MPFloatPtr*) addr;
		if (memcmp(fptr->sig, "_MP_", 4) == 0)
		{
			size_t size = fptr->length * 16;
			if (mpChecksum(fptr, size) == 0)
			{
				DONE();
				mpFound(fptr);
				return;
			};
		};
	};
	
	FAILED();
	panic("could not find MP tables!");
};

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

#include <glidix/console.h>
#include <glidix/common.h>
#include <glidix/idt.h>
#include <glidix/physmem.h>
#include <glidix/memory.h>
#include <glidix/pagetab.h>
#include <glidix/isp.h>
#include <glidix/string.h>
#include <glidix/port.h>
#include <glidix/sched.h>

extern int _bootstrap_stack;
extern int end;

void expandHeap();

void kmain(MultibootInfo *info)
{
	initConsole();
	kprintf("Successfully booted into 64-bit mode\n");
#if 0
	uint64_t tmp = (uint64_t) (&_bootstrap_stack);
	kprintf("Bootstrap stack: %d\n", (int) (tmp));
	tmp = (uint64_t) &end;
	kprintf("End of memory: %d\n", (int) tmp);
#endif
	kprintf("Initializing the IDT... ");
	initIDT();
	kprintf("%$\x02" "Done%#\n");

	kprintf("Checking amount of memory... ");
	int memSize = info->memLower + info->memUpper;
	if (info->flags & 1)
	{
		kprintf("%$\x01%dMB%#\n", (memSize/1024));
	}
	else
	{
		kprintf("%$\x04" "Failed%#\n");
		memSize = 1024 * 64;
		kprintf("w: Assuming 64MB of RAM\n");
	};

	kprintf("Initializing memory allocation phase 1... ");
	initMemoryPhase1();
	kprintf("%$\x02" "Done%#\n");

	kprintf("Initializing the physical memory manager... ");
	initPhysMem(memSize/4);
	kprintf("%$\x02" "Done%#\n");

	kprintf("Initializing the ISP... ");
	ispInit();
	kprintf("%$\x02" "Done%#\n");

	kprintf("Initializing memory allocation phase 2... ");
	initMemoryPhase2();
	kprintf("%$\x02" "Done%#\n");

	kprintf("Initializing the frame stack... ");
	initPhysMem2();
	kprintf("%$\x02" "Done%#\n");

	kprintf("Initializing the PIT... ");
	uint16_t divisor = 1193180 / 50;		// 50 Hz
	outb(0x43, 0x36);
	uint8_t l = (uint8_t)(divisor & 0xFF);
	uint8_t h = (uint8_t)( (divisor>>8) & 0xFF );
	outb(0x40, l);
	outb(0x40, h);
	kprintf("%$\x02" "Done%#\n");

	kprintf("Initializing the scheduler... ");
	initSched();
	// "Done" will be displayed by initSched()
};

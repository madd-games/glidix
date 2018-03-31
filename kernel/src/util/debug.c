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

#include <glidix/util/common.h>
#include <glidix/display/console.h>
#include <glidix/module/symtab.h>
#include <glidix/thread/sched.h>
#include <glidix/module/module.h>
#include <glidix/util/string.h>
#include <glidix/thread/semaphore.h>
#include <glidix/util/time.h>
#include <glidix/hw/cpu.h>
#include <glidix/util/memory.h>
#include <glidix/util/catch.h>

void enterDebugContext(Regs *regs);			// sched.asm

void stackTrace(uint64_t rip, uint64_t rbp)
{
	kprintf("Stack trace:\n");
	kprintf("RIP                RBP                Module:Symbol+Offset\n");
	
	if (catch() == 0)
	{
		while (1)
		{
			SymbolInfo info;
			findDebugSymbolInModules(rip, &info);
			kprintf("0x%016lX 0x%016lX %s:%s+%lu\n", rip, rbp, info.modname, info.symname, info.offset);
			if (rbp == 0) break;
			uint64_t *lastFrame = (uint64_t*) rbp;
			rbp = lastFrame[0];
			rip = lastFrame[1];
			if (rbp == 0) break;
			if (rip < 0xFFFF800000000000) break;
		};
		uncatch();
	}
	else
	{
		kprintf("EXCEPTION DURING TRACE\n");
	};
	kprintf("END.\n");
};

void debugKernel(Regs *regs)
{
	while (1)
	{
		kprintf_debug("RAX=0  - panic and stop\n");
		kprintf_debug("RAX=1  - show stack trace\n");
		kprintf_debug("RAX=2  - dump runqueue\n");
		kprintf_debug("RAX=3  - dump module list\n");
		kprintf_debug("RAX=4  - switch to a different process\n");
		kprintf_debug("RAX=5  - dump current context\n");
		kprintf_debug("RAX=6  - dump memory mappings\n");
		kprintf_debug("RAX=7  - show semaphore state\n");
		kprintf_debug("RAX=8  - debug the current context in bochs\n");
		kprintf_debug("RAX=9  - show stack trace of interrupted state\n");
		kprintf_debug("RAX=10 - show interrupted registers\n");
		kprintf_debug("RAX=11 - show heap dump\n");
		uint64_t option = getDebugInput();

		switch (option)
		{
		case 0:
			panic("Kernel died");
			break;
		case 1:
			stackTrace(getCurrentThread()->regs.rip, getCurrentThread()->regs.rbp);
			break;
		case 2:
			dumpRunqueue();
			break;
		case 3:
			dumpModules();
			break;
		case 4:
			kprintf_debug(" *** PLEASE SET RAX TO INDEX ***\n");
			switchTaskToIndex((int)getDebugInput());
			break;
		case 5:
			kprintf("CPU: %d\n", getCurrentCPU()->id);
			int pid = 0;
			if (getCurrentThread()->creds != NULL)
			{
				pid = getCurrentThread()->creds->pid;
			};
			kprintf("PID: %d, '%s'\n", pid, getCurrentThread()->name);
			kdumpregs(&getCurrentThread()->regs);
			break;
		case 6:
			kprintf("PID: %d, '%s'\n", getCurrentThread()->creds->pid, getCurrentThread()->name);
			if (getCurrentThread()->pm == NULL)
			{
				kprintf("This thread has no memory\n");
				break;
			};
			kprintf(" *** YOU MAY SPECIFY AN ADDRESS TO CHECK FOR IN RAX ***\n");
			vmDump(getCurrentThread()->pm, getDebugInput());
			break;
		case 7:
			kprintf(" *** SET RAX TO SEMAPHORE ADDRESS ***\n");
			//semDump((Semaphore*)getDebugInput());
			break;
		case 8:
			getCurrentThread()->regs.rflags &= ~(1 << 9);	// cli
			enterDebugContext(&getCurrentThread()->regs);
			break;
		case 9:
			stackTrace(regs->rip, regs->rbp);
			break;
		case 10:
			kdumpregs(regs);
			break;
		case 11:
			heapDump();
			break;
		};
	};
};

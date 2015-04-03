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

#include <glidix/common.h>
#include <glidix/console.h>
#include <glidix/symtab.h>
#include <glidix/sched.h>
#include <glidix/module.h>
#include <glidix/string.h>
#include <glidix/semaphore.h>

void enterDebugContext(Regs *regs);			// sched.asm

void stackTrace(uint64_t rip, uint64_t rbp)
{
	kprintf("Stack trace:\n");
	kprintf("RIP                RBP                Module:Symbol+Offset\n");
	while (1)
	{
		//Symbol *symbol = findSymbolForAddr(rip);
		SymbolInfo info;
		findDebugSymbolInModules(rip, &info);
		kprintf("%a %a %s:%s+%d\n", rip, rbp, info.modname, info.symname, info.offset);
		if (rbp == 0) break;
		uint64_t *lastFrame = (uint64_t*) rbp;
		rbp = lastFrame[0];
		rip = lastFrame[1];
		if (rip == 0) break;
	};
	kprintf("END.\n");
};

void debugKernel(Regs *regs)
{
	Thread *thread = getCurrentThread();
	fpuSave(&thread->fpuRegs);
	memcpy(&thread->regs, regs, sizeof(Regs));

	while (1)
	{
		kprintf_debug("RAX=0 - panic and stop\n");
		kprintf_debug("RAX=1 - show stack trace\n");
		kprintf_debug("RAX=2 - dump runqueue\n");
		kprintf_debug("RAX=3 - dump module list\n");
		kprintf_debug("RAX=4 - switch to a different process\n");
		kprintf_debug("RAX=5 - dump current context\n");
		kprintf_debug("RAX=6 - dump memory mappings\n");
		kprintf_debug("RAX=7 - show semaphore state\n");
		kprintf_debug("RAX=8 - debug the current context in bochs\n");
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
			kprintf("PID: %d, '%s'\n", getCurrentThread()->pid, getCurrentThread()->name);
			kdumpregs(&getCurrentThread()->regs);
			break;
		case 6:
			kprintf("PID: %d, '%s'\n", getCurrentThread()->pid, getCurrentThread()->name);
			if (getCurrentThread()->pm == NULL)
			{
				kprintf("This thread has no memory\n");
				break;
			};
			kprintf(" *** YOU MAY SPECIFY AN ADDRESS TO CHECK FOR IN RAX ***\n");
			dumpProcessMemory(getCurrentThread()->pm, getDebugInput());
			break;
		case 7:
			kprintf(" *** SET RAX TO SEMAPHORE ADDRESS ***\n");
			semDump((Semaphore*)getDebugInput());
			break;
		case 8:
			getCurrentThread()->regs.rflags &= ~(1 << 9);	// cli
			enterDebugContext(&getCurrentThread()->regs);
			break;
		};
	};
};

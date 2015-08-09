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
#include <glidix/mount.h>
#include <glidix/initrdfs.h>
#include <glidix/procfs.h>
#include <glidix/procmem.h>
#include <glidix/vfs.h>
#include <glidix/ktty.h>
#include <glidix/symtab.h>
#include <glidix/module.h>
#include <glidix/devfs.h>
#include <glidix/pci.h>
#include <glidix/storage.h>
#include <glidix/fsdriver.h>
#include <glidix/time.h>
#include <glidix/interp.h>
#include <glidix/fpu.h>
#include <glidix/apic.h>
#include <glidix/acpi.h>
#include <glidix/netif.h>

extern int _bootstrap_stack;
extern int end;
extern uint32_t quantumTicks;
KernelStatus kernelStatus;

void expandHeap();
extern int masterHeader;
void _syscall_entry();
void trapKernel();

void kmain(MultibootInfo *info)
{
	ASM("cli");
	kernelStatus = KERNEL_RUNNING;
	initConsole();
	kprintf("Successfully booted into 64-bit mode\n");
	if (info->modsCount != 1)
	{
		panic("the initrd was not loaded");
	};

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
		panic("could not determine memory size");
	};

	if ((info->flags & (1 << 6)) == 0)
	{
		panic("no memory map from bootloader");
	};

	uint64_t mmapAddr = (uint64_t) info->mmapAddr + 0xFFFF800000000000;
	uint64_t mmapEnd = mmapAddr + info->mmapLen;
	kprintf("Memory map address: %a memory map size = %d\n", mmapAddr, info->mmapLen);
	MultibootMemoryMap *mmap = (MultibootMemoryMap*) mmapAddr;
	kprintf("Size\tBase\tLen\tType\n");
	while ((uint64_t)mmap < mmapEnd)
	{
		kprintf("%d\t%a\t%d\t%d\n", mmap->size, mmap->baseAddr, mmap->len, mmap->type);
		mmap = (MultibootMemoryMap*) ((uint64_t) mmap + mmap->size + 4);
	};

	MultibootModule *mod = (MultibootModule*) ((uint64_t) info->modsAddr + 0xFFFF800000000000);
	uint64_t end = (uint64_t) mod->modEnd + 0xFFFF800000000000;

	kprintf("Initializing memory allocation phase 1 (base=%a)... ", end);
	initMemoryPhase1(end);
	kprintf("%$\x02" "Done%#\n");

	kprintf("Initializing the physical memory manager (%d pages)... ", (memSize/4));
	initPhysMem(memSize/4, (MultibootMemoryMap*) mmapAddr, mmapEnd);
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

	initModuleInterface();

	kprintf("Getting ACPI info... ");
	acpiInit();

	kprintf("Initializing the FPU... ");
	fpuInit();
	DONE();

	kprintf("Initializing the VFS... ");
	vfsInit();
	kprintf("%$\x02" "Done%#\n");

	kprintf("Initializing the initrdfs... ");
	initInitrdfs(info);
	kprintf("%$\x02" "Done%#\n");

	kprintf("Initializing the procfs... ");
	initProcfs();
	kprintf("%$\x02" "Done%#\n");

	kprintf("Initializing the devfs... ");
	initDevfs();
	kprintf("%$\x02" "Done%#\n");

	kprintf("Initializing PCI... ");
	pciInit();
	kprintf("%$\x02" "Done%#\n");

	kprintf("Initializing the FS driver interface... ");
	initFSDrivers();
	kprintf("%$\x02" "Done%#\n");

	kprintf("Initializing the PIT... ");
	uint16_t divisor = 1193180 / 1000;		// 1000 Hz
	outb(0x43, 0x36);
	uint8_t l = (uint8_t)(divisor & 0xFF);
	uint8_t h = (uint8_t)( (divisor>>8) & 0xFF );
	outb(0x40, l);
	outb(0x40, h);
	kprintf("%$\x02" "Done%#\n");

	kprintf("Initializing the APIC timer...");
	ASM("sti");
	apic->timerDivide = 3;
	apic->timerInitCount = 0xFFFFFFFF;
	sleep(35);
	apic->lvtTimer = 0;
	quantumTicks = 0xFFFFFFFF - apic->timerCurrentCount;
	apic->timerInitCount = 0;
	// put the timer in single-shot mode at the appropriate interrupt vector.
	apic->lvtTimer = I_APIC_TIMER;
	DONE();

	kprintf_debug(" *** TO TRAP THE KERNEL *** \n");
	kprintf_debug(" set r15=rip\n");
	kprintf_debug(" set rip=%a\n", &trapKernel);
	kprintf_debug(" *** END OF INFO *** \n");

	kprintf("Initializing the scheduler and syscalls... ");
	//msrWrite(0xC0000080, msrRead(0xC0000080) | 1);
	//msrWrite(0xC0000081, ((uint64_t)8 << 32));
	//msrWrite(0xC0000082, (uint64_t)(&_syscall_entry));
	//msrWrite(0xC0000084, (1 << 9));
	initSched();
	// "Done" will be displayed by initSched(), and then kmain2() will be called.
};

extern void _jmp_usbs(void *stack);
static void spawnProc(void *stack)
{
	kprintf("%$\x02" "Done%#\n");

	initInterp();

	kprintf("Allocating memory for bootstrap... ");
	FrameList *fl = palloc(2);
	AddSegment(getCurrentThread()->pm, 1, fl, PROT_READ | PROT_WRITE | PROT_EXEC);
	pdownref(fl);
	SetProcessMemory(getCurrentThread()->pm);
	kprintf("%$\x02" "Done%#\n");

	kprintf("Setting up the terminal... ");
	setupTerminal(getCurrentThread()->ftab);
	kprintf("%$\x02" "Done%#\n");

	kprintf("Loading /initrd/usbs... ");
	int err;
	File *file = vfsOpen("/initrd/usbs", VFS_CHECK_ACCESS, &err);
	if (file == NULL)
	{
		kprintf("%$\x04" "Failed%#\n");
		panic("failed to open /initrd/usbs");
	};
	ssize_t count = vfsRead(file, (void*) 0x1000, 0x1000);
	if (count < 1)
	{
		kprintf("%$\x04" "Failed%#\n");
		panic("read() /initrd/usbs: %d\n", count);
	};
	vfsClose(file);
	kprintf("%$\x02" "%d bytes%#\n", count);

	kprintf("Control will be transferred to usbs now.\n");
	_jmp_usbs(stack);
};

extern uint64_t getFlagsRegister();

void kmain2()
{
	kprintf("Initializing SDI... ");
	sdInit();
	kprintf("%$\x02" "Done%#\n");

	initMount();
	initSymtab();

	kprintf("Initializing the RTC... ");
	initRTC();
	kprintf("%$\x02" "Done%#\n");

	kprintf("Initializing the network interface... ");
	initNetIf();
	kprintf("%$\x02" "Done%#\n");
	
	kprintf("Starting the spawn process... ");
	MachineState state;
	void *spawnStack = kmalloc(0x1000);
	state.rip = (uint64_t) &spawnProc;
	state.rsp = (uint64_t) spawnStack + 0x1000 - 16;
	((uint64_t*)state.rsp)[0] = 0;
	((uint64_t*)state.rsp)[1] = 0;
	state.rdi = (uint64_t) spawnStack;
	state.rbp = 0;
	Regs regs;
	regs.cs = 8;
	regs.ds = 16;
	regs.rflags = getFlagsRegister() | (1 << 9);
	regs.ss = 0;
	threadClone(&regs, 0, &state);
	// "Done" is displayed by the spawnProc() and that's our job done pretty much.
	// Mark this thread as waiting so that it never wastes any CPU time.
	getCurrentThread()->flags = THREAD_WAITING;
};

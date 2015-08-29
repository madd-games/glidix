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

#include <glidix/idt.h>
#include <glidix/string.h>
#include <glidix/console.h>
#include <glidix/common.h>
#include <glidix/port.h>
#include <glidix/syscall.h>
#include <glidix/sched.h>
#include <glidix/signal.h>
#include <glidix/memory.h>
#include <glidix/apic.h>
#include <glidix/time.h>

IDTEntry idt[256];
IDTPointer idtPtr;
static IRQHandler irqHandlers[16];
static volatile int uptime;

extern void loadIDT();
extern void isr0();
extern void isr1();
extern void isr2();
extern void isr3();
extern void isr4();
extern void isr5();
extern void isr6();
extern void isr7();
extern void isr8();
extern void isr9();
extern void isr10();
extern void isr11();
extern void isr12();
extern void isr13();
extern void isr14();
extern void isr15();
extern void isr16();
extern void isr17();
extern void isr18();
extern void isr19();
extern void isr20();
extern void isr21();
extern void isr22();
extern void isr23();
extern void isr24();
extern void isr25();
extern void isr26();
extern void isr27();
extern void isr28();
extern void isr29();
extern void isr30();
extern void isr31();
extern void irq0();
extern void irq1();
extern void irq2();
extern void irq3();
extern void irq4();
extern void irq5();
extern void irq6();
extern void irq7();
extern void irq8();
extern void irq9();
extern void irq10();
extern void irq11();
extern void irq12();
extern void irq13();
extern void irq14();
extern void irq15();
extern void isr48();

int kernelDead = 0;

int getUptime()						// <glidix/time.h>
{
	return uptime;
};

static void setGate(int index, void *isr)
{
	uint64_t offset = (uint64_t) isr;
	idt[index].offsetLow = offset & 0xFFFF;
	idt[index].codeSegment = 8;
	idt[index].reservedIst = 0;
	idt[index].flags = 0x8E;		// present, DPL=0, type=interrupt gate
	idt[index].offsetMiddle = (offset >> 16) & 0xFFFF;
	idt[index].offsetHigh = (offset >> 32) & 0xFFFFFFFF;
	idt[index].reserved = 0;
};

void initIDT()
{
	// remap the IRQs
#if 0
	outb(0x20, 0x11);
	outb(0xA0, 0x11);
	outb(0x21, 0x20);
	outb(0xA1, 0x28);
	outb(0x21, 0x04);
	outb(0xA1, 0x02);
	outb(0x21, 0x01);
	outb(0xA1, 0x01);
	outb(0x21, 0x0);
	outb(0xA1, 0x0);
#endif

	outb(0xA1, 0xFF);
	outb(0x21, 0xFF);

	memset(idt, 0, 256*sizeof(IDTEntry));
	setGate(0, isr0);
	setGate(1, isr1);
	setGate(2, isr2);
	setGate(3, isr3);
	setGate(4, isr4);
	setGate(5, isr5);
	setGate(6, isr6);
	setGate(7, isr7);
	setGate(8, isr8);
	setGate(9, isr9);
	setGate(10, isr10);
	setGate(11, isr11);
	setGate(12, isr12);
	setGate(13, isr13);
	setGate(14, isr14);
	setGate(15, isr15);
	setGate(16, isr16);
	setGate(17, isr17);
	setGate(18, isr18);
	setGate(19, isr19);
	setGate(20, isr20);
	setGate(21, isr21);
	setGate(22, isr22);
	setGate(23, isr23);
	setGate(24, isr24);
	setGate(25, isr25);
	setGate(26, isr26);
	setGate(27, isr27);
	setGate(28, isr28);
	setGate(29, isr29);
	setGate(30, isr30);
	setGate(31, isr31);
	setGate(32, irq0);
	setGate(33, irq1);
	setGate(34, irq2);
	setGate(35, irq3);
	setGate(36, irq4);
	setGate(37, irq5);
	setGate(38, irq6);
	setGate(39, irq7);
	setGate(40, irq8);
	setGate(41, irq9);
	setGate(42, irq10);
	setGate(43, irq11);
	setGate(44, irq12);
	setGate(45, irq13);
	setGate(46, irq14);
	setGate(47, irq15);
	setGate(48, isr48);

	idtPtr.addr = (uint64_t) &idt[0];
	idtPtr.limit = (sizeof(IDTEntry) * 256) - 1;
	loadIDT();

	memset(irqHandlers, 0, sizeof(IRQHandler)*16);
	uptime = 0;
};

void idtReboot()
{
	ASM("cli");
	idtPtr.addr = 0;
	idtPtr.limit = 0;
	loadIDT();
	ASM("int $0x70");
	while (1) ASM("cli; hlt");
};

static void printbyte(uint8_t byte)
{
	uint8_t high = (byte >> 4) & 0xF;
	uint8_t low =  byte & 0xF;

	const char *hexd = "0123456789ABCDEF";
	kprintf("%c%c ", hexd[high], hexd[low]);
};

static void onPageFault(Regs *regs)
{
	//kprintf_debug("pf\n");
	uint64_t faultAddr;
	ASM ("mov %%cr2, %%rax" : "=a" (faultAddr));

	if (getCurrentThread() != NULL)
	{
		if (regs->errCode & 2)
		{
			// caused by a write
			if (tryCopyOnWrite(faultAddr) == 0)
			{
				return;
			};
		};

		if (tryLoadOnDemand(faultAddr) == 0)
		{
			return;
		};
	};

	if ((getCurrentThread() == NULL) || (regs->cs == 8))
	//if (1)
	{
		ASM("cli");
		kernelDead = 1;
		//heapDump();
		kdumpregs(regs);
		kprintf("A page fault occured (rip=%a)\n", regs->rip);
		if ((regs->errCode & 1) == 0)
		{
			kprintf("[non-present]");
		};

		if (regs->errCode & 2)
		{
			kprintf("[write]");
		}
		else
		{
			kprintf("[read]");
		};

		if (regs->errCode & 4)
		{
			kprintf("[user]");
		}
		else
		{
			kprintf("[kernel]");
		};

		if (regs->errCode & 8)
		{
			kprintf("[reserved]");
		};

		if (regs->errCode & 16)
		{
			kprintf("[fetch]");
		};

		kprintf("\nVirtual address: %a\n", faultAddr);
#if 0
		uint32_t wait = 0xFFFFFFFF;
		while (wait--);
		kprintf("Peek at RIP: ");
		uint8_t *peek = (uint8_t*) regs->rip;
		size_t sz = 16;
		while (sz--)
		{
			printbyte(*peek++);
		};
		kprintf("\n");
#endif
		debugKernel(regs);
	}
	else
	{
		Thread *thread = getCurrentThread();

		siginfo_t siginfo;
		siginfo.si_signo = SIGSEGV;
		if ((regs->errCode & 1) == 0)
		{
			siginfo.si_code = SEGV_MAPERR;
		}
		else
		{
			siginfo.si_code = SEGV_ACCERR;
		};
		siginfo.si_addr = (void*) faultAddr;

		ASM("cli");
		sendSignal(thread, &siginfo);
		switchTask(regs);
	};
};

static void onGPF(Regs *regs)
{
	if (1)
	{
		ASM("cli");
		kernelDead = 1;
		//heapDump();
		kdumpregs(regs);
		kprintf("GPF (rip=%a)\n", regs->rip);
		kprintf("Peek at RIP: ");
		uint8_t *peek = (uint8_t*) regs->rip;
		size_t sz = 16;
		while (sz--)
		{
			printbyte(*peek++);
		};
		kprintf("\n");
		debugKernel(regs);
	};
};

void switchTask(Regs *regs);		// sched.c

typedef struct
{
	uint16_t			ud2;		// must be 0x0B0F
	uint16_t			num;		// syscall number
} PACKED SyscallOpcode;

void sendCPUErrorSignal(Regs *regs, int signal, int code, void *addr)
{
	Thread *thread = getCurrentThread();

	siginfo_t siginfo;
	siginfo.si_signo = signal;
	siginfo.si_code = code;
	siginfo.si_addr = addr;

	ASM("cli");
	sendSignal(thread, &siginfo);
	switchTask(regs);
};

void onInvalidOpcodeOrSyscall(Regs *regs)
{
	SyscallOpcode *syscall = (SyscallOpcode*) regs->rip;
	if (syscall->ud2 == 0x0B0F)
	{
		ASM("sti");
		syscallDispatch(regs, syscall->num);
	}
	else
	{
		panic("invalid opcode");
	};
};

void isrHandler(Regs *regs)
{
	//kprintf("INTNO: %d\n", regs->intNo);
	if ((regs->intNo == IRQ14) && (regs->intNo == IRQ15))
	{
		panic("INT: %d\n", regs->intNo);
	};
	
	if (kernelDead)
	{
		panic("interrupt %d after kernel died.", regs->intNo);
	};

	// ignore spurious IRQs
	if ((regs->intNo == IRQ7) || (regs->intNo == IRQ15))
	{
		//apic->eoi = 0;
		return;
	};

	if ((regs->intNo >= IRQ0) && (regs->intNo <= I_APIC_TIMER))
	{
		// IRQ
		apic->eoi = 0;
	};

	switch (regs->intNo)
	{
	case IRQ0:
		__sync_fetch_and_add(&uptime, 1);
		//kprintf("UPTIME: %d\n", uptime);
		break;
	case I_DIV_ZERO:
		sendCPUErrorSignal(regs, SIGFPE, FPE_INTDIV, (void*) regs->rip);
		break;
	case I_UNDEF_OPCODE:
		onInvalidOpcodeOrSyscall(regs);
		break;
	case I_GPF:
		onGPF(regs);
		break;
	case I_PAGE_FAULT:
		onPageFault(regs);
		break;
	case I_SIMD_EX:
		sendCPUErrorSignal(regs, SIGFPE, FPE_FLTUND, (void*) regs->rip);
		break;
	case I_APIC_TIMER:
		if (apic->timerCurrentCount == 0)
		{
			switchTask(regs);
		};
		break;
	case 15:
		// sometimes generated for seemingly no reason... perhaps related to the PIT going off before
		// the interrupts are reprogremmed? i don't know
		break;
	default:
		if (regs->intNo >= IRQ0)
		{
			if (irqHandlers[regs->intNo-IRQ0] != NULL) irqHandlers[regs->intNo-IRQ0](regs->intNo-IRQ0);
			break;
		};
		heapDump();
		kdumpregs(regs);
		panic("Unhandled interrupt: %d\n", regs->intNo);
		break;
	};
};

IRQHandler registerIRQHandler(int irq, IRQHandler handler)
{
	IRQHandler old = irqHandlers[irq];
	irqHandlers[irq] = handler;
	return old;
};

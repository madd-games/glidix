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

#include <glidix/hw/idt.h>
#include <glidix/util/string.h>
#include <glidix/display/console.h>
#include <glidix/util/common.h>
#include <glidix/hw/port.h>
#include <glidix/int/syscall.h>
#include <glidix/thread/sched.h>
#include <glidix/thread/signal.h>
#include <glidix/util/memory.h>
#include <glidix/hw/apic.h>
#include <glidix/util/time.h>
#include <glidix/hw/pci.h>
#include <glidix/hw/cpu.h>
#include <glidix/util/catch.h>

IDTEntry idt[256];
IDTPointer idtPtr;
static IRQHandler irqHandlers[16];
static volatile uint64_t uptime;
extern Thread firstThread;

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
extern void isr49();
extern void isr50();
extern void isr51();
extern void isr52();
extern void isr53();
extern void isr54();
extern void isr55();
extern void isr56();
extern void isr57();
extern void isr58();
extern void isr59();
extern void isr60();
extern void isr61();
extern void isr62();
extern void isr63();
extern void isr64();
extern void isr65();
extern void isr112();
extern void isr113();
extern void irq_ditch();

int kernelDead = 0;

uint64_t getUptime()						// <glidix/time.h>
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

static void setGateIST(int index, int ist)
{
	idt[index].reservedIst = ist;
};

void initIDT()
{
	// remap PIC interrups to the 0x80-0x8F range, so that we can ignore
	// them.
	outb(0x20, 0x11);
	outb(0xA0, 0x11);
	outb(0x21, 0x80);
	outb(0xA1, 0x88);
	outb(0x21, 4);
	outb(0xA1, 2);
	outb(0x21, 1);
	outb(0xA1, 1);
	
	// disable the legacy PIC
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
	setGate(49, isr49);
	setGate(50, isr50);
	setGate(51, isr51);
	setGate(52, isr52);
	setGate(53, isr53);
	setGate(54, isr54);
	setGate(55, isr55);
	setGate(56, isr56);
	setGate(57, isr57);
	setGate(58, isr58);
	setGate(59, isr59);
	setGate(60, isr60);
	setGate(61, isr61);
	setGate(62, isr62);
	setGate(63, isr63);
	setGate(64, isr64);
	setGate(65, isr65);
	setGate(0x70, isr112);
	setGate(0x71, isr113);
	
	// set up IST for some
	setGateIST(I_NMI, 1);
	setGateIST(I_DOUBLE, 1);
	
	// PIC IRQs to be ignored
	int i;
	for (i=0x80; i<0x90; i++)
	{
		setGate(i, irq_ditch);
	};
	
	idtPtr.addr = (uint64_t) &idt[0];
	idtPtr.limit = (sizeof(IDTEntry) * 256) - 1;
	loadIDT();

	memset(irqHandlers, 0, sizeof(IRQHandler)*16);
	uptime = 0;
};

void idtReboot()
{
	cli();
	idtPtr.addr = 0;
	idtPtr.limit = 0;
	loadIDT();
	ASM("int $0x70");
	while (1) ASM("cli; hlt");
};

static void onPageFault(Regs *regs)
{
	uint64_t faultAddr;
	ASM ("mov %%cr2, %%rax" : "=a" (faultAddr));

	Thread *currentThread = getCurrentThread();
	
	if (regs->rflags & (1 << 9)) sti();
	if (currentThread != NULL)
	{
		if (regs->rip == TRAP_SIGRET)
		{
			char buffer[1024];
			uint64_t frameAddr = regs->rsp - 8;
			if ((frameAddr & 0xF) != 0)
			{
				faultAddr = frameAddr;
			}
			else if (memcpy_u2k(buffer, (void*)frameAddr, sizeSignalStackFrame) == 0)
			{
				sigret(buffer);
			}
			else
			{
				faultAddr = frameAddr;
			};
		};
		
		if (faultAddr < ADDR_MAX)
		{
			ProcMem *pm = currentThread->pm;
			if (pm != NULL)
			{
				vmFault(regs, faultAddr, regs->errCode);
				return;
			};
		};
	};

	if ((currentThread == NULL) || ((regs->cs & 0xFFFF) == 8))
	{
		if (currentThread != NULL)
		{
			throw(EX_PAGE_FAULT);
		};

		const char *name = currentThread != NULL ? currentThread->name : "<early-boot>";
		
		enableDebugTerm();
		cli();
		kernelDead = 1;
		//heapDump();
		kdumpregs(regs);
		kprintf("A page fault occured (rip=0x%016lX, thread='%s')\n", regs->rip, name);
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

		kprintf("\nVirtual address: 0x%016lX\n", faultAddr);
		debugKernel(regs);
	}
	else
	{
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
		
		cli();
		sendSignal(currentThread, &siginfo);
		switchTask(regs);
	};
};

void sendCPUErrorSignal(Regs *regs, int signal, int code, void *addr)
{
	Thread *thread = getCurrentThread();

	siginfo_t siginfo;
	memset(&siginfo, 0, sizeof(siginfo_t));
	siginfo.si_signo = signal;
	siginfo.si_code = code;
	siginfo.si_addr = addr;

	cli();
	sendSignal(thread, &siginfo);
	switchTask(regs);
};

static void onGPF(Regs *regs)
{
	throw(EX_GPF);
	
	if ((regs->cs & 3) == 0)
	{
		enableDebugTerm();
		cli();
		haltAllCPU();
		kernelDead = 1;
		kdumpregs(regs);
		kprintf("GPF (rip=0x%016lX)\n", regs->rip);
		stackTrace(regs->rip, regs->rbp);
		debugKernel(regs);
	}
	else
	{
		sendCPUErrorSignal(regs, SIGILL, ILL_ILLOPN, (void*) regs->rip);
	};
};

extern int panicking;	/* panic.c */
extern char initVideoRAM[];
void isrHandler(Regs *regs)
{
	static int gotNMI = 0;
	
	// ignore spurious IRQs
	Thread *thread;
	if (regs->intNo == IRQ7)
	{
		apic->eoi = 0;
		return;
	};

	if ((regs->intNo >= IRQ0) && (regs->intNo <= I_APIC_TIMER))
	{
		// IRQ
		if ((regs->intNo < I_PCI0) || (regs->intNo > I_PCI15))
		{
			// only EOI non-PCI interrupts (pciInterrupt() will send EOI for everything else)
			apic->eoi = 0;
		};
	};

	switch (regs->intNo)
	{
	case IRQ0:
		__sync_fetch_and_add(&uptime, 1);
		cli();
		onTick();
		break;
	case I_DIV_ZERO:
		sendCPUErrorSignal(regs, SIGFPE, FPE_INTDIV, (void*) regs->rip);
		break;
	case I_DOUBLE:
		panic("double fault");
		break;
	case I_NMI:
		kernelDead = 1;
		if (__sync_fetch_and_add(&gotNMI, 1) != 0)
		{
			while (1)
			{
				cli();
				hlt();
			};
		};
		
		if (panicking)
		{
			while (1)
			{
				cli();
				hlt();
			};
		};
		
		enableDebugTerm();
		haltAllCPU();
		dumpRunqueue();
		stackTraceHere();
		
		thread = &firstThread;
		do
		{
			kprintf("=== THREAD '%s' (THID=%d) ===\n", thread->name, thread->thid);
			kdumpregs(&thread->regs);
			stackTrace(thread->regs.rip, thread->regs.rbp);
			thread = thread->next;
			
			uint32_t count = 0xF000F000;
			while (count--);
		} while (thread != &firstThread);
		
		panic("NMI");
		
		break;
	case I_UNDEF_OPCODE:
		sendCPUErrorSignal(regs, SIGILL, ILL_ILLOPC, (void*) regs->rip);
		break;
	case I_GPF:
		onGPF(regs);
		break;
	case I_PAGE_FAULT:
		onPageFault(regs);
		break;
	case I_FLOAT_EX:
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
	case IRQ_IDE:
		break;
	case I_PCI0:
	case I_PCI1:
	case I_PCI2:
	case I_PCI3:
	case I_PCI4:
	case I_PCI5:
	case I_PCI6:
	case I_PCI7:
	case I_PCI8:
	case I_PCI9:
	case I_PCI10:
	case I_PCI11:
	case I_PCI12:
	case I_PCI13:
	case I_PCI14:
	case I_PCI15:
		pciInterrupt(regs->intNo);
		switchTask(regs);
		break;
	case I_IPI_HALT:
		while (1) {cli(); hlt();};
		break;
	case I_IPI_SCHED_HINT:
		// scheduler hint. this basically wakes up a CPU to indicate that the state of
		// one of its threads has changed and it might need to stop halting. we do nothing
		// in response.
		apic->eoi = 0;
		break;
	default:
		if ((regs->intNo >= IRQ0) && (regs->intNo <= IRQ15))
		{
			if (irqHandlers[regs->intNo-IRQ0] != NULL) irqHandlers[regs->intNo-IRQ0](regs->intNo-IRQ0);
			break;
		};
		//heapDump();
		kdumpregs(regs);
		panic("unhandled interrupt: %d\n", regs->intNo);
		break;
	};
};

IRQHandler registerIRQHandler(int irq, IRQHandler handler)
{
	if ((irq < 0) || (irq >= 16))
	{
		stackTraceHere();
		panic("invalid IRQ number (%d)! must be 0-15.\n", irq);
	};
	
	IRQHandler old = irqHandlers[irq];
	irqHandlers[irq] = handler;
	return old;
};

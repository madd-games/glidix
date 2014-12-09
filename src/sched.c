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

#include <glidix/sched.h>
#include <glidix/memory.h>
#include <glidix/string.h>
#include <glidix/console.h>
#include <glidix/spinlock.h>

static Thread firstThread;
static Thread *currentThread;
static Spinlock schedLock;		// for PID and stuff
static int nextPid;

extern uint64_t getFlagsRegister();
void kmain2();

void dumpRunqueue()
{
	Thread *th = &firstThread;
	do
	{
		kprintf("%s (prev=%s, next=%s)\n", th->name, th->prev->name, th->next->name);
		th = th->next;
	} while (th != &firstThread);
};

static void startupThread()
{
	kprintf("%$\x02" "Done%#\n");
	kmain2();

	while (1);		// never return! the stack below us is invalid.
};

void initSched()
{
	nextPid = 1;

	// create a new stack for this initial process
	firstThread.stack = kmalloc(0x4000);
	firstThread.stackSize = 0x4000;

	// the value of registers do not matter except RSP and RIP,
	// also the startup function should never return.
	memset(&firstThread.regs, 0, sizeof(Regs));
	firstThread.regs.rip = (uint64_t) &startupThread;
	firstThread.regs.rsp = (uint64_t) firstThread.stack + firstThread.stackSize;
	firstThread.regs.cs = 8;
	firstThread.regs.ds = 16;
	firstThread.regs.ss = 0;
	firstThread.regs.rflags = getFlagsRegister() | (1 << 9);		// enable interrupts

	// other stuff
	firstThread.name = "Startup thread";
	firstThread.flags = 0;
	firstThread.pm = NULL;
	firstThread.pid = 0;
	firstThread.ftab = NULL;

	// linking
	firstThread.prev = &firstThread;
	firstThread.next = &firstThread;

	// switch to this new thread's context
	currentThread = &firstThread;
	switchContext(&firstThread.regs);
};

void switchTask(Regs *regs)
{
	// remember the context of this thread.
	memcpy(&currentThread->regs, regs, sizeof(Regs));

	// get the next thread
	do
	{
		currentThread = currentThread->next;
	} while (currentThread->flags & THREAD_WAITING);

	// switch address space
	if (currentThread->pm != NULL) SetProcessMemory(currentThread->pm);

	// switch context
	switchContext(&currentThread->regs);
};

static void kernelThreadExit()
{
	// just to be safe
	if (currentThread == &firstThread)
	{
		panic("kernel startup thread tried to exit (this is a bug)");
	};

	// we need to do all of this with interrupts disabled, else if this gets interrupted
	// half way though, we might get a memory leak due to something not being kfree()'d.
	acquireHeap();
	ASM("cli");
	releaseHeap();
	currentThread->prev->next = currentThread->next;
	currentThread->next->prev = currentThread->prev;

	kfree(currentThread->stack);
	kfree(currentThread);

	// enable interrupt then halt, effectively waiting for a context switch that will
	// never return to this thread.
	ASM("sti; hlt");
};

void CreateKernelThread(KernelThreadEntry entry, KernelThreadParams *params, void *data)
{
	// params
	uint64_t stackSize = 0x4000;
	if (params != NULL)
	{
		stackSize = params->stackSize;
	};
	const char *name = "Nameless thread";
	if (params != NULL)
	{
		name = params->name;
	};

	// allocate and fill in the thread structure
	Thread *thread = (Thread*) kmalloc(sizeof(Thread));
	thread->stack = kmalloc(stackSize);
	thread->stackSize = stackSize;

	memset(&thread->regs, 0, sizeof(Regs));
	thread->regs.rip = (uint64_t) entry;
	thread->regs.rsp = (uint64_t) thread->stack + thread->stackSize - 8;		// -8 because we'll push the return address...
	thread->regs.cs = 8;
	thread->regs.ds = 16;
	thread->regs.ss = 0;
	thread->regs.rflags = getFlagsRegister() | (1 << 9);				// enable interrupts in that thread
	thread->name = name;
	thread->flags = 0;
	thread->pm = NULL;
	thread->pid = 0;
	thread->ftab = NULL;

	// this will simulate a call from kernelThreadExit() to "entry()"
	// this is so that when entry() returns, the thread can safely exit.
	thread->regs.rdi = (uint64_t) data;
	*((uint64_t*)thread->regs.rsp) = (uint64_t) &kernelThreadExit;

	// link into the runqueue (disable interrupts for the duration of this).
	ASM("cli");
	currentThread->next->prev = thread;
	thread->next = currentThread->next;
	thread->prev = currentThread;
	currentThread->next = thread;
	// there is no need to update currentThread->prev, it will only be broken for the init
	// thread, which never exits, and therefore its prev will never need to be valid.
	ASM("sti");
};

Thread *getCurrentThread()
{
	return currentThread;
};

void waitThread(Thread *thread)
{
	ASM("cli");
	thread->flags |= THREAD_WAITING;
	ASM("sti");
};

void signalThread(Thread *thread)
{
	ASM("cli");
	thread->flags &= ~THREAD_WAITING;
	ASM("sti");
};

int threadClone(Regs *regs, int flags, MachineState *state)
{
	Thread *thread = (Thread*) kmalloc(sizeof(Thread));
	memcpy(&thread->regs, regs, sizeof(Regs));
	thread->regs.rax = 0;

	if (state != NULL)
	{
		thread->regs.rdi = state->rdi;
		thread->regs.rsi = state->rsi;
		thread->regs.rbp = state->rbp;
		thread->regs.rbx = state->rbx;
		thread->regs.rdx = state->rdx;
		thread->regs.rcx = state->rcx;
		thread->regs.rax = state->rax;
		thread->regs.r8  = state->r8 ;
		thread->regs.r9  = state->r9 ;
		thread->regs.r10 = state->r10;
		thread->regs.r11 = state->r11;
		thread->regs.r12 = state->r12;
		thread->regs.r13 = state->r13;
		thread->regs.r14 = state->r14;
		thread->regs.r15 = state->r15;
		thread->regs.rip = state->rip;
		thread->regs.rsp = state->rsp;
	};

	// no kernel stack really.
	thread->stack = NULL;
	thread->stackSize = 0;

	thread->name = "";
	thread->flags = 0;

	// process memory
	if (flags & CLONE_SHARE_MEMORY)
	{
		UprefProcessMemory(currentThread->pm);
		thread->pm = currentThread->pm;
	}
	else
	{
		if (currentThread->pm != NULL)
		{
			thread->pm = DuplicateProcessMemory(currentThread->pm);
		}
		else
		{
			thread->pm = CreateProcessMemory();
		};
	};

	// assign pid
	spinlockAcquire(&schedLock);
	thread->pid = nextPid++;
	spinlockRelease(&schedLock);

	// file table
	if (flags & CLONE_SHARE_FTAB)
	{
		ftabUpref(currentThread->ftab);
		thread->ftab = currentThread->ftab;
	}
	else
	{
		if (currentThread->ftab != NULL)
		{
			thread->ftab = ftabDup(currentThread->ftab);
		}
		else
		{
			thread->ftab = ftabCreate();
		};
	};

	// link into the runqueue (disable interrupts for the duration of this).
	ASM("cli");
	currentThread->next->prev = thread;
	thread->next = currentThread->next;
	thread->prev = currentThread;
	currentThread->next = thread;
	ASM("sti");

	return 0;
};

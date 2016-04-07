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

#include <glidix/signal.h>
#include <glidix/sched.h>
#include <glidix/common.h>
#include <glidix/string.h>
#include <glidix/memory.h>
#include <glidix/console.h>
#include <glidix/procmem.h>
#include <glidix/syscall.h>

/**
 * OR of all flags that userspace should be allowed to set.
 */
#define	OK_USER_FLAGS (\
	(1 << 0)	/* carry flag */ \
	| (1 << 2)	/* parity flag */ \
	| (1 << 4)	/* adjust flag */ \
	| (1 << 6)	/* zero flag */ \
	| (1 << 7)	/* sign flag */ \
	| (1 << 10)	/* direction flag */ \
	| (1 << 11)	/* overflow flag */ \
)

typedef struct
{
	uint64_t			trapSigRet;		// TRAP_SIGRET
	uint64_t			sigmask;
	MachineState			mstate;			// must be aligned on 16 byte boundary
	siginfo_t			si;
} PACKED SignalStackFrame;

uint64_t sizeSignalStackFrame = sizeof(SignalStackFrame);

void jumpToHandler(SignalStackFrame *frame, uint64_t handler)
{
	uint64_t addr = (frame->mstate.rsp - sizeof(SignalStackFrame) - 128) & ~((uint64_t)0xF);
	memcpy((void*)addr, frame, sizeof(SignalStackFrame));
	
	SignalStackFrame *uframe = (SignalStackFrame*) addr;
	
	Regs regs;
	initUserRegs(&regs);
	regs.rip = handler;
	regs.rsp = addr;
	*((int*)&regs.rdi) = frame->si.si_signo;
	regs.rsi = (uint64_t) (&uframe->si);
	regs.rdx = 0;
	switchContext(&regs);
};

int isDeathSig(int signo)
{
	switch (signo)
	{
	case SIGHUP:
	case SIGINT:
	case SIGQUIT:
	case SIGILL:
	case SIGABRT:
	case SIGFPE:
	case SIGSEGV:
	case SIGPIPE:
	case SIGALRM:
	case SIGTERM:
	case SIGUSR1:
	case SIGUSR2:
	case SIGSYS:
		return 1;
	default:
		return 0;
	};
};

void dispatchSignal(Thread *thread)
{
	int i;
	siginfo_t *siginfo = NULL;
	for (i=0; i<64; i++)
	{
		uint64_t mask = (1UL << i);
		if (thread->pendingSet & mask)
		{
			uint64_t sigbit = (1UL << thread->pendingSigs[i].si_signo);
			if ((thread->sigmask & sigbit) == 0)
			{
				// not blocked
				siginfo = &thread->pendingSigs[i];
				thread->pendingSet &= ~mask;
				break;
			};
		};
	};
	
	if (siginfo == NULL)
	{
		panic("dispatchSignal called when no signals are waiting");
	};

	if (siginfo->si_signo == SIGKILL)
	{
		// not allowed to override SIGKILL
		Regs regs;				// doesn't matter, it will die
		cli();
		unlockSched();				// threadExit() calls lockSched()
		threadExit(thread, -SIGKILL);
		switchTask(&regs);
	};
	
	// what action do we take?
	SigAction *action = &thread->sigdisp->actions[siginfo->si_signo];
	uint64_t handler;
	if (action->sa_flags & SA_SIGINFO)
	{
		handler = (uint64_t) action->sa_sigaction;
	}
	else if (action->sa_handler == SIG_IGN)
	{
		return;
	}
	else if (action->sa_handler == SIG_DFL)
	{
		if (isDeathSig(siginfo->si_signo))
		{
			Regs regs;				// doesn't matter, it will die
			cli();
			unlockSched();				// threadExit() calls lockSched()
			threadExit(thread, -siginfo->si_signo);
			switchTask(&regs);
		}
		else
		{
			return;
		};
	}
	else
	{
		handler = (uint64_t) action->sa_handler;
	};

	// try pushing the signal stack frame. also, don't break the red zone!
	uint64_t addr = (thread->regs.rsp - sizeof(SignalStackFrame) - 128) & (~((uint64_t)0xF));
	SetProcessMemory(thread->pm);
	if (!isPointerValid(addr, sizeof(SignalStackFrame), PROT_READ | PROT_WRITE))
	{
		// signal stack broken, kill the process with SIGABRT.
		Regs regs;
		unlockSched();
		threadExit(thread, -SIGABRT);
		switchTask(&regs);
	};
	
	// basically, the scheduler which calls us is not reentant, so we cannot copy
	// the frame directly onto the user stack as that may cause a page fault (which
	// is an interrupt), which will lead to absolute disaster. signals are only
	// dispatched if the current code segment of the thread is in user mode, and so
	// the kernel stack for that thread is empty, so we push the signal frame onto
	// the kernel stack and transfer control to jumpToHandler, on that stack, which
	// will then run within the context of the thread, and copy the frame onto the
	// user stack
	uint64_t frameAddr = (uint64_t) thread->stack;
	SignalStackFrame *frame = (SignalStackFrame*) frameAddr;
	frame->trapSigRet = TRAP_SIGRET;
	frame->sigmask = thread->sigmask;
	memcpy(&frame->mstate.fpuRegs, &thread->fpuRegs, sizeof(FPURegs));
	frame->mstate.rdi = thread->regs.rdi;
	frame->mstate.rsi = thread->regs.rsi;
	frame->mstate.rbp = thread->regs.rbp;
	frame->mstate.rax = thread->regs.rax;
	frame->mstate.rbx = thread->regs.rbx;
	frame->mstate.rcx = thread->regs.rcx;
	frame->mstate.rdx = thread->regs.rdx;
	frame->mstate.r8  = thread->regs.r8;
	frame->mstate.r9  = thread->regs.r9;
	frame->mstate.r10 = thread->regs.r10;
	frame->mstate.r11 = thread->regs.r11;
	frame->mstate.r12 = thread->regs.r12;
	frame->mstate.r13 = thread->regs.r13;
	frame->mstate.r14 = thread->regs.r14;
	frame->mstate.r15 = thread->regs.r15;
	frame->mstate.rip = thread->regs.rip;
	frame->mstate.rsp = thread->regs.rsp;
	frame->mstate.rflags = thread->regs.rflags;
	memcpy(&frame->si, siginfo, sizeof(siginfo_t));

	thread->regs.rsp = ((uint64_t) thread->stack + (uint64_t) thread->stackSize) & ~((uint64_t)0xF);
	thread->regs.rdi = frameAddr;
	thread->regs.rsi = handler;
	thread->regs.rip = (uint64_t) &jumpToHandler;
	
	// switch to kernel mode
	thread->regs.cs = 0x08;
	thread->regs.ds = 0x10;
	thread->regs.ss = 0;
	
	thread->sigmask |= action->sa_mask;
};

void sendSignal(Thread *thread, siginfo_t *siginfo)
{
	if (thread->flags & THREAD_TERMINATED)
	{
		return;
	};

	if (siginfo->si_signo == SIGCONT)
	{
		// wake up the thread but do not dispatch any signal
		//thread->flags &= ~THREAD_WAITING;
		signalThread(thread);
		return;
	};
	
	if (thread->sigdisp->actions[siginfo->si_signo].sa_handler == SIG_IGN)
	{
		// the thread does not care
		// we don't need to dispatch the signal, but we must wake up the thread
		// because the signal might be a notification that something must be looked
		// at by the thread; e.g. it could be the SIGCHLD signal indicating that the
		// thread might need to wait() for the child.
		signalThread(thread);
		return;
	};
	
	if (thread->pendingSet == 0xFFFFFFFFFFFFFFFF)
	{
		// no place to put the signal in; discard
		signalThread(thread);
		return;
	};
	
	int i;
	for (i=0; i<64; i++)
	{
		uint64_t mask = (1UL << i);
		if ((thread->pendingSet & mask) == 0)
		{
			thread->pendingSet |= mask;
			break;
		};
	};
	
	if (i == 64)
	{
		// what the fuck? this is not possible
		panic("this condition is not logically possible; the universe might be ending");
	};

	memcpy(&thread->pendingSigs[i], siginfo, sizeof(siginfo_t));
	
	signalThread(thread);
};

void sigret(void *ret)
{
	Regs regs;
	initUserRegs(&regs);
	SignalStackFrame *frame = (SignalStackFrame*) ret;
	regs.rax = frame->mstate.rax;
	regs.rbx = frame->mstate.rbx;
	regs.rcx = frame->mstate.rcx;
	regs.rdx = frame->mstate.rdx;
	regs.rbp = frame->mstate.rbp;
	regs.rsp = frame->mstate.rsp;
	regs.rip = frame->mstate.rip;
	regs.rdi = frame->mstate.rdi;
	regs.rsi = frame->mstate.rsi;
	regs.r8 = frame->mstate.r8;
	regs.r9 = frame->mstate.r9;
	regs.r10 = frame->mstate.r10;
	regs.r11 = frame->mstate.r11;
	regs.r12 = frame->mstate.r12;
	regs.r13 = frame->mstate.r13;
	regs.r14 = frame->mstate.r14;
	regs.r15 = frame->mstate.r15;
	regs.rflags = (frame->mstate.rflags & OK_USER_FLAGS) | (getFlagsRegister() & ~OK_USER_FLAGS) | (1 << 9);
	fpuLoad(&frame->mstate.fpuRegs);
	getCurrentThread()->sigmask = frame->sigmask;

	cli();
	switchContext(&regs);
};

SigDisp* sigdispCreate()
{
	SigDisp *sd = NEW(SigDisp);
	sd->refcount = 1;
	
	int i;
	for (i=0; i<SIG_NUM; i++)
	{
		sd->actions[i].sa_handler = SIG_DFL;
		sd->actions[i].sa_mask = 0;
		sd->actions[i].sa_flags = 0;
		sd->actions[i].sa_sigaction = NULL;
	};
	
	return sd;
};

void sigdispUpref(SigDisp *sd)
{
	__sync_fetch_and_add(&sd->refcount, 1);
};

void sigdispDownref(SigDisp *sd)
{
	if (__sync_fetch_and_add(&sd->refcount, -1) == 1)
	{
		kfree(sd);
	};
};

SigDisp *sigdispExec(SigDisp *old)
{
	SigDisp *new = sigdispCreate();
	
	int i;
	for (i=0; i<SIG_NUM; i++)
	{
		if (old->actions[i].sa_handler == SIG_IGN)
		{
			new->actions[i].sa_handler = SIG_IGN;
		};
	};
	
	sigdispDownref(old);
	return new;
};

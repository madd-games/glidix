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

#include <glidix/signal.h>
#include <glidix/sched.h>
#include <glidix/common.h>
#include <glidix/string.h>
#include <glidix/memory.h>
#include <glidix/console.h>
#include <glidix/procmem.h>
#include <glidix/syscall.h>
#include <glidix/msr.h>
#include <glidix/trace.h>

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
	siginfo_t			si;
	MachineState			mstate;			// must be aligned on 16 byte boundary
} PACKED SignalStackFrame;

uint64_t sizeSignalStackFrame = sizeof(SignalStackFrame);

extern char usup_sigret;
extern char usup_start;

void jumpToHandler(SignalStackFrame *frame, uint64_t handler)
{
	// push GPRs onto the stack first; here alignment does not matter but relation to original
	// RSP does.
	uint64_t addrGPR = frame->mstate.rsp - (sizeof(MachineState) - 512) - 128;
	if (memcpy_k2u((void*)addrGPR, &frame->mstate.rflags, sizeof(MachineState)-512) != 0)
	{
		processExit(-SIGABRT);
	};
	
	// push the siginfo_t
	uint64_t addrInfo = addrGPR - sizeof(siginfo_t);
	if (memcpy_k2u((void*)addrInfo, &frame->si, sizeof(siginfo_t)) != 0)
	{
		processExit(-SIGABRT);
	};
	
	// now push the FPU registers; this must be 16-byte aligned.
	uint64_t addrFPU = (addrInfo - 512) & ~0xF;
	if (memcpy_k2u((void*)addrFPU, &frame->mstate, 512) != 0)
	{
		processExit(-SIGABRT);
	};
	
	// push the return address
	uint64_t addrRet = addrFPU - 8;
	uint64_t sigretRIP = (uint64_t)(&usup_sigret) - (uint64_t)(&usup_start) + 0xFFFF808000003000UL;
	if (memcpy_k2u((void*)addrRet, &sigretRIP, 8) != 0)
	{
		processExit(-SIGABRT);
	};
	
	Regs regs;
	initUserRegs(&regs);
	regs.rip = handler;
	regs.rsp = addrRet;
	*((int*)&regs.rdi) = frame->si.si_signo;
	regs.rsi = addrInfo;
	regs.rdx = 0;
	regs.rbx = addrFPU;
	regs.r12 = addrGPR;
	regs.r13 = frame->sigmask;
	regs.fsbase = msrRead(MSR_FS_BASE);
	regs.gsbase = msrRead(MSR_GS_BASE);
	switchContext(&regs);
};

static int isDeathSig(int signo)
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
	case SIGBUS:
	case SIGSYS:
		return 1;
	default:
		return 0;
	};
};

int isUnblockableSig(int signo)
{
	switch (signo)
	{
	case SIGKILL:
	case SIGTHKILL:
	case SIGSTOP:
		return 1;
	default:
		return 0;
	};
};

void dispatchSignal()
{
	Thread *thread = getCurrentThread();
	
	int i;
	siginfo_t *siginfo = NULL;
	for (i=0; i<64; i++)
	{
		uint64_t mask = (1UL << i);
		if (thread->pendingSet & mask)
		{
			uint64_t sigbit = (1UL << thread->pendingSigs[i].si_signo);
			if (((thread->sigmask & sigbit) == 0) || isUnblockableSig(thread->pendingSigs[i].si_signo))
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

	if (siginfo->si_signo == SIGTHKILL)
	{
		// not allowed to override SIGTHKILL
		switchToKernelSpace(&thread->regs);
		thread->regs.rip = (uint64_t) &threadExit;
		thread->regs.rsp = ((uint64_t) thread->stack + (uint64_t) thread->stackSize) & ~((uint64_t)0xF);
		return;
	};
	
	if (siginfo->si_signo == SIGKILL)
	{
		// not allowed to override SIGKILL
		switchToKernelSpace(&thread->regs);
		thread->regs.rip = (uint64_t) &processExit;
		thread->regs.rdi = 0;
		*((int64_t*)&thread->regs.rdi) = -SIGKILL;
		thread->regs.rsp = ((uint64_t) thread->stack + (uint64_t) thread->stackSize) & ~((uint64_t)0xF);
		return;
	};
	
	if (getCurrentThread()->flags & DBG_SIGNALS)
	{
		traceTrapEx(&getCurrentThread()->regs, TR_SIGNAL, siginfo->si_signo);
	};

#if 0
	if ((siginfo->si_signo == SIGSEGV) && (thread->creds->pid == 1))
	{
		panic("SIGSEGV in init, address=%p, rip=%p", siginfo->si_addr, thread->regs.rip);
	};
#endif

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
			switchToKernelSpace(&thread->regs);
			thread->regs.rip = (uint64_t) &processExit;
			thread->regs.rdi = 0;
			*((int64_t*)&thread->regs.rdi) = -siginfo->si_signo;
			thread->regs.rsp = ((uint64_t) thread->stack + (uint64_t) thread->stackSize) & ~((uint64_t)0xF);
		};
		
		return;
	}
	else
	{
		handler = (uint64_t) action->sa_handler;
	};
	
	// try pushing the signal stack frame. also, don't break the red zone!
	vmSwitch(thread->pm);
	
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
	frame->trapSigRet = (uint64_t)(&usup_sigret) - (uint64_t)(&usup_start) + 0xFFFF808000003000UL;
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
	/* do not preserve fsbase and gsbase; those are expected not to change since they should
	   be per-thread and not modified by signal handlers. */
	frame->mstate.fsbase = frame->mstate.gsbase = 0;
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

int sendSignalEx(Thread *thread, siginfo_t *siginfo, int flags)
{
	if (siginfo->si_signo >= SIG_NUM)
	{
		stackTraceHere();
		panic("invalid signal number passed to sendSignal(): %d", siginfo->si_signo);
	};
	
	if (thread->flags & THREAD_TERMINATED)
	{
		return -1;
	};

	if (siginfo->si_signo == SIGCONT)
	{
		// XXX: what is even going on here? why is it waking up an entire process?
		//      is there any reason for this to be specially handled? if so, we must
		//      get rid of that reason and ensure that this signal is treated normally
		//      (except it wakes up from a SIGSTOP).
		// XXX: it appears that the reason is that it was used for waking in the bad
		//      old synchronisation API or something.
		
		// wake up the thread but do not dispatch any signal
		// also we return -1 so that signalPid() wakes up all
		// threads in the process.
		signalThread(thread);
		return -1;
	};
	
	if (siginfo->si_signo == SIGTHWAKE)
	{
		// wake up the thread but do not dispatch any signal
		signalThread(thread);
		return 0;
	};
	
	// only allow the signal to be ignored if it's not SIGKILL, SIGTHKILL or SIGSTOP
	if ((siginfo->si_signo != SIGKILL) && (siginfo->si_signo != SIGTHKILL) && (siginfo->si_signo != SIGSTOP))
	{
		if (thread->sigdisp->actions[siginfo->si_signo].sa_handler == SIG_IGN)
		{
			// the thread does not care
			// we don't need to dispatch the signal, but we must wake up the thread
			// because the signal might be a notification that something must be looked
			// at by the thread; e.g. it could be the SIGCHLD signal indicating that the
			// thread might need to wait() for the child.
			signalThread(thread);
			return -1;
		};
		
		if (flags & SS_NONBLOCKED)
		{
			// drop the signal if it's blocked
			uint64_t sigbit = (1UL << siginfo->si_signo);
			if ((thread->sigmask & sigbit) != 0)
			{
				// blocked
				return -1;
			};
		};
	};
	
	if (thread->pendingSet == 0xFFFFFFFFFFFFFFFF)
	{
		// no place to put the signal in; discard
		signalThread(thread);
		return -1;
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
	return 0;
};

int sendSignal(Thread *thread, siginfo_t *siginfo)
{
	return sendSignalEx(thread, siginfo, 0);
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

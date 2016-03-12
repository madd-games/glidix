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

typedef struct
{
	MachineState			mstate;
	siginfo_t			si;
} PACKED SignalStackFrame;

uint64_t sizeSignalStackFrame = sizeof(SignalStackFrame);

void dispatchSignal(Thread *thread)
{
	if (thread->sigcnt == 0) return;

	siginfo_t *siginfo = &thread->sigq[thread->sigfetch];
	thread->sigfetch = (thread->sigfetch + 1) % SIGQ_SIZE;
	thread->sigcnt--;

	if (siginfo->si_signo == SIGKILL)
	{
		// not allowed to override SIGKILL
		Regs regs;				// doesn't matter, it will die
		cli();
		unlockSched();				// threadExit() calls lockSched()
		threadExit(thread, -SIGKILL);
		switchTask(&regs);
	};

	if (thread->rootSigHandler == 0)
	{
		// there is no sig handler; discard.
		return;
	};
	
	// try pushing the signal stack frame. also, don't break the red zone!
	uint64_t addr = (thread->regs.rsp - sizeof(SignalStackFrame) - 128) & (uint64_t)(~0xF);
	if (!isPointerValid(addr, sizeof(SignalStackFrame), PROT_READ | PROT_WRITE))
	{
		// signal stack broken, kill the process with SIGABRT.
		Regs regs;
		ASM("cli");
		unlockSched();
		threadExit(thread, -SIGABRT);
		switchTask(&regs);
	};

	SetProcessMemory(thread->pm);
	SignalStackFrame *frame = (SignalStackFrame*) addr;
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
	memcpy(&frame->si, siginfo, sizeof(siginfo_t));

	thread->regs.rsp = addr;
	thread->regs.rdi = addr;
	thread->regs.rsi = (uint64_t) (&frame->si);
	thread->regs.rip = thread->rootSigHandler;

	thread->flags &= ~THREAD_WAITING;
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
		thread->flags &= ~THREAD_WAITING;
		return;
	};
	
	if (thread->sigcnt == SIGQ_SIZE)
	{
		// drop the signal because the queue is full.
		return;
	};

	memcpy(&thread->sigq[thread->sigput], siginfo, sizeof(siginfo_t));
	thread->sigput = (thread->sigput + 1) % SIGQ_SIZE;
	thread->sigcnt++;

	thread->flags &= ~THREAD_WAITING;
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
	fpuLoad(&frame->mstate.fpuRegs);

	cli();
	switchContext(&regs);
};

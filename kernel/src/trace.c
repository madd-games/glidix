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

#include <glidix/trace.h>
#include <glidix/sched.h>
#include <glidix/syscall.h>
#include <glidix/string.h>

static int tcGetRegs(int thid, MachineState *state)
{
	cli();
	lockSched();
	
	Thread *ct = getCurrentThread();
	Thread *thread = ct;
	
	int result = -1;
	ERRNO = ESRCH;
	
	do
	{
		if (thread->thid == thid)
		{
			if (thread->creds == NULL)
			{
				// not our child
				ERRNO = ESRCH;
				break;
			}
			else if (thread->creds->ppid != ct->creds->pid)
			{
				// not our child
				ERRNO = ESRCH;
				break;
			}
			else if ((thread->flags & THREAD_TRACED) == 0)
			{
				// not traced
				ERRNO = EPERM;
				break;
			}
			else
			{
				result = 0;
				memcpy(&state->fpuRegs, &thread->fpuRegs, sizeof(FPURegs));
				state->rflags = thread->regs.rflags;
				state->rip = thread->regs.rip;
				state->rdi = thread->regs.rdi;
				state->rsi = thread->regs.rsi;
				state->rbp = thread->regs.rbp;
				state->rbx = thread->regs.rbx;
				state->rdx = thread->regs.rdx;
				state->rcx = thread->regs.rcx;
				state->rax = thread->regs.rax;
				state->r8 = thread->regs.r8;
				state->r9 = thread->regs.r9;
				state->r10 = thread->regs.r10;
				state->r11 = thread->regs.r11;
				state->r12 = thread->regs.r12;
				state->r13 = thread->regs.r13;
				state->r14 = thread->regs.r14;
				state->r15 = thread->regs.r15;
				state->rsp = thread->regs.rsp;
				state->fsbase = thread->regs.fsbase;
				state->gsbase = thread->regs.gsbase;
				break;
			};
		};
		
		thread = thread->next;		
	} while (thread != ct);
	
	unlockSched();
	sti();
	
	return result;
};

static int tcSetFlags(int thid, int flags)
{
	cli();
	lockSched();
	
	Thread *ct = getCurrentThread();
	Thread *thread = ct;
	
	int result = -1;
	ERRNO = ESRCH;
	
	do
	{
		if (thread->thid == thid)
		{
			if (thread->creds == NULL)
			{
				// not our child
				ERRNO = ESRCH;
				break;
			}
			else if (thread->creds->ppid != ct->creds->pid)
			{
				// not our child
				ERRNO = ESRCH;
				break;
			}
			else if ((thread->flags & THREAD_TRACED) == 0)
			{
				// not traced
				ERRNO = EPERM;
				break;
			}
			else
			{
				result = 0;
				thread->debugFlags = (thread->debugFlags & (DBG_DEBUGGER | DBG_DEBUG_MODE)) | (flags & ~(DBG_DEBUGGER | DBG_DEBUG_MODE));
				break;
			};
		};
		
		thread = thread->next;		
	} while (thread != ct);
	
	unlockSched();
	sti();
	
	return result;
};

static int tcCont(int thid)
{
	cli();
	lockSched();
	
	Thread *ct = getCurrentThread();
	Thread *thread = ct;
	
	int result = -1;
	ERRNO = ESRCH;
	
	do
	{
		if (thread->thid == thid)
		{
			if (thread->creds == NULL)
			{
				// not our child
				ERRNO = ESRCH;
				break;
			}
			else if (thread->creds->ppid != ct->creds->pid)
			{
				// not our child
				ERRNO = ESRCH;
				break;
			}
			else if ((thread->flags & THREAD_TRACED) == 0)
			{
				// not traced
				ERRNO = EPERM;
				break;
			}
			else
			{
				thread->flags &= ~THREAD_TRACED;
				thread->flags |= THREAD_WAITING;	// force signalThread to queue it
				signalThread(thread);
				result = 0;
				break;
			};
		};
		
		thread = thread->next;		
	} while (thread != ct);
	
	unlockSched();
	sti();
	
	return result;
};

int sys_trace(int cmd, int thid, void *param)
{
	MachineState state;
	switch (cmd)
	{
	case TC_DEBUG:
		if (getCurrentThread()->debugFlags != 0)
		{
			ERRNO = EPERM;
			return -1;
		};
		__sync_fetch_and_or(&getCurrentThread()->debugFlags, DBG_DEBUGGER);
		return 0;
	case TC_GETREGS:
		if (tcGetRegs(thid, &state) != 0)
		{
			return -1;
		};
		
		if (memcpy_k2u(param, &state, sizeof(MachineState)) != 0)
		{
			return -1;
		};
		
		return 0;
	case TC_CONT:
		return tcCont(thid);
		break;
	case TC_SET_FLAGS:
		return tcSetFlags(thid, (int) (uint64_t) param);
		break;
	default:
		ERRNO = EINVAL;
		return -1;
	};
};

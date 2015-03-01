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

#include <glidix/sched.h>
#include <glidix/memory.h>
#include <glidix/string.h>
#include <glidix/console.h>
#include <glidix/spinlock.h>
#include <glidix/errno.h>

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
	spinlockRelease(&schedLock);

	// create a new stack for this initial process
	firstThread.stack = kmalloc(DEFAULT_STACK_SIZE);
	firstThread.stackSize = DEFAULT_STACK_SIZE;

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
	firstThread.rootSigHandler = 0;
	firstThread.sigput = 0;
	firstThread.sigfetch = 0;
	firstThread.sigcnt = 0;

	// UID/GID stuff
	firstThread.euid = 0;
	firstThread.suid = 0;
	firstThread.ruid = 0;
	firstThread.egid = 0;
	firstThread.sgid = 0;
	firstThread.rgid = 0;

	// set the working directory to /initrd by default.
	strcpy(firstThread.cwd, "/initrd");

	// no executable
	firstThread.fpexec = NULL;

	// no error ptr
	firstThread.errnoptr = NULL;

	// linking
	firstThread.prev = &firstThread;
	firstThread.next = &firstThread;

	// switch to this new thread's context
	currentThread = &firstThread;
	switchContext(&firstThread.regs);
};

extern void reloadTR();

static void jumpToTask()
{
	// switch kernel stack
	ASM("cli");
	_tss.rsp0 = ((uint64_t) currentThread->stack + currentThread->stackSize) & (uint64_t)~0xF;

	// reload the TSS
	reloadTR();

	// switch address space
	if (currentThread->pm != NULL) SetProcessMemory(currentThread->pm);
	ASM("cli");

	// make sure IF is set
	currentThread->regs.rflags |= (1 << 9);

	// switch context
	switchContext(&currentThread->regs);
};

void lockSched()
{
	spinlockAcquire(&schedLock);
};

void unlockSched()
{
	spinlockRelease(&schedLock);
};

void switchTask(Regs *regs)
{
	if (currentThread == NULL) return;
	if (spinlockTry(&schedLock)) return;
	ASM("cli");

	// remember the context of this thread.
	memcpy(&currentThread->regs, regs, sizeof(Regs));

	// get the next thread
	do
	{
		currentThread = currentThread->next;
	} while (currentThread->flags & THREAD_NOSCHED);

	// if there are signals waiting, and not currently being handled, then handle them.
	if (((currentThread->flags & THREAD_SIGNALLED) == 0) && (currentThread->sigcnt != 0))
	{
		// if the syscall is interruptable, do the switch-back.
		if (currentThread->flags & THREAD_INT_SYSCALL)
		{
			kprintf_debug("signal in queue, THREAD_INT_SYSCALL ok\n");
			memcpy(&currentThread->regs, &currentThread->intSyscallRegs, sizeof(Regs));
			*((int64_t*)&currentThread->regs.rax) = -1;
			currentThread->therrno = EINTR;
		};
		
		// i've found that catching signals in kernel mode is a bad idea
		if ((currentThread->regs.cs & 3) == 3)
		{
			dispatchSignal(currentThread);
		};
	};

	spinlockRelease(&schedLock);
	jumpToTask();
};

static void kernelThreadExit()
{
	panic("TODO: we must properly implement kernel thrads exiting");

	// just to be safe
	if (currentThread == &firstThread)
	{
		panic("kernel startup thread tried to exit (this is a bug)");
	};

	Thread *nextThread = currentThread->next;

	// we need to do all of this with interrupts disabled, else if this gets interrupted
	// half way though, we might get a memory leak due to something not being kfree()'d.
	ASM("cli");
	currentThread->prev->next = currentThread->next;
	currentThread->next->prev = currentThread->prev;

	kfree(currentThread->stack);
	kfree(currentThread);

	// switch tasks
	currentThread = nextThread;
	jumpToTask();
};

void CreateKernelThread(KernelThreadEntry entry, KernelThreadParams *params, void *data)
{
	// params
	uint64_t stackSize = DEFAULT_STACK_SIZE;
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
	thread->regs.rsp = ((uint64_t) thread->stack + thread->stackSize - 8) & ~0xF;	// -8 because we'll push the return address...
	thread->regs.cs = 8;
	thread->regs.ds = 16;
	thread->regs.ss = 0;
	thread->regs.rflags = getFlagsRegister() | (1 << 9);				// enable interrupts in that thread
	thread->name = name;
	thread->flags = 0;
	thread->pm = NULL;
	thread->pid = 0;
	thread->ftab = NULL;
	thread->rootSigHandler = 0;
	thread->sigput = 0;
	thread->sigfetch = 0;
	thread->sigcnt = 0;

	// kernel threads always run as root
	thread->euid = 0;
	thread->suid = 0;
	thread->ruid = 0;
	thread->egid = 0;
	thread->sgid = 0;
	thread->rgid = 0;

	// start all kernel threads in "/initrd"
	strcpy(thread->cwd, "/initrd");

	// no executable attached
	thread->fpexec = NULL;

	// no errnoptr
	thread->errnoptr = NULL;

	// this will simulate a call from kernelThreadExit() to "entry()"
	// this is so that when entry() returns, the thread can safely exit.
	thread->regs.rdi = (uint64_t) data;
	*((uint64_t*)thread->regs.rsp) = (uint64_t) &kernelThreadExit;

	// link into the runqueue
	spinlockAcquire(&schedLock);
	currentThread->next->prev = thread;
	thread->next = currentThread->next;
	thread->prev = currentThread;
	currentThread->next = thread;
	// there is no need to update currentThread->prev, it will only be broken for the init
	// thread, which never exits, and therefore its prev will never need to be valid.
	spinlockRelease(&schedLock);
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

	// kernel stack
	thread->stack = kmalloc(DEFAULT_STACK_SIZE);
	thread->stackSize = DEFAULT_STACK_SIZE;

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

	// remember parent pid
	thread->pidParent = currentThread->pid;

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

	// inherit UIDs/GIDs from the parent
	thread->euid = currentThread->euid;
	thread->suid = currentThread->suid;
	thread->ruid = currentThread->ruid;
	thread->egid = currentThread->egid;
	thread->sgid = currentThread->sgid;
	thread->rgid = currentThread->rgid;

	// inherit the working directory
	strcpy(thread->cwd, currentThread->cwd);

	// duplicate the executable description.
	if (currentThread->fpexec == NULL)
	{
		thread->fpexec = NULL;
	}
	else
	{
		File *fpexec = (File*) kmalloc(sizeof(File));
		memset(fpexec, 0, sizeof(File));
		currentThread->fpexec->dup(currentThread->fpexec, fpexec, sizeof(File));
		thread->fpexec = fpexec;
	};
	
	// inherit the root signal handler
	thread->rootSigHandler = currentThread->rootSigHandler;

	// empty signal queue
	thread->sigput = 0;
	thread->sigfetch = 0;
	thread->sigcnt = 0;

	// exec params
	if (currentThread->pid != 0)
	{
		thread->execPars = (char*) kmalloc(currentThread->szExecPars);
		memcpy(thread->execPars, currentThread->execPars, currentThread->szExecPars);
		thread->szExecPars = currentThread->szExecPars;
	}
	else
	{
		thread->execPars = NULL;
		thread->szExecPars = 0;
	};

	thread->therrno = 0;

	// no errnoptr
	thread->errnoptr = NULL;

	// link into the runqueue
	spinlockAcquire(&schedLock);
	currentThread->next->prev = thread;
	thread->next = currentThread->next;
	thread->prev = currentThread;
	currentThread->next = thread;
	spinlockRelease(&schedLock);

	return thread->pid;
};

void threadExit(Thread *thread, int status)
{
	if (thread->pid == 0)
	{
		panic("a kernel thread tried to threadExit()");
	};

	if (thread->pid == 1)
	{
		panic("init terminated with status %d!", status);
	};

	// don't break the CPU runqueue
	ASM("cli");

	// init will adopt all orphans
	Thread *scan = thread;
	do
	{
		if (scan->pidParent == thread->pid)
		{
			scan->pidParent = 1;
		};
		scan = scan->next;
	} while (scan != thread);

	// terminate us
	thread->status = status;
	thread->flags |= THREAD_TERMINATED;

	// find the parent
	Thread *parent = thread;
	do
	{
		parent = parent->next;
	} while (parent->pid != thread->pidParent);

	// tell the parent that its child has died
	siginfo_t siginfo;
	siginfo.si_signo = SIGCHLD;
	if (status >= 0)
	{
		siginfo.si_code = CLD_EXITED;
	}
	else
	{
		siginfo.si_code = CLD_KILLED;
	};
	siginfo.si_pid = thread->pid;
	siginfo.si_status = status;
	siginfo.si_uid = thread->ruid;
	ASM("sti");
	sendSignal(parent, &siginfo);
};

int pollThread(Regs *regs, int pid, int *stat_loc, int flags)
{
	ASM("cli");
	Thread *threadToKill = NULL;
	Thread *thread = currentThread->next;
	while (thread != currentThread)
	{
		if (thread->pidParent == currentThread->pid)
		{
			if ((thread->pid == pid) || (pid == -1))
			{
				if (thread->flags & THREAD_TERMINATED)
				{
					threadToKill = thread;
					*stat_loc = thread->status;

					// unlink from the runqueue
					thread->prev->next = thread->next;
					thread->next->prev = thread->prev;

					break;
				};
			};
		};
		thread = thread->next;
	};

	// when WNOHANG is clear
	if ((threadToKill == NULL) && ((flags & WNOHANG) == 0))
	{
		//currentThread->flags |= THREAD_WAITING;
		//currentThread->therrno = ECHILD;
		//*((int*)&regs->rax) = -1;
		//switchTask(regs);
		return -2;
	};

	ASM("sti");

	// when WNOHANG is set
	if (threadToKill == NULL)
	{
		currentThread->therrno = ECHILD;
		return -1;
	};

	// there is a process ready to be deleted, it's already removed from the runqueue.
	kfree(thread->stack);
	DownrefProcessMemory(thread->pm);
	ftabDownref(thread->ftab);

	if (thread->fpexec != NULL)
	{
		if (thread->fpexec->close != NULL) thread->fpexec->close(thread->fpexec);
		kfree(thread->fpexec);
	};

	if (thread->execPars != NULL) kfree(thread->execPars);
	int ret = thread->pid;
	kfree(thread);

	return ret;
};

static int canSendSignal(Thread *src, Thread *dst, int signo)
{
	switch (signo)
	{
	// list all signals that can be sent at all
	case SIGCONT:
	case SIGHUP:
	case SIGINT:
	case SIGKILL:
	case SIGQUIT:
	case SIGSTOP:
	case SIGTERM:
	case SIGTSTP:
	case 0:
		break;
	default:
		return 0;
	};

	if (dst->pid == 0)
	{
		return 0;
	};

	if ((dst->pid == 1) && ((signo == SIGKILL) || (signo == SIGSTOP)))
	{
		return 0;
	};

	if ((src->euid == 0) || (src->ruid == 0))
	{
		return 1;
	};

	if ((src->euid == dst->euid) || (src->ruid == dst->euid) || (src->euid == dst->suid) || (src->ruid == dst->ruid))
	{
		return 1;
	};

	if ((dst->pidParent == src->pid) && (((dst->flags & THREAD_REBEL) == 0) || (signo == SIGINT)))
	{
		return 1;
	};

	return 0;
};

int signalPid(int pid, int signo)
{
	if (pid == currentThread->pid)
	{
		currentThread->therrno = EINVAL;
		return -1;
	};

	lockSched();
	Thread *thread = currentThread->next;

	while (thread != currentThread)
	{
		if (thread->pid == pid)
		{
			break;
		};
		thread = thread->next;
	};

	if (thread->flags & THREAD_TERMINATED)
	{
		thread = currentThread;
	};

	if (thread == currentThread)
	{
		currentThread->therrno = ESRCH;
		unlockSched();
		return -1;
	};

	if (!canSendSignal(currentThread, thread, signo))
	{
		currentThread->therrno = EPERM;
		unlockSched();
		return -1;
	};

	siginfo_t si;
	si.si_signo = signo;
	if (signo != 0) sendSignal(thread, &si);

	unlockSched();
	return 0;
};

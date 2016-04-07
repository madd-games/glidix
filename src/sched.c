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

#include <glidix/sched.h>
#include <glidix/memory.h>
#include <glidix/string.h>
#include <glidix/console.h>
#include <glidix/spinlock.h>
#include <glidix/errno.h>
#include <glidix/apic.h>
#include <glidix/time.h>
#include <glidix/syscall.h>
#include <glidix/cpu.h>

static Thread firstThread;
PER_CPU Thread *currentThread;		// don't make it static; used by syscall.asm
static Spinlock schedLock;		// for PID and stuff
static int nextPid;

extern uint64_t getFlagsRegister();
void kmain2();
uint32_t quantumTicks;			// initialised by init.c, how many APIC timer ticks to do per process.

typedef struct
{
	char symbol;
	uint64_t mask;
} ThreadFlagInfo;

static ThreadFlagInfo threadFlags[] = {
	{'W', THREAD_WAITING},
	{'S', THREAD_SIGNALLED},
	{'T', THREAD_TERMINATED},
	{'R', THREAD_REBEL},
	{0, 0}
};

static void printThreadFlags(uint64_t flags)
{
	ThreadFlagInfo *info;
	for (info=threadFlags; info->symbol!=0; info++)
	{
		if (flags & info->mask)
		{
			kprintf("%$\x02%c%#", info->symbol);
		}
		else
		{
			kprintf("%$\x04%c%#", info->symbol-'A'+'a');
		};
	};
};

void dumpRunqueue()
{
	kprintf("#\tPID\tPARENT\tCPU\tNAME                            FLAGS\tEUID\tEGID\n");
	Thread *th = &firstThread;
	int i=0;
	do
	{
		char name[33];
		memset(name, ' ', 32);
		name[32] = 0;
		memcpy(name, th->name, strlen(th->name));
		kprintf("%d\t%d\t%d\t%d\t%s", i++, th->pid, th->pidParent, th->cpuID, name);
		printThreadFlags(th->flags);
		if (th->regs.cs == 8)
		{
			kprintf("%$\x02" "K%#");
		}
		else
		{
			kprintf("%$\x04" "K%#");
		};
		kprintf("\t%d\t%d\n", th->euid, th->egid);
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
	fpuSave(&firstThread.fpuRegs);
	
	// the value of registers do not matter except RSP and RIP,
	// also the startup function should never return.
	memset(&firstThread.fpuRegs, 0, 512);
	memset(&firstThread.regs, 0, sizeof(Regs));
	firstThread.regs.rip = (uint64_t) &startupThread;
	firstThread.regs.rsp = (uint64_t) firstThread.stack + firstThread.stackSize;
	firstThread.regs.cs = 8;
	firstThread.regs.ds = 16;
	firstThread.regs.ss = 0;
	firstThread.regs.rflags = getFlagsRegister() | (1 << 9);		// enable interrupts

	// other stuff
	strcpy(firstThread.name, "Startup thread");
	firstThread.flags = 0;
	firstThread.pm = NULL;
	firstThread.pid = 0;
	firstThread.ftab = NULL;

	// UID/GID stuff
	firstThread.euid = 0;
	firstThread.suid = 0;
	firstThread.ruid = 0;
	firstThread.egid = 0;
	firstThread.sgid = 0;
	firstThread.rgid = 0;

	// sid/pgid
	firstThread.sid = 0;
	firstThread.pgid = 0;
	
	// set the working directory to /initrd by default.
	strcpy(firstThread.cwd, "/initrd");

	// no executable
	firstThread.fpexec = NULL;

	// no error ptr
	firstThread.errnoptr = NULL;

	// no wakeing
	firstThread.wakeTime = 0;

	// no umask
	firstThread.umask = 0;

	// no supplementary groups
	firstThread.numGroups = 0;
	
	// do not send alarms
	firstThread.alarmTime = 0;

	// run on the BSP
	firstThread.cpuID = 0;
	
	// signal stuff
	firstThread.sigdisp = NULL;
	firstThread.pendingSet = 0;
	firstThread.sigmask = 0;
	
	// linking
	firstThread.prev = &firstThread;
	firstThread.next = &firstThread;

	// switch to this new thread's context
	currentThread = &firstThread;
	apic->timerInitCount = quantumTicks;
	switchContext(&firstThread.regs);
};

void initSchedAP()
{
	// create a thread just like the first thread, that will be constantly sleeping,
	// and is assigned to this CPU
	Thread *idleThread = NEW(Thread);
	memcpy(idleThread, &firstThread, sizeof(Thread));
	idleThread->flags = THREAD_WAITING;
	idleThread->cpuID = getCurrentCPU()->id;
	strformat(idleThread->name, 256, "CPU %d idle thread", idleThread->cpuID);
		
	// interrupts are already disabled
	// link the idle thread into the runqueue
	lockSched();
	idleThread->prev = &firstThread;
	idleThread->next = firstThread.next;
	firstThread.next->prev = idleThread;
	firstThread.next = idleThread;
	unlockSched();
	
	// set up any valid register set for the initial task switch
	Regs regs;
	memcpy(&regs, &idleThread->regs, sizeof(Regs));
	
	// we are ready
	currentThread = idleThread;
	switchTask(&regs);
};

extern void reloadTR();

static void jumpToTask()
{
	// switch kernel stack
	ASM("cli");
	_tss.rsp0 = ((uint64_t) currentThread->stack + currentThread->stackSize) & (uint64_t)~0xF;
	currentThread->syscallStackPointer = ((uint64_t) currentThread->stack + currentThread->stackSize) & (uint64_t)~0xF;

	// reload the TSS
	reloadTR();

	// switch address space
	if (currentThread->pm != NULL) SetProcessMemory(currentThread->pm);
	ASM("cli");

	// make sure IF is set
	currentThread->regs.rflags |= (1 << 9);

	// switch context
	fpuLoad(&currentThread->fpuRegs);
	apic->timerInitCount = quantumTicks;
	switchContext(&currentThread->regs);
};

void lockSched()
{
	spinlockAcquire(&schedLock);
};

int isSchedLocked()
{
	return (schedLock._ != 0);
};

void unlockSched()
{
	spinlockRelease(&schedLock);
};

int canSched(Thread *thread)
{
	if (getCurrentCPU() != NULL)
	{
		// do not even try waking/alarming threads that do not belong to this CPU!
		if (thread->cpuID != getCurrentCPU()->id) return 0;
	};

	if (thread->wakeTime != 0)
	{
		uint64_t currentTime = (uint64_t) getTicks();
		if (currentTime >= thread->wakeTime)
		{
			thread->wakeTime = 0;
			thread->flags &= ~THREAD_WAITING;
		};
	};

	if (thread->alarmTime != 0)
	{
		uint64_t currentTime = getNanotime();
		if (currentTime >= thread->alarmTime)
		{
			siginfo_t si;
			si.si_signo = SIGALRM;
			si.si_code = 0;
			si.si_errno = 0;
			si.si_pid = 0;
			si.si_uid = 0;
			si.si_addr = NULL;
			si.si_status = 0;
			si.si_band = 0;
			si.si_value.sival_int = 0;
			thread->alarmTime = 0;
			sendSignal(thread, &si);
		};
	};
	
	if (thread->flags & THREAD_NOSCHED) return 0;
	return 1;
};

void switchTask(Regs *regs)
{
	cli();
	if (currentThread == NULL)
	{
		apic->timerInitCount = quantumTicks;
		return;
	};
	
	spinlockAcquire(&schedLock);

	Thread *threadPrev = currentThread;
	
	// remember the context of this thread.
	fpuSave(&currentThread->fpuRegs);
	memcpy(&currentThread->regs, regs, sizeof(Regs));

	// get the next thread
	while (1)
	{
		do
		{
			currentThread = currentThread->next;
		} while ((!canSched(currentThread)) && (currentThread != threadPrev));

		if (currentThread == threadPrev)
		{
			if (!canSched(currentThread))
			{
				// got nothing to do
				spinlockRelease(&schedLock);
				cooloff();
				spinlockAcquire(&schedLock);
				continue;
			};
		};
		
		break;
	};
	
	// if there are signals ready to dispatch, dispatch them.
	if (haveReadySigs(currentThread))
	{
		// i've found that catching signals in kernel mode is a bad idea
		if ((currentThread->regs.cs & 3) == 3)
		{
			dispatchSignal(currentThread);
		};
	};

	spinlockRelease(&schedLock);
	
	// the scheduler lock is now released, but we know the thread is not terminated,
	// and so currentThread will not be suddenly released so it is safe to use it.
	// it can only terminate when we dispatch a signal, and only this CPU can do this.
	jumpToTask();
};

int haveReadySigs(Thread *thread)
{
	if (thread->sigdisp == NULL) return 0;
	
	int i;
	for (i=0; i<64; i++)
	{
		uint64_t mask = (1UL << i);
		if (thread->pendingSet & mask)
		{
			uint64_t sigbit = (1UL << thread->pendingSigs[i].si_signo);
			if ((thread->sigmask & sigbit) == 0)
			{
				// not blocked
				return 1;
			};
		};
	};
	
	return 0;
};

int wasSignalled()
{
	cli();
	lockSched();
	int result = haveReadySigs(currentThread);
	unlockSched();
	sti();
	return result;
};

static void kernelThreadExit()
{
	// just to be safe
	if (currentThread == &firstThread)
	{
		panic("kernel startup thread tried to exit (this is a bug)");
	};

	// we need to do all of this with interrupts disabled. we remove ourselves from the runqueue,
	// but do not free the stack nor the thread description; this will be done by ReleaseKernelThread()
	ASM("cli");
	lockSched();
	currentThread->prev->next = currentThread->next;
	currentThread->next->prev = currentThread->prev;
	currentThread->flags |= THREAD_TERMINATED;
	downrefCPU(currentThread->cpuID);
	unlockSched();
	kyield();
};

void ReleaseKernelThread(Thread *thread)
{
	// busy-wait until the thread terminates
	while ((thread->flags & THREAD_TERMINATED) != 0)
	{
		__sync_synchronize();
	};
	
	// release the stack and thread description
	kfree(thread->stack);
	kfree(thread);
};

Thread* CreateKernelThread(KernelThreadEntry entry, KernelThreadParams *params, void *data)
{
	// params
	uint64_t stackSize = DEFAULT_STACK_SIZE;
	if (params != NULL)
	{
		if (params->stackSize != 0) stackSize = params->stackSize;
	};
	const char *name = "Nameless thread";
	if (params != NULL)
	{
		if (params->name != NULL) name = params->name;
	};
	int threadFlags = 0;
	if (params != NULL)
	{
		threadFlags = params->flags;
	};

	// allocate and fill in the thread structure
	Thread *thread = (Thread*) kmalloc(sizeof(Thread));
	thread->stack = kmalloc(stackSize);
	thread->stackSize = stackSize;

	memset(&thread->fpuRegs, 0, 512);
	memset(&thread->regs, 0, sizeof(Regs));
	thread->regs.rip = (uint64_t) entry;
	thread->regs.rsp = ((uint64_t) thread->stack + thread->stackSize - 8) & ~0xF;	// -8 because we'll push the return address...
	thread->regs.cs = 8;
	thread->regs.ds = 16;
	thread->regs.ss = 0;
	thread->regs.rflags = getFlagsRegister() | (1 << 9);				// enable interrupts in that thread
	strcpy(thread->name, name);
	thread->flags = threadFlags;
	thread->pm = NULL;
	thread->pid = 0;
	thread->pidParent = 0;
	thread->ftab = NULL;
	thread->alarmTime = 0;
	
	thread->sigdisp = NULL;
	thread->pendingSet = 0;
	thread->sigmask = 0;

	// kernel threads always run as root
	thread->euid = 0;
	thread->suid = 0;
	thread->ruid = 0;
	thread->egid = 0;
	thread->sgid = 0;
	thread->rgid = 0;

	// they are also a special session and process group
	thread->sid = 0;
	thread->pgid = 0;
	
	// start all kernel threads in "/initrd"
	strcpy(thread->cwd, "/initrd");

	// no executable attached
	thread->fpexec = NULL;

	// no errnoptr
	thread->errnoptr = NULL;

	// do not wake
	thread->wakeTime = 0;

	// no umask
	thread->umask = 0;

	// this will simulate a call from kernelThreadExit() to "entry()"
	// this is so that when entry() returns, the thread can safely exit.
	thread->regs.rdi = (uint64_t) data;
	*((uint64_t*)thread->regs.rsp) = (uint64_t) &kernelThreadExit;

	// allocate a CPU to run this thread on
	thread->cpuID = allocCPU();
	
	// link into the runqueue
	cli();
	spinlockAcquire(&schedLock);
	currentThread->next->prev = thread;
	thread->next = currentThread->next;
	thread->prev = currentThread;
	currentThread->next = thread;
	// there is no need to update currentThread->prev, it will only be broken for the init
	// thread, which never exits, and therefore its prev will never need to be valid.
	signalThread(thread);
	spinlockRelease(&schedLock);
	sti();
	
	return thread;
};

Thread *getCurrentThread()
{
	return currentThread;
};

void waitThread(Thread *thread)
{
	thread->flags |= THREAD_WAITING;
};

void signalThread(Thread *thread)
{
	thread->flags &= ~THREAD_WAITING;
	if (thread->cpuID != getCurrentCPU()->id)
	{
		// interrupt the thread's host CPU in case it is in cooloff state
		sendHintToCPU(thread->cpuID);
	};
};

int threadClone(Regs *regs, int flags, MachineState *state)
{
	Thread *thread = (Thread*) kmalloc(sizeof(Thread));
	fpuSave(&thread->fpuRegs);
	memcpy(&thread->regs, regs, sizeof(Regs));

	if (state != NULL)
	{
		memcpy(&thread->fpuRegs, &state->fpuRegs, 512);
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
		thread->regs.rflags = getFlagsRegister() | (1 << 9);
	};

	// kernel stack
	thread->stack = kmalloc(DEFAULT_STACK_SIZE);
	thread->stackSize = DEFAULT_STACK_SIZE;

	strcpy(thread->name, currentThread->name);
	thread->flags = 0;

	// process memory
	if (flags & CLONE_SHARE_MEMORY)
	{
		UprefProcessMemory(currentThread->pm);
		thread->pm = currentThread->pm;
		
		sigdispUpref(currentThread->sigdisp);
		thread->sigdisp = currentThread->sigdisp;
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
		
		thread->sigdisp = sigdispCreate();
		if (currentThread->sigdisp != NULL)
		{
			memcpy(thread->sigdisp->actions, currentThread->sigdisp->actions, sizeof(SigAction)*SIG_NUM);
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

	// inherit the session and process group IDs
	thread->sid = currentThread->sid;
	thread->pgid = currentThread->pgid;
	
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
	thread->wakeTime = 0;
	thread->umask = 0;

	memcpy(thread->groups, currentThread->groups, sizeof(gid_t)*16);
	thread->numGroups = currentThread->numGroups;
	
	// alarms shall be cancelled in the new process/thread
	thread->alarmTime = 0;
	
	// allocate a CPU to run this thread on
	thread->cpuID = allocCPU();
	
	// if the address space is shared, the errnoptr is now invalid;
	// otherwise, it can just stay where it is.
	if (flags & CLONE_SHARE_MEMORY)
	{
		thread->errnoptr = NULL;
	}
	else
	{
		thread->errnoptr = currentThread->errnoptr;
	};

	// link into the runqueue
	cli();
	spinlockAcquire(&schedLock);
	currentThread->next->prev = thread;
	thread->next = currentThread->next;
	thread->prev = currentThread;
	currentThread->next = thread;
	signalThread(thread);
	spinlockRelease(&schedLock);
	sti();

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

	sti();
	ftabDownref(thread->ftab);
	thread->ftab = NULL;
	
	// don't break the CPU runqueue
	cli();
	lockSched();

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
	sendSignal(parent, &siginfo);
	downrefCPU(thread->cpuID);
	unlockSched();
	sti();
};

static Thread *findThreadToKill(int pid, int *stat_loc, int flags)
{
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
					if (stat_loc != NULL) *stat_loc = thread->status;

					// unlink from the runqueue
					thread->prev->next = thread->next;
					thread->next->prev = thread->prev;

					break;
				}
				else if (flags & WDETACH)
				{
					// we can detach
					thread->pidParent = 1;
					unlockSched();
					sti();
					return NULL;
				};
			};
		};
		thread = thread->next;
	};

	return threadToKill;
};

int pollThread(int pid, int *stat_loc, int flags)
{
	if (kernelStatus != KERNEL_RUNNING)
	{
		currentThread->therrno = EPERM;
		return -1;
	};

	// a thread cannot wait for itself.
	if (pid == currentThread->pid)
	{
		ERRNO = ECHILD;
		return -1;
	};

	cli();
	lockSched();
	Thread *threadToKill = findThreadToKill(pid, stat_loc, flags);

	// when WNOHANG is clear
	while ((threadToKill == NULL) && ((flags & WNOHANG) == 0))
	{
		//getCurrentThread()->flags |= THREAD_WAITING;
		waitThread(getCurrentThread());
		unlockSched();
		kyield();
		cli();
		lockSched();
		threadToKill = findThreadToKill(pid, stat_loc, flags);
		if (threadToKill != NULL) break;
		
		if (haveReadySigs(currentThread))
		{
			unlockSched();
			sti();
			ERRNO = EINTR;
			return -1;
		};
	};

	unlockSched();
	sti();

	// when WNOHANG is set
	if (threadToKill == NULL)
	{
		currentThread->therrno = ECHILD;
		return -1;
	};

	// there is a process ready to be deleted, it's already removed from the runqueue.
	kfree(threadToKill->stack);
	DownrefProcessMemory(threadToKill->pm);
	sigdispDownref(threadToKill->sigdisp);

	if (threadToKill->fpexec != NULL)
	{
		if (threadToKill->fpexec->close != NULL) threadToKill->fpexec->close(threadToKill->fpexec);
		kfree(threadToKill->fpexec);
	};

	if (threadToKill->execPars != NULL) kfree(threadToKill->execPars);
	int ret = threadToKill->pid;
	kfree(threadToKill);
	
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
	case SIGUSR1:
	case SIGUSR2:
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

	if ((src->ruid == dst->euid) || (src->ruid == dst->ruid))
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

	cli();
	lockSched();
	Thread *thread = currentThread->next;

	int result = 0;
	ERRNO = ESRCH;
	while (thread != currentThread)
	{
		if ((thread->pid == pid) || (thread->pgid == -pid) || (pid == -1)
			|| ((pid == 0) && (thread->pgid == currentThread->pgid)))
		{
			if (thread->flags & THREAD_TERMINATED)
			{
				thread = thread->next;
				continue;
			};

			if (!canSendSignal(currentThread, thread, signo))
			{
				ERRNO = EPERM;
				thread = thread->next;
				continue;
			};

			siginfo_t si;
			si.si_signo = signo;
			if (signo != 0) sendSignal(thread, &si);
			result = 0;
		};
		thread = thread->next;
	};

	unlockSched();
	sti();
	return result;
};

void switchTaskToIndex(int index)
{
	currentThread = &firstThread;
	while (index--)
	{
		currentThread = currentThread->next;
	};
};

Thread *getThreadByID(int pid)
{
	if (pid == 0) return NULL;

	Thread *th = currentThread;
	while (th->pid != pid)
	{
		th = th->next;
		if (th == currentThread) return NULL;
	};

	return th;
};

void _preempt(); /* common.asm */
void kyield()
{
	// safely turn off the APIC timer, realising that _preempt() is not reentant.
	// enable interrupts in case it already fired, disable it, do 2 NOPs, and disable
	// it again so that there is no chance of it firing at the moment we disable it.
	sti();
	apic->timerInitCount = 0;
	nop();
	nop();
	apic->timerInitCount = 0;
	
	// call the assembly-level _preempt() which saves callee-save registers and switches task.
	_preempt();
};

void initUserRegs(Regs *regs)
{
	memset(regs, 0, sizeof(Regs));
	regs->ds = 0x23;
	regs->cs = 0x1B;
	regs->ss = 0x23;
	regs->rflags = getFlagsRegister() | (1 << 9);
};

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
extern PER_CPU TSS* localTSSPtr;

static Spinlock expireLock;
static Thread *threadSysMan;

static Spinlock notifLock;
static SchedNotif *firstNotif;

typedef struct
{
	char symbol;
	uint64_t mask;
} ThreadFlagInfo;

static ThreadFlagInfo threadFlags[] = {
	{'W', THREAD_WAITING},
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
		int pid = 0, ppid = 0, euid = 0, egid = 0;
		if (th->creds != NULL)
		{
			pid = th->creds->pid;
			ppid = th->creds->ppid;
			euid = th->creds->euid;
			egid = th->creds->egid;
		};
		kprintf("%d\t%d\t%d\t%d\t%s", i++, pid, ppid, th->cpuID, name);
		printThreadFlags(th->flags);
		if (th->regs.cs == 8)
		{
			kprintf("%$\x02" "K%#");
		}
		else
		{
			kprintf("%$\x04" "K%#");
		};
		kprintf("\t%d\t%d\n", euid, egid);
		th = th->next;
	} while (th != &firstThread);
};

Creds *credsNew()
{
	Creds *creds = NEW(Creds);
	creds->refcount = 1;
	creds->pid = 1;
	creds->ppid = 1;
	creds->euid = creds->suid = creds->ruid = 0;
	creds->egid = creds->sgid = creds->rgid = 0;
	creds->sid = 0;
	creds->pgid = 0;
	creds->umask = 0;
	creds->numGroups = 0;
	creds->status = 0;
	return creds;
};

Creds *credsDup(Creds *old)
{
	Creds *new = NEW(Creds);
	memcpy(new, old, sizeof(Creds));
	new->refcount = 1;
	return new;
};

void credsUpref(Creds *creds)
{
	__sync_fetch_and_add(&creds->refcount, 1);
};

void credsDownref(Creds *creds)
{
	int oldCount = __sync_fetch_and_add(&creds->refcount, -1);
	if (oldCount == 1)
	{
		spinlockAcquire(&notifLock);
		
		// orphanate all children of this process
		cli();
		lockSched();
		
		// if the parent died between the time our last thread was removed from
		// the runqueue and the removal of the credentials object, we changed our
		// parent to "init" (pid 1) now.
		int parentPid = 1;
		Thread *thread = currentThread;
		do
		{
			if (thread->creds != NULL)
			{
				if (thread->creds->ppid == creds->pid)
				{
					thread->creds->ppid = 1;
				};
				
				if (thread->creds->pid == creds->ppid)
				{
					// our parent lives!
					parentPid = creds->ppid;
				};
			};
			
			thread = thread->next;
		} while (thread != currentThread);
		
		unlockSched();
		sti();
		
		// delete all notifications directed at us
		SchedNotif *notif = firstNotif;
		while (notif != NULL)
		{
			if (notif->dest == creds->pid)
			{
				if (notif == firstNotif) firstNotif = notif->next;
				if (notif->prev != NULL) notif->prev->next = notif->next;
				if (notif->next != NULL) notif->next->prev = notif->prev;
				SchedNotif *next = notif->next;
				kfree(notif);
				notif = next;
			}
			else
			{
				notif = notif->next;
			};
		};
		
		// before releasing the spinlock, place a notification of our own termination
		notif = NEW(SchedNotif);
		notif->type = SHN_PROCESS_DEAD;
		notif->dest = parentPid;
		notif->source = creds->pid;
		notif->info.status = creds->status;
		
		notif->prev = NULL;
		notif->next = firstNotif;
		if (firstNotif != NULL) firstNotif->prev = notif;
		firstNotif = notif;

		spinlockRelease(&notifLock);
		
		// send the SIGCHLD signal to the parent
		siginfo_t siginfo;
		siginfo.si_signo = SIGCHLD;
		if (creds->status >= 0)
		{
			siginfo.si_code = CLD_EXITED;
		}
		else
		{
			siginfo.si_code = CLD_KILLED;
		};
		siginfo.si_pid = creds->pid;
		siginfo.si_status = creds->status;
		siginfo.si_uid = creds->ruid;
		signalPidEx(parentPid, &siginfo);
		
		// wake the parent
		wakeProcess(parentPid);
		
		kfree(creds);
	};
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
	spinlockRelease(&notifLock);
	firstNotif = NULL;
	
	// create a new stack for this initial process
	firstThread.stack = kmalloc(DEFAULT_STACK_SIZE);
	firstThread.stackSize = DEFAULT_STACK_SIZE;
	fpuSave(&firstThread.fpuRegs);
	firstThread.catchRegs[7] = 0;
	
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
	firstThread.ftab = NULL;

	// no credentials for kernel threads
	firstThread.creds = NULL;
	firstThread.thid = 0;

	// set the working directory to /initrd by default.
	strcpy(firstThread.cwd, "/initrd");

	// no error ptr
	firstThread.errnoptr = NULL;

	// no wakeing
	firstThread.wakeTime = 0;
	
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

static Spinlock sysManReadySignal;
static Spinlock sysManLock;
static int numThreadsToClean;
static void sysManFunc(void *ignore)
{
	(void)ignore;
	
	// SYSTEM MANAGER THREAD
	// This thread, when signalled, scans the thread queue for terminated
	// threads, removes them from the queue, and performs cleanup operations.
	// it is necessary because, for example, when a thread exits, it must free
	// the stack that it runs on.
	
	numThreadsToClean = 0;
	spinlockRelease(&sysManLock);
	while (1)
	{
		spinlockAcquire(&sysManLock);
		while (numThreadsToClean > 0)
		{
			numThreadsToClean--;
			
			Thread *threadFound = NULL;
			cli();
			lockSched();
			threadFound = currentThread;
			do
			{
				if (threadFound->flags & THREAD_TERMINATED)
				{
					if (threadFound->creds != NULL)
					{
						break;
					};
				};
				
				threadFound = threadFound->next;
			} while (threadFound != currentThread);
			
			if (threadFound != currentThread)
			{
				threadFound->prev->next = threadFound->next;
				threadFound->next->prev = threadFound->prev;
			};
			
			unlockSched();
			sti();
			
			if (threadFound != currentThread)
			{
				// removed from runqueue, now do cleanup
				kfree(threadFound->stack);
				if (threadFound->pm != NULL) DownrefProcessMemory(threadFound->pm);
				if (threadFound->ftab != NULL) ftabDownref(threadFound->ftab);
				if (threadFound->execPars != NULL) kfree(threadFound->execPars);
				if (threadFound->sigdisp != NULL) sigdispDownref(threadFound->sigdisp);
				if (threadFound->creds != NULL) credsDownref(threadFound->creds);
				kfree(threadFound);

			};
		};
		
		cli();
		lockSched();
		waitThread(currentThread);
		spinlockRelease(&sysManLock);
		spinlockRelease(&sysManReadySignal);
		unlockSched();
		kyield();
	};
};

void initSched2()
{
	spinlockRelease(&expireLock);
	
	spinlockRelease(&sysManReadySignal);
	spinlockAcquire(&sysManReadySignal);

	KernelThreadParams pars;
	memset(&pars, 0, sizeof(KernelThreadParams));
	pars.stackSize = DEFAULT_STACK_SIZE;
	pars.name = "System Manager Thread";
	threadSysMan = CreateKernelThread(sysManFunc, &pars, NULL);
	
	// wait for the system manager to release the lock, indicating that it is ready
	spinlockAcquire(&sysManReadySignal);
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
	cli();
	localTSSPtr->rsp0 = ((uint64_t) currentThread->stack + currentThread->stackSize) & (uint64_t)~0xF;
	currentThread->syscallStackPointer = ((uint64_t) currentThread->stack + currentThread->stackSize) & (uint64_t)~0xF;

	// reload the TSS
	reloadTR();

	// switch address space
	if (currentThread->pm != NULL) SetProcessMemory(currentThread->pm);

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

void switchTaskUnlocked(Regs *regs)
{
	Thread *threadPrev = currentThread;
	
	// remember the context of this thread.
	fpuSave(&currentThread->fpuRegs);
	memcpy(&currentThread->regs, regs, sizeof(Regs));

	// get the next thread
	while (1)
	{
		int numSeenFirst = 0;
		do
		{
			currentThread = currentThread->next;
			if (currentThread == &firstThread) numSeenFirst++;
		} while ((!canSched(currentThread)) && (numSeenFirst < 2));

		if (numSeenFirst >= 2)
		{
			if (!canSched(threadPrev))
			{
				// got nothing to do
				spinlockRelease(&schedLock);
				cooloff();
				spinlockAcquire(&schedLock);
				continue;
			}
			else
			{
				currentThread = threadPrev;
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
			dispatchSignal();
		};
	};

	spinlockRelease(&schedLock);
	
	// the scheduler lock is now released, but we know the thread is not terminated,
	// and so currentThread will not be suddenly released so it is safe to use it.
	// it can only terminate when we dispatch a signal, and only this CPU can do this.
	jumpToTask();
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
	switchTaskUnlocked(regs);
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
	Regs regs;
	lockSched();
	currentThread->prev->next = currentThread->next;
	currentThread->next->prev = currentThread->prev;
	currentThread->flags |= THREAD_TERMINATED;
	downrefCPU(currentThread->cpuID);
	switchTaskUnlocked(&regs);
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
	thread->catchRegs[7] = 0;
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
	thread->creds = NULL;
	thread->thid = 0;
	thread->ftab = NULL;
	thread->alarmTime = 0;
	
	thread->sigdisp = NULL;
	thread->pendingSet = 0;
	thread->sigmask = 0;

	// start all kernel threads in "/initrd"
	strcpy(thread->cwd, "/initrd");

	// no errnoptr
	thread->errnoptr = NULL;

	// do not wake
	thread->wakeTime = 0;

	// this will simulate a call from kernelThreadExit() to "entry()"
	// this is so that when entry() returns, the thread can safely exit.
	thread->regs.rdi = (uint64_t) data;
	*((uint64_t*)thread->regs.rsp) = (uint64_t) &kernelThreadExit;

	// allocate a CPU to run this thread on
	thread->cpuID = allocCPU();
	
	// link into the runqueue
	cli();
	lockSched();
	currentThread->next->prev = thread;
	thread->next = currentThread->next;
	thread->prev = currentThread;
	currentThread->next = thread;
	// there is no need to update currentThread->prev, it will only be broken for the init
	// thread, which never exits, and therefore its prev will never need to be valid.
	signalThread(thread);
	unlockSched();
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
	thread->catchRegs[7] = 0;
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
	if (flags & CLONE_THREAD)
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
	
	// credentials
	if (flags & CLONE_THREAD)
	{
		if (currentThread->creds != NULL)
		{
			credsUpref(currentThread->creds);
			thread->creds = currentThread->creds;
		}
		else
		{
			thread->creds = credsNew();
		};
	}
	else
	{
		if (currentThread->creds != NULL)
		{
			thread->creds = credsDup(currentThread->creds);
		}
		else
		{
			thread->creds = credsNew();
		};
	};

	// assign pid/thid
	spinlockAcquire(&schedLock);
	if ((flags & CLONE_THREAD) == 0)
	{
		thread->creds->pid = nextPid++;
	};
	thread->thid = nextPid++;
	spinlockRelease(&schedLock);

	// remember parent pid if this is a new process
	if ((flags & CLONE_THREAD) == 0)
	{
		if (currentThread->creds != NULL)
		{
			thread->creds->ppid = currentThread->creds->pid;
		}
		else
		{
			thread->creds->ppid = 1;
		};
	};

	// file table
	if (flags & CLONE_THREAD)
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
	
	// inherit the working directory
	strcpy(thread->cwd, currentThread->cwd);

	// inherit signal mask
	thread->sigmask = currentThread->sigmask;
	
	// exec params
	if (currentThread->execPars != NULL)
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
	
	// alarms shall be cancelled in the new process/thread
	thread->alarmTime = 0;
	
	// allocate a CPU to run this thread on
	thread->cpuID = allocCPU();
	
	// if the address space is shared, the errnoptr is now invalid;
	// otherwise, it can just stay where it is.
	if (flags & CLONE_THREAD)
	{
		thread->errnoptr = NULL;
	}
	else
	{
		thread->errnoptr = currentThread->errnoptr;
	};
	
	// detach if necessary
	if (flags & CLONE_DETACHED)
	{
		thread->flags |= THREAD_DETACHED;
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

	if (flags & CLONE_THREAD)
	{
		return thread->thid;
	}
	else
	{
		return thread->creds->pid;
	};
};

void threadExitEx(uint64_t retval)
{
	if (currentThread->creds == NULL)
	{
		panic("a kernel thread called threadExitEx()");
	};

	spinlockAcquire(&notifLock);
	if ((currentThread->flags & THREAD_DETACHED) == 0)
	{
		// this thread is not detached (i.e. it's joinable), so we make
		// a scheduler notification
		SchedNotif *notif = NEW(SchedNotif);
		notif->type = SHN_THREAD_DEAD;
		notif->dest = currentThread->creds->pid;
		notif->source = currentThread->thid;
		notif->info.retval = retval;
		notif->prev = NULL;
		
		notif->next = firstNotif;
		if (firstNotif != NULL) firstNotif->prev = notif;
		firstNotif = notif;
		spinlockRelease(&notifLock);
		
		// wake the other threads
		wakeProcess(currentThread->creds->pid);
	}
	else
	{
		spinlockRelease(&notifLock);
	};
		
	spinlockAcquire(&sysManLock);
	cli();
	lockSched();
	currentThread->flags |= THREAD_TERMINATED;
	numThreadsToClean++;
	spinlockRelease(&sysManLock);
	signalThread(threadSysMan);
	downrefCPU(currentThread->cpuID);
	Regs regs;
	switchTaskUnlocked(&regs);
};

void processExit(int status)
{
	killOtherThreads();
	currentThread->creds->status = status;
	threadExit();
};

void threadExit()
{
	threadExitEx(0);
};

int processWait(int pid, int *stat_loc, int flags)
{
	if (pid == currentThread->creds->pid)
	{
		ERRNO = ECHILD;
		return -1;
	};
	
	while (1)
	{
		spinlockAcquire(&notifLock);
		SchedNotif *notif = firstNotif;
		while (notif != NULL)
		{
			if ((notif->dest == currentThread->creds->pid) && ((notif->source == pid) || (pid == -1)) && (notif->type == SHN_PROCESS_DEAD))
			{
				*stat_loc = notif->info.status;
				if (notif == firstNotif) firstNotif = notif->next;
				if (notif->next != NULL) notif->next->prev = notif->prev;
				if (notif->prev != NULL) notif->prev->next = notif->next;
				int outpid = notif->source;
				kfree(notif);
				spinlockRelease(&notifLock);
				return outpid;
			}
			else
			{
				notif = notif->next;
			};
		};
		
		if (flags & WNOHANG)
		{
			ERRNO = ECHILD;
			spinlockRelease(&notifLock);
			return -1;
		};
		
		cli();
		lockSched();
		waitThread(currentThread);
		spinlockRelease(&notifLock);
		unlockSched();
		kyield();
		
		if (wasSignalled())
		{
			ERRNO = EINTR;
			return -1;
		};
	};
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

	if (dst->creds == NULL)
	{
		return 0;
	};
	
	if (src->creds == NULL)
	{
		// kernel threads can signal whoever they want
		return 1;
	};

	if ((dst->creds->pid == 1) && ((signo == SIGKILL) || (signo == SIGSTOP)))
	{
		return 0;
	};

	if ((src->creds->euid == 0) || (src->creds->ruid == 0))
	{
		return 1;
	};

	if ((src->creds->ruid == dst->creds->euid) || (src->creds->ruid == dst->creds->ruid))
	{
		return 1;
	};

	if ((dst->creds->ppid == src->creds->pid) && (((dst->flags & THREAD_REBEL) == 0) || (signo == SIGINT)))
	{
		return 1;
	};

	return 0;
};

int signalPidEx(int pid, siginfo_t *si)
{
	// keep track of which pids we've already successfully delivered signals to
	// in this call.
	int pidsDelivered[100];
	int pidDelivIndex = 0;
	
	cli();
	lockSched();
	
	int result = -1;
	ERRNO = ESRCH;

	int mypgid = 0;
	if (currentThread->creds != NULL)
	{
		mypgid = currentThread->creds->pgid;
	};

	Thread *thread = currentThread;
	do
	{
		if (thread->creds != NULL)
		{
			if ((thread->creds->pid == pid) || (thread->creds->pgid == -pid) || (pid == -1)
				|| ((pid == 0) && (thread->creds->pgid == mypgid)))
			{
				int i;
				int found = 0;
				for (i=0; i<pidDelivIndex; i++)
				{
					if (pidsDelivered[i] == thread->creds->pid)
					{
						found = 1;
						break;
					};
				};
			
				if (!found)
				{
					if (!canSendSignal(currentThread, thread, si->si_signo))
					{
						ERRNO = EPERM;
						thread = thread->next;
						continue;
					};
			
					if (si != NULL)
					{
						if (sendSignal(thread, si) == 0)
						{
							if (pidDelivIndex == 100)
							{
								ERRNO = ENOMEM;
								break;
							};
							pidsDelivered[pidDelivIndex++] = thread->creds->pid;
						};
					};
				
					result = 0;
				};
			};
		};
		
		thread = thread->next;
	} while (thread != currentThread);
	
	unlockSched();
	sti();
	
	if (result != 0)
	{
		// if the target pid is a zombie, we still report the sending to be successful
		spinlockAcquire(&notifLock);
		SchedNotif *notif;
		for (notif=firstNotif; notif!=NULL; notif=notif->next)
		{
			if ((notif->type == SHN_PROCESS_DEAD) && (notif->source == pid))
			{
				result = 0;
				break;
			};
		};
		spinlockRelease(&notifLock);
	};
	
	return result;
};

int signalPid(int pid, int sig)
{
	if (sig == 0)
	{
		return signalPidEx(pid, NULL);
	};
	
	siginfo_t si;
	memset(&si, 0, sizeof(siginfo_t));
	si.si_signo = sig;
	return signalPidEx(pid, &si);
};

void switchTaskToIndex(int index)
{
	currentThread = &firstThread;
	while (index--)
	{
		currentThread = currentThread->next;
	};
};

Thread *getThreadByTHID(int thid)
{
	Thread *th = currentThread;
	while (th->thid != thid)
	{
		th = th->next;
		if (th == currentThread) return NULL;
	};

	return th;
};

Thread *getThreadByPID(int pid)
{
	Thread *th = currentThread;
	do
	{
		if (th->creds != NULL)
		{
			if (th->creds->pid == pid) return th;
		};
		
		th = th->next;
	} while (th != currentThread);

	return NULL;
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

void wakeProcess(int pid)
{
	cli();
	lockSched();
	
	Thread *thread = currentThread;
	do
	{
		if (thread->creds != NULL)
		{
			if (thread->creds->pid == pid)
			{
				signalThread(thread);
			};
		};
		thread = thread->next;
	} while (thread != currentThread);
	
	unlockSched();
	sti();
};

void killOtherThreads()
{
	siginfo_t si;
	si.si_signo = SIGTHKILL;
	
	cli();
	lockSched();
	
	Thread *thread = currentThread;
	do
	{
		if (thread->creds != NULL)
		{
			if (thread->creds->pid == currentThread->creds->pid)
			{
				if (thread != currentThread)
				{
					sendSignal(thread, &si);
				};
			};
		};
		thread = thread->next;
	} while (thread != currentThread);
	
	unlockSched();
	sti();
	
	// wait for the termination
	while (currentThread->creds->refcount != 1)
	{
		__sync_synchronize();
		kyield();
	};
	
	// remove scheduler notifications directed at us
	spinlockAcquire(&notifLock);
	SchedNotif *notif = firstNotif;
	while (notif != NULL)
	{
		if (notif->dest == currentThread->creds->pid)
		{
			if (notif == firstNotif) firstNotif = notif->next;
			if (notif->prev != NULL) notif->prev->next = notif->next;
			if (notif->next != NULL) notif->next->prev = notif->prev;
			SchedNotif *next = notif->next;
			kfree(notif);
			notif = next;
		}
		else
		{
			notif = notif->next;
		};
	};
	spinlockRelease(&notifLock);
};

int joinThread(int thid, uint64_t *retval)
{
	if (thid == getCurrentThread()->thid)
	{
		return -1;
	};
	
	while (1)
	{
		spinlockAcquire(&notifLock);
		SchedNotif *notif = firstNotif;
		while (notif != NULL)
		{
			if (notif->dest == currentThread->creds->pid)
			{
				if ((notif->type == SHN_THREAD_DEAD) && (notif->source == thid))
				{
					*retval = notif->info.retval;
					if (notif->prev != NULL) notif->prev->next = notif->next;
					if (notif->next != NULL) notif->next->prev = notif->prev;
					if (firstNotif == notif) firstNotif = notif->next;
					kfree(notif);
					spinlockRelease(&notifLock);
					return 0;
				};
				notif = notif->next;
			}
			else
			{
				notif = notif->next;
			};
		};
	
		// no found
		cli();
		lockSched();
		waitThread(currentThread);
		spinlockRelease(&notifLock);
		unlockSched();
		kyield();
		
		if (wasSignalled())
		{
			return -2;
		};
	};
};

int detachThread(int thid)
{
	spinlockAcquire(&notifLock);
	cli();
	lockSched();
	
	Thread *thread = currentThread;
	do
	{
		if (thread->thid == thid)
		{
			if (thread->creds != NULL)
			{
				if (thread->creds->pid == currentThread->creds->pid)
				{
					// found the thread to detach
					if (thread->flags & THREAD_DETACHED)
					{
						// already detached
						unlockSched();
						sti();
						spinlockRelease(&notifLock);
						return EINVAL;
					};
					
					thread->flags |= THREAD_DETACHED;

					// remove all notifications about it
					SchedNotif *notif = firstNotif;
					SchedNotif *notifToDelete = NULL;
					while (notif != NULL)
					{
						if (notif->dest == currentThread->creds->pid)
						{
							if ((notif->type == SHN_THREAD_DEAD) && (notif->source == thid))
							{
								if (notif->prev != NULL) notif->prev->next = notif->next;
								if (notif->next != NULL) notif->next->prev = notif->prev;
								if (firstNotif == notif) firstNotif = notif->next;
								notifToDelete = notif;
								spinlockRelease(&notifLock);
								return 0;
							};
							notif = notif->next;
						}
						else
						{
							notif = notif->next;
						};
					};
					
					// success
					unlockSched();
					sti();
					spinlockRelease(&notifLock);
					
					if (notifToDelete != NULL) kfree(notifToDelete);
					return 0;
				};
			};
			
			// we know the thread exists but is either a kernel thread or belongs to another
			// process!
			break;
		};
		thread = thread->next;
	} while (thread != currentThread);
	
	unlockSched();
	sti();
	spinlockRelease(&notifLock);
	
	// not found
	return ESRCH;
};

int signalThid(int thid, int sig)
{
	if ((sig < 0) || (sig >= SIG_NUM))
	{
		return EINVAL;
	};
	
	siginfo_t si;
	memset(&si, 0, sizeof(siginfo_t));
	si.si_signo = sig;
	cli();
	lockSched();
	
	Thread *thread = currentThread;
	do
	{
		if (thread->thid == thid)
		{
			if (thread->creds != NULL)
			{
				if (thread->creds->pid == currentThread->creds->pid)
				{
					sendSignal(thread, &si);
					unlockSched();
					sti();
					return 0;
				};
			};
			
			// we know the thread exists but is either a kernel thread or belongs to another
			// process!
			break;
		};
		thread = thread->next;
	} while (thread != currentThread);
	
	unlockSched();
	sti();
	return ESRCH;
};

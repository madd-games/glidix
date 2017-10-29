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
#include <glidix/pagetab.h>
#include <glidix/msr.h>
#include <glidix/signal.h>
#include <glidix/trace.h>
#include <glidix/ftree.h>

Thread firstThread;
PER_CPU Thread *currentThread;		// don't make it static; used by syscall.asm
static PER_CPU Thread *idleThread;

static Spinlock schedLock;
static int nextPid;
static int closingPid;

extern uint64_t getFlagsRegister();
void kmain2();
uint32_t quantumTicks;			// initialised by init.c, how many APIC timer ticks to do per process.
extern PER_CPU TSS* localTSSPtr;

static Spinlock expireLock;
static Thread *threadSysMan;

static Spinlock notifLock;
static SchedNotif *firstNotif;

static RunqueueEntry *runqFirst[NUM_PRIO_Q];
static RunqueueEntry *runqLast[NUM_PRIO_Q];

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
			kprintf("%c", info->symbol);
		}
		else
		{
			kprintf("%c", info->symbol-'A'+'a');
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
		kprintf("%d\t%d\t%d\t%s", i++, pid, ppid, name);
		printThreadFlags(th->flags);
		if (th->regs.cs == 8)
		{
			kprintf("K");
		}
		else
		{
			kprintf("k");
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
	creds->ps.ps_ticks = 0;
	creds->ps.ps_entries = 0;
	creds->ps.ps_quantum = quantumTicks;
	return creds;
};

Creds *credsDup(Creds *old)
{
	Creds *new = NEW(Creds);
	memcpy(new, old, sizeof(Creds));
	new->refcount = 1;
	new->ps.ps_ticks = 0;
	new->ps.ps_entries = 0;
	new->ps.ps_quantum = quantumTicks;
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

					// get our children out of debugging mode
					if (thread->flags & THREAD_TRACED)
					{
						thread->debugFlags = 0;
						thread->flags &= ~THREAD_TRACED;
						thread->flags |= THREAD_WAITING;	// to force signalThread to queue it
						signalThread(thread);
					};
			
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
		signalPidEx(parentPid, &siginfo, SP_NOPERM);
		
		// wake the parent
		wakeProcess(parentPid);
		
		kfree(creds);
	};
};

static void startupThread()
{
	DONE();
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
	firstThread.regs.fsbase = 0;
	firstThread.regs.gsbase = 0;
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
	//firstThread.wakeTime = 0;
	
	// do not send alarms
	firstThread.alarmTime = 0;
	timedPost(&firstThread.alarmTimer, 0);
	
	// signal stuff
	firstThread.sigdisp = NULL;
	firstThread.pendingSet = 0;
	firstThread.sigmask = 0;
	
	firstThread.wakeCounter = 0;
	
	// permissions
	firstThread.oxperm = XP_ALL;
	firstThread.dxperm = XP_ALL;
	
	// linking
	firstThread.prev = &firstThread;
	firstThread.next = &firstThread;

	// normal priority
	firstThread.niceVal = 0;
	
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
					
					if (threadFound->flags & THREAD_DETACHED)
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
				if (threadFound->creds != NULL)
				{
					closingPid = threadFound->creds->pid;
				};
				
				kfree(threadFound->stack);
				if (threadFound->pm != NULL) vmDown(threadFound->pm);
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

static void idleThreadFunc()
{
	while (1)
	{
		while (cpuSleeping())
		{
			hlt();
		};
		
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
	
	// the idle thread needs 2 things: the registers and flags indicating
	// it is always waiting. everthing else can be uninitialized
	idleThread = NEW(Thread);
	memset(idleThread, 0, sizeof(Thread));
	idleThread->regs.fsbase = 0;
	idleThread->regs.gsbase = 0;
	idleThread->regs.cs = 0x08;
	idleThread->regs.ds = 0x10;
	idleThread->regs.ss = 0;
	idleThread->regs.rsp = (uint64_t) kmalloc(0x4000) + 0x4000 - 8;
	idleThread->regs.rip = (uint64_t) &idleThreadFunc;
	idleThread->regs.rbp = 0;
	idleThread->flags = THREAD_WAITING;
};

void initSchedAP()
{
	// the idle thread needs 2 things: the registers and flags indicating
	// it is always waiting. everthing else can be uninitialized
	idleThread = NEW(Thread);
	memset(idleThread, 0, sizeof(Thread));
	idleThread->regs.fsbase = 0;
	idleThread->regs.gsbase = 0;
	idleThread->regs.cs = 0x08;
	idleThread->regs.ds = 0x10;
	idleThread->regs.ss = 0;
	idleThread->regs.rsp = (uint64_t) kmalloc(0x4000) + 0x4000 - 8;
	idleThread->regs.rip = (uint64_t) &idleThreadFunc;
	idleThread->regs.rbp = 0;
	idleThread->flags = THREAD_WAITING;

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
	localTSSPtr->rsp0 = ((uint64_t) currentThread->stack + currentThread->stackSize) & ~0xFUL;
	currentThread->syscallStackPointer = ((uint64_t) currentThread->stack + currentThread->stackSize) & ~0xFUL;
	msrWrite(MSR_KERNEL_GS_BASE, (uint64_t) currentThread);
	
	// reload the TSS
	reloadTR();

	// switch address space
	if (currentThread->pm != NULL) vmSwitch(currentThread->pm);

	// make sure IF is set
	currentThread->regs.rflags |= (1 << 9);

	// switch context
	fpuLoad(&currentThread->fpuRegs);
	apic->timerInitCount = quantumTicks;
	switchContext(&currentThread->regs);
};

void lockSched()
{
	if (getFlagsRegister() & (1 << 9))
	{
		stackTraceHere();
		panic("lockSched() called with interrupts enabled");
	};
	
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
	if (thread->flags & THREAD_NOSCHED) return 0;
	return 1;
};

static int getPrio(Thread *thread)
{
	return (NUM_PRIO_Q/2) + thread->niceVal;
};

void switchTaskUnlocked(Regs *regs)
{
	// get number of ticks used
	uint64_t ticks = quantumTicks - apic->timerCurrentCount;
	currentThread->ps.ps_ticks += ticks;
	currentThread->ps.ps_entries++;
	
	// wake up
	cpuBusy();
	
	// update process statistics if attached
	if (currentThread->creds != NULL)
	{
		currentThread->creds->ps.ps_ticks += ticks;
		currentThread->creds->ps.ps_entries++;
	};
	
	// remember the context of this thread.
	fpuSave(&currentThread->fpuRegs);
	memcpy(&currentThread->regs, regs, sizeof(Regs));

	// put the current thread back into the queue if still running
	if (canSched(currentThread))
	{
		if (currentThread->runq.thread == NULL)
		{
			currentThread->runq.thread = currentThread;
			currentThread->runq.next = NULL;
			if (runqLast[getPrio(currentThread)] == NULL)
			{
				runqFirst[getPrio(currentThread)] = runqLast[getPrio(currentThread)] = &currentThread->runq;
			}
			else
			{
				runqLast[getPrio(currentThread)]->next = &currentThread->runq;
				runqLast[getPrio(currentThread)] = &currentThread->runq;
				cpuDispatch();
			};
		};
	};

	// wait for the next thread to execute
	int i;
	for (i=0; i<NUM_PRIO_Q; i++)
	{
		if (runqFirst[i] != NULL)
		{
			break;
		};
	};
	
	if (i == NUM_PRIO_Q)
	{
		// no thread waiting; go idle
		cpuReady();
		currentThread = idleThread;
	}
	else
	{	
		currentThread = runqFirst[i]->thread;
		runqFirst[i] = runqFirst[i]->next;
		if (runqFirst[i] == NULL) runqLast[i] = NULL;
		currentThread->runq.thread = NULL;
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
			if (((thread->sigmask & sigbit) == 0) || isUnblockableSig(thread->pendingSigs[i].si_signo))
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

	if (currentThread->flags & THREAD_DETACHED)
	{
		spinlockAcquire(&sysManLock);
		cli();
		lockSched();
		timedCancel(&currentThread->alarmTimer);
		currentThread->flags |= THREAD_TERMINATED;
		numThreadsToClean++;
		spinlockRelease(&sysManLock);
		signalThread(threadSysMan);
		Regs regs;
		switchTaskUnlocked(&regs);
	}
	else
	{
		// we need to do all of this with interrupts disabled. we remove ourselves from the runqueue,
		// but do not free the stack nor the thread description; this will be done by ReleaseKernelThread()
		cli();
		Regs regs;
		lockSched();
		currentThread->prev->next = currentThread->next;
		currentThread->next->prev = currentThread->prev;
		currentThread->flags |= THREAD_TERMINATED;
		switchTaskUnlocked(&regs);
	};
};

void ReleaseKernelThread(Thread *thread)
{
	// busy-wait until the thread terminates
	while ((thread->flags & THREAD_TERMINATED) == 0)
	{
		__sync_synchronize();
		kyield();
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
	memset(thread, 0, sizeof(Thread));
	thread->catchRegs[7] = 0;
	thread->stack = kmalloc(stackSize);
	thread->stackSize = stackSize;

	memset(&thread->fpuRegs, 0, 512);
	memset(&thread->regs, 0, sizeof(Regs));
	thread->regs.rip = (uint64_t) entry;
	thread->regs.rsp = (((uint64_t) thread->stack + thread->stackSize) & ~0xF) - 8;
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
	timedPost(&thread->alarmTimer, 0);
	
	thread->sigdisp = NULL;
	thread->pendingSet = 0;
	thread->sigmask = 0;

	thread->wakeCounter = 0;
	
	thread->oxperm = XP_ALL;
	thread->dxperm = XP_ALL;
	
	// start all kernel threads in "/initrd"
	strcpy(thread->cwd, "/initrd");

	// no errnoptr
	thread->errnoptr = NULL;

	// this will simulate a call from kernelThreadExit() to "entry()"
	// this is so that when entry() returns, the thread can safely exit.
	thread->regs.rdi = (uint64_t) data;
	*((uint64_t*)thread->regs.rsp) = (uint64_t) &kernelThreadExit;
	
	// normal priority by default
	thread->niceVal = 0;
	
	// zero statistics
	thread->ps.ps_ticks = 0;
	thread->ps.ps_entries = 0;
	thread->ps.ps_quantum = quantumTicks;
	
	// doesn't block on anything
	thread->blockPhys = 0;
	
	// no debugging
	thread->debugFlags = 0;

	// link into the runqueue
	cli();
	lockSched();
	currentThread->next->prev = thread;
	thread->next = currentThread->next;
	thread->prev = currentThread;
	currentThread->next = thread;
	
	if (canSched(thread))
	{
		thread->runq.thread = thread;
		thread->runq.next = NULL;
		if (runqLast[NUM_PRIO_Q/2 - 1] == NULL)
		{
			runqFirst[NUM_PRIO_Q/2 - 1] = runqLast[NUM_PRIO_Q/2 - 1] = &thread->runq;
		}
		else
		{
			runqLast[NUM_PRIO_Q/2 - 1]->next = &thread->runq;
			runqLast[NUM_PRIO_Q/2 - 1] = &thread->runq;
		};
	};
	
	// there is no need to update currentThread->prev, it will only be broken for the init
	// thread, which never exits, and therefore its prev will never need to be valid.
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
	if (thread->wakeCounter == 0)
	{
		thread->flags |= THREAD_WAITING;
	}
	else
	{
		thread->wakeCounter--;
	};
};

int signalThread(Thread *thread)
{
	if (thread->alarmTime != 0)
	{
		uint64_t currentTime = getNanotime();
		if (currentTime >= thread->alarmTime)
		{
			timedCancel(&thread->alarmTimer);
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

	if (thread->flags & THREAD_WAITING)
	{
		thread->flags &= ~THREAD_WAITING;

		if (thread->runq.thread == NULL)
		{
			thread->runq.thread = thread;
			thread->runq.next = NULL;
			if (runqLast[getPrio(thread)] == NULL)
			{
				runqFirst[getPrio(thread)] = runqLast[getPrio(thread)] = &thread->runq;
			}
			else
			{
				runqLast[getPrio(thread)]->next = &thread->runq;
				runqLast[getPrio(thread)] = &thread->runq;
			};
		};
		
		cpuDispatch();
	}
	else
	{
		thread->wakeCounter++;
	};

	return thread->niceVal < currentThread->niceVal;
};

int threadClone(Regs *regs, int flags, MachineState *state)
{
	Thread *thread = (Thread*) kmalloc(sizeof(Thread));
	memset(thread, 0, sizeof(Thread));
	thread->catchRegs[7] = 0;
	fpuSave(&thread->fpuRegs);
	memcpy(&thread->regs, regs, sizeof(Regs));

	if (state != NULL)
	{
		memcpy(&thread->fpuRegs, &state->fpuRegs, 512);
		thread->regs.fsbase = state->fsbase;
		thread->regs.gsbase = state->gsbase;
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
		vmUp(currentThread->pm);
		thread->pm = currentThread->pm;
		
		sigdispUpref(currentThread->sigdisp);
		thread->sigdisp = currentThread->sigdisp;
	}
	else
	{
		thread->pm = vmClone();
		
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
	cli();
	spinlockAcquire(&schedLock);
	if ((flags & CLONE_THREAD) == 0)
	{
		thread->creds->pid = nextPid++;
	};
	thread->thid = nextPid++;
	spinlockRelease(&schedLock);
	sti();
	
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
	
	// clear pending signals
	thread->pendingSet = 0;
	
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
	//thread->wakeTime = 0;
	
	// alarms shall be cancelled in the new process/thread
	thread->alarmTime = 0;
	timedPost(&thread->alarmTimer, 0);
	
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

	thread->wakeCounter = 0;
	
	thread->oxperm = currentThread->oxperm;
	thread->dxperm = currentThread->dxperm;
	
	// inherit priority
	thread->niceVal = currentThread->niceVal;
	
	// initialize statistics
	thread->ps.ps_ticks = 0;
	thread->ps.ps_entries = 0;
	thread->ps.ps_quantum = quantumTicks;
	
	// initialize block address
	thread->blockPhys = 0;

	// stop-on-exec if we are being spawned by a debugger
	thread->debugFlags = 0;
	if (currentThread->debugFlags & DBG_DEBUGGER)
	{
		thread->debugFlags = DBG_STOP_ON_EXEC | DBG_DEBUG_MODE;
	};

	// link into the runqueue
	cli();
	spinlockAcquire(&schedLock);

	currentThread->next->prev = thread;
	thread->next = currentThread->next;
	thread->prev = currentThread;
	currentThread->next = thread;

	thread->runq.thread = thread;
	thread->runq.next = NULL;
	if (runqLast[getPrio(thread)] == NULL)
	{
		runqFirst[getPrio(thread)] = runqLast[getPrio(thread)] = &thread->runq;
	}
	else
	{
		runqLast[getPrio(thread)]->next = &thread->runq;
		runqLast[getPrio(thread)] = &thread->runq;
	};

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

	if (currentThread->debugFlags & DBG_DEBUG_MODE)
	{
		siginfo_t si;
		memset(&si, 0, sizeof(siginfo_t));
		si.si_signo = SIGTRACE;
		si.si_pid = currentThread->thid;
		si.si_code = TR_EXIT;
		signalPidEx(currentThread->creds->ppid, &si, SP_NOPERM);
	};
	
	vmUnmapThread();
	
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
	timedCancel(&currentThread->alarmTimer);
	currentThread->flags |= THREAD_TERMINATED;
	numThreadsToClean++;
	spinlockRelease(&sysManLock);
	signalThread(threadSysMan);
	Regs regs;
	switchTaskUnlocked(&regs);
};

void processExit(int status)
{
	if (currentThread->creds->pid == 1)
	{
		panic("init terminated with status %d", status);
	};
	
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

static int canSendSignal(Thread *src, Thread *dst, int signo, int flags)
{
	if ((signo == SIGTRACE) && (flags & SP_NOPERM)) return 1;

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

	if (flags & SP_NOPERM)
	{
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

static int signalPidUnlocked(int pid, siginfo_t *si, int flags)
{
	// keep track of which pids we've already successfully delivered signals to
	// in this call.
	int pidsDelivered[100];
	int pidDelivIndex = 0;
	
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
					if (!canSendSignal(currentThread, thread, si->si_signo, flags))
					{
						ERRNO = EPERM;
						thread = thread->next;
						continue;
					};
			
					if (si != NULL)
					{
						if (sendSignalEx(thread, si, SS_NONBLOCKED) == 0)
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
	
	return result;
};

int signalPidEx(int pid, siginfo_t *si, int flags)
{
	cli();
	lockSched();
	
	int result = signalPidUnlocked(pid, si, flags);
	
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
		return signalPidEx(pid, NULL, 0);
	};
	
	siginfo_t si;
	memset(&si, 0, sizeof(siginfo_t));
	si.si_signo = sig;
	
	if (currentThread->creds != NULL)
	{
		si.si_pid = currentThread->creds->pid;
		si.si_uid = currentThread->creds->ruid;
	};
	
	return signalPidEx(pid, &si, 0);
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

void switchToKernelSpace(Regs *regs)
{
	regs->cs = 0x08;
	regs->ds = 0x10;
	regs->ss = 0x00;
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

void suspendOtherThreads()
{
	int total = 0;
	int suspended = 0;
	
	siginfo_t si;
	si.si_signo = SIGTHSUSP;
	si.si_addr = &suspended;
	
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
					total++;
					sendSignal(thread, &si);
				};
			};
		};
		thread = thread->next;
	} while (thread != currentThread);
	
	unlockSched();
	sti();
	
	while (suspended < total) kyield();
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

int havePerm(uint64_t xperm)
{
	if (currentThread->creds == NULL)
	{
		// the kernel can do anything
		return 1;
	};
	
	if (currentThread->creds->euid == 0)
	{
		// root can do anything right now
		// TODO: removing the concept of superuser
		return 1;
	};
	
	return currentThread->oxperm & xperm;
};

int thnice(int incr)
{
	cli();
	lockSched();
	
	int newVal = currentThread->niceVal + incr;
	if (newVal <= -(NUM_PRIO_Q/2))
	{
		newVal = -(NUM_PRIO_Q/2) + 1;
	};
	
	if (newVal >= (NUM_PRIO_Q/2))
	{
		newVal = (NUM_PRIO_Q/2) - 1;
	};
	
	currentThread->niceVal = newVal;
	
	unlockSched();
	if (incr > 0)
	{
		// we dropped priority
		kyield();
	};
	sti();
		
	return newVal;
};

uint64_t mapTempFrame(uint64_t frame)
{
	uint64_t virt = (uint64_t) tmpframe();
	PTe *pte = (PTe*) ((virt >> 9) | 0xFFFFFF8000000000UL);
	
	uint64_t old = pte->framePhysAddr;
	pte->framePhysAddr = frame;
	
	invlpg(tmpframe());
	return old;
};

void* tmpframe()
{
	uint64_t virt = (uint64_t) currentThread->stack;
	virt += 0x1000 - (virt & 0xFFF);		// page-align forward
	return (void*) virt;
};

void traceTrapEx(Regs *regs, int reason, int value)
{
	siginfo_t si;
	memset(&si, 0, sizeof(siginfo_t));
	si.si_signo = SIGTRACE;
	si.si_pid = currentThread->thid;
	si.si_code = reason;
	si.si_value.sival_int = value;
	
	signalPidUnlocked(currentThread->creds->ppid, &si, SP_NOPERM);
	currentThread->flags |= THREAD_TRACED;
	switchTaskUnlocked(regs);
};

void traceTrap(Regs *regs, int reason)
{
	cli();
	lockSched();
	traceTrapEx(regs, reason, 0);
};

void detachMe()
{
	cli();
	lockSched();
	getCurrentThread()->flags |= THREAD_DETACHED;
	unlockSched();
	sti();
};

int getClosingPid()
{
	if (currentThread == threadSysMan)
	{
		return closingPid;
	}
	else if (currentThread->creds == NULL)
	{
		return 0;
	}
	else
	{
		return currentThread->creds->pid;
	};
};

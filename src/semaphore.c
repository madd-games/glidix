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

#include <glidix/semaphore.h>
#include <glidix/spinlock.h>
#include <glidix/common.h>
#include <glidix/memory.h>
#include <glidix/console.h>
#include <glidix/time.h>

void semInit(Semaphore *sem)
{
	semInit2(sem, 1);
};

void semInit2(Semaphore *sem, int count)
{
	spinlockRelease(&sem->lock);
	spinlockRelease(&sem->countLock);
	sem->lastHolder = NULL;
	sem->waiter = NULL;
	sem->count = count;
	sem->first = NULL;
	sem->last = NULL;
	sem->countWaiter = NULL;
	sem->terminated = 0;
};

extern uint64_t getFlagsRegister();
void semWait(Semaphore *sem)
{
	if ((getFlagsRegister() & (1 << 9)) == 0)
	{
		stackTraceHere();
		panic("semWait when IF=0");
	};

	spinlockAcquire(&sem->lock);
	Thread *me = getCurrentThread();

	//if (sem->owner != NULL)
	if ((sem->count == 0) || (sem->first != NULL))
	{
		SemWaitThread *thwait = (SemWaitThread*) kmalloc(sizeof(SemWaitThread));
		thwait->thread = me;
		thwait->next = NULL;
		thwait->waiting = 1;

		if (sem->last == NULL)
		{
			sem->first = thwait;
			sem->last = thwait;
		}
		else
		{
			sem->last->next = thwait;
			sem->last = thwait;
		};

		// wait without hanging up the whole system
		while (thwait->waiting)
		{
			ASM("cli");
			//getCurrentThread()->flags |= THREAD_WAITING;
			waitThread(getCurrentThread());
			spinlockRelease(&sem->lock);
			kyield();
			spinlockAcquire(&sem->lock);
		};
		kfree(thwait);
	}
	else
	{
		sem->count--;
	};

	if ((sem->count > 0) && (sem->first != NULL))
	{
		// make sure we signal the next thread so that it could get its share
		// of resources.
		SemWaitThread *thwait = sem->first;
		if (sem->last == thwait)
		{
			sem->last = NULL;
		};

		Thread *thread = thwait->thread;
		sem->first = thwait->next;
		thwait->waiting = 0;			// the waiter will free this

		sem->count--;
		signalThread(thread);
	};

	sem->lastHolder = me;
	spinlockRelease(&sem->lock);
};

int semWait2(Semaphore *sem, int count)
{
	spinlockAcquire(&sem->lock);
	if (sem->countWaiter != NULL)
	{
		spinlockRelease(&sem->lock);
		return 0;
	};


	while (sem->count == 0)
	{
		//kprintf("try again\n");
		sem->countWaiter = getCurrentThread();
		ASM("cli");
		//getCurrentThread()->flags |= THREAD_WAITING;
		waitThread(getCurrentThread());
		spinlockRelease(&sem->lock);
		kyield();
		spinlockAcquire(&sem->lock);
		if (wasSignalled())
		{
			sem->countWaiter = NULL;
			spinlockRelease(&sem->lock);
			return -1;
		};
		//kprintf("MAYBE? COUNT=%d\n", sem->count);
	};

	sem->countWaiter = NULL;
	spinlockRelease(&sem->lock);
	
	spinlockAcquire(&sem->countLock);
	spinlockRelease(&sem->countLock);

	spinlockAcquire(&sem->lock);
	if (sem->count < count) count = sem->count;
	spinlockRelease(&sem->lock);

	int out = 0;
	//kprintf("wait %d\n", count);
	while (count--)
	{
		semWait(sem);
		out++;
	};

	return out;
};

int semWaitTimeout(Semaphore *sem, int count, uint64_t timeout)
{
	int deadline = 0;
	if (timeout != 0)
	{
		deadline = getUptime() + (int) (timeout/1000000);
	};
	
	spinlockAcquire(&sem->lock);
	if (sem->countWaiter != NULL)
	{
		spinlockRelease(&sem->lock);
		return 0;
	};

	while (sem->count == 0)
	{
		if (sem->terminated)
		{
			getCurrentThread()->wakeTime = 0;
			sem->countWaiter = NULL;
			spinlockRelease(&sem->lock);
			return 0;
		};
		
		sem->countWaiter = getCurrentThread();
		cli();
		lockSched();
		getCurrentThread()->wakeTime = deadline;
		waitThread(getCurrentThread());
		spinlockRelease(&sem->lock);
		unlockSched();
		kyield();
		spinlockAcquire(&sem->lock);
		if (wasSignalled())
		{
			sem->countWaiter = NULL;
			spinlockRelease(&sem->lock);
			return SEM_INTERRUPT;
		};
		if ((getUptime() >= deadline) && (deadline != 0))
		{
			sem->countWaiter = NULL;
			spinlockRelease(&sem->lock);
			return SEM_TIMEOUT;
		};
	};

	getCurrentThread()->wakeTime = 0;
	
	spinlockRelease(&sem->lock);
	sem->countWaiter = NULL;
	spinlockAcquire(&sem->countLock);
	spinlockRelease(&sem->countLock);

	spinlockAcquire(&sem->lock);
	if (sem->count < count) count = sem->count;
	spinlockRelease(&sem->lock);

	int out = 0;
	while (count--)
	{
		semWait(sem);
		out++;
	};

	return out;
};

int semWaitNoblock(Semaphore *sem, int count)
{
	spinlockAcquire(&sem->lock);
	if (sem->countWaiter != NULL)
	{
		spinlockRelease(&sem->lock);
		return 0;
	};

	int status = 0;

	if (sem->count == 0)
	{
		// no resources available, return -1 indicating EWOULDBLOCK, unless it's terminated
		if (sem->terminated)
		{
			spinlockRelease(&sem->lock);
			return 0;
		};
		
		status = -1;
	};

	spinlockRelease(&sem->lock);
	sem->countWaiter = NULL;
	spinlockAcquire(&sem->countLock);
	spinlockRelease(&sem->countLock);

	spinlockAcquire(&sem->lock);
	if (sem->count < count) count = sem->count;
	spinlockRelease(&sem->lock);

	if (status == -1)
	{
		return -1;
	};

	int out = 0;
	while (count--)
	{
		semWait(sem);
		out++;
	};

	return out;
};

static void semSignalGen(Semaphore *sem, int wait)
{
	if ((getFlagsRegister() & (1 << 9)) == 0)
	{
		panic("semSignal when IF=0");
	};

	spinlockAcquire(&sem->lock);
	if (sem->countWaiter != NULL)
	{
		signalThread(sem->countWaiter);
	};

	sem->count++;
	if ((sem->first != NULL) && (sem->count == 1))
	{
		SemWaitThread *thwait = sem->first;
		if (sem->last == thwait)
		{
			sem->last = NULL;
		};

		Thread *thread = thwait->thread;
		sem->first = thwait->next;
		thwait->waiting = 0;			// the waiter will free this

		sem->count--;
		signalThread(thread);
	};

	if (wait)
	{
		cli();
		lockSched();
		waitThread(getCurrentThread());
		spinlockRelease(&sem->lock);
		unlockSched();
		kyield();
	}
	else
	{
		spinlockRelease(&sem->lock);
	};
};

void semTerminate(Semaphore *sem)
{
	kprintf_debug("semTerminate was called\n");
	spinlockAcquire(&sem->lock);
	sem->terminated = 1;
	if (sem->countWaiter != NULL) signalThread(sem->countWaiter);
	spinlockRelease(&sem->lock);
};

void semSignal(Semaphore *sem)
{
	semSignalGen(sem, 0);
};

void semSignalAndWait(Semaphore *sem)
{
	semSignalGen(sem, 1);
};

void semSignal2(Semaphore *sem, int count)
{
	spinlockAcquire(&sem->countLock);
	while (count--) semSignal(sem);
	spinlockRelease(&sem->countLock);
};

uint64_t getFlagsRegister();
void semDump(Semaphore *sem)
{
	uint64_t shouldSTI = getFlagsRegister() & (1 << 9);
	ASM("cli");
	if (sem->lastHolder == NULL)
	{
		kprintf_debug("LAST HOLDER: NONE\n");
	}
	else
	{
		kprintf_debug("LAST HOLDER: %s\n", sem->lastHolder->name);
	};
	kprintf_debug("I AM: %s\n", getCurrentThread()->name);
	kprintf_debug("SEM ADDR: %a\n", sem);
	kprintf_debug("AVAIALBLE: %d\n", sem->count);
	kprintf_debug("QUEUE FOR SEMAPHORE:\n");

	SemWaitThread *thwait = sem->first;
	while (thwait != NULL)
	{
		kprintf_debug("%s\n", thwait->thread->name);
		thwait = thwait->next;
	};

	kprintf_debug("END OF QUEUE\n");
	if (shouldSTI) ASM("sti");
};

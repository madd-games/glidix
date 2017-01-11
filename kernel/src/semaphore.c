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

#include <glidix/semaphore.h>
#include <glidix/spinlock.h>
#include <glidix/common.h>
#include <glidix/memory.h>
#include <glidix/console.h>
#include <glidix/time.h>
#include <glidix/sched.h>

void semInit(Semaphore *sem)
{
	semInit2(sem, 1);
};

void semInit2(Semaphore *sem, int count)
{
	spinlockRelease(&sem->lock);
	sem->count = count;
	sem->terminated = 0;
	sem->first = NULL;
	sem->last = NULL;
};

int semWaitGen(Semaphore *sem, int count, int flags, uint64_t nanotimeout)
{
	if (count == 0)
	{
		return -EAGAIN;
	};
	
	uint64_t deadline;
	if (nanotimeout == 0)
	{
		deadline = 0;
	}
	else
	{
		deadline = getNanotime() + nanotimeout;
	};
	
	cli();
	spinlockAcquire(&sem->lock);
	
	if (count == -1)
	{
		if (sem->count == 0)
		{
			spinlockRelease(&sem->lock);
			sti();
			return -EAGAIN;
		}
		else
		{
			count = sem->count;
		};
	};
	
	SemWaitThread waiter;
	waiter.thread = getCurrentThread();
	waiter.give = 0;
	waiter.prev = waiter.next = NULL;
	
	while (sem->count == 0)
	{
		waiter.give = 0;
		waiter.prev = waiter.next = NULL;

		if (flags & SEM_W_NONBLOCK)
		{
			spinlockRelease(&sem->lock);
			sti();
			return -EAGAIN;
		};
		
		if (sem->first == NULL)
		{
			sem->first = sem->last = &waiter;
		}
		else
		{
			waiter.prev = sem->last;
			sem->last->next = &waiter;
			sem->last = &waiter;
		};
		
		lockSched();
		TimedEvent ev;
		timedPost(&ev, deadline);
		
		while (!waiter.give)
		{
			if ((getNanotime() >= deadline) && (deadline != 0))
			{
				timedCancel(&ev);
				unlockSched();
				
				if (waiter.prev != NULL) waiter.prev->next = waiter.next;
				if (waiter.next != NULL) waiter.next->prev = waiter.prev;
				if (sem->first == &waiter) sem->first = waiter.next;
				if (sem->last == &waiter) sem->last = waiter.prev;
				
				spinlockRelease(&sem->lock);
				sti();
				return -ETIMEDOUT;
			};
			
			if (flags & SEM_W_INTR)
			{
				if (haveReadySigs(getCurrentThread()))
				{
					timedCancel(&ev);
					unlockSched();
					
					if (waiter.prev != NULL) waiter.prev->next = waiter.next;
					if (waiter.next != NULL) waiter.next->prev = waiter.prev;
					if (sem->first == &waiter) sem->first = waiter.next;
					if (sem->last == &waiter) sem->last = waiter.prev;
					
					spinlockRelease(&sem->lock);
					sti();
					return -EINTR;
				};
			};
			
			waitThread(getCurrentThread());
			spinlockRelease(&sem->lock);
			unlockSched();
			kyield();
			
			cli();
			spinlockAcquire(&sem->lock);
			lockSched();
		};
		
		// it's our turn; and since waiter.give was set to 1, that means the thread which
		// "gave" the semaphore to us also removed us from the queue
		timedCancel(&ev);
		unlockSched();
	};

	int result;
	if (sem->count == -1)
	{
		// semaphore terminated
		result = 0;
	}
	else if (sem->count >= count)
	{
		sem->count -= count;
		result = count;
	}
	else
	{
		result = sem->count;
		sem->count = 0;
	};
	
	int doResched = 0;
	if (sem->count != 0)
	{
		// if there is another thread waiting, notify it
		if (sem->first != NULL)
		{
			Thread *thread = sem->first->thread;
			if (sem->first == sem->last) sem->last = NULL;
			sem->first->give = 1;
			sem->first = sem->first->next;
			if (sem->first != NULL) sem->first->prev = NULL;
			
			lockSched();
			doResched = doResched || signalThread(thread);
			unlockSched();
		};
	}
	else if (sem->terminated)
	{
		sem->count = -1;
	};
	
	spinlockRelease(&sem->lock);
	if (doResched) kyield();
	sti();
	return result;
};

void semWait(Semaphore *sem)
{
	int result = semWaitGen(sem, 1, 0, 0);
	if (result != 1)
	{
		stackTraceHere();
		panic("semWaitGen() did not return 1 in semWait(); it returned %d (count=%d, terminated=%d, first=%p, last=%p)", result, sem->count, sem->terminated, sem->first, sem->last);
	};
};

void semSignal(Semaphore *sem)
{
	semSignal2(sem, 1);
};

void semSignal2(Semaphore *sem, int count)
{
	if (count < 0)
	{
		stackTraceHere();
		panic("count must be zero or more; got %d", count);
	};
	
	if (count == 0) return;
	
	cli();
	spinlockAcquire(&sem->lock);
	
	if (sem->terminated)
	{
		// terminated
		spinlockRelease(&sem->lock);
		sti();
		return;
	};
	
	int doResched = 0;
	sem->count += count;
	if (sem->first != NULL)
	{
		Thread *thread = sem->first->thread;
		if (sem->first == sem->last) sem->last = NULL;
		sem->first->give = 1;
		sem->first = sem->first->next;
		if (sem->first != NULL) sem->first->prev = NULL;
		
		lockSched();
		doResched = signalThread(thread);
		unlockSched();
	};
	
	spinlockRelease(&sem->lock);
	if (doResched) kyield();
	sti();
};

void semTerminate(Semaphore *sem)
{
	cli();
	spinlockAcquire(&sem->lock);
	
	if (sem->terminated)
	{
		stackTraceHere();
		panic("attempted to terminate an already-terminated semaphore");
	};
	
	int doResched = 0;
	sem->terminated = 1;
	if (sem->count == 0)
	{
		sem->count = -1;
		if (sem->first != NULL)
		{
			Thread *thread = sem->first->thread;
			if (sem->first == sem->last) sem->last = NULL;
			sem->first->give = 1;
			sem->first = sem->first->next;
			if (sem->first != NULL) sem->first->prev = NULL;
			
			lockSched();
			doResched = doResched || signalThread(thread);
			unlockSched();
		};
	};
	
	spinlockRelease(&sem->lock);
	if (doResched) kyield();
	sti();
};

int semPoll(int numSems, Semaphore **sems, uint8_t *bitmap, int flags, uint64_t nanotimeout)
{
	// initialize our wait structures on our own stack.
	int i;
	SemWaitThread *waiters = (SemWaitThread*) kalloca(sizeof(SemWaitThread)*numSems);
	for (i=0; i<numSems; i++)
	{
		waiters[i].thread = getCurrentThread();
		waiters[i].give = 0;
		waiters[i].prev = NULL;
		waiters[i].next = NULL;
	};
	
	// disable interrupts before working on any semaphores!
	cli();
	
	// acquire all the spinlocks
	for (i=0; i<numSems; i++)
	{
		if (sems[i] != NULL)
		{
			spinlockAcquire(&sems[i]->lock);
		};
	};
	
	// see if any semaphores are ALREADY free (and set the appropriate bitmap bits if necessary)
	int numFreeSems = 0;
	for (i=0; i<numSems; i++)
	{
		if (sems[i] != NULL)
		{
			if (sems[i]->count != 0)
			{
				bitmap[i/8] |= (1 << (i%8));
				numFreeSems++;
			};
		};
	};
	
	if ((numFreeSems != 0) || (flags & SEM_W_NONBLOCK))
	{
		// either there are free semaphores, or we don't want to block
		// either way, release all spinlocks and return the result
		for (i=0; i<numSems; i++)
		{
			if (sems[i] != NULL)
			{
				spinlockRelease(&sems[i]->lock);
			};
		};
		
		sti();
		return numFreeSems;
	};
	
	// none of them are currently available, and blocking is OK; add us to the end of every
	// queue and then every time we are woken up, loop to see if any semaphores became free,
	// or we got interrupted
	uint64_t deadline;
	if (nanotimeout == 0)
	{
		deadline = 0;
	}
	else
	{
		deadline = getNanotime() + nanotimeout;
	};
	
	lockSched();
	TimedEvent ev;
	timedPost(&ev, deadline);
	
	for (i=0; i<numSems; i++)
	{
		if (sems[i] != NULL)
		{
			if (sems[i]->first == NULL)
			{
				sems[i]->first = sems[i]->last = &waiters[i];
			}
			else
			{
				waiters[i].prev = sems[i]->last;
				sems[i]->last->next = &waiters[i];
				sems[i]->last = &waiters[i];
			};
		};
	};
	
	// wait for at least one semaphore to be "given" to us
	// we both enter and leave the loop with all spinlocks acquired
	while (1)
	{
		if (numFreeSems > 0)
		{
			break;
		};
		
		if (haveReadySigs(getCurrentThread()))
		{
			numFreeSems = -EINTR;
			break;
		};
		
		if ((getNanotime() >= deadline) && (deadline != 0))
		{
			break;
		};
		
		for (i=0; i<numSems; i++)
		{
			if (sems[i] != NULL)
			{
				spinlockRelease(&sems[i]->lock);
			};
		};
		
		waitThread(getCurrentThread());
		unlockSched();
		kyield();
		
		cli();
		lockSched();
		
		for (i=0; i<numSems; i++)
		{
			if (sems[i] != NULL)
			{
				spinlockAcquire(&sems[i]->lock);
			};
		};
		
		for (i=0; i<numSems; i++)
		{
			if (sems[i] != NULL)
			{
				if (waiters[i].give)
				{
					bitmap[i/8] |= (1 << (i%8));
					numFreeSems++;
				};
			};
		};
	};
	
	timedCancel(&ev);
	
	int doResched = 0;
	// remove ourselves from all queues; and wake up any necessary threads!
	for (i=0; i<numSems; i++)
	{
		if (sems[i] != NULL)
		{
			if (waiters[i].give)
			{
				// we are already removed from this queue; but wake up the next thread
				// if necessary
				if (sems[i]->first != NULL)
				{
					Thread *thread = sems[i]->first->thread;
					sems[i]->first->give = 1;
					if (sems[i]->first == sems[i]->last) sems[i]->last = NULL;
					sems[i]->first = sems[i]->first->next;
					if (sems[i]->first != NULL) sems[i]->first->prev = NULL;
					
					doResched = doResched || signalThread(thread);
				};
			}
			else
			{
				if (waiters[i].prev != NULL) waiters[i].prev->next = waiters[i].next;
				if (waiters[i].next != NULL) waiters[i].next->prev = waiters[i].prev;
				if (sems[i]->first == &waiters[i]) sems[i]->first = waiters[i].next;
				if (sems[i]->last == &waiters[i]) sems[i]->last = waiters[i].prev;
			};
			
			// this time we can release the spinlock in the same loop; we won't be working on this
			// semaphore any more.
			spinlockRelease(&sems[i]->lock);
		};
	};
	
	unlockSched();
	if (doResched) kyield();
	sti();
	return numFreeSems;
};

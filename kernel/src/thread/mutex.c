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

#include <glidix/thread/mutex.h>
#include <glidix/display/console.h>
#include <glidix/util/string.h>
#include <glidix/thread/sched.h>

void mutexInit(Mutex *mutex)
{
	memset(mutex, 0, sizeof(Mutex));
};

void mutexLock(Mutex *mutex)
{
	if (kernelDead) return;
	
	// mutexes are unnecessary while we still have no multithreading working
	if (getCurrentThread() == NULL) return;
	
	// if we are the owner, just increment our lock count
	// note that no other thread would ever set 'owner' to our thread ID so this comparison
	// is thread-safe.
	if (mutex->owner == getCurrentThread())
	{
		mutex->numLocks++;
		return;
	};
	
	// take the lock
	uint64_t flags = getFlagsRegister();
	cli();
	spinlockAcquire(&mutex->lock);
	
	if (mutex->owner == NULL)
	{
		mutex->owner = getCurrentThread();
		mutex->numLocks = 1;
		spinlockRelease(&mutex->lock);
		setFlagsRegister(flags);
		return;
	};
	
	// couldn't immediately acquire, add us to the queue
	MutexWaiter waiter;
	waiter.thread = getCurrentThread();
	waiter.next = NULL;
	waiter.awaken = 0;
	
	if (mutex->first == NULL)
	{
		mutex->first = mutex->last = &waiter;
	}
	else
	{
		mutex->last->next = &waiter;
		mutex->last = &waiter;
	};
	
	while (!waiter.awaken)
	{
		waitThread(getCurrentThread());
		spinlockRelease(&mutex->lock);
		kyield();
		
		cli();
		spinlockAcquire(&mutex->lock);
	};
	
	// mutex->owner was set by the previous owner to us when they set awaken to 1
	// and also they removed out waiter from the queue
	spinlockRelease(&mutex->lock);
	setFlagsRegister(flags);
	
	mutex->numLocks = 1;
};

int mutexTryLock(Mutex *mutex)
{
	if (kernelDead) return 0;
	
	if (mutex->owner == getCurrentThread())
	{
		mutex->numLocks++;
		return 0;
	};
	
	cli();
	spinlockAcquire(&mutex->lock);
	
	if (mutex->owner == NULL)
	{
		mutex->owner = getCurrentThread();
		mutex->numLocks = 1;
		spinlockRelease(&mutex->lock);
		sti();
		return 0;
	};
	
	spinlockRelease(&mutex->lock);
	sti();
	return -1;
};

void mutexUnlock(Mutex *mutex)
{
	if (kernelDead) return;
	
	// mutexes are unnecessary while we still have no multithreading working
	if (getCurrentThread() == NULL) return;
	
	if (mutex->owner != getCurrentThread())
	{
		stackTraceHere();
		panic("attempted to unlock a mutex that does not belong to the calling thread");
	};
	
	if ((--mutex->numLocks) != 0)
	{
		// the mutex still belongs to us
		return;
	};
	
	uint64_t flags = getFlagsRegister();
	cli();
	spinlockAcquire(&mutex->lock);
	
	if (mutex->first != NULL)
	{
		mutex->first->awaken = 1;
		signalThread(mutex->first->thread);
		mutex->owner = mutex->first->thread;
		mutex->first = mutex->first->next;
	}
	else
	{
		mutex->owner = NULL;
	};
	
	spinlockRelease(&mutex->lock);
	setFlagsRegister(flags);
};

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

#include <glidix/mutex.h>
#include <glidix/memory.h>

void mutexInit(Mutex *mutex)
{
	spinlockRelease(&mutex->lock);
	mutex->owner = NULL;
	mutex->count = 0;
	mutex->queueHead = NULL;
	mutex->queueTail = NULL;
};

void mutexLock(Mutex *mutex)
{
	spinlockAcquire(&mutex->lock);
	
	if (mutex->owner == getCurrentThread())
	{
		// we already own it; recursive acquire
		mutex->count++;
	}
	else if (mutex->owner == NULL)
	{
		// we can acquire immediately
		mutex->owner = getCurrentThread();
		mutex->count = 1;
	}
	else
	{
		// add us to the mutex wait queue
		MutexWaiter *waiter = (MutexWaiter*) kmalloc(sizeof(MutexWaiter));
		waiter->thread = getCurrentThread();
		
		if (mutex->queueHead == NULL)
		{
			mutex->queueHead = mutex->queueTail = waiter;
			waiter->prev = waiter->next = NULL;
		}
		else
		{
			mutex->queueTail->next = waiter;
			waiter->prev = mutex->queueTail;
			waiter->next = NULL;
		};
		
		// loop until we acquire the mutex
		while (mutex->owner != getCurrentThread())
		{
			lockSched();
			getCurrentThread()->flags |= THREAD_WAITING;
			spinlockRelease(&mutex->lock);
			unlockSched();
			kyield();
			spinlockAcquire(&mutex->lock);
		};
		
		// and report a count of 1
		mutex->count = 1;
	};
	
	spinlockRelease(&mutex->lock);
};

void mutexUnlock(Mutex *mutex)
{
	spinlockAcquire(&mutex->lock);
	
	if (mutex->owner != getCurrentThread())
	{
		panic("thread %s attempted to release a mutex it does not own", getCurrentThread()->name);
	};
	
	if ((--mutex->count) == 0)
	{
		if (mutex->queueHead != NULL)
		{
			mutex->owner = mutex->queueHead->thread;
			MutexWaiter *toFree = mutex->queueHead;
			mutex->queueHead = mutex->queueHead->next;
			kfree(toFree);
			
			// wakey wakey
			lockSched();
			mutex->owner->flags &= ~THREAD_WAITING;
			unlockSched();
		}
		else
		{
			mutex->owner = NULL;
		};
	};
	
	spinlockRelease(&mutex->lock);
};

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

#include <glidix/mutex.h>
#include <glidix/console.h>
#include <glidix/string.h>

void mutexInit(Mutex *mutex)
{
	memset(mutex, 0, sizeof(Mutex));
};

void mutexLock(Mutex *mutex)
{
	// if we are the owner, just increment our lock count
	// note that no other thread would ever set 'owner' to our thread ID so this comparison
	// is thread-safe.
	if (mutex->owner == getCurrentThread())
	{
		mutex->numLocks++;
		return;
	};
	
	// wait until we are allowed to attempt lazy locking
	while (1)
	{
		uint16_t waiters = mutex->numQueue;
		if (waiters == MUTEX_MAX_QUEUE)
		{
			continue;
		};
		
		if (atomic_compare_and_swap16(&mutex->numQueue, waiters, waiters+1) == waiters)
		{
			break;
		};
	};
	
	// get a ticket and enter the queue
	uint16_t ticket = __sync_fetch_and_add(&mutex->cntEntry, 1) & (MUTEX_MAX_QUEUE-1);
	mutex->queue[ticket] = getCurrentThread();
	
	while ((mutex->cntExit & (MUTEX_MAX_QUEUE-1)) != ticket)
	{
		cli();
		lockSched();
		waitThread(getCurrentThread());
		unlockSched();
		kyield();
	};
	
	mutex->owner = getCurrentThread();
	mutex->queue[ticket] = NULL;
	mutex->numLocks = 1;
	__sync_fetch_and_add(&mutex->numQueue, -1);
};

void mutexUnlock(Mutex *mutex)
{
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
	
	mutex->owner = NULL;
	
	uint16_t nextToGo = __sync_add_and_fetch(&mutex->cntExit, 1) & (MUTEX_MAX_QUEUE-1);
	Thread *thread = mutex->queue[nextToGo];
	
	if (thread != NULL)
	{
		int doResched = 0;
		cli();
		lockSched();
		thread = mutex->queue[nextToGo];
		if (thread != NULL)
		{
			doResched = signalThread(thread);
		};
		unlockSched();
		if (doResched) kyield();
		sti();
	};
};

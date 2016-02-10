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

#ifndef __glidix_mutex_h
#define __glidix_mutex_h

/**
 * A mutex can be held by 1 thread at a time and can be locked recursively,
 * unlike a semaphore. The number of calls to mutexLock() and mutexUnlock()
 * must match.
 */

#include <glidix/common.h>
#include <glidix/sched.h>
#include <glidix/spinlock.h>

typedef struct MutexWaiter_
{
	Thread*				thread;
	struct MutexWaiter_*		prev;
	struct MutexWaiter_*		next;
} MutexWaiter;

typedef struct
{
	/**
	 * Synchronizes access to the mutex.
	 */
	Spinlock			lock;

	/**
	 * Current holder of the mutex (NULL means no holder), and the number of times
	 * they locked the mutex.
	 */
	Thread*				owner;
	int				count;
	
	/**
	 * Queue of threads waiting for the mutex.
	 */
	MutexWaiter*			queueHead;
	MutexWaiter*			queueTail;
} Mutex;

void mutexInit(Mutex *mutex);
void mutexLock(Mutex *mutex);
void mutexUnlock(Mutex *mutex);

#endif

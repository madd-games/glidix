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
 * Recursive mutexes.
 */

#include <glidix/common.h>
#include <glidix/sched.h>

/**
 * Number of threads that are allowed to go into a mutex queue at once. Must be power of
 * 2. When the queue is exhausted, busy-waiting will be employed so beware.
 */
#define	MUTEX_MAX_QUEUE					512

typedef struct
{
	/**
	 * Number of threads queing up.
	 */
	uint16_t					numQueue;
	
	/**
	 * Number of times the current owner has locked the mutex.
	 * BELONGS TO THE OWNER.
	 */
	uint16_t					numLocks;
	
	/**
	 * Entry and exit counters. 'cntExit' is written to ONLY by the unlocking
	 * thread.
	 */
	uint16_t					cntEntry;
	uint16_t					cntExit;
	
	/**
	 * Current owner.
	 */
	Thread*						owner;
	
	/**
	 * Queue of threads.
	 */
	Thread*						queue[MUTEX_MAX_QUEUE];
} Mutex;

void mutexInit(Mutex *mutex);
void mutexLock(Mutex *mutex);
void mutexUnlock(Mutex *mutex);

#endif

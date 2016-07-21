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

#ifndef __glidix_semaphore_h
#define __glidix_semaphore_h

/**
 * Implements semaphores for safe access to resources between threads.
 */

#include <glidix/spinlock.h>
#include <glidix/sched.h>

#define	SEM_INTERRUPT				-1
#define	SEM_TIMEOUT				-2

typedef struct _SemWaitThread
{
	Thread *thread;
	int waiting;
	struct _SemWaitThread *next;
} SemWaitThread;

typedef struct Semaphore_
{
	// for debugging only; DO NOT READ OUTSIDE OF semDump()
	// last thread to have acquired the semaphore.
	Thread *lastHolder;
	
	Spinlock lock;
	Thread *waiter;
	volatile int count;

	// the queue of threads waiting for this semaphore.
	SemWaitThread *first;
	SemWaitThread *last;

	// for semWait2/semSignal2, to keep some synchronisation of locks
	Thread *countWaiter;
	Spinlock countLock;
	
	// this is set to one by semTerminate()
	int terminated;
} Semaphore;

void semInit(Semaphore *sem);
void semInit2(Semaphore *sem, int count);
void semWait(Semaphore *sem);
int  semWait2(Semaphore *sem, int count);
int  semWaitNoblock(Semaphore *sem, int count);
void semSignal(Semaphore *sem);
void semSignal2(Semaphore *sem, int count);
void semSignalAndWait(Semaphore *sem);
void semDump(Semaphore *sem);
void semTerminate(Semaphore *sem);

/**
 * Wait for up to a specified number of resources with a timeout (given in nanoseconds). If the timeout is zero,
 * wait indefinitely. Returns a positive value, indicating the number of resources that became available, on success.
 * Returns 0 if the semaphore was terminated.
 * Otherwise returns one of the following negative values:
 *  - SEM_INTERRUPT		The operation was interrupted by the reception of a signal.
 *  - SEM_TIMEOUT		The operation has timed out.
 */
int semWaitTimeout(Semaphore *sem, int count, uint64_t timeout);

#endif

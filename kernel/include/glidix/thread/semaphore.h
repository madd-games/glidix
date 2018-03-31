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

#ifndef __glidix_semaphore_h
#define __glidix_semaphore_h

/**
 * Implements semaphores for safe access to resources between threads.
 */

#include <glidix/thread/spinlock.h>
#include <glidix/util/errno.h>

/**
 * When passed as a flag to semWaitGen(), causes it to return -EINTR if a signal arrives
 * before resources become available.
 */
#define	SEM_W_INTR				(1 << 0)

/**
 * When passed as a flag to semWaitGen(), causes it to return -EAGAIN if no resources are
 * available immediately.
 *
 * NOTE: This MUST have the same value as O_NONBLOCK in <glidix/vfs.h>
 */
#define	SEM_W_NONBLOCK				(1 << 8)

/**
 * This macro converts file descriptor flags (O_*) into flags appropriate for semWaitGen():
 * That is, SEM_W_INTR, and if O_NONBLOCK is passed, SEM_W_NONBLOCK.
 */
#define	SEM_W_FILE(oflag)			(((oflag) & (SEM_W_NONBLOCK)) | SEM_W_INTR)

/**
 * Semaphore flags (for the 'flags' field).
 */
#define	SEM_TERMINATED				(1 << 0)		/* terminated semaphore */
#define	SEM_DEBUG				(1 << 1)		/* debugged semaphore */

/**
 * Represents an entry in the semaphore wait queue.
 */
typedef struct SemWaitThread_
{
	/**
	 * The thread that is waiting.
	 */
	struct _Thread*				thread;
	
	/**
	 * This is initialized to 0 by the waiting thread, and set to 1 by a thread when it
	 * signals the semaphore.
	 */
	int					give;
	
	/**
	 * Previous and next element in the queue; it must be double-linked since a thread
	 * may give up waiting for resources.
	 */
	struct SemWaitThread_*			prev;
	struct SemWaitThread_*			next;
} SemWaitThread;

/**
 * The structure representing a semaphore. This may be allocated statically or using kmalloc().
 */
typedef struct Semaphore_
{
	/**
	 * The spinlock that protects the semaphore from multiple accesses by different CPUs.
	 * Interrupt masking protects from concurrency on the same CPU, and makes it priority-safe.
	 */
	Spinlock				lock;
	
	/**
	 * Number of resources currently available. A value of -1 means terminated and empty.
	 */
	int					count;
	
	/**
	 * Semaphore flags.
	 */
	int					flags;
	
	/**
	 * Queue of waiting threads.
	 */
	SemWaitThread*				first;
	SemWaitThread*				last;
} Semaphore;

/**
 * Initialize a semaphore. The contents of 'sem' are considered undefined before this call. It MUST NOT
 * be called on a semaphore that is in use, or already initialized. 'count' is the initial number of
 * resources on the semaphore; semInit() initializes this to 1, making the semaphore useable as a mutex,
 * initially unlocked.
 */
void semInit(Semaphore *sem);
void semInit2(Semaphore *sem, int count);

/**
 * Generic semaphore waiting function; used to implement all of the wait functions below. 'sem' names the
 * semaphore to wait on. 'count' is the maximum number of resources to wait for. 'flags' describes how to
 * wait (see the SEM_W_* macros above). 'nanotimeout' is the maximum amount of time, in nanoseconds, that
 * we should try waiting; 0 means infinity.
 *
 * If 'count' is 0, the function returns -AGAIN. If it is -1, this function returns all available resources;
 * or -EAGAIN if none are available.
 *
 * Returns the number of acquired resources on success (which may be zero if the semaphore was terminated
 * with a call to semTerminate()). It returns an error number converted to negative on error; for example
 * -EINTR. Possible errors:
 *
 * EINTR	The SEM_W_INTR flag was passed, and a signal arrived before any resourced became
 *		available.
 * ETIMEDOUT	A timeout was given and the time passed before any resources became available.
 * EAGAIN	The SEM_W_NONBLOCK flag was passed, and no resources are currently available.
 *
 * When waiting on a file read semaphore, the file desciptor flags (fp->oflag) must be taken into account,
 * and the function can be called as follows:
 *	int count = semWaitGen(semRead, (int) readSize, SEM_W_FILE(fp->oflag), timeout);
 * Which makes the function interruptable (SEM_W_INTR) and nonblocking if the O_NONBLOCK flag is in oflag.
 */
int semWaitGen(Semaphore *sem, int count, int flags, uint64_t nanotimeout);

/**
 * Simplification of the above; waits for a single resource and only returns on success. This is used when
 * a semaphore is being used as a mutual exclusion lock.
 *
 * WARNING: This will cause a kernel panic if used on a semaphore that can be terminated!
 */
void semWait(Semaphore *sem);

/**
 * Return resources to a semaphore (increment it by 'count'). semSignal() increments by 1.
 */
void semSignal(Semaphore *sem);
void semSignal2(Semaphore *sem, int count);

/**
 * Terminate a semaphore. It is not valid to signal it anymore after it is terminated; and, once all resourced
 * have been consumed, semWaitGen() will always return 0 when attempting to wait. This is used to mark "EOF" on
 * file read semaphores; e.g. when the write end of a pipe is closed.
 */
void semTerminate(Semaphore *sem);

/**
 * Poll a group of semaphores. 'numSems' specifies the number of semaphores in the array; 'sems' is an array of
 * pointers to semaphores. 'bitmap' points to a bitmap of results; this function never clears any bits, only sets
 * them if necessary. 'flags' and 'nanotimeout' are the same as for semWaitGen().
 *
 * This function waits for at least one semaphore in the list to become available (have a nonzero count). If
 * SEM_W_INTR is set, and NONE of the semaphores become available before delivery of a signal, returns -EINTR.
 * If SEM_W_NONBLOCK is set, and NONE of the semaphores are immediately available, returns 0. On success,
 * returns the number of semaphores that became free; 0 meaning the operation timed out before any semaphores
 * became free.
 *
 * For each semaphore that is free, this function sets a bit in the 'bitmap' before returning; that is, if
 * sems[n] become free, then testing the mask (1 << (n%8)) against bitmap[n/8] will return nonzero.
 *
 * This function does NOT change the count of any of the semaphores; you must still call semWaitGen() or semWait()
 * on a free semaphore to acquire it.
 *
 * A NULL pointer indicates a semaphore which NEVER becomes free.
 *
 * A terminated semaphore is considered free.
 *
 * Note that false positives are possible: for example, there may be a race condition where another thread acquires
 * all resources of a semaphore after semPoll() reported the semaphore free, but before the caller of semPoll()
 * called semWait() or semWaitGen(). To handle this case, semWaitGen() must be called with the SEM_W_NONBLOCK flag.
 */
int semPoll(int numSems, Semaphore **sems, uint8_t *bitmap, int flags, uint64_t nanotimeout);

/**
 * Switch the given semaphore to debug mode. This enables the debug terminal, and also causes semWait() and semSignal()
 * to print a stack trace and other information whenever they are called on this semaphore. Useful for debugging
 * deadlocks and stuff.
 *
 * NOTE: You must call this straight after semInit() or semInit2(), before any concurrency begins.
 */
void semDebug(Semaphore *sem);

#endif

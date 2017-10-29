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

#ifndef __glidix_rlock_h
#define __glidix_rlock_h

#include <glidix/common.h>
#include <glidix/semaphore.h>

/**
 * Types of locks.
 */
#define	RL_UNLOCK				0
#define	RL_READ					1
#define	RL_WRITE				2

/**
 * Structure forming a linked list of locked ranges.
 */
typedef struct LockedRange_
{
	/**
	 * Links.
	 */
	struct LockedRange_*			prev;
	struct LockedRange_*			next;
	
	/**
	 * Type of lock.
	 */
	int					type;
	
	/**
	 * The key (owner) of the lock.
	 */
	uint64_t				key;
	
	/**
	 * Beginning of the range.
	 */
	uint64_t				start;
	
	/**
	 * Number of units.
	 */
	uint64_t				size;
} LockedRange;

/**
 * Entry in the pending request queue.
 */
typedef struct PendingRangeRequest_
{
	/**
	 * Links.
	 */
	struct PendingRangeRequest_*		prev;
	struct PendingRangeRequest_*		next;
	
	/**
	 * Semaphore to be signalled when an unlock occurs, to allow the request
	 * to re-try.
	 */
	Semaphore				sem;
} PendingRangeRequest;

/**
 * Range lock.
 *
 * This is a synchronisation primitive, whose purpose is to provide mutual exclusion
 * over discrete ranges within an abstract ordered list to specific 'keys'. Each index
 * in the range can only be locked by one key at a time, and we typically lock whole
 * ranges of indices.
 */
typedef struct
{
	/**
	 * Lock for metadata.
	 */
	Semaphore				lock;
	
	/**
	 * List of locked ranges. Sorted by 'start'.
	 */
	LockedRange*				ranges;
	
	/**
	 * List of pending requests.
	 */
	PendingRangeRequest*			requests;
} RangeLock;

/**
 * Initialize a range lock. All units are initially unlocked.
 */
void rlInit(RangeLock *rl);

/**
 * Attempt to set a lock (read, write or unlock) on the given range, for the given key, in the
 * given range lock. Returns 0 if the lock was successfully set, or an error number on error.
 * The possible errors are:
 *
 *	EINTR		A signal has been made pending before the lock has been acquired.
 *	EAGAIN		'block' is 0 and the lock could not be immediately acquired.
 *	EDEADLK		Deadlock detected.
 *	EOVERFLOW	Values out of range.
 */
int rlSet(RangeLock *rl, int type, uint64_t key, uint64_t start, uint64_t size, int block);

/**
 * Get information about the first lock found which overlaps 'start', if attempt to re-lock it
 * would result in a block (or fail with a deadlock).
 *
 * On entry, 'type' points to a variable holding the type of lock (RL_*), and 'start' points to the
 * requested location. On exit, 'type' is set to the type of lock found, 'key' to its key, and 'start'
 * and 'size' to its range. If there is no lock, 'type' is set to RL_UNLOCK and nothing else happens.
 *
 * This function cannot fail.
 */
void rlGet(RangeLock *rl, int *type, uint64_t *key, uint64_t *start, uint64_t *size);

/**
 * Remove all locks with the given key.
 */
void rlReleaseKey(RangeLock *rl, uint64_t key);

#endif

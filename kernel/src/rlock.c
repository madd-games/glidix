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

#include <glidix/rlock.h>
#include <glidix/errno.h>
#include <glidix/memory.h>
#include <glidix/console.h>

void rlInit(RangeLock *rl)
{
	semInit(&rl->lock);
	rl->ranges = NULL;
	rl->requests = NULL;
};

static int rlUnlock(RangeLock *rl, uint64_t key, uint64_t start, uint64_t size)
{
	semWait(&rl->lock);
	
	// go through the list of locked ranges and remove the overlaps
	LockedRange *range = rl->ranges;
	while (range != NULL)
	{
		// do not do anything with ranges that don't belong to the key
		if (range->key != key)
		{
			range = range->next;
			continue;
		};
		
		// case 1: the range is completly engulfed by the unlock, so release it
		if (range->start >= start && (range->start+range->size) <= start+size)
		{
			if (range->prev != NULL) range->prev->next = range->next;
			if (range->next != NULL) range->next->prev = range->prev;
			if (rl->ranges == range) rl->ranges = range->next;

			LockedRange *next = range->next;
			kfree(range);
			range = next;
			continue;
		};
		
		// case 2: the lower part of the range is overlapped by the unlock, so push
		// it up
		if (range->start >= start && (range->start+range->size) >= start+size)
		{
			uint64_t delta = start + size - range->start;
			range->start += delta;
			range->size -= delta;
			range = range->next;
			continue;
		};
		
		// case 3: the upper part of the range is overlapped by the unlock, so cut it
		// off
		if (range->start <= start && (range->start+range->size) <= start+size)
		{
			uint64_t delta = range->start + range->size - start;
			range->size -= delta;
			range = range->next;
			continue;
		};
		
		// case 4: the unlock is in the middle of the range, split it into two parts
		if (range->start < start && (range->start+range->size) > start+size)
		{
			uint64_t rightSize = (range->start+range->size) - (start+size);
			range->size = start - range->start;
			
			LockedRange *right = NEW(LockedRange);
			right->next = range->next;
			right->prev = range;
			range->next = right;
			
			right->type = range->type;
			right->key = key;
			right->start = start+size;
			right->size = rightSize;
			
			range = right->next;
			continue;
		};
		
		// all other cases: nothing
		range = range->next;
	};
	
	// notify blocking requests to re-try
	while (rl->requests != NULL)
	{
		PendingRangeRequest *rq = rl->requests;
		rl->requests = rq->next;
		if (rl->requests != NULL) rl->requests->prev = NULL;
		
		rq->prev = NULL;
		rq->next = NULL;
		semSignal(&rq->sem);
	};
	
	semSignal(&rl->lock);
	return 0;
};

int rlSet(RangeLock *rl, int type, uint64_t key, uint64_t start, uint64_t size, int block)
{
	// check the range
	if (size > ~start)
	{
		return EOVERFLOW;
	};
	
	if (type == RL_UNLOCK)
	{
		return rlUnlock(rl, key, start, size);
	};
	
	while (1)
	{
		semWait(&rl->lock);
	
		// first see if there are no overlapping locks; if so, we can acquire immediately
		LockedRange *placeAfter = NULL;
		int found = 0;
		LockedRange *range;
		for (range=rl->ranges; range!=NULL; range=range->next)
		{
			if (range->start < start)
			{
				placeAfter = range;
			};
		
			if ((range->start+range->size) > start && range->start < (start+size))
			{
				// detect deadlock
				if (range->key == key)
				{
					semSignal(&rl->lock);
					return EDEADLK;
				};
				
				// only detect incompatible locks
				if (range->type == RL_WRITE || type == RL_WRITE)
				{
					found = 1;
					break;
				};
			};
		};
		
		if (!found)
		{
			// ok, we have acquired a lock, put us on the list
			range = NEW(LockedRange);
			range->type = type;
			range->key = key;
			range->start = start;
			range->size = size;
			
			if (placeAfter == NULL)
			{
				range->prev = NULL;
				range->next = rl->ranges;
				if (rl->ranges != NULL) rl->ranges->prev = range;
				rl->ranges = range;
			}
			else
			{
				range->prev = placeAfter;
				range->next = placeAfter->next;
				placeAfter->next = range;
				if (range->next != NULL) range->next->prev = range;
			};
			
			semSignal(&rl->lock);
			return 0;
		};
		
		// failed to acquire a lock; if non-blocking, fail
		if (!block)
		{
			semSignal(&rl->lock);
			return EAGAIN;
		};
		
		// blocking; add us to the request queue
		PendingRangeRequest *rq = NEW(PendingRangeRequest);
		semInit2(&rq->sem, 0);
		
		if (rl->requests == NULL)
		{
			rq->prev = rq->next = NULL;
			rl->requests = rq;
		}
		else
		{
			PendingRangeRequest *last = rl->requests;
			while (last->next != NULL) last = last->next;
			last->next = rq;
			rq->prev = last;
			rq->next = NULL;
		};
		
		// wait until someone unlocks
		semSignal(&rl->lock);
		
		int result = semWaitGen(&rq->sem, 1, SEM_W_INTR, 0);
		if (result == 1)
		{
			// the request was removed from the queue, and we can safely re-try
			kfree(rq);
			continue;
		};
		
		// we were interrupted by a signal; we must remove ourselves from the queue
		semWait(&rl->lock);
		if (rq->prev != NULL) rq->prev->next = rq->next;
		if (rq->next != NULL) rq->next->prev = rq->prev;
		if (rl->requests == rq) rl->requests = rq->next;
		semSignal(&rl->lock);
		
		kfree(rq);
		return EINTR;
	};
};

void rlGet(RangeLock *rl, int *type, uint64_t *key, uint64_t *start, uint64_t *size)
{
	semWait(&rl->lock);
	
	// default
	*type = RL_UNLOCK;
	
	LockedRange *range;
	for (range=rl->ranges; range!=NULL; range=range->next)
	{
		if (range->start >= (*start) && (range->start+range->size) < (*start))
		{
			if (range->type == RL_WRITE || (*type) == RL_WRITE)
			{
				*type = range->type;
				*key = range->key;
				*start = range->start;
				*size = range->size;
				break;
			};
		};
	};
	
	semSignal(&rl->lock);
};

void rlReleaseKey(RangeLock *rl, uint64_t key)
{
	rlUnlock(rl, key, 0, 0xFFFFFFFFFFFFFFFFUL);
};

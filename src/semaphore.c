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

#include <glidix/semaphore.h>
#include <glidix/spinlock.h>
#include <glidix/common.h>
#include <glidix/memory.h>

void semInit(Semaphore *sem)
{
	spinlockRelease(&sem->lock);
	sem->owner = NULL;
	sem->first = NULL;
	sem->last = NULL;
};

void semWait(Semaphore *sem)
{
	spinlockAcquire(&sem->lock);
	Thread *me = getCurrentThread();

	if (sem->owner != NULL)
	{
		if (sem->owner == me)
		{
			panic("a thread attempted to wait for a semaphore that it already acquired");
		};

		SemWaitThread *thwait = (SemWaitThread*) kmalloc(sizeof(SemWaitThread));
		thwait->thread = me;
		thwait->next = NULL;

		if (sem->last == NULL)
		{
			sem->first = thwait;
			sem->last = thwait;
		}
		else
		{
			sem->last->next = thwait;
			sem->last = thwait;
		};

		// wait without hanging up the whole system
		while (sem->owner != me)
		{
			spinlockRelease(&sem->lock);
			ASM("hlt");
			spinlockAcquire(&sem->lock);
		};
	}
	else
	{
		sem->owner = me;
	};

	spinlockRelease(&sem->lock);
};

void semSignal(Semaphore *sem)
{
	spinlockAcquire(&sem->lock);

	if (sem->first == NULL)
	{
		sem->owner = NULL;
	}
	else
	{
		SemWaitThread *thwait = sem->first;
		if (sem->last == thwait)
		{
			sem->last = NULL;
		};

		Thread *thread = thwait->thread;
		sem->first = thwait->next;
		kfree(thwait);

		sem->owner = thread;
	};

	spinlockRelease(&sem->lock);
};

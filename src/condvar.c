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

#include <glidix/condvar.h>
#include <glidix/time.h>
#include <glidix/memory.h>

void cvInit(CondVar *cv)
{
	spinlockRelease(&cv->lock);
	cv->value = 0;
	cv->waiters = NULL;
};

int cvWait(CondVar *cv, uint64_t nanotimeout)
{
	//int deadline = getTicks() + (int) (nanotimeout/1000000);
	uint64_t deadline = getNanotime() + nanotimeout;
	if (nanotimeout == 0)
	{
		deadline = 0;
	};
	
	spinlockAcquire(&cv->lock);
	int result = 0;
	if (!cv->value)
	{
		CondVarWaiter *waiter = NEW(CondVarWaiter);
		waiter->thread = getCurrentThread();
		waiter->next = NULL;
		
		if (cv->waiters == NULL)
		{
			cv->waiters = waiter;
		}
		else
		{
			CondVarWaiter *last = cv->waiters;
			while (last->next != NULL)
			{
				last = last->next;
			};
			
			last->next = waiter;
		};

		cli();
		lockSched();
		TimedEvent ev;
		timedPost(&ev, deadline);
		
		while (!cv->value)
		{
			waitThread(getCurrentThread());
			spinlockRelease(&cv->lock);
			unlockSched();
			kyield();
			
			spinlockAcquire(&cv->lock);
			cli();
			lockSched();
			
			if ((getNanotime() >= deadline) && (deadline > 0))
			{
				result = -1;
				break;
			};
		};
		
		unlockSched();
		sti();
	};
	
	spinlockRelease(&cv->lock);
	return result;
};

void cvSignal(CondVar *cv)
{
	spinlockAcquire(&cv->lock);
	if (!cv->value)
	{
		cv->value = 1;
		
		CondVarWaiter *waiter = cv->waiters;
		while (waiter != NULL)
		{
			signalThread(waiter->thread);
			CondVarWaiter *next = waiter->next;
			kfree(waiter);
			waiter = next;
		};
	};
	spinlockRelease(&cv->lock);
};

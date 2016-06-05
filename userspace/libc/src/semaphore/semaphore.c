/*
	Glidix Runtime

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

#include <semaphore.h>
#include <errno.h>
#include <stdlib.h>		/* abort */

int sem_init(sem_t *sem, int pshared, unsigned value)
{
	if (pshared)
	{
		errno = ENOSYS;
		return -1;
	};
	
	pthread_spin_unlock(&sem->spinlock);
	sem->value = value;
	sem->firstWaiter = NULL;
	sem->lastWaiter = NULL;
	
	return 0;
};

int sem_destroy(sem_t *sem)
{
	return 0;
};

int sem_wait(sem_t *sem)
{
	pthread_spin_lock(&sem->spinlock);
	
	if (sem->value > 0)
	{
		sem->value--;
		pthread_spin_unlock(&sem->spinlock);
		return 0;
	};
	
	// nothing currently available, put us on the waiters list
	__sem_waiter_t waiter;
	waiter.thread = pthread_self();
	waiter.complete = 0;			// waiting not finished
	waiter.next = NULL;
	
	if (sem->firstWaiter == NULL)
	{
		sem->firstWaiter = sem->lastWaiter = &waiter;
	}
	else
	{
		sem->lastWaiter->next = &waiter;
		sem->lastWaiter = &waiter;
	};

	while (!waiter.complete)
	{
		int status = _glidix_store_and_sleep(&sem->spinlock, 0);
		if (status != 0)
		{
			abort();
		};
		
		pthread_spin_lock(&sem->spinlock);
		__sync_synchronize();
	};
	
	// count remains set to 0, but we now own a single resource
	pthread_spin_unlock(&sem->spinlock);
	return 0;
};

int sem_post(sem_t *sem)
{
	pthread_spin_lock(&sem->spinlock);
	
	if (sem->firstWaiter != NULL)
	{
		pthread_t thread = sem->firstWaiter->thread;
		sem->firstWaiter->complete = 1;
		__sync_synchronize();
		pthread_spin_unlock(&sem->spinlock);
		
		pthread_kill(thread, 36);		// SIGTHWAKE
	}
	else
	{
		sem->value++;
	};
	
	pthread_spin_unlock(&sem->spinlock);
	return 0;
};

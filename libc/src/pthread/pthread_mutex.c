/*
	Glidix Runtime

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

#include <pthread.h>
#include <errno.h>
#include <stdlib.h>		/* abort */

int pthread_mutexattr_init(pthread_mutexattr_t *attr)
{
	attr->type = PTHREAD_MUTEX_DEFAULT;
	attr->protocol = PTHREAD_PRIO_NONE;
	attr->prioceiling = 0;
	return 0;
};

int pthread_mutexattr_destroy(pthread_mutexattr_t *attr)
{
	return 0;
};

int pthread_mutex_init(pthread_mutex_t *mutex, const pthread_mutexattr_t *attr)
{
	int type = PTHREAD_MUTEX_DEFAULT;
	if (attr != NULL)
	{
		type = attr->type;
	};
	
	if ((type != PTHREAD_MUTEX_NORMAL) && (type != PTHREAD_MUTEX_ERRORCHECK) && (type != PTHREAD_MUTEX_RECURSIVE))
	{
		return EINVAL;
	};
	
	pthread_spin_unlock(&mutex->spinlock);
	mutex->type = type;
	mutex->owner = 0;
	mutex->count = 0;
	mutex->firstWaiter = NULL;
	mutex->lastWaiter = NULL;
	
	return 0;
};

int pthread_mutex_destroy(pthread_mutex_t *mutex)
{
	pthread_spin_lock(&mutex->spinlock);
	if (mutex->firstWaiter != NULL)
	{
		pthread_spin_unlock(&mutex->spinlock);
		return EBUSY;
	};
	
	if (mutex->count != 0)
	{
		pthread_spin_unlock(&mutex->spinlock);
		return EBUSY;
	};
	
	// no need to release spinlock; the object is destroyed
	return 0;
};

int pthread_mutex_lock(pthread_mutex_t *mutex)
{
	pthread_spin_lock(&mutex->spinlock);
	
	if (mutex->count == 0)
	{
		// we can already acquire!
		mutex->owner = pthread_self();
		mutex->count = 1;
		__sync_synchronize();
		pthread_spin_unlock(&mutex->spinlock);
		return 0;
	};
	
	// do recursive locking and error checks if necessary
	if (mutex->type == PTHREAD_MUTEX_RECURSIVE)
	{
		if (mutex->owner == pthread_self())
		{
			mutex->count++;
			__sync_synchronize();
			pthread_spin_unlock(&mutex->spinlock);
			return 0;
		};
	}
	else if (mutex->type == PTHREAD_MUTEX_ERRORCHECK)
	{
		if (mutex->owner == pthread_self())
		{
			pthread_spin_unlock(&mutex->spinlock);
			return EDEADLK;
		};
	};
	
	// set up a waiter for us
	__pthread_mutex_waiter waiter;
	waiter.thread = pthread_self();
	waiter.next = NULL;
	
	// push the waiter to the end of the waiter queue
	if (mutex->firstWaiter == NULL)
	{
		mutex->firstWaiter = mutex->lastWaiter = &waiter;
	}
	else
	{
		mutex->lastWaiter->next = &waiter;
		mutex->lastWaiter = &waiter;
	};
	
	// wait for the mutex to be handed to us
	while (mutex->owner != pthread_self())
	{
		// atomically release the mutex spinlock and go to sleep; we will be woken
		// up once it's our turn.
		int status = _glidix_store_and_sleep(&mutex->spinlock, 0);
		
		if (status != 0)
		{
			// if this failed, it means that for some reason the mutex spinlock is no
			// longer a valid address. there's no good way to handle this
			abort();
		};
		
		// re-acquire the mutex spinlock before repeating the loop
		pthread_spin_lock(&mutex->spinlock);
		
		__sync_synchronize();
	};
	
	// we now own the mutex, the count was set to 1 for us by the unlocking thread, the owner was
	// set to us, the waiter was removed from the queue by the unlocking thread, we can safely return.
	pthread_spin_unlock(&mutex->spinlock);
	return 0;
};

int pthread_mutex_trylock(pthread_mutex_t *mutex)
{
	pthread_spin_lock(&mutex->spinlock);
	
	if (mutex->count == 0)
	{
		// we can indeed acquire now
		mutex->owner = pthread_self();
		mutex->count = 1;
		__sync_synchronize();
		pthread_spin_unlock(&mutex->spinlock);
		return 0;
	};
	
	// do recursive locking and error checks if necessary
	if (mutex->type == PTHREAD_MUTEX_RECURSIVE)
	{
		if (mutex->owner == pthread_self())
		{
			mutex->count++;
			__sync_synchronize();
			pthread_spin_unlock(&mutex->spinlock);
			return 0;
		};
	}
	else if (mutex->type == PTHREAD_MUTEX_ERRORCHECK)
	{
		if (mutex->owner == pthread_self())
		{
			pthread_spin_unlock(&mutex->spinlock);
			return EDEADLK;
		};
	};
	
	// we cannot acquire right now
	pthread_spin_unlock(&mutex->spinlock);
	return EBUSY;
};

int pthread_mutex_unlock(pthread_mutex_t *mutex)
{
	pthread_spin_lock(&mutex->spinlock);
	
	// do error checking if we're supposed to
	if (mutex->type == PTHREAD_MUTEX_ERRORCHECK)
	{
		if ((mutex->owner != pthread_self()) || (mutex->count != 0))
		{
			pthread_spin_unlock(&mutex->spinlock);
			return EPERM;
		};
	};
	
	// decrease the count, and if it's not yet zero, then simply return without
	// waking anyone up (we still own the mutex recursively)
	if ((--mutex->count) != 0)
	{
		pthread_spin_unlock(&mutex->spinlock);
		return 0;
	};
	
	// we no longer own the mutex; if any thread is waiting for it, wake it up
	// note that we don't need to set lastWaiter to NULL when the list is empty;
	// pthread_mutex_lock() will check if firstWaiter is NULL and ignore the value
	// of lastWaiter in this case.
	if (mutex->firstWaiter != NULL)
	{
		pthread_t newOwner = mutex->firstWaiter->thread;
		mutex->owner = newOwner;
		mutex->count = 1;
		mutex->firstWaiter = mutex->firstWaiter->next;
		pthread_spin_unlock(&mutex->spinlock);
		
		// wake up the owner now, after the spinlock was released. we must do this after the
		// release because the new owner might be a higher-priotity thread, and it would now
		// block on the spinlock indefinitely, using up all CPU time, and never returning
		// control to us to unlock the spinlock, leading to a deadlock.
		pthread_kill(newOwner, 36);		// SIGTHWAKE
		return 0;
	};
	
	// nobody is contending
	pthread_spin_unlock(&mutex->spinlock);
	return 0;
};

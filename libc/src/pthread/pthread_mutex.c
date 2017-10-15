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

#include <sys/call.h>
#include <pthread.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>

int pthread_mutexattr_init(pthread_mutexattr_t *attr)
{
	attr->__type = PTHREAD_MUTEX_DEFAULT;
	attr->__protocol = PTHREAD_PRIO_NONE;
	attr->__prioceiling = 0;
	return 0;
};

int pthread_mutexattr_destroy(pthread_mutexattr_t *attr)
{
	return 0;
};

int pthread_mutexattr_gettype(const pthread_mutexattr_t *attr, int *type)
{
	*type = attr->__type;
	return 0;
};

int pthread_mutexattr_settype(pthread_mutexattr_t *attr, int type)
{
	attr->__type = type;
	return 0;
};

int pthread_mutex_init(pthread_mutex_t *mutex, const pthread_mutexattr_t *attr)
{
	int type = PTHREAD_MUTEX_DEFAULT;
	if (attr != NULL)
	{
		type = attr->__type;
	};
	
	if ((type != PTHREAD_MUTEX_NORMAL) && (type != PTHREAD_MUTEX_ERRORCHECK) && (type != PTHREAD_MUTEX_RECURSIVE))
	{
		return EINVAL;
	};
	
	mutex->__tickets = 0;
	mutex->__cakes = 0;
	mutex->__type = type;
	mutex->__owner = 0;
	mutex->__count = 0;
	
	return 0;
};

int pthread_mutex_destroy(pthread_mutex_t *mutex)
{
	return 0;
};

int pthread_mutex_lock(pthread_mutex_t *mutex)
{
	if (mutex->__owner == pthread_self())
	{
		if (mutex->__type == PTHREAD_MUTEX_RECURSIVE)
		{
			mutex->__count++;
			return 0;
		}
		else
		{
			return EDEADLK;
		};
	};
	
	uint64_t ticket = __sync_fetch_and_add(&mutex->__tickets, 1);
	while (1)
	{
		uint64_t cakes = mutex->__cakes;
		if (cakes == ticket)
		{
			break;
		};
		
		uint64_t errnum = __syscall(__SYS_block_on, &mutex->__cakes, cakes);
		if (errnum != 0)
		{
			return errnum;
		};
	};
	
	mutex->__owner = pthread_self();
	mutex->__count = 1;
	return 0;
};

int pthread_mutex_trylock(pthread_mutex_t *mutex)
{
	if (mutex->__owner == pthread_self())
	{
		if (mutex->__type == PTHREAD_MUTEX_RECURSIVE)
		{
			mutex->__count++;
			return 0;
		}
		else
		{
			return EBUSY;
		};
	};

	uint64_t tickets = mutex->__tickets;
	if (tickets != mutex->__cakes)
	{
		return EBUSY;
	};
	
	// succeed only if nobody gets a ticket before us
	if (__sync_val_compare_and_swap(&mutex->__tickets, tickets, tickets+1) != tickets)
	{
		return EBUSY;
	};
	
	return 0;
};

int pthread_mutex_unlock(pthread_mutex_t *mutex)
{
	if (mutex->__owner != pthread_self())
	{
		return EPERM;
	};
	
	if (__sync_add_and_fetch(&mutex->__count, -1) == 0)
	{
		mutex->__owner = 0;
		
		__sync_fetch_and_add(&mutex->__cakes, 1);
		if (mutex->__cakes != mutex->__tickets)
		{
			return __syscall(__SYS_unblock, &mutex->__cakes);
		};
	};
	
	return 0;
};

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
#include <semaphore.h>
#include <errno.h>

int sem_init(sem_t *sem, int pshared, unsigned value)
{
	(void)pshared;			/* semaphores are always suitable for sharing */
	sem->__value = (int64_t) value;
	return 0;
};

int sem_destroy(sem_t *sem)
{
	return 0;
};

int sem_wait(sem_t *sem)
{
	while (1)
	{
		int64_t value = sem->__value;
		if (value == -1)
		{
			// block
			if (__syscall(__SYS_block_on, &sem->__value, value) != 0)
			{
				errno = EINVAL;
				return -1;
			};
			
			continue;
		}
		else if (value == 0)
		{
			// indicate that we are blocking
			if (__sync_val_compare_and_swap(&sem->__value, 0, -1) != 0)
			{
				// atomic compare-and-swap failed, try again
				continue;
			};
			
			// block
			if (__syscall(__SYS_block_on, &sem->__value, -1L) != 0)
			{
				errno = EINVAL;
				return -1;
			};
			
			continue;
		}
		else
		{
			if (__sync_val_compare_and_swap(&sem->__value, value, value-1) == value)
			{
				// yay! successfully got 1 resource
				return 0;
			};
		};
	};
};

int sem_post(sem_t *sem)
{
	while (1)
	{
		int64_t value = sem->__value;
		if (value == -1)
		{
			// set to 1, and wake up any waiters if successful
			if (__sync_val_compare_and_swap(&sem->__value, value, 1UL) == value)
			{
				if (__syscall(__SYS_unblock, &sem->__value) != 0)
				{
					errno = EINVAL;
					return -1;
				};
				
				return 0;
			};
		}
		else
		{
			if (__sync_val_compare_and_swap(&sem->__value, value, value+1) == value)
			{
				return 0;
			};
		};
	};
};

int sem_getvalue(sem_t *sem, int *valptr)
{
	*valptr = (int) sem->__value;
	return 0;
};

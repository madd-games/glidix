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

#ifndef _PTHREAD_H
#define _PTHREAD_H

#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * This data structure is pointed to by pthread_t, which expands to void*
 * in this pthreads implementation.
 */
typedef struct
{	
	/**
	 * The thread's pid.
	 */
	int					_pid;
	
	/**
	 * The thread's stack; this must be free()d after the thread exits.
	 */
	void*					_stack;
	
	/**
	 * The start routine and its argument.
	 */
	void*					(*_start)(void*);
	void*					_start_param;
	
	/**
	 * The return value from the start routine.
	 */
	void*					_ret;
	
	/**
	 * Thread sets this to 1 when it is OK for the creator to continue.
	 */
	int					_cont_ok;
} __pthread;

/**
 * Thread-local information. This is created on a new thread's stack, to store errno
 * and the thread ID. pthread_self() uses the fact that errno is the first field in order
 * to find the pointer to this structure using _glidix_geterrnoptr(). It is not safe to
 * use the _tid field for anything other than a return value from pthread_self(), since the
 * stack is released when a thread exits.
 */
typedef struct
{
	int					_errno;
	pthread_t				_tid;
} __thread_local_info;

int		pthread_create(pthread_t *thread, const pthread_attr_t *attr, void*(*start_routine)(void*), void *arg);
pthread_t	pthread_self();
int		pthread_join(pthread_t thread, void **retval);
int		pthread_detach(pthread_t thread);
int		pthread_kill(pthread_t thread, int sig);
int		pthread_spin_init(pthread_spinlock_t *sl);
int		pthread_spin_destroy(pthread_spinlock_t *sl);
int		pthread_spin_lock(pthread_spinlock_t *lock);
int		pthread_spin_trylock(pthread_spinlock_t *lock);
int		pthread_spin_unlock(pthread_spinlock_t *lock);

#ifdef __cplusplus
};	/* extern "C" */
#endif

#endif

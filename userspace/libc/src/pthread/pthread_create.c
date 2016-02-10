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

#include <sys/glidix.h>
#include <pthread.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <_heap.h>

extern pthread_spinlock_t __heap_lock;
extern void __release_and_exit(pthread_spinlock_t *lock);

static void thread_entry_func(__pthread *thread)
{
	/**
	 * IMPORTANT:
	 * This function must not return, as it is called at the bottom of a new thread's
	 * stack and so there is no return address to pop. Instead, we call _exit(), which
	 * is a system call (unlike exit() which would call cleanup functions), so that the
	 * thread itself terminates, without cleaning up the rest of the process.
	 */
	__thread_local_info li;
	li._tid = thread;
	volatile __pthread *vp = (volatile __pthread*) thread;
	void *stack = vp->_stack;
	while (vp->_pid == 0) __sync_synchronize();
	_glidix_seterrnoptr(&li._errno);
	thread->_cont_ok = 1;
	thread->_ret = thread->_start(thread->_start_param);
	
	// free our own stack, and then release the heap lock and exit without using the stack
	pthread_spin_lock(&__heap_lock);
	_heap_free(stack);
	__release_and_exit(&__heap_lock);
};

int pthread_create(pthread_t *thread_out, const pthread_attr_t *attr, void*(*start_routine)(void*), void *arg)
{
	if (attr != NULL)
	{
		// TODO
		return EINVAL;
	};
	
	/**
	 * Initialize the thread structure; we set _pid to 0 so that the new thread knows
	 * we have not finished storing its pid yet.
	 */
	__pthread *thread = (__pthread*) malloc(sizeof(__pthread));
	thread->_pid = 0;
	thread->_stack = malloc(0x200000);
	thread->_start = start_routine;
	thread->_start_param = arg;
	thread->_cont_ok = 0;

	/**
	 * Start the thread on a new stack with a call to thread_entry_func(),
	 * which will set everything up and call start_routine().
	 */
	_glidix_mstate mstate;
	mstate.rbp = 0;
	mstate.rsp = (uint64_t) thread->_stack + 0x200000;
	mstate.rip = (uint64_t) &thread_entry_func;
	mstate.rdi = (uint64_t) thread;			// argument to thread_entry_func()
	
	/**
	 * Create the thread; it will wait until we set the _pid to a nonzero value.
	 */
	thread->_pid = _glidix_clone(_GLIDIX_CLONE_SHARE_MEMORY | _GLIDIX_CLONE_SHARE_FTAB, &mstate);
	if (thread->_pid == -1)
	{
		free(thread->_stack);
		free(thread);
		return EAGAIN;
	};
	
	while (!thread->_cont_ok)
	{
		__sync_synchronize();
	};
	
	*thread_out = thread;
	return 0;
};

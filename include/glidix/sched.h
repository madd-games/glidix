/*
	Glidix kernel

	Copyright (c) 2014, Madd Games.
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

#ifndef __glidix_sched_h
#define __glidix_sched_h

/**
 * The thread scheduler.
 */

#include <glidix/common.h>
#include <stdint.h>
#include <stddef.h>

typedef struct _Thread
{
	/**
	 * The thread's registers.
	 */
	Regs				regs;

	/**
	 * The thread's stack and its size. Must be kfree()'d when the thread terminates.
	 */
	void				*stack;
	size_t				stackSize;

	/**
	 * Thread name (for debugging).
	 */
	const char			*name;

	/**
	 * Previous and next thread. Threads are stored in a circular list; this is never NULL.
	 */
	struct _Thread			*prev;
	struct _Thread			*next;
} Thread;

void initSched();
void switchContext(Regs *regs);
void dumpRunqueue();

/**
 * Prototype for a kernel thread entry point.
 */
typedef void (*KernelThreadEntry)(void *data);

/**
 * Parameters for a new kernel thread (passed to CreateKernelThread() ).
 */
typedef struct
{
	size_t				stackSize;
	const char			*name;
} KernelThreadParams;

/**
 * Create a new kernel thread.
 * @param entry Thread entry point.
 * @param params Thread initialization params (may be NULL).
 * @param data An arbitrary pointer to pass to the thread.
 */
void CreateKernelThread(KernelThreadEntry entry, KernelThreadParams *params, void *data);

#endif

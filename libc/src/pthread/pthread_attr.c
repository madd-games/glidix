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

int pthread_attr_init(pthread_attr_t *attr)
{
	attr->scope = PTHREAD_SCOPE_SYSTEM;
	attr->detachstate = PTHREAD_CREATE_JOINABLE;
	attr->inheritsched = PTHREAD_INHERIT_SCHED;
	attr->stack = NULL;
	attr->stacksize = 0x200000;			// 2MB
	return 0;
};

int pthread_attr_destroy(pthread_attr_t *attr)
{
	return 0;
};

int pthread_attr_setscope(pthread_attr_t *attr, int scope)
{
	if (scope != PTHREAD_SCOPE_SYSTEM)
	{
		return EINVAL;
	};
	
	attr->scope = scope;
	return 0;
};

int pthread_attr_getscope(const pthread_attr_t *attr, int *scope)
{
	*scope = attr->scope;
	return 0;
};

int pthread_attr_setdetachstate(pthread_attr_t *attr, int detachstate)
{
	if ((detachstate != PTHREAD_CREATE_JOINABLE) && (detachstate != PTHREAD_CREATE_DETACHED))
	{
		return EINVAL;
	};
	
	attr->detachstate = detachstate;
	return 0;
};

int pthread_attr_getdetachstate(const pthread_attr_t *attr, int *detachstate)
{
	*detachstate = attr->detachstate;
	return 0;
};

int pthread_attr_setinheritsched(pthread_attr_t *attr, int inheritsched)
{
	if ((inheritsched != PTHREAD_INHERIT_SCHED) && (inheritsched != PTHREAD_EXPLICIT_SCHED))
	{
		return EINVAL;
	};
	
	attr->inheritsched = inheritsched;
	return 0;
};

int pthread_attr_getinheritsched(const pthread_attr_t *attr, int *inheritsched)
{
	*inheritsched = attr->inheritsched;
	return 0;
};

int pthread_attr_setstackaddr(pthread_attr_t *attr, void *stackaddr)
{
	attr->stack = stackaddr;
	return 0;
};

int pthread_attr_getstackaddr(const pthread_attr_t *attr, void **stackaddr)
{
	*stackaddr = attr->stack;
	return 0;
};

int pthread_attr_setstack(pthread_attr_t *attr, void *stackaddr, size_t stacksize)
{
	if (stacksize < PTHREAD_STACK_MIN)
	{
		return EINVAL;
	};
	
	attr->stack = stackaddr;
	attr->stacksize = stacksize;
	return 0;
};

int pthread_attr_getstack(const pthread_attr_t *attr, void **stackaddr, size_t *stacksize)
{
	*stackaddr = attr->stack;
	*stacksize = attr->stacksize;
	return 0;
};

int pthread_attr_setstacksize(pthread_attr_t *attr, size_t stacksize)
{
	if (stacksize < PTHREAD_STACK_MIN)
	{
		return EINVAL;
	};
	
	attr->stacksize = stacksize;
	return 0;
};

int pthread_attr_getstacksize(const pthread_attr_t *attr, size_t *stacksize)
{
	*stacksize = attr->stacksize;
	return 0;
};

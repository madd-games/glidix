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

#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>

typedef void (*__atexit_func)(void);
static __atexit_func *atexit_array = NULL;
static int atexit_count = 0;
static pthread_mutex_t atexit_lock = PTHREAD_MUTEX_INITIALIZER;

typedef void (*__cxa_atexit_func)(void*);
typedef struct
{
	__cxa_atexit_func func;
	void *arg;
} __cxa_atexit_info;
static __cxa_atexit_info *cxa_atexit_array = NULL;
static int cxa_atexit_count = 0;

int atexit(__atexit_func func)
{
	pthread_mutex_lock(&atexit_lock);
	atexit_count++;
	atexit_array = realloc(atexit_array, sizeof(__atexit_func)*(atexit_count));
	atexit_array[atexit_count-1] = func;
	pthread_mutex_unlock(&atexit_lock);
	return 0;
};

void exit(int status)
{
	// iterate with index, because atexit() might be called by one of the exit functions.
	int i;
	for (i=0; i<atexit_count; i++)
	{
		atexit_array[i]();
	};
	
	for (i=0; i<cxa_atexit_count; i++)
	{
		cxa_atexit_array[i].func(cxa_atexit_array[i].arg);
	};
	
	_Exit(status);
};

void _Exit(int status)
{
	_exit(status);
};

int __cxa_atexit(void (*func) (void *), void *arg, void *d)
{
	pthread_mutex_lock(&atexit_lock);
	cxa_atexit_count++;
	cxa_atexit_array = realloc(cxa_atexit_array, sizeof(__cxa_atexit_info)*cxa_atexit_count);
	cxa_atexit_array[cxa_atexit_count-1].func = func;
	cxa_atexit_array[cxa_atexit_count-1].arg = arg;
	pthread_mutex_unlock(&atexit_lock);
	return 0;
};


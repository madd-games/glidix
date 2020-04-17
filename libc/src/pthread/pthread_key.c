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
#include <stdlib.h>
#include <errno.h>

int pthread_key_create(pthread_key_t *keyOut, void (*dest)(void*))
{
	// TODO: destructors are not yet supported, due to lack of pthread_atexit() and stuff
	if (dest != NULL)
	{
		return EAGAIN;
	};
	
	pthread_key_t key = (pthread_key_t) malloc(sizeof(struct __pthread_key));
	key->__head = NULL;
	pthread_mutex_init(&key->__lock, NULL);
	
	*keyOut = key;
	return 0;
};

int pthread_setspecific(pthread_key_t key, const void *value)
{
	pthread_mutex_lock(&key->__lock);
	pthread_t me = pthread_self();
	
	struct __pthread_key_mapping *m;
	for (m=key->__head; m!=NULL; m=m->__next)
	{
		if (m->__thid == me)
		{
			m->__value = (void*) value;
			break;
		};
	};
	
	if (m == NULL)
	{
		m = (struct __pthread_key_mapping*) malloc(sizeof(struct __pthread_key_mapping));
		m->__thid = me;
		m->__value = (void*) value;
		m->__next = key->__head;
		key->__head = m;
	};
	
	pthread_mutex_unlock(&key->__lock);
	return 0;
};

void* pthread_getspecific(pthread_key_t key)
{
	// we don't need a lock as we are only reading immutable data
	pthread_t me = pthread_self();
	
	struct __pthread_key_mapping *m;
	for (m=key->__head; m!=NULL; m=m->__next)
	{
		if (m->__thid == me)
		{
			return m->__value;
		};
	};
	
	return NULL;
};
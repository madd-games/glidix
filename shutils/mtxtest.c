/*
	Glidix Shell Utilities

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
#include <stdio.h>
#include <pthread.h>

int main()
{
	printf("Initializing mutex as recursive...\n");
	pthread_mutex_t mutex;
	pthread_mutexattr_t attr;
	pthread_mutexattr_init(&attr);
	pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
	if (pthread_mutex_init(&mutex, &attr) != 0)
	{
		fprintf(stderr, "pthread_mutex_init failed\n");
		return 1;
	};
	
	pthread_t me = pthread_self();
	printf("MY THID: %d\n", me);
	
	printf("Trying to first-lock the mutex...\n");
	pthread_mutex_lock(&mutex);
	printf("Mutex type: %d; PTHREAD_MUTEX_RECUSRIVE=%d\n", mutex.__type, PTHREAD_MUTEX_RECURSIVE);
	printf("MUTEX OWNER: %d\n", mutex.__owner);
	
	printf("Trying to second-lock the mutex...\n");
	pthread_mutex_lock(&mutex);
	
	printf("Releasing the mutex twice...\n");
	pthread_mutex_unlock(&mutex);
	pthread_mutex_unlock(&mutex);
	
	printf("OK\n");
	return 0;
};

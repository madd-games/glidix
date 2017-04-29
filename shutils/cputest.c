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

#include <sys/glidix.h>
#include <pthread.h>
#include <unistd.h>

#define	NUM_THREADS		16

pthread_t threads[NUM_THREADS];

volatile int *currentTimers;
int realTimers[16];
int falseTimers[16];

void* test_thread(void *ignore)
{
	(void)ignore;
	
	while (1)
	{
		__sync_fetch_and_add(&currentTimers[_glidix_cpuno()], 1);
	};
	
	return NULL;
};

int main()
{
	int i;
	currentTimers = realTimers;
	
	for (i=0; i<NUM_THREADS; i++)
	{
		pthread_create(&threads[i], NULL, test_thread, NULL);
	};
	
	sleep(10);
	currentTimers = falseTimers;
	
	for (i=0; i<16; i++)
	{
		printf("Timer %d: %d\n", i, realTimers[i]);
	};
	
	return 0;
};

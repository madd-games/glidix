#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>

#define	THREAD_NUM		3

void* testfunc(void *arg)
{
	while (1) printf("i am thread i=%d, id=%d\n", (int) (uint64_t) arg, pthread_self());
	return (void*) (uint64_t) pthread_self();
};
	
int main()
{
	printf("My pid is %d\n", getpid());
	pthread_t threads[THREAD_NUM];
	int i;
	
	for (i=0; i<THREAD_NUM; i++)
	{
		pthread_create(&threads[i], NULL, testfunc, (void*) (uint64_t) i);
	};
	
	while (1);
	return 0;
};


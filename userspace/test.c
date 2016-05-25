#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <signal.h>

#define	THREAD_NUM		3

void* testfunc(void *arg)
{
	printf("i am thread i=%d, id=%d\n", (int) (uint64_t) arg, pthread_self());
	return (void*) (uint64_t) pthread_self();
};
	
int main()
{
	pthread_t threads[THREAD_NUM];
	int i;
	
	for (i=0; i<THREAD_NUM; i++)
	{
		pthread_create(&threads[i], NULL, testfunc, (void*) (uint64_t) i);
	};
	
	for (i=0; i<THREAD_NUM; i++)
	{
		void *result;
		int errnum = pthread_join(threads[i], &result);
		if (errnum != 0)
		{
			fprintf(stderr, "pthread_join failed: %s\n", strerror(errno));
			return 1;
		};
		
		printf("Thread %d returned %d\n", threads[i], (int) (uint64_t) result);
	};
	
	printf("bye bye\n");
	
	return 0;
};


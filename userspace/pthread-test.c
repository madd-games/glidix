#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <pthread.h>

#define	NUM_THREADS 3

void* my_thread(void *pnum)
{
	int num = (int) pnum;
	printf("Hello from thread %d!\n", num+1);
};

int main()
{
	pthread_t threads[NUM_THREADS];
	int i;
	for (i=0; i<NUM_THREADS; i++)
	{
		if (pthread_create(&threads[i], NULL, my_thread, (void*)i) != 0)
		{
			fprintf(stderr, "pthread_create failed!\n");
			return 1;
		};
	};
	
	for (i=0; i<NUM_THREADS; i++)
	{
		if (pthread_join(threads[i], NULL) != 0)
		{
			fprintf(stderr, "pthread_join failed!\n");
			return 1;
		};
	};

	printf("threads terminated, thank\n");
	return 0;
};

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/glidix.h>
#include <time.h>
#include <signal.h>

#define	NUM_THREADS			100
#define	NUM_REPEATS			100000

pthread_mutex_t mutex;

uint8_t ticketCounter = 0;
uint8_t exitCounter = 0;
pthread_t mutexQueue[256];

void lock()
{
	sigset_t mxset, oldset;
	sigemptyset(&mxset);
	sigaddset(&mxset, SIGMXULK);
	
	pthread_sigmask(SIG_BLOCK, &mxset, &oldset);
	
	uint8_t ticket = __sync_fetch_and_add(&ticketCounter, 1);
	mutexQueue[ticket] = pthread_self();
	
	while (exitCounter != ticket)
	{
		sigwaitinfo(&mxset, NULL);
	};
	
	mutexQueue[ticket] = 0;
	pthread_sigmask(SIG_SETMASK, &oldset, NULL);
};

void unlock()
{
	uint8_t nextTicket = __sync_add_and_fetch(&exitCounter, 1);
	pthread_t nextThread = mutexQueue[nextTicket];
	
	if (nextThread != 0)
	{
		pthread_kill(nextThread, SIGMXULK);
	};
};

void *test_thread_a(void *ignore)
{
	int counter = NUM_REPEATS;
	while (counter--)
	{
		pthread_mutex_lock(&mutex);
		pthread_mutex_unlock(&mutex);
	};
	
	return NULL;
};

void *test_thread_b(void *ignore)
{
	int counter = NUM_REPEATS;
	while (counter--)
	{
		pthread_mutex_lock(&mutex);
		_glidix_yield();
		pthread_mutex_unlock(&mutex);
	};
	
	return NULL;
};

void *test_thread_c(void *ignore)
{
	int counter = NUM_REPEATS;
	while (counter--)
	{
		lock();
		unlock();
	};
	
	return NULL;
};

void *test_thread_d(void *ignore)
{
	int counter = NUM_REPEATS;
	while (counter--)
	{
		lock();
		_glidix_yield();
		unlock();
	};
	
	return NULL;
};

int main()
{
	pthread_t threads[NUM_THREADS];
	pthread_mutex_init(&mutex, NULL);
	
	clock_t start = clock();
	int i;
	for (i=0; i<NUM_THREADS; i++)
	{
		if (pthread_create(&threads[i], NULL, test_thread_a, NULL) != 0)
		{
			printf("thread creation failed\n");
			return 1;
		};
	};
	
	for (i=0; i<NUM_THREADS; i++)
	{
		pthread_join(threads[i], NULL);
	};
	clock_t end = clock();
	
	clock_t timeA = end - start;
	printf("Test A (bad mutex, auto sched) = %d ms\n", (int) (timeA*1000/CLOCKS_PER_SEC));
	
	start = clock();
	for (i=0; i<NUM_THREADS; i++)
	{
		if (pthread_create(&threads[i], NULL, test_thread_b, NULL) != 0)
		{
			printf("thread creation failed\n");
			return 1;
		};
	};
	
	for (i=0; i<NUM_THREADS; i++)
	{
		pthread_join(threads[i], NULL);
	};
	end = clock();
	
	clock_t timeB = end - start;
	printf("Test B (bad mutex, manual sched) = %d ms\n", (int) (timeB*1000/CLOCKS_PER_SEC));

	start = clock();
	for (i=0; i<NUM_THREADS; i++)
	{
		if (pthread_create(&threads[i], NULL, test_thread_c, NULL) != 0)
		{
			printf("thread creation failed\n");
			return 1;
		};
	};
	
	for (i=0; i<NUM_THREADS; i++)
	{
		pthread_join(threads[i], NULL);
	};
	end = clock();
	
	clock_t timeC = end - start;
	printf("Test C (good mutex, auto sched) = %d ms\n", (int) (timeC*1000/CLOCKS_PER_SEC));
	
	start = clock();
	for (i=0; i<NUM_THREADS; i++)
	{
		if (pthread_create(&threads[i], NULL, test_thread_d, NULL) != 0)
		{
			printf("thread creation failed\n");
			return 1;
		};
	};
	
	for (i=0; i<NUM_THREADS; i++)
	{
		pthread_join(threads[i], NULL);
	};
	end = clock();
	
	clock_t timeD = end - start;
	printf("Test D (good mutex, manual sched) = %d ms\n", (int) (timeD*1000/CLOCKS_PER_SEC));
	
	return 0;
};

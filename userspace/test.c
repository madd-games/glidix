#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <pthread.h>
#include <fcntl.h>
#include <sys/glidix.h>

void _heap_dump();
int main()
{
	void *test = malloc(0x200000 - 128);
	void *b = malloc(32);
	malloc(128);
	malloc(256);
	_heap_dump();
	printf("OK\n");
	return 0;
};

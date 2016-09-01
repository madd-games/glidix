#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/glidix.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>
#include <poll.h>
#include <sys/stat.h>
#include <libddi.h>
#include <time.h>

void _heap_dump();

int main(int argc, char *argv[])
{
	printf("1:\n");
	_heap_dump();
	
	void *a = malloc(1);
	void *b = malloc(1);
	void *c = malloc(1);
	void *d = malloc(1);
	void *e = malloc(1);
	
	printf("2:\n");
	_heap_dump();
	
	free(b);
	
	printf("3:\n");
	_heap_dump();
	
	free(d);
	printf("4:\n");
	_heap_dump();
	
	free(c);
	printf("5:\n");
	_heap_dump();
	
	return 0;
};

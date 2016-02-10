#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <pthread.h>

int main()
{
	float a = 0.5;
	float b = 0.4;
	float c = a + b / 0.2;
	printf("i memed\n");
	return 0;
};

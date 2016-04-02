#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <pthread.h>
#include <fcntl.h>
#include <sys/glidix.h>

int main()
{
	FILE *fp = fopen("/media/cdrom/test.txt", "rb");
	if (fp == NULL)
	{
		perror("open test.txt");
		return 1;
	};
	
	unsigned long x = 5;
	int count = fscanf(fp, "%lu", &x);
	int c = fgetc(fp);
	
	printf("USING FILE READ:\n");
	printf("COUNT = %d\n", count);
	printf("X = %lu\n", x);
	printf("C = %c\n", c);
	
	fclose(fp);
	
	printf("USING STRING READ:\n");
	x = 4;
	count = sscanf("2016abc", "%lu", &x);
	printf("COUNT = %d\n", count);
	printf("X = %lu\n", x);
	return 0;
};

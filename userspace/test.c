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
	int fd = open("/dev/ptmx", O_RDONLY | O_NOCTTY);
	close(fd);
	return 0;
};

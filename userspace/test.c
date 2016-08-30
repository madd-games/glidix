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

#define TEST_STRING "GET / HTTP\\1.1\r\nHost: 192.168.56.1\r\nUser-Agent: Glidix\r\n\r\n"

int main(int argc, char *argv[])
{
	int sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sockfd == -1)
	{
		perror("socket");
		return 1;
	};
	
	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(struct sockaddr_in));
	addr.sin_family = AF_INET;
	inet_pton(AF_INET, "192.168.56.1", &addr.sin_addr);
	addr.sin_port = htons(80);
	
	if (connect(sockfd, (struct sockaddr*)&addr, sizeof(struct sockaddr_in)) != 0)
	{
		perror("connect");
		return 1;
	};
	
	printf("connection established\n");
	ssize_t size = write(sockfd, TEST_STRING, strlen(TEST_STRING));
	printf("sent %ld thanks\n", size);
	while (1)
	{
		char buffer[512];
		ssize_t sz = read(sockfd, buffer, 512);
		
		if (sz == -1)
		{
			perror("read");
			return 1;
		};
		
		if (sz == 0)
		{
			break;
		};
		
		write(1, buffer, sz);
	};
	
	printf("closing\n");
	close(sockfd);
	printf("byebye\n");
	
	return 0;
};

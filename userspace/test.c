#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/glidix.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>
#include <poll.h>

int main(int argc, char *argv[])
{
	int sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
	if (sockfd == -1)
	{
		perror("socket");
		return 1;
	};
	
	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(struct sockaddr_in));
	addr.sin_family = AF_INET;
	
	if (bind(sockfd, (struct sockaddr*)&addr, sizeof(struct sockaddr_in)) != 0)
	{
		perror("bind");
		close(sockfd);
		return 1;
	};
	
	uint8_t bitmapReq[64];
	uint8_t bitmapRes[64];
	
	while (1)
	{
		struct pollfd pfd;
		pfd.fd = sockfd;
		pfd.events = POLLIN | POLLOUT;
		
		int num = poll(&pfd, 1, -1);
	
		printf("num = %d, revents = %hd\n", num, pfd.revents);
	};
	
	return 0;
};

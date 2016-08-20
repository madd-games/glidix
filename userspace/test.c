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
		memset(bitmapReq, 0, 64);
		bitmapReq[sockfd] = POLLIN | POLLOUT;
		int num = _glidix_fpoll(bitmapReq, bitmapRes, 0, 0);
		if (num == -1)
		{
			perror("_glidix_fpoll");
			close(sockfd);
			return 1;
		};
	
		printf("num = %d, res = %hhu\n", num, bitmapRes[sockfd]);
	};
	
	return 0;
};

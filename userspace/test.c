#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/glidix.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>

char data[3000];
char buffer[5000];

int main(int argc, char *argv[])
{
	if (argc != 2) return 1;
	
	int sockfd = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);
	if (sockfd == -1)
	{
		perror("socket");
		return 1;
	};
	
	int i;
	for (i=0; i<3000; i++)
	{
		data[i] = (char)i;
	};
	
	if (strcmp(argv[1], "send") == 0)
	{
		struct sockaddr_in6 dest;
		memset(&dest, 0, sizeof(struct sockaddr_in6));
		dest.sin6_family = AF_INET6;
		inet_pton(AF_INET6, "2a02:c7f:a821:2300::999", &dest.sin6_addr);
		dest.sin6_port = htons(5050);
	
		while (1)
		{
			if (sendto(sockfd, data, 3000, 0, (struct sockaddr*)&dest, sizeof(struct sockaddr_in6)) == -1)
			{
				perror("sendto");
			};
		
			sleep(1);
		};
	}
	else if (strcmp(argv[1], "recv") == 0)
	{
		struct sockaddr_in6 me;
		memset(&me, 0, sizeof(struct sockaddr_in6));
		me.sin6_family = AF_INET6;
		me.sin6_port = htons(5050);
		
		if (bind(sockfd, (struct sockaddr*)&me, sizeof(struct sockaddr_in6)) != 0)
		{
			perror("bind");
			return 1;
		};
		
		struct sockaddr_in6 src;
		while (1)
		{
			socklen_t addrlen = sizeof(struct sockaddr_in6);
			ssize_t size = recvfrom(sockfd, buffer, 5000, 0, (struct sockaddr*) &src, &addrlen);
			if (size == -1)
			{
				perror("recvfrom");
				continue;
			};
			
			if (size != 3000)
			{
				printf("Received a message that isn't 3000 bytes long\n");
				continue;
			};
			
			if (memcmp(buffer, data, 3000) == 0)
			{
				printf("Received the CORRECT datagram\n");
			}
			else
			{
				printf("Received the WRONG datagram\n");
			};
		};
	};
	
	close(sockfd);
	
	return 0;
};

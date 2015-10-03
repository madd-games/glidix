#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>
#include <stdlib.h>

uint8_t srcAddr[] = {127, 0, 0, 1};
uint8_t destAddr[] = {127, 0, 0, 1};

typedef struct
{
	uint8_t			type;
	uint8_t			code;
	uint16_t		checksum;
	uint16_t		id;
	uint16_t		seq;
	char			payload[];
} __attribute__ ((packed)) PingPongPacket;

uint16_t checksum(void* vdata,size_t length)
{
	// Cast the data pointer to one that can be indexed.
	char* data=(char*)vdata;

	// Initialise the accumulator.
	uint32_t acc=0xffff;

	// Handle complete 16-bit blocks.
	size_t i;
	for (i=0;i+1<length;i+=2) {
		uint16_t word;
		memcpy(&word,data+i,2);
		acc+=__builtin_bswap16(word);
		if (acc>0xffff)
		{
			acc-=0xffff;
		}
	}

	// Handle any partial block at the end of the data.
	if (length&1)
	{
		uint16_t word=0;
		memcpy(&word,data+length-1,1);
		acc+=__builtin_bswap16(word);
		if (acc>0xffff)
		{
			acc-=0xffff;
		}
	}

	// Return the checksum in network byte order.
	return __builtin_bswap16(~acc);
}

int main()
{
	char *buffer = (char*) malloc(6000);
	int i;
	for (i=0; i<6000; i++)
	{
		buffer[i] = (char) i;
	};
	
	int sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
	if (sockfd == -1)
	{
		perror("socket");
		return 1;
	};
	
	struct sockaddr_in localhost;
	localhost.sin_family = AF_INET;
	inet_pton(AF_INET, "127.0.0.1", &localhost.sin_addr);
	
	ssize_t size = sendto(sockfd, buffer, 6000, 0, (struct sockaddr*) &localhost, sizeof(struct sockaddr_in));
	if (size == -1)
	{
		perror("sendto");
		close(sockfd);
		return 1;
	};
	
	printf("sending OK, now trying to receive...\n");
	memset(buffer, 0, 6000);
	
	socklen_t len = sizeof(struct sockaddr_in);
	struct sockaddr_in addr;
	size = recvfrom(sockfd, buffer, 6000, 0, (struct sockaddr*) &addr, &len);
	if (size == -1)
	{
		perror("recvfrom");
		close(sockfd);
		return 1;
	};
	
	char convbuf[INET_ADDRSTRLEN];
	printf("Received %d bytes from: %s\n", (int)size, inet_ntop(AF_INET, &addr.sin_addr, convbuf, INET_ADDRSTRLEN));
	printf("Validating...\n");
	
	for (i=0; i<6000; i++)
	{
		char c = (char) i;
		if (buffer[i] != c)
		{
			printf("ERROR! at %d\n", i);
			close(sockfd);
			return 1;
		};
	};
	
	printf("Everything is valid!\n");
	close(sockfd);
	return 0;
};

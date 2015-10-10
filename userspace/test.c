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
	char data[16] = "0123456789ABCDEF";
	
	printf("Create socket...\n");
	int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	if (sockfd == -1)
	{
		perror("socket");
		return 1;
	};
	
	printf("Loading address...\n");
	struct sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(1234);
	inet_pton(AF_INET, "192.168.10.1", &addr.sin_addr);
	
	printf("Sending...\n");
	if (sendto(sockfd, data, 16, 0, (struct sockaddr*) &addr, sizeof(struct sockaddr_in)) == -1)
	{
		perror("sendto");
		close(sockfd);
		return 1;
	};
	
	printf("Closing...\n");
	close(sockfd);
	
	printf("UDP datagram sent\n");
	return 0;
};

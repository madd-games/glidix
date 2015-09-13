#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>

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
	char *strAddr = "::ffff:0567:2345";
	uint8_t addr[16];
	
	if (inet_pton(AF_INET6, strAddr, addr) != 1)
	{
		printf("inet_pton failed\n");
		return 1;
	};
	
	char buffer[INET6_ADDRSTRLEN];
	
	if (inet_ntop(AF_INET6, addr, buffer, INET6_ADDRSTRLEN) == NULL)
	{
		printf("inet_ntop failed\n");
		return 1;
	};
	
	printf("Got back: [%s]\n", buffer);
	return 0;
};

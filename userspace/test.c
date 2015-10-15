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

void test_disp(const char *expr, int result, int lineno)
{
	if (result)
	{
		printf("TEST(%s) at %d OK\n", expr, lineno);
	}
	else
	{
		printf("TEST(%s) at %d FAILED\n", expr, lineno);
		abort();
	};
};

#define	TEST(expr)	test_disp(#expr, expr, __LINE__)

void test_strtol()
{
	char *endptr;
	int x = (int) strtol("123", &endptr, 10);
	printf("x=%d, endptr='%s'\n", x, endptr);
	TEST((x == 123) && (*endptr == 0));
	
	x = (int) strtol("  123", &endptr, 10);
	printf("x=%d, endptr='%s'\n", x, endptr);
	TEST((x == 123) && (*endptr == 0));
	
	x = (int) strtol("  123abc", &endptr, 10);
	printf("x=%d, endptr='%s'\n", x, endptr);
	TEST((x == 123) && (*endptr == 'a'));

	x = (int) strtol("123abc", &endptr, 10);
	printf("x=%d, endptr='%s'\n", x, endptr);
	TEST((x == 123) && (*endptr == 'a'));
	
	x = (int) strtol("feFE", &endptr, 16);
	printf("x=%x, endptr='%s'\n", x, endptr);
	TEST((x == 0xFEFE) && (*endptr == 0));
	
	x = (int) strtol("feFEX", &endptr, 16);
	printf("x=%x, endptr='%s'\n", x, endptr);
	TEST((x == 0xFEFE) && (*endptr == 'X'));

	x = (int) strtol("12345x", &endptr, 0);
	printf("x=%d, endptr='%s'\n", x, endptr);
	TEST((x == 12345) && (*endptr == 'x'));
	
	x = (int) strtol("12345", &endptr, 0);
	printf("x=%d, endptr='%s'\n", x, endptr);
	TEST((x == 12345) && (*endptr == 0));

	x = (int) strtol("0765", &endptr, 0);
	printf("x=%o, endptr='%s'\n", x, endptr);
	TEST((x == 0765) && (*endptr == 0));

	x = (int) strtol("0xBEEF", &endptr, 0);
	printf("x=%x, endptr='%s'\n", x, endptr);
	TEST((x == 0xBEEF) && (*endptr == 0));

	x = (int) strtol("0xBEEF?", &endptr, 0);
	printf("x=%x, endptr='%s'\n", x, endptr);
	TEST((x == 0xBEEF) && (*endptr == '?'));
};

void test_scan()
{
	char buf1[64];
	char buf2[64];
	
	int a, b, c, d;
	int count = sscanf("56", "%d", &a);
	printf("count=%d, a=%d\n", count, a);
	TEST((count == 1) && (a == 56));
	
	count = sscanf("40/90/100/23", "%d/%d/%d/%d", &a, &b, &c, &d);
	printf("count=%d, a=%d, b=%d, c=%d, d=%d\n", count, a, b, c, d);
	TEST((count == 4) && (a == 40) && (b == 90) && (c == 100) && (d == 23));
	
	count = sscanf("40/90//100/23", "%d/%d/%d/%d", &a, &b, &c, &d);
	printf("count=%d, a=%d, b=%d, c=%d, d=%d\n", count, a, b, c, d);
	TEST((count == 2) && (a == 40) && (b == 90));
	
	count = sscanf("hello world", "%s %s", buf1, buf2);
	printf("count=%d, buf1='%s', buf2='%s'\n", count, buf1, buf2);
	TEST((count == 2) && (strcmp(buf1, "hello") == 0) && (strcmp(buf2, "world") == 0));
};

int main()
{
	test_strtol();
	test_scan();
	printf("All tests completed successfully.\n");
	return 0;
};

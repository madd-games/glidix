#include <stdio.h>

int main()
{
	const char *testString = "1234abc";
	int x;
	int n;
	
	int result = sscanf(testString, "%d%n", &x, &n);
	printf("result=%d, x=%d, n=%d\n", result, x, n);
	return 0;
};

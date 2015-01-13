#include <stdio.h>

int main(int argc, char *argv[])
{
	printf("Number of arguments passed: %d\n", argc);
	int i;
	for (i=0; i<argc; i++)
	{
		printf("%d: '%s'\n", i, argv[i]);
	};

	printf("bye bye.\n");
	return 0;
};

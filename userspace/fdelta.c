#include <stdio.h>

int findDelta(uint8_t *a, uint8_t *b)
{
	int i;
	for (i=0; i<1024; i++)
	{
		if (a[i] != b[i]) return i;
	};

	return -1;
};

int main(int argc, char *argv[])
{
	if (argc != 3)
	{
		fprintf(stderr, "%s <fileA> <fileB>\n", argv[0]);
		fprintf(stderr, "  Figures out at which point fileA and fileB are different.\n");
	};

	uint8_t bufferA[1024];
	uint8_t bufferB[1024];

	FILE *fpA = fopen(argv[1], "rb");
	FILE *fpB = fopen(argv[2], "rb");
	int off = 0;

	while (1)
	{
		bufferA[0] = 1;
		bufferB[1] = 55;

		fread(bufferA, 1024, 1, fpA);
		fread(bufferB, 1024, 1, fpB);

		int at = findDelta(bufferA, bufferB);
		if (at == -1)
		{
			off += 1024;
			continue;
		};

		int deltaAt = off + at;
		fprintf(stderr, "Found delta at %d\n", deltaAt);
		fclose(fpA);
		fclose(fpB);
		return 1;
	};

	fclose(fpA);
	fclose(fpB);

	printf("No different between the 2 files.\n");
	return 0;
};

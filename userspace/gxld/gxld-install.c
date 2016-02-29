/*
	Glidix Loader

	Copyright (c) 2014-2016, Madd Games.
	All rights reserved.
	
	Redistribution and use in source and binary forms, with or without
	modification, are permitted provided that the following conditions are met:
	
	* Redistributions of source code must retain the above copyright notice, this
	  list of conditions and the following disclaimer.
	
	* Redistributions in binary form must reproduce the above copyright notice,
	  this list of conditions and the following disclaimer in the documentation
	  and/or other materials provided with the distribution.
	
	THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
	AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
	IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
	DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
	FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
	DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
	SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
	CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
	OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
	OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>

const char *progName;

typedef struct
{
	uint8_t							flags;
	uint8_t							sig1;			// 0x14
	uint16_t						startHigh;
	uint8_t							systemID;
	uint8_t							sig2;			// 0xEB
	uint16_t						lenHigh;
	uint32_t						startLow;
	uint32_t						lenLow;
} __attribute__ ((packed)) Partition;

typedef struct
{
	uint8_t							bootloader[446];
	Partition						parts[4];
	uint8_t							bootsig[2];
} __attribute__ ((packed)) MBR;

void usage()
{
	fprintf(stderr, "USAGE:\t%s <mbr> <stage2> <device>\n", progName);
	fprintf(stderr, "\tInstalls the Glidix bootloader. You need to provide the\n");
	fprintf(stderr, "\tMBR file and the stage2.bin file, and a device to install\n");
	fprintf(stderr, "\ton. The stage2.bin will be installed on the partition marked\n");
	fprintf(stderr, "\tas bootable.\n");
};

int main(int argc, char *argv[])
{
	progName = argv[0];
	if (argc != 4)
	{
		usage();
		return 1;
	};
	
	printf("Loading partition table...\n");
	int dev = open(argv[3], O_RDWR);
	if (dev == -1)
	{
		fprintf(stderr, "%s: cannot open %s: %s\n", argv[0], argv[3], strerror(errno));
		return 1;
	};
	
	MBR bootsector;
	read(dev, &bootsector, 512);
	
	int bootPartIndex;
	for (bootPartIndex=0; bootPartIndex<4; bootPartIndex++)
	{
		if (bootsector.parts[bootPartIndex].flags == 0x80) break;
	};
	
	if (bootPartIndex == 4)
	{
		close(dev);
		fprintf(stderr, "%s: no bootable partition found on %s\n", argv[0], argv[3]);
		return 1;
	};
	
	printf("Installing the MBR...\n");
	int mbr = open(argv[1], O_RDONLY);
	if (mbr == -1)
	{
		close(dev);
		fprintf(stderr, "%s: cannot open %s: %s\n", argv[0], argv[1], strerror(errno));
		return 1;
	};
	
	read(mbr, &bootsector, 446);
	close(mbr);
	
	memcpy(bootsector.bootsig, "\x55\xAA", 2);
	
	lseek(dev, 0, SEEK_SET);
	write(dev, &bootsector, 512);
	
	printf("Installing stage2 bootloader...\n");
	off_t partOffset = 512 * bootsector.parts[bootPartIndex].startLow;
	uint64_t cisOffset;
	lseek(dev, partOffset+16, SEEK_SET);
	read(dev, &cisOffset, 8);
	lseek(dev, partOffset, SEEK_SET);
	
	int stage2 = open(argv[2], O_RDONLY);
	if (stage2 == -1)
	{
		close(dev);
		fprintf(stderr, "%s: cannot open %s: %s\n", argv[0], argv[2], strerror(errno));
		return 1;
	};
	
	char buf[512];
	ssize_t count;
	do
	{
		count = read(stage2, buf, 512);
		write(dev, buf, count);
	} while (count == 512);
	
	lseek(dev, partOffset+16, SEEK_SET);
	write(dev, &cisOffset, 8);
	close(dev);
	
	printf("Bootloader installation complete.\n");
	return 0;
};

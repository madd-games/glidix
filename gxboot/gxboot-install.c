/*
	Glidix bootloader (gxboot)

	Copyright (c) 2014-2017, Madd Games.
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

#define _GNU_SOURCE
#define _GLIDIX_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>

#define MBR_GPT_ID				0xEE

typedef struct
{
	uint8_t flags;
	uint8_t startHead;
	uint16_t startCylSector;
	uint8_t systemID;
	uint8_t endHead;
	uint16_t endCylSector;
	uint32_t startLBA;
	uint32_t partSize;
} __attribute__ ((packed)) MBRPart;

typedef struct
{
	char bootstrap[446];
	MBRPart parts[4];
	uint16_t sig;
} __attribute__ ((packed)) MBR;

typedef struct
{
	uint64_t				sig;
	uint32_t				rev;
	uint32_t				headerSize;
	uint32_t				headerCRC32;
	uint32_t				zero;
	uint64_t				myLBA;
	uint64_t				altLBA;
	uint64_t				firstUseableLBA;
	uint64_t				lastUseableLBA;
	uint8_t					diskGUID[16];
	uint64_t				partListLBA;
	uint32_t				numPartEnts;
	uint32_t				partEntSize;
	uint32_t				partArrayCRC32;
} GPTHeader;

typedef struct
{
	uint8_t					type[16];
	uint8_t					partid[16];
	uint64_t				startLBA;
	uint64_t				endLBA;
	uint64_t				attr;
	uint16_t				partName[36];		/* UTF-16LE */
} GPTPartition;

char vbrBuffer[0x200000];

uint8_t rootPartType[16] = {0x9C, 0xAD, 0xC1, 0x81, 0xC4, 0xBD, 0x09, 0x48, 0x8D, 0x9F, 0xDC, 0xB2, 0xA9, 0xB8, 0x5D, 0x01};

int main(int argc, char *argv[])
{
	const char *imagePath = getenv("GXBOOT_IMAGE_PATH");
	if (imagePath == NULL)
	{
		imagePath = "/usr/share/gxboot";
	};

	char *mbrPath;
	char *vbrPath;

	asprintf(&mbrPath, "%s/mbr.bin", imagePath);
	asprintf(&vbrPath, "%s/vbr.bin", imagePath);

	if (argc != 2)
	{
		fprintf(stderr, "USAGE:\t%s <device>\n", argv[0]);
		fprintf(stderr, "\tInstall gxboot on the specified device\n");
		return 1;
	};
	
	int drive = open(argv[1], O_RDWR);
	if (drive == -1)
	{
		fprintf(stderr, "%s: cannot open %s: %s\n", argv[0], argv[1], strerror(errno));
		return 1;
	};
	
	MBR mbr;
	if (pread(drive, &mbr, 512, 0) != 512)
	{
		fprintf(stderr, "%s: failed to read full MBR from %s\n", argv[0], argv[1]);
		close(drive);
		return 1;
	};
	
	if (mbr.sig != 0xAA55)
	{
		fprintf(stderr, "%s: device %s does not contain a valid MBR\n", argv[0], argv[1]);
		close(drive);
		return 1;
	};
	
	int gptFound = 0;
	for (int part=0; part<4; part++)
	{
		if (mbr.parts[part].systemID == MBR_GPT_ID)
		{
			gptFound = 1;
			break;
		};
	};
	
	if (!gptFound)
	{
		fprintf(stderr, "%s: no protective MBR found on %s\n", argv[0], argv[1]);
		close(drive);
		return 1;
	};

	GPTHeader gptHeader;
	if (pread(drive, &gptHeader, sizeof(GPTHeader), 512) != sizeof(GPTHeader))
	{
		fprintf(stderr, "%s: failed to read the GPT header from %s\n", argv[0], argv[1]);
		close(drive);
		return 1;
	};

	if (gptHeader.partEntSize != sizeof(GPTPartition))
	{
		fprintf(stderr, "%s: the file %s has an invalid partition table entry size\n", argv[0], argv[1]);
		close(drive);
		return 1;
	};

	size_t partTableSize = sizeof(GPTPartition) * gptHeader.numPartEnts;
	GPTPartition *parts = (GPTPartition*) malloc(partTableSize);

	if (pread(drive, parts, partTableSize, gptHeader.partListLBA * 512) != partTableSize)
	{
		fprintf(stderr, "%s: failed to read the partition table from %s\n", argv[0], argv[1]);
		close(drive);
		return 1;
	};

	int rootPartIndex;
	for (rootPartIndex=0; rootPartIndex<gptHeader.numPartEnts; rootPartIndex++)
	{
		GPTPartition *part = &parts[rootPartIndex];
		if (memcmp(part->type, rootPartType, 16) == 0)
		{
			break;
		};
	};

	if (rootPartIndex == gptHeader.numPartEnts)
	{
		fprintf(stderr, "%s: there is no glidix root partition on %s\n", argv[0], argv[1]);
		close(drive);
		return 1;
	};
	
	int fd = open(mbrPath, O_RDONLY);
	if (fd == -1)
	{
		fprintf(stderr, "%s: cannot open %s: %s\n", argv[0], mbrPath, strerror(errno));
		close(drive);
		return 1;
	};
	
	ssize_t size = read(fd, &mbr, 512);
	if (size == -1)
	{
		fprintf(stderr, "%s: cannot read %s: %s\n", argv[0], mbrPath, strerror(errno));
		close(fd);
		close(drive);
		return 1;
	};
	
	close(fd);
	
	if (size > 436)			/* leave space for "disk ID" just in case! */
	{
		fprintf(stderr, "%s: MBR image too large!\n", argv[0]);
		close(drive);
		return 1;
	};
	
	if (pwrite(drive, &mbr, 512, 0) != 512)
	{
		fprintf(stderr, "%s: failed to write MBR back!\n", argv[0]);
		close(drive);
		return 1;
	};
	
	fd = open(vbrPath, O_RDONLY);
	if (fd == -1)
	{
		fprintf(stderr, "%s: cannot open %s: %s\n", argv[0], vbrPath, strerror(errno));
		close(drive);
		return 1;
	};
	
	ssize_t vbrSize = read(fd, vbrBuffer, 0x200000);
	if (vbrSize == -1)
	{
		fprintf(stderr, "%s: cannot read %s: %s\n", argv[0], vbrPath, strerror(errno));
		close(drive);
		close(fd);
		return 1;
	};
	
	close(fd);
	
	if (pwrite(drive, vbrBuffer, vbrSize, 512UL * parts[rootPartIndex].startLBA) != vbrSize)
	{
		fprintf(stderr, "%s: failed to write VBR: %s\n", argv[0], strerror(errno));
		close(drive);
		return 1;
	};
	
	close(drive);
	return 0;
};

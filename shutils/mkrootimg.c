/*
	Glidix Shell Utilities

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

#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <fcntl.h>
#include <stdint.h>

/**
 * Formats a disk or partition to the Glidix V3 filesystem (GXFS3).
 */

#define	GXFS_MAGIC				(*((const uint64_t*)"__GXFS__"))

#define	GXFS_FEATURE_BASE			(1 << 0)

typedef struct
{
	uint64_t sbhMagic;
	uint8_t  sbhBootID[16];
	uint64_t sbhFormatTime;
	uint64_t sbhWriteFeatures;
	uint64_t sbhReadFeatures;
	uint64_t sbhOptionalFeatures;
	uint64_t sbhResv[2];
	uint64_t sbhChecksum;
} GXFS_SuperblockHeader;

typedef struct
{
	uint64_t sbbResvBlocks;
	uint64_t sbbUsedBlocks;
	uint64_t sbbTotalBlocks;
	uint64_t sbbFreeHead;
	uint64_t sbbLastMountTime;
	uint64_t sbbLastCheckTime;
	uint64_t sbbRuntimeFlags;
} GXFS_SuperblockBody;

typedef struct
{
	uint32_t arType;	/* "ATTR" */
	uint32_t arRecordSize;	/* sizeof(GXFS_AttrRecord) */
	uint64_t arLinks;
	uint32_t arFlags;
	uint16_t arOwner;
	uint16_t arGroup;
	uint64_t arSize;
	uint64_t arATime;
	uint64_t arMTime;
	uint64_t arCTime;
	uint64_t arBTime;
	uint32_t arANano;
	uint32_t arMNano;
	uint32_t arCNano;
	uint32_t arBNano;
	uint64_t arIXPerm;
	uint64_t arOXPerm;
	uint64_t arDXPerm;
} GXFS_AttrRecord;

typedef struct
{
	uint32_t drType;	/* "DENT" */
	uint32_t drRecordSize;	/* sizeof(GXFS_DentRecord) + strlen(filename), 8-byte-aligned */
	uint64_t drInode;
	uint8_t drInoType;
	char drName[];
} GXFS_DentRecord;

typedef struct
{
	uint32_t trType;	/* "TREE" */
	uint32_t trSize;	/* sizeof(GXFS_TreeRecord) */
	uint64_t trDepth;
	uint64_t trHead;
} GXFS_TreeRecord;

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

int driveFD;
off_t rootPartOffset;
size_t rootPartSize;

uint8_t rootPartType[16] = {0x9C, 0xAD, 0xC1, 0x81, 0xC4, 0xBD, 0x09, 0x48, 0x8D, 0x9F, 0xDC, 0xB2, 0xA9, 0xB8, 0x5D, 0x01};

ssize_t partWrite(const void *buffer, size_t size, off_t offset)
{
	return pwrite(driveFD, buffer, size, offset + rootPartOffset);
};

void generateMGSID(uint8_t *buffer)
{
	uint64_t timestamp = (uint64_t) time(NULL);
	uint64_t counter = (uint64_t) clock();
	
	uint8_t *stampref = (uint8_t*) &timestamp;
	uint8_t *countref = (uint8_t*) &counter;
	
	uint8_t stampSize = 8;
	uint8_t countSize = 8;
	
	int i;
	for (i=7; i>=0; i--)
	{
		if (stampref[i] != 0)
		{
			break;
		};
		
		stampSize--;
	};
	
	for (i=7; i>=0; i--)
	{
		if (countref[i] != 0)
		{
			break;
		};
		
		countSize--;
	};
	
	if ((stampSize + countSize) > 14)
	{
		countSize = 14 - stampSize;
	};
	
	buffer[0] = (stampSize << 4) | countSize;
	buffer[1] = 0x01;
	
	uint8_t *put = &buffer[2];
	memcpy(put, stampref, stampSize);
	put += stampSize;
	memcpy(put, countref, countSize);
	put += countSize;
	
	uint64_t padding = 14 - stampSize - countSize;
	if (padding != 0)
	{
		int fd = open("/dev/urandom", O_RDONLY);
		if (fd != -1)
		{
			read(fd, put, padding);
			close(fd);
		};
	};
};

void doChecksum(uint64_t *ptr)
{
	size_t count = 9;		/* 9 quadwords before the sbhChecksum field */
	uint64_t state = 0xF00D1234BEEFCAFEUL;
	
	while (count--)
	{
		state = (state << 1) ^ (*ptr++);
	};
	
	*ptr = state;
};

int main(int argc, char *argv[])
{
	const char *filename = NULL;
	const char *rootdir = NULL;

	int n;
	for (n=1; n<argc; n++)
	{
		if (argv[n][0] != '-')
		{
			if (filename == NULL)
			{
				filename = argv[n];
			}
			else if (rootdir == NULL)
			{
				rootdir = argv[n];
			}
			else
			{
				fprintf(stderr, "%s: too many arguments\n", argv[0]);
				return 1;
			};
		}
		else
		{
			fprintf(stderr, "%s: unknown command-line switch: %s\n", argv[0], argv[n]);
			return 1;
		};
	};
	
	if (filename == NULL || rootdir == NULL)
	{
		fprintf(stderr, "USAGE:\t%s <imgfile> <rootdir>\n", argv[0]);
		fprintf(stderr, "\tThe supplied image file is expected to have a GPT, containing\n");
		fprintf(stderr, "\texactly 1 Glidix root partition. This program will create a GXFS\n");
		fprintf(stderr, "\tfilesystem on the root partition, and will populate it with a copy\n");
		fprintf(stderr, "\tof the 'rootdir'.\n");
		fprintf(stderr, "\tWARNING: This will delete all data on the Glidix root partition of the image!\n");
		return 1;
	};
	
	driveFD = open(filename, O_RDWR);
	if (driveFD == -1)
	{
		fprintf(stderr, "%s: cannot open %s: %s\n", argv[0], filename, strerror(errno));
		return 1;
	};

	GPTHeader gptHeader;
	if (pread(driveFD, &gptHeader, sizeof(GPTHeader), 512) != sizeof(GPTHeader))
	{
		fprintf(stderr, "%s: failed to read the GPT header from %s\n", argv[0], argv[1]);
		close(driveFD);
		return 1;
	};

	if (gptHeader.partEntSize != sizeof(GPTPartition))
	{
		fprintf(stderr, "%s: the file %s has an invalid partition table entry size\n", argv[0], argv[1]);
		close(driveFD);
		return 1;
	};

	size_t partTableSize = sizeof(GPTPartition) * gptHeader.numPartEnts;
	GPTPartition *parts = (GPTPartition*) malloc(partTableSize);

	if (pread(driveFD, parts, partTableSize, gptHeader.partListLBA * 512) != partTableSize)
	{
		fprintf(stderr, "%s: failed to read the partition table from %s\n", argv[0], argv[1]);
		close(driveFD);
		return 1;
	};

	int rootPartIndex;
	for (rootPartIndex=0; rootPartIndex<gptHeader.numPartEnts; rootPartIndex++)
	{
		GPTPartition *part = &parts[rootPartIndex];
		if (memcmp(part->type, rootPartType, 16) == 0)
		{
			rootPartOffset = part->startLBA * 512;
			rootPartSize = (part->endLBA - part->startLBA + 1) * 512;
			break;
		};
	};

	if (rootPartIndex == gptHeader.numPartEnts)
	{
		fprintf(stderr, "%s: there is no glidix root partition on %s\n", argv[0], argv[1]);
		close(driveFD);
		return 1;
	};

#if 0
	struct stat st;
	if (fstat(driveFD, &st) != 0)
	{
		fprintf(stderr, "%s: fstat failed: %s\n", argv[0], strerror(errno));
		close(driveFD);
		return 1;
	};

	if (st.st_size < 0x1000000)
	{
		fprintf(stderr, "%s: a GXFS partiton must be at least 16MB large\n", argv[0]);
		close(driveFD);
		return 1;
	};
#endif

	// Put the "int 0x18" instruction in the VBR, making it report that it's not bootable/
	// After formatting, you should install the bootloader if that is necessary.
	partWrite("\xCD\x18", 2, 0);
	
	// Create the superblock.
	char block[4096];
	memset(block, 0, 4096);
	
	GXFS_SuperblockHeader *sbh = (GXFS_SuperblockHeader*) block;
	GXFS_SuperblockBody *sbb = (GXFS_SuperblockBody*) &sbh[1];
	
	time_t formatTime = time(NULL);
	sbh->sbhMagic = GXFS_MAGIC;
	generateMGSID(sbh->sbhBootID);
	sbh->sbhFormatTime = formatTime;
	sbh->sbhWriteFeatures = GXFS_FEATURE_BASE;
	sbh->sbhReadFeatures = GXFS_FEATURE_BASE;
	sbh->sbhOptionalFeatures = 0;
	doChecksum((uint64_t*) sbh);

	sbb->sbbResvBlocks = 8;
	sbb->sbbUsedBlocks = 9;			/* reserved + /boot (inode 8) */
	sbb->sbbTotalBlocks = (rootPartSize - 0x200000) >> 12;
	sbb->sbbFreeHead = 0;
	sbb->sbbLastMountTime = formatTime;
	sbb->sbbLastCheckTime = formatTime;
	sbb->sbbRuntimeFlags = 0;
	
	partWrite(block, 4096, 0x200000);
	
	// create the "bad blocks" inode
	memset(block, 0, 4096);
	
	// (skip over the inode header, ihNext=0)
	GXFS_AttrRecord *ar = (GXFS_AttrRecord*) &block[8];
	ar->arType = (*((const uint32_t*)"ATTR"));
	ar->arRecordSize = sizeof(GXFS_AttrRecord);
	ar->arLinks = 1;
	ar->arFlags = 0600;	// regular file, only accessible to root
	ar->arOwner = 0;
	ar->arGroup = 0;
	ar->arSize = 0;
	ar->arATime = ar->arMTime = ar->arCTime = ar->arBTime = formatTime;
	
	partWrite(block, 4096, 0x201000);
	
	// create the root directory inode (again, skip inode header, ihNext=0)
	// ar already set to the right place
	// also create a "/boot" entry
	ar->arFlags = 0x1000 | 01755;	// directory, sticky, mode 755
	GXFS_DentRecord *dr = (GXFS_DentRecord*) &ar[1];
	dr->drType = (*((const uint32_t*)"DENT"));
	dr->drRecordSize = 24;
	dr->drInode = 8;		/* use inode 8 for the /boot directory */
	dr->drInoType = 1;		/* type = directory */
	strcpy(dr->drName, "boot");
	partWrite(block, 4096, 0x202000);
	
	// inode 8 (/boot) is almost the same
	ar->arFlags = 0x1000 | 0755;
	// size the same
	dr->drInode = 3;
	dr->drInoType = 3;		/* "block device" */
	strcpy(dr->drName, "loader");
	partWrite(block, 4096, 0x208000);
	
	// now onto inode 3
	memset(block, 0, 4096);
	ar->arType = (*((const uint32_t*)"ATTR"));
	ar->arRecordSize = sizeof(GXFS_AttrRecord);
	ar->arLinks = 1;
	ar->arFlags = 0x13000 | 0600;	// fixed size, block device, only accessible to root
	ar->arOwner = 0;
	ar->arGroup = 0;
	ar->arSize = 2 * 1024 * 1024;	// 2MB
	ar->arATime = ar->arMTime = ar->arCTime = ar->arBTime = formatTime;
	
	GXFS_TreeRecord *tr = (GXFS_TreeRecord*) &ar[1];
	tr->trType = (*((const uint32_t*)"TREE"));
	tr->trSize = sizeof(GXFS_TreeRecord);
	tr->trDepth = 1;
	tr->trHead = 4;
	
	partWrite(block, 4096, 0x203000);
	
	// finally the pointer block (4)
	uint64_t *table = (uint64_t*) block;
	int i;
	for (i=0; i<512; i++)
	{
		table[i] = (uint64_t) (i-512);
	};
	
	partWrite(block, 4096, 0x204000);
	
	// thanks
	close(driveFD);
	return 0;
};

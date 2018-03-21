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
	int n;
	for (n=1; n<argc; n++)
	{
		if (argv[n][0] != '-')
		{
			if (filename != NULL)
			{
				fprintf(stderr, "%s: drive specified multiple times\n", argv[0]);
				return 1;
			}
			else
			{
				filename = argv[n];
			}
		}
		else
		{
			fprintf(stderr, "%s: unknown command-line switch: %s\n", argv[0], argv[n]);
			return 1;
		};
	};
	
	if (filename == NULL)
	{
		fprintf(stderr, "USAGE:\t%s <device>\n", argv[0]);
		fprintf(stderr, "\tCreate a GXFS filesystem on the specified device or image.\n");
		fprintf(stderr, "\tWARNING: This will delete all data on the device!\n");
		return 1;
	};
	
	int fd = open(filename, O_RDWR);
	if (fd == -1)
	{
		fprintf(stderr, "%s: cannot open %s: %s\n", argv[0], filename, strerror(errno));
		return 1;
	};
	
	struct stat st;
	if (fstat(fd, &st) != 0)
	{
		fprintf(stderr, "%s: fstat failed: %s\n", argv[0], strerror(errno));
		close(fd);
		return 1;
	};
	
	if (st.st_size < 0x1000000)
	{
		fprintf(stderr, "%s: a GXFS partiton must be at least 16MB large\n", argv[0]);
		close(fd);
		return 1;
	};
	
	// put the "int 0x18" instruction in the VBR, making it report that it's not bootable
	// after formatting, you should install the bootloader if that is necessary
	write(fd, "\xCD\x18", 2);
	
	// create the superblock
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
	sbb->sbbUsedBlocks = 8;
	sbb->sbbTotalBlocks = (st.st_size - 0x200000) >> 12;
	sbb->sbbFreeHead = 0;
	sbb->sbbLastMountTime = formatTime;
	sbb->sbbLastCheckTime = formatTime;
	sbb->sbbRuntimeFlags = 0;
	
	pwrite(fd, block, 4096, 0x200000);
	
	// create the "bad blocks" inode
	memset(block, 0, 4096);
	
	// (skip over the inode header, ihNext=0)
	GXFS_AttrRecord *ar = (GXFS_AttrRecord*) &block[8];
	ar->arType = (*((const uint32_t*)"ATTR"));
	ar->arRecordSize = sizeof(GXFS_AttrRecord);
	ar->arLinks = 1;
	ar->arFlags = 0600;	// regular files, only accessible to root
	ar->arOwner = 0;
	ar->arGroup = 0;
	ar->arSize = 0;
	ar->arATime = ar->arMTime = ar->arCTime = ar->arBTime = formatTime;
	
	pwrite(fd, block, 4096, 0x201000);
	
	// create the root directory inode (again, skip inode header, ihNext=0)
	// ar already set to the right place
	ar->arFlags = 0x1000 | 01755;	// directory, sticky, mode 755
	pwrite(fd, block, 4096, 0x202000);
	
	// thanks
	close(fd);
	return 0;
};

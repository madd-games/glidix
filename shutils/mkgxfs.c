/*
	Glidix Shell Utilities

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

#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <fcntl.h>

/**
 * Formats a disk or partition to the Glidix V2 filesystem (GXFS2).
 */

#define	GXFS_MAGIC				(*((const uint64_t*)"GLIDIXFS"))

typedef struct
{
	uint64_t				sbMagic;
	uint8_t					sbMGSID[16];
	uint64_t				sbFormatTime;
	uint64_t				sbTotalBlocks;
	uint64_t				sbChecksum;
	uint64_t				sbUsedBlocks;
	uint64_t				sbFreeHead;
	uint8_t					sbPad[512-0x40];
} Superblock;

typedef struct
{
	uint16_t				inoOwner;
	uint16_t				inoGroup;
	uint16_t				inoMode;
	uint16_t				inoTreeDepth;
	uint64_t				inoLinks;
	uint64_t				inoSize;
	uint64_t				inoBirthTime;
	uint64_t				inoChangeTime;
	uint64_t				inoModTime;
	uint64_t				inoAccessTime;
	uint32_t				inoBirthNano;
	uint32_t				inoChangeNano;
	uint32_t				inoModNano;
	uint32_t				inoAccessNano;
	uint64_t				inoIXPerm;
	uint64_t				inoOXPerm;
	uint64_t				inoDXPerm;
	uint64_t				inoRoot;
	uint64_t				inoACL;
	char					inoPath[256];
	uint8_t					inoPad[512-0x170];
} Inode;

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

void doSuperblockChecksum(Superblock *sb)
{
	uint64_t *scan = (uint64_t*) sb;
	sb->sbChecksum = -(scan[0] + scan[1] + scan[2] + scan[3] + scan[4]);
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
	
	uint64_t numBlocks = (st.st_size - 0x200000)/512;
	uint64_t formatTime = (uint64_t) time(NULL);
	
	Superblock sb;
	memset(&sb, 0, 512);
	sb.sbMagic = GXFS_MAGIC;
	generateMGSID(sb.sbMGSID);
	sb.sbFormatTime = formatTime;
	sb.sbTotalBlocks = numBlocks;
	doSuperblockChecksum(&sb);
	sb.sbUsedBlocks = 3;
	sb.sbFreeHead = 0;
	
	// block 0
	lseek(fd, 0x200000, SEEK_SET);
	write(fd, &sb, 512);
	
	// block 1; zero it all out. we allocate it to the root directory
	char zeroes[512];
	memset(zeroes, 0, 512);
	write(fd, zeroes, 512);
	
	// block 2: root directory inode
	Inode ino;
	memset(&ino, 0, 512);
	ino.inoOwner = 0;
	ino.inoGroup = 0;
	ino.inoMode = 011775;	/* directory, sticky, owner RWX, group RWX, world RX */
	ino.inoTreeDepth = 0;
	ino.inoLinks = 1;
	ino.inoSize = 0;
	ino.inoBirthTime = formatTime;
	ino.inoChangeTime = formatTime;
	ino.inoModTime = formatTime;
	ino.inoAccessTime = formatTime;
	// leave the nanotimes as zero
	// leave executable permissions as zero
	ino.inoRoot = 1;
	// ACL and path zero
	write(fd, &ino, 512);
	
	// thanks
	close(fd);
	return 0;
};

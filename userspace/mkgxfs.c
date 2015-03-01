/*
	Glidix Runtime

	Copyright (c) 2014-2015, Madd Games.
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
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <string.h>
#include <sys/ioctl.h>
#include "gxfs.h"

#include <glidix/storage.h>

size_t diskBlockSize;
size_t diskTotalSize;

void calculateBlocksToMake(uint64_t space, uint64_t blockSize, uint64_t *outBlocks, uint64_t *outBitmapSize)
{
	// TODO: make this more space efficient
	uint64_t blocks = 0;
	uint64_t bitmapSize = 0;

	while (space > (blockSize*8+1))
	{
		blocks += 8;
		bitmapSize++;
		space -= (blockSize*8+1);
	};

	*outBlocks = blocks;
	*outBitmapSize = bitmapSize;
};

void writeZeroes(int fd, size_t count)
{
	char c = 0;
	while (count--) write(fd, &c, 1);
};

int main(int argc, char *argv[])
{
	if (argc != 2)
	{
		fprintf(stderr, "%s <device>\n", argv[0]);
		fprintf(stderr, "  Create a Glidix File System on the specified device.\n");
		return 1;
	};

	int fd = open(argv[1], O_RDWR);
	if (fd < 0)
	{
		fprintf(stderr, "%s: error: could not open %s\n", argv[0], argv[1]);
		return 1;
	};

	SDParams pars;
	if (ioctl(fd, IOCTL_SDI_IDENTITY, &pars) != 0)
	{
		close(fd);
		fprintf(stderr, "%s: error: could not get information about %s\n", argv[0], argv[1]);
		return 1;
	};
	diskBlockSize = pars.blockSize;
	diskTotalSize = pars.totalSize;

	lseek(fd, 0x10, SEEK_SET);
	uint64_t f = 0x400;
	write(fd, &f, 8);

	uint64_t sectionsToMake = 2;
	uint64_t bytesPerInode = 0x4000;		// 16KB
	uint64_t fsBlockSize = pars.blockSize;

	uint64_t sectionPlacement = 0x400 + sizeof(gxfsCIS) + sectionsToMake * sizeof(gxfsSD);
	uint64_t inodesPerSection = (diskTotalSize - sectionPlacement) / bytesPerInode / sectionsToMake;
	uint64_t inodeBitmapSize = inodesPerSection / 8;
	if (inodesPerSection % 8) inodeBitmapSize++;
	uint64_t blocksPerSection, blockBitmapSize;
	calculateBlocksToMake((diskTotalSize - sectionPlacement - inodesPerSection * sectionsToMake * sizeof(gxfsInode) - inodeBitmapSize)/sectionsToMake,
				fsBlockSize, &blocksPerSection, &blockBitmapSize);

	printf("GXFS Details (consistency check):\n");
	printf("\tSize of CIS:           %d\n", sizeof(gxfsCIS));
	printf("\tSize of SD:            %d\n", sizeof(gxfsSD));
	printf("\tSize of inode:         %d\n", sizeof(gxfsInode));
	printf("\tSize of dirent:        %d\n", sizeof(struct dirent));

	printf("Filesystem plan:\n");
	printf("\tSections:              %d\n", sectionsToMake);
	printf("\tData per inode:        %dKB\n", bytesPerInode/1024);
	printf("\tBlock size:            %d bytes\n", fsBlockSize);
	printf("\tInodes per section:    %d\n", inodesPerSection);
	printf("\tInode bitmap size:     %d bytes\n", inodeBitmapSize);
	printf("\tBlocks per section:    %d\n", blocksPerSection);
	printf("\tBlock bitmap size:     %d bytes\n", blockBitmapSize);

	lseek(fd, 0x400, SEEK_SET);
	gxfsCIS cis;
	memcpy(cis.cisMagic, "GXFS", 4);
	cis.cisTotalIno = inodesPerSection * sectionsToMake;
	cis.cisInoPerSection = inodesPerSection;
	cis.cisTotalBlocks = blocksPerSection * sectionsToMake;
	cis.cisBlocksPerSection = blocksPerSection;
	cis.cisBlockSize = fsBlockSize;
	cis.cisCreateTime = time(NULL);
	cis.cisFirstDataIno = 8;
	cis.cisOffSections = 0x440;		// straight after the CIS
	cis.cisZero = 0;
	write(fd, &cis, 64);

	uint64_t imap0, bmap0, itab0, btab0;

	uint64_t i;
	gxfsSD sect;
	for (i=0; i<sectionsToMake; i++)
	{
		sect.sdOffMapIno = sectionPlacement;
		if (i == 0) imap0 = sectionPlacement;
		sectionPlacement += inodeBitmapSize;
		sect.sdOffMapBlocks = sectionPlacement;
		if (i == 0) bmap0 = sectionPlacement;
		sectionPlacement += blockBitmapSize;
		sect.sdOffTabIno = sectionPlacement;
		if (i == 0) itab0 = sectionPlacement;
		sectionPlacement += inodesPerSection * sizeof(gxfsInode);
		sect.sdOffTabBlocks = sectionPlacement;
		if (i == 0) btab0 = sectionPlacement;
		sectionPlacement += blocksPerSection * fsBlockSize;
		write(fd, &sect, 32);
		printf("Section %d: imap=%p; bmap=%p; itab=%p; btab=%p\n", i, (void*)sect.sdOffMapIno, (void*)sect.sdOffMapBlocks,
				(void*)sect.sdOffTabIno, (void*)sect.sdOffTabBlocks);
	};

	// we need to clear the inode and block bitmaps
	for (i=0; i<sectionsToMake; i++)
	{
		writeZeroes(fd, inodeBitmapSize);
		writeZeroes(fd, blockBitmapSize);
		lseek(fd, inodesPerSection * sizeof(gxfsInode) + blocksPerSection * fsBlockSize, SEEK_CUR);
	};

	// mark inode 2 (index 1) and inode 3 (index 2) as used, and fill in the structure.
	// This effectively creates the root and lost+found directory.
	printf("Creating root directory (inode 2) imap0=%p...\n", (void*)imap0);
	lseek(fd, imap0, SEEK_SET);
	uint8_t b = (1 << 1) | (1 << 2);
	write(fd, &b, 1);

	lseek(fd, itab0 + sizeof(gxfsInode), SEEK_SET);
	gxfsInode inode;
	inode.inoMode = 011755;
	inode.inoSize = sizeof(struct dirent);			// for the lost+found directory.
	inode.inoLinks = 1;
	inode.inoCTime = cis.cisCreateTime;
	inode.inoATime = cis.cisCreateTime;
	inode.inoMTime = cis.cisCreateTime;
	inode.inoOwner = 0;
	inode.inoGroup = 0;
	memset(inode.inoFrags, 0, 16*sizeof(gxfsFragment));
	// assign block 7 to the root inode.
	inode.inoFrags[0].fOff = 0;
	inode.inoFrags[0].fBlock = 7;
	inode.inoFrags[0].fExtent = 1;
	write(fd, &inode, sizeof(gxfsInode));

	printf("Creating the lost+found directory (inode 3)...\n");
	lseek(fd, itab0 + 2 * sizeof(gxfsInode), SEEK_SET);
	inode.inoMode = 011700;
	inode.inoSize = 0;
	inode.inoLinks = 1;
	inode.inoCTime = cis.cisCreateTime;
	inode.inoATime = cis.cisCreateTime;
	inode.inoMTime = cis.cisCreateTime;
	inode.inoOwner = 0;
	inode.inoGroup = 0;
	memset(inode.inoFrags, 0, 16*sizeof(gxfsFragment));
	// assign the first 7 blocks to the lost+found directory.
	inode.inoFrags[0].fOff = 0;
	inode.inoFrags[0].fBlock = 0;
	inode.inoFrags[0].fExtent = 7;
	write(fd, &inode, sizeof(gxfsInode));

	// create the directory entry for lost+found in the root directory.
	lseek(fd, btab0 + 7 * fsBlockSize, SEEK_SET);
	struct dirent ent;
	ent.d_ino = 3;
	strcpy(ent.d_name, "lost+found");
	write(fd, &ent, sizeof(struct dirent));

	printf("Marking the first 8 blocks as used...\n");
	lseek(fd, bmap0, SEEK_SET);
	b = 0xFF;
	write(fd, &b, 1);

	close(fd);
	printf("Filesystem created, you may mount now.\n");
	return 0;
};

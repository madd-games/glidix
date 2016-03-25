/*
	GXFS Formatting Utility

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
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <string.h>
#include <sys/ioctl.h>
#include "gxfs.h"
#include <stdlib.h>
#include <errno.h>

#include <glidix/storage.h>

char *progName;
size_t diskBlockSize;
size_t diskTotalSize;

void usage()
{
	fprintf(stderr, "USAGE:\t%s device [-v vbr_size] [-i data_per_inode] [-s sections]\n", progName);
	fprintf(stderr, "\tFormats the specified drive (or file) for the Glidix fileystem.\n");
	fprintf(stderr, "\tOptions:\n");
	fprintf(stderr, "\t-v vbr_size\n");
	fprintf(stderr, "\t\tSpecifies how much space to reserve for the\n");
	fprintf(stderr, "\t\tbootloader. Default is 4KB.\n");
	fprintf(stderr, "\t-i data_per_inode\n");
	fprintf(stderr, "\t\tSpecifies the amount of space to reserve\n");
	fprintf(stderr, "\t\tper inode. Default is 1MB.\n");
	fprintf(stderr, "\t-s sections\n");
	fprintf(stderr, "\t\tSpecifies the number of sections to create.\n");
	fprintf(stderr, "\t\tDefault is 16.\n");
	exit(1);
};

size_t parseSize(const char *str)
{
	size_t size;
	char unit;
	int count = sscanf(str, "%lu%c", &size, &unit);
	
	if (count == 1)
	{
		return size;
	};
	
	switch (unit)
	{
	case 'T':
		size *= 1024;
	case 'G':
		size *= 1024;
	case 'M':
		size *= 1024;
	case 'K':
		size *= 1024;
	case 'B':
		break;
	default:
		usage();
		exit(1);
	};
	
	return size;
};

void writeZeroes(int fd, size_t size)
{
	char zeroes[4096];
	memset(zeroes, 0, 4096);
	
	while (size > 0)
	{
		size_t writeNow = size;
		if (writeNow > 4096) writeNow = 4096;
		write(fd, zeroes, writeNow);
		size -= writeNow;
	};
};

int main(int argc, char *argv[])
{
	progName = argv[0];
	const char *deviceName = NULL;
	size_t vbrSize = 0x2000;		// default 8KB for the Volume Boot Record
	size_t dataPerInode = 0x400;		// default 16KB per inode
	size_t numSections = 2;			// default 2 sections
	size_t blockSize = 512;			// default 512-byte blocks
	
	size_t i;
	for (i=1; i<argc; i++)
	{
		if (argv[i][0] != '-')
		{
			if (deviceName != NULL)
			{
				usage();
				return 1;
			};
			
			deviceName = argv[i];
		}
		else if (strcmp(argv[i], "-v") == 0)
		{
			if (i == argc-1)
			{
				usage();
				return 1;
			};
			
			vbrSize = parseSize(argv[i+1]);
			i++;
		}
		else if (strcmp(argv[i], "-i") == 0)
		{
			if (i == argc-1)
			{
				usage();
				return 1;
			};
			
			dataPerInode = parseSize(argv[i+1]);
			i++;
		}
		else if (strcmp(argv[i], "-s") == 0)
		{
			if (i == argc-1)
			{
				usage();
				return 1;
			};
			
			if (sscanf(argv[i+1], "%lu", &numSections) != 1)
			{
				usage();
				return 1;
			};
			i++;
		}
		else
		{
			usage();
			return 1;
		};
	};

	if (deviceName == NULL)
	{
		usage();
		return 1;
	};
	
	int fd = open(deviceName, O_RDWR);
	if (fd < 0)
	{
		fprintf(stderr, "%s: error: could not open %s: %s\n", argv[0], deviceName, strerror(errno));
		return 1;
	};

	SDParams pars;
	if (ioctl(fd, IOCTL_SDI_IDENTITY, &pars) != 0)
	{
		close(fd);
		fprintf(stderr, "%s: error: could not get information about %s: %s\n", argv[0], deviceName, strerror(errno));
		return 1;
	};
	diskBlockSize = pars.blockSize;
	diskTotalSize = pars.totalSize;
	
	size_t offCIS = vbrSize;				// CIS right after the VBR
	size_t offSections = offCIS + 64;			// sections right after the CIS
	size_t offSpace = offSections + 32 * numSections;
	
	size_t spacePerSection = (diskTotalSize - offSpace) / numSections;
	size_t inodesPerSection = 8;
	size_t blockTableSize, inodeTableSize, blockMapSize, inodeMapSize, blocksPerSection;
	
	while (1)
	{
		blocksPerSection = ((inodesPerSection * dataPerInode) / blockSize) & ~((size_t)7);
		blockTableSize = blockSize * blocksPerSection;
		inodeTableSize = 359 * inodesPerSection;
		blockMapSize = blocksPerSection / 8;
		inodeMapSize = inodesPerSection / 8;
		size_t totalSize = blockTableSize + inodeTableSize + blockMapSize + inodeMapSize;
		
		if (totalSize > spacePerSection)
		{
			inodesPerSection -= 8;
			break;
		};
		
		inodesPerSection += 8;
	};

	blocksPerSection = ((inodesPerSection * dataPerInode) / blockSize) & ~((size_t)7);
	blockTableSize = blockSize * blocksPerSection;
	inodeTableSize = 359 * inodesPerSection;
	blockMapSize = blocksPerSection / 8;
	inodeMapSize = inodesPerSection / 8;

	if (inodesPerSection < 16)
	{
		close(fd);
		fprintf(stderr, "%s: the current configuration has less than 16 inodes per section", argv[0]);
		return 1;
	};
	
	printf("Filesystem plan:\n");
	printf("\tInodes per section: %lu\n", inodesPerSection);
	printf("\tBlocks per section: %lu\n", blocksPerSection);

	lseek(fd, 0x10, SEEK_SET);
	write(fd, &offCIS, 8);
	
	lseek(fd, offCIS, SEEK_SET);
	gxfsCIS cis;
	memcpy(cis.cisMagic, "GXFS", 4);
	cis.cisTotalIno = inodesPerSection * numSections;
	cis.cisInoPerSection = inodesPerSection;
	cis.cisTotalBlocks = blocksPerSection * numSections;
	cis.cisBlocksPerSection = blocksPerSection;
	cis.cisBlockSize = blockSize;
	cis.cisCreateTime = time(NULL);
	cis.cisFirstDataIno = 8;
	cis.cisOffSections = offSections;
	cis.cisZero = 0;
	write(fd, &cis, 64);
	
	uint64_t imap0, bmap0, itab0, btab0;
	
	gxfsSD sect;
	for (i=0; i<numSections; i++)
	{
		size_t placement = offSpace + spacePerSection * i;
		sect.sdOffMapIno = placement;
		if (i == 0) imap0 = placement;
		placement += inodeMapSize;
		sect.sdOffMapBlocks = placement;
		if (i == 0) bmap0 = placement;
		placement += blockMapSize;
		sect.sdOffTabIno = placement;
		if (i == 0) itab0 = placement;
		placement += inodeTableSize;
		sect.sdOffTabBlocks = placement;
		if (i == 0) btab0 = placement;
		write(fd, &sect, 32);
		printf("Section %d: imap=%p; bmap=%p; itab=%p; btab=%p\n", (int)i, (void*)sect.sdOffMapIno,
			 (void*)sect.sdOffMapBlocks, (void*)sect.sdOffTabIno, (void*)sect.sdOffTabBlocks);
	};
	
	// zero out the bitmaps
	for (i=0; i<numSections; i++)
	{
		size_t pos = offSpace + spacePerSection * i;
		lseek(fd, pos, SEEK_SET);
		writeZeroes(fd, blockMapSize + inodeMapSize);
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
	inode.inoSize = /*sizeof(struct dirent);*/ 25;			// for the lost+found directory.
	inode.inoLinks = 2;						// number of entries + 1
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
	inode.inoSize = 15;
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
	lseek(fd, btab0 + 7 * blockSize, SEEK_SET);
	//struct dirent ent;
	//ent.d_ino = 3;
	//strcpy(ent.d_name, "lost+found");
	//write(fd, &ent, sizeof(struct dirent));
	char direntdata[26];
	gxfsDirHeader *dhead = (gxfsDirHeader*) &direntdata[0];
	gxfsDirent *lfent = (gxfsDirent*) &direntdata[5];
	dhead->dhCount = 1;
	dhead->dhFirstSz = 20;
	lfent->deInode = 3;
	lfent->deNextSz = 0;
	lfent->deNameLen = 10;		// strlen("lost+found")
	strcpy(lfent->deName, "lost+found");
	write(fd, direntdata, 25);

	// create the null entry in lost+found
	lseek(fd, btab0 + blockSize, SEEK_SET);
	dhead->dhCount = 1;
	dhead->dhFirstSz = 0;
	lfent->deInode = 0;
	lfent->deNextSz = 0;
	lfent->deNameLen = 0;
	write(fd, direntdata, 15);

	printf("Marking the first 8 blocks as used...\n");
	lseek(fd, bmap0, SEEK_SET);
	b = 0xFF;
	write(fd, &b, 1);

	close(fd);
	printf("Filesystem created, you may mount now.\n");
	return 0;
};

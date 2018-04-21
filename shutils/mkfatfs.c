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
#include <stddef.h>

/**
 * Formats a disk or patition to the FAT32 filesystem.
 */

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
	/* BPB */
	uint8_t				jump[3];
	char				oemid[8];
	uint16_t			sectorSize;
	uint8_t				sectorsPerCluster;
	uint16_t			reservedSectors;
	uint8_t				numFats;
	uint16_t			ndents;			/* zero for FAT32 */
	uint16_t			sectorsSmall;
	uint8_t				mediaType;
	uint16_t			sectorsPerFatOld;	/* not valid for FAT32 */
	uint16_t			sectorsPerTrack;
	uint16_t			numHeads;
	uint32_t			hiddenSectors;
	uint32_t			sectorsLarge;
	
	/* EBPB */
	uint32_t			sectorsPerFat;
	uint16_t			flags;
	uint16_t			version;
	uint32_t			rootCluster;
	uint16_t			infoSector;
	uint16_t			backupBootSector;
	char				reserved[12];
	uint8_t				drive;
	uint8_t				winNTFlags;
	uint8_t				sig;			/* 0x28 or 0x29 */
	uint32_t			volumeSerial;
	uint8_t				volumeLabel[11];
	uint8_t				systemID[8];
	char				bootCode[356];
	MBRPart				parts[4];
	uint16_t			bootsig;		/* 0xAA55 */
} __attribute__ ((packed)) FATBootRecord;

typedef struct
{
	char				filename[11];
	uint8_t				attr;
	uint8_t				winnt;
	uint8_t				creatSecs;
	uint16_t			creatTime;
	uint16_t			creatDate;
	uint16_t			accessDate;
	uint16_t			clusterHigh;
	uint16_t			modTime;
	uint16_t			modDate;
	uint16_t			clusterLow;
	uint32_t			size;
} __attribute__ ((packed)) FATDent;

typedef struct
{
	uint32_t			sig1;
	char				resv[480];
	uint32_t			sig2;
	uint32_t			freeClusters;
	uint32_t			lastAllocated;
	char				resv2[12];
	uint32_t			sig3;
} __attribute__ ((packed)) FSInfo;

int main(int argc, char *argv[])
{
	const char *device = NULL;
	size_t numFats = 2;
	size_t sectorsPerCluster = 8;
	
	int i;
	for (i=1; i<argc; i++)
	{
		if (argv[i][0] != '-')
		{
			if (device != NULL)
			{
				fprintf(stderr, "%s: multiple devices specified\n", argv[0]);
				return 1;
			};
			
			device = argv[i];
		}
		else if (strcmp(argv[i], "-f") == 0)
		{
			i++;
			if (argv[i] == NULL)
			{
				fprintf(stderr, "%s: `-f' option expects a parameter\n", argv[0]);
				return 1;
			};
			
			sscanf(argv[i], "%lu", &numFats);
		}
		else
		{
			fprintf(stderr, "%s: unknown command-line option: `%s'\n", argv[0], argv[i]);
			return 1;
		};
	};
	
	if (device == NULL)
	{
		fprintf(stderr, "USAGE:\t%s <device>\n", argv[0]);
		fprintf(stderr, "\tFormats the specified device using a FAT32 filesystem.\n");
		return 1;
	};
	
	int fd = open(device, O_RDWR);
	if (fd == -1)
	{
		fprintf(stderr, "%s: failed to open `%s': %s\n", argv[0], device, strerror(errno));
		return 1;
	};
	
	struct stat st;
	if (fstat(fd, &st) != 0)
	{
		fprintf(stderr, "%s: failed to fstat: %s\n", argv[0], strerror(errno));
		return 1;
	};
	
	size_t totalSectors = st.st_size / 512;
	size_t useableSectors = totalSectors - 32;
	size_t sectorsPerFatSector = (512 / 4) * sectorsPerCluster;
	
	// (sectorsPerFat * numFats) + (sectorsPerFat * sectorsPerFatSector) = useableSectors
	// sectorsPerFat * (numFats + sectorsPerFatSector) = useableSectors
	size_t sectorsPerFat = useableSectors / (numFats + sectorsPerFatSector) + 1;

	FATBootRecord vbr;
	memset(&vbr, 0, sizeof(FATBootRecord));
	
	vbr.jump[0] = 0xEB;
	vbr.jump[1] = offsetof(FATBootRecord, bootCode);
	vbr.jump[2] = 0x90;
	
	memcpy(vbr.oemid, " GLIDIX ", 8);
	vbr.sectorSize = 512;
	vbr.sectorsPerCluster = sectorsPerCluster;
	vbr.reservedSectors = 32;
	vbr.numFats = numFats;
	vbr.mediaType = 0xF0;
	vbr.sectorsLarge = st.st_size / 512;
	vbr.sectorsPerFat = sectorsPerFat;
	vbr.rootCluster = 2;
	vbr.infoSector = 1;
	vbr.backupBootSector = 6;		// linux does it, i'll just follow the example
	vbr.sig = 0x29;
	vbr.volumeSerial = (uint32_t) time(NULL);
	memcpy(vbr.volumeLabel, "NO NAME    ", 11);
	memcpy(vbr.systemID, "FAT32   ", 8);
	
	// "INT 18H"
	vbr.bootCode[0] = 0xCD;
	vbr.bootCode[1] = 0x18;
	
	// mark the entire disk a single non-bootable partition
	vbr.parts[0].flags = 0;
	vbr.parts[0].systemID = 0x0C;
	vbr.parts[0].startLBA = 0;
	vbr.parts[0].partSize = totalSectors;
	
	vbr.bootsig = 0xAA55;
	
	// write to sector 0 and also sector 6 (backup)
	pwrite(fd, &vbr, 512, 0);
	pwrite(fd, &vbr, 512, 6*512);
	
	// FS information sector
	FSInfo info;
	memset(&info, 0, 512);
	info.sig1 = *((const uint32_t*)"RRaA");
	info.sig2 = *((const uint32_t*)"rrAa");
	info.freeClusters = 0xFFFFFFFF;
	info.lastAllocated = 0xFFFFFFFF;
	info.sig3 = 0xAA55;
	pwrite(fd, &info, 512, 512);
	
	// create FATs
	char zeroes[512];
	memset(zeroes, 0, 512);
	
	static uint32_t initFat[3] = {0x0FFFFFFF, 0x0FFFFFFF, 0x0FFFFFFF};
	
	for (i=0; i<numFats; i++)
	{
		off_t pos = 512 * (32 + sectorsPerFat * i);
		
		size_t j;
		for (j=0; j<sectorsPerFat; j++)
		{
			off_t finalPos = pos + 512 * j;
			pwrite(fd, zeroes, 512, finalPos);
		};
		
		pwrite(fd, initFat, 3*4, pos);
	};
	
	// zero out root directory
	off_t pos = 512 * (32 + sectorsPerFat * numFats);
	size_t clusterSize = 512 * sectorsPerCluster;
	char padding[clusterSize];
	memset(padding, 0, clusterSize);
	pwrite(fd, padding, clusterSize, pos);
	
	// create the root directory with "." and ".." entries
	FATDent dent;
	memset(&dent, 0, 32);
	
	memcpy(dent.filename, ".          ", 11);
	dent.clusterLow = 2;
	dent.attr = (1 << 4);
	pwrite(fd, &dent, 32, pos);
	
	dent.filename[1] = '.';
	pwrite(fd, &dent, 32, pos+32);
	
	dent.filename[0] = 0;
	pwrite(fd, &dent, 32, pos+64);
	
	// we done
	close(fd);
	return 0;
};

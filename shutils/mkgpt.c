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
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>

#define	MBR_SIG					0xAA55

#define	GPT_ATTR_REQ				(1UL << 0)

/**
 * GXFS-specific attributes.
 */
#define	GPT_ATTR_BOOT				(1UL << 48)
#define	GPT_ATTR_MNTINFO			(1UL << 49)

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

#define CRCPOLY2 0xEDB88320UL  /* left-right reversal */

static uint32_t crc32(const void *data, size_t n)
{
	const uint8_t *c = (const uint8_t*) data;
	int i, j;
	uint32_t r;

	r = 0xFFFFFFFFUL;
	for (i = 0; i < n; i++)
	{
		r ^= c[i];
		for (j = 0; j < 8; j++)
			if (r & 1) r = (r >> 1) ^ CRCPOLY2;
			else       r >>= 1;
	}
	return r ^ 0xFFFFFFFFUL;
}

int parseGUID(const char *string, uint8_t *buffer)
{
	// first group = 4 bytes little endian (uint32_t)
	int elems = sscanf(string, "%8x", (uint32_t*) buffer);
	if (elems != 1) return -1;
	buffer += 4;
	string += 8;
	
	if (*string++ != '-') return -1;
	
	// next group = 2 bytes little endian (uint16_t)
	elems = sscanf(string, "%4hx", (uint16_t*) buffer);
	if (elems != 1) return -1;
	buffer += 2;
	string += 4;
	
	if (*string++ != '-') return -1;
	
	// and again
	elems = sscanf(string, "%4hx", (uint16_t*) buffer);
	if (elems != 1) return -1;
	buffer += 2;
	string += 4;
	
	if (*string++ != '-') return -1;
	
	// but now it's reversed for some reason
	int count = 2;
	while (count--)
	{
		elems = sscanf(string, "%2hhx", buffer++);
		if (elems != 1) return -1;
		string += 2;
	};
	
	if (*string++ != '-') return -1;
	
	// and final 6 also big endian
	count = 6;
	while (count--)
	{
		elems = sscanf(string, "%2hhx", buffer++);
		if (elems != 1) return -1;
		string += 2;
	};
	
	if (*string != 0) return -1;
	return 0;
};

int parsePartType(const char *type, uint8_t *buffer)
{
	if (strcmp(type, "fatfs") == 0)
	{
		return parseGUID("EBD0A0A2-B9E5-4433-87C0-68B6B72699C7", buffer);
	}
	else if (strcmp(type, "gxfs") == 0)
	{
		return parseGUID("a38a2f2e-61ee-496f-b19f-cda55d34c0f8", buffer);
	}
	else if (strcmp(type, "efisys") == 0)
	{
		return parseGUID("C12A7328-F81F-11D2-BA4B-00A0C93EC93B", buffer);
	}
	else
	{
		return parseGUID(type, buffer);
	};
};

int main(int argc, char *argv[])
{
	int rfd = open("/dev/urandom", O_RDONLY);
	assert(rfd != -1);
	
	if (argc < 2)
	{
		fprintf(stderr, "USAGE:\t%s <device> [commands...]\n", argv[0]);
		fprintf(stderr, "\tCreate a GPT partition table on the specified device.\n");
		return 1;
	};
	
	int fd = open(argv[1], O_RDWR);
	if (fd == -1)
	{
		fprintf(stderr, "%s: failed to open `%s': %s\n", argv[0], argv[1], strerror(errno));
		return 1;
	};
	
	struct stat st;
	if (fstat(fd, &st) != 0)
	{
		fprintf(stderr, "%s: fstat failed: %s\n", argv[0], strerror(errno));
		close(fd);
		return 1;
	};
	
	// everything in MB units
	size_t diskSize = st.st_size / (1024*1024) - 1;
	size_t currentPos = 1;
	size_t nextPart = 0;
	
	if (diskSize < 2)
	{
		fprintf(stderr, "%s: disk smaller than 2MB; cannot create GPT\n", argv[0]);
		close(fd);
		return 1;
	};

	MBR mbr;
	memset(&mbr, 0, 512);
	
	// if no bootloader is specified, default to "boot failure" (INT 18H)
	mbr.bootstrap[0] = 0xCD;
	mbr.bootstrap[1] = 0x18;
	
	// create the fake partition
	size_t sizeInSectors = st.st_size / 512 - 1;
	if (sizeInSectors > 0xFFFFFFFF) sizeInSectors = 0xFFFFFFFF;
	mbr.parts[0].startHead = 0;
	mbr.parts[0].startCylSector = 1;
	mbr.parts[0].systemID = 0xEE;
	mbr.parts[0].endHead = 0xFF;
	mbr.parts[0].endCylSector = 0xFFFF;
	mbr.parts[0].startLBA = 1;
	mbr.parts[0].partSize = sizeInSectors;
	
	// signature
	mbr.sig = MBR_SIG;
	
	// allocate a buffer for the partition table
	// we're using the full 1MB at the start of the disk for the partition table,
	// since partitions must be 1MB-aligned anyway
	size_t numPartEnts = (1 * 1024 * 1024 - 2 * 512) / 128;
	GPTPartition *parts = (GPTPartition*) malloc(sizeof(GPTPartition) * numPartEnts);
	memset(parts, 0, sizeof(GPTPartition) * numPartEnts);
	
	// GXFS partition type
	uint8_t typeGXFS[16];
	int status = parseGUID("a38a2f2e-61ee-496f-b19f-cda55d34c0f8", typeGXFS);
	assert(status == 0);
	
	// parse options
	int i = 2;
	while (argv[i] != NULL)
	{
		if (strcmp(argv[i], "--loader") == 0)
		{
			i++;
			const char *loader = argv[i];
			i++;
			
			if (loader == NULL)
			{
				fprintf(stderr, "%s: --loader expects an argument\n", argv[0]);
				close(fd);
				return 1;
			};
			
			int lfd = open(loader, O_RDONLY);
			if (lfd == -1)
			{
				fprintf(stderr, "%s: cannot open loader file `%s': %s\n", argv[0], loader, strerror(errno));
				close(fd);
				return 1;
			};
			
			read(lfd, mbr.bootstrap, 446);
			close(lfd);
		}
		else if (strcmp(argv[i], "--skip") == 0)
		{
			i++;
			if (argv[i] == NULL)
			{
				fprintf(stderr, "%s: --skip expects an argument\n", argv[0]);
				close(fd);
				return 1;
			};
			
			size_t toSkip = 0;
			sscanf(argv[i], "%lu", &toSkip);
			
			i++;
			if (toSkip == 0)
			{
				fprintf(stderr, "%s: invalid skip amount\n", argv[0]);
				close(fd);
				return 1;
			};
			
			if (toSkip > (diskSize-currentPos))
			{
				fprintf(stderr, "%s: skipping past end of disk!\n", argv[0]);
				close(fd);
				return 1;
			};
			
			currentPos += toSkip;
		}
		else if (strcmp(argv[i], "--part") == 0)
		{
			if (nextPart == numPartEnts)
			{
				fprintf(stderr, "%s: too many partitions\n", argv[0]);
				close(fd);
				return 1;
			};
			
			i++;
			int boot = 0;
			int mntinfo = 0;
			int req = 0;
			size_t partSize = 0;
			uint8_t type[16];
			memcpy(type, typeGXFS, 16);
			uint16_t name[36];
			memset(name, 0, 36*2);
			memcpy(name, "N\0O\0 \0N\0A\0M\0E\0\0\0", 16);
			
			while (argv[i] != NULL && argv[i][0] != '-')
			{
				const char *opt = argv[i++];
				
				if (strcmp(opt, "boot") == 0)
				{
					boot = 1;
				}
				else if (strcmp(opt, "mntinfo") == 0)
				{
					mntinfo = 1;
				}
				else if (strcmp(opt, "req") == 0)
				{
					req = 1;
				}
				else if (memcmp(opt, "type=", 5) == 0)
				{
					status = parsePartType(&opt[5], type);
					if (status != 0)
					{
						fprintf(stderr, "%s: unknown partition type `%s'\n", argv[0], &opt[5]);
						close(fd);
						return 1;
					};
				}
				else if (memcmp(opt, "size=", 5) == 0)
				{
					partSize = 0;
					sscanf(&opt[5], "%lu", &partSize);
					
					if (partSize == 0)
					{
						fprintf(stderr, "%s: invalid partition size (%s)\n", argv[0], opt);
						close(fd);
						return 1;
					};
					
					if (partSize > (diskSize-currentPos))
					{
						fprintf(stderr, "%s: partitions do not fit on disk!\n", argv[0]);
						close(fd);
						return 1;
					};
				}
				else if (memcmp(opt, "name=", 5) == 0)
				{
					const char *asciiName = &opt[5];
					if (strlen(asciiName) >= 36)
					{
						fprintf(stderr, "%s: partition name `%s' too long\n", argv[0], asciiName);
						close(fd);
						return 1;
					};
					
					memset(name, 0, 36*2);
					uint16_t *put = name;
					
					while (*asciiName != 0)
					{
						char c = *asciiName++;
						*put++ = (uint16_t) (c & 0x7F);
					};
				}
				else
				{
					fprintf(stderr, "%s: unknown partition option `%s'\n", argv[0], opt);
					close(fd);
					return 1;
				};
			};
			
			if (partSize == 0)
			{
				partSize = diskSize - currentPos;
			};
			
			memcpy(parts[nextPart].type, type, 16);
			read(rfd, parts[nextPart].partid, 16);
			parts[nextPart].startLBA = currentPos * 2UL * 1024UL;
			parts[nextPart].endLBA = parts[nextPart].startLBA + partSize * 2UL * 1024UL - 1UL;
			if (req) parts[nextPart].attr |= GPT_ATTR_REQ;
			
			if (memcmp(type, typeGXFS, 16) == 0)
			{
				if (boot) parts[nextPart].attr |= GPT_ATTR_BOOT;
				if (mntinfo) parts[nextPart].attr |= GPT_ATTR_MNTINFO;
			};
			
			memcpy(parts[nextPart].partName, name, 36*2);

			currentPos += partSize;
			nextPart++;
		}
		else
		{
			fprintf(stderr, "%s: unknown command `%s'\n", argv[0], argv[i]);
			close(fd);
			return 1;
		};
	};
	
	// write the MBR
	pwrite(fd, &mbr, 512, 0);
	
	// now create the GPT header
	char sector[512];
	memset(sector, 0, 512);
	GPTHeader *header = (GPTHeader*) sector;
	
	header->sig = *((const uint64_t*)"EFI PART");
	header->rev = 0x00010000;
	header->headerSize = 92;
	header->myLBA = 1;
	header->altLBA = (diskSize + 1) * 2 * 1024 - 1;
	header->firstUseableLBA = 2 * 1024;
	header->lastUseableLBA = diskSize * 2 * 1024;
	read(rfd, header->diskGUID, 16);
	header->partListLBA = 2;
	header->numPartEnts = numPartEnts;
	header->partEntSize = 128;
	header->partArrayCRC32 = crc32(parts, sizeof(GPTPartition) * numPartEnts);
	header->headerCRC32 = crc32(header, 92);
	pwrite(fd, sector, 512, 512);
	
	// alternate header
	header->myLBA = header->altLBA;
	header->altLBA = 1;
	header->headerCRC32 = 0;
	header->partListLBA = diskSize * 2 * 1024 + 1;
	header->headerCRC32 = crc32(header, 92);
	pwrite(fd, sector, 512, (diskSize+1) * 1024 * 1024 - 512);
	
	// write partitions
	pwrite(fd, parts, sizeof(GPTPartition) * numPartEnts, 1024);
	pwrite(fd, parts, sizeof(GPTPartition) * numPartEnts, diskSize * 1024 * 1024 + 512);
	
	close(fd);
	return 0;
};

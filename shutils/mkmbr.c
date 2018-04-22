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

#define	MBR_SIG					0xAA55

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

uint8_t parsePartType(const char *type)
{
	if (strcmp(type, "fatfs") == 0)
	{
		return 0x0C;
	}
	else if (strcmp(type, "gxfs") == 0)
	{
		return 0x7F;
	}
	else
	{
		uint8_t result = 0;
		sscanf(type, "%hhx", &result);
		return result;
	};
};

int main(int argc, char *argv[])
{
	if (argc < 2)
	{
		fprintf(stderr, "USAGE:\t%s <device> [commands...]\n", argv[0]);
		fprintf(stderr, "\tCreate an MBR partition table on the specified device.\n");
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
	size_t diskSize = st.st_size / (1024*1024);
	size_t currentPos = 1;
	int nextPart = 0;
	
	if (diskSize < 2)
	{
		fprintf(stderr, "%s: disk smaller than 2MB; cannot create MBR\n", argv[0]);
		close(fd);
		return 1;
	};
	
	if (diskSize > 1024*1024*2)
	{
		diskSize = 1024 * 1024 * 2;
		fprintf(stderr, "%s: warning: disk larger than 2TB; limiting to 2TB!\n", argv[0]);
	};
	
	MBR mbr;
	memset(&mbr, 0, 512);
	
	// if no bootloader is specified, default to "boot failure" (INT 18H)
	mbr.bootstrap[0] = 0xCD;
	mbr.bootstrap[1] = 0x18;
	
	// signature
	mbr.sig = MBR_SIG;
	
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
			if (nextPart == 4)
			{
				fprintf(stderr, "%s: too many partitions\n", argv[0]);
				close(fd);
				return 1;
			};
			
			i++;
			int boot = 0;
			size_t partSize = 0;
			uint8_t type = 0x7F;
			
			while (argv[i] != NULL && argv[i][0] != '-')
			{
				const char *opt = argv[i++];
				
				if (strcmp(opt, "boot") == 0)
				{
					boot = 1;
				}
				else if (memcmp(opt, "type=", 5) == 0)
				{
					type = parsePartType(&opt[5]);
					if (type == 0)
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
			
			if (boot) mbr.parts[nextPart].flags = 0x80;
			mbr.parts[nextPart].systemID = type;
			mbr.parts[nextPart].startLBA = currentPos * 2 * 1024;
			mbr.parts[nextPart].partSize = partSize * 2 * 1024;
			
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
	
	// flush
	pwrite(fd, &mbr, 512, 0);
	close(fd);
	return 0;
};

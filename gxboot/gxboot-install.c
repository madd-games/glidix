/*
	Glidix bootloader (gxboot)

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
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#define	MBR_BIN					"/usr/share/gxboot/mbr.bin"
#define	VBR_BIN					"/usr/share/gxboot/vbr.bin"

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

char vbrBuffer[0x200000];

int main(int argc, char *argv[])
{
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
	
	int bootPart;
	for (bootPart=0; bootPart<4; bootPart++)
	{
		if (mbr.parts[bootPart].flags == 0x80)
		{
			break;
		};
	};
	
	if (bootPart == 4)
	{
		fprintf(stderr, "%s: no boot partition found on device %s\n", argv[0], argv[1]);
		close(drive);
		return 1;
	};
	
	int fd = open(MBR_BIN, O_RDONLY);
	if (fd == -1)
	{
		fprintf(stderr, "%s: cannot open " MBR_BIN ": %s\n", argv[0], strerror(errno));
		close(drive);
		return 1;
	};
	
	ssize_t size = read(fd, &mbr, 512);
	if (size == -1)
	{
		fprintf(stderr, "%s: cannot read " MBR_BIN ": %s\n", argv[0], strerror(errno));
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
	
	fd = open(VBR_BIN, O_RDONLY);
	if (fd == -1)
	{
		fprintf(stderr, "%s: cannot open " VBR_BIN ": %s\n", argv[0], strerror(errno));
		close(drive);
		return 1;
	};
	
	ssize_t vbrSize = read(fd, vbrBuffer, 0x200000);
	if (vbrSize == -1)
	{
		fprintf(stderr, "%s: cannot read " VBR_BIN ": %s\n", argv[0], strerror(errno));
		close(drive);
		close(fd);
		return 1;
	};
	
	close(fd);
	
	if (pwrite(drive, vbrBuffer, vbrSize, 512 * mbr.parts[bootPart].startLBA) != vbrSize)
	{
		fprintf(stderr, "%s: failed to write VBR: %s\n", argv[0], strerror(errno));
		close(drive);
		return 1;
	};
	
	close(drive);
	return 0;
};

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

#include "gxboot.h"

#define	GXBOOT_DEBUG

#ifdef GXBOOT_DEBUG
#	define	dtermput		termput
#else
#	define	dtermput(...)
#endif

int consoleX, consoleY;
char *vidmem = (char*) 0xB8000;
extern dword_t part_start;
qword_t blockBase;			/* LBA of start of block table */

void termput(const char *str)
{
	for (; *str!=0; str++)
	{
		if (*str == '\n')
		{
			consoleX = 0;
			consoleY++;
		}
		else
		{
			vidmem[2 * (consoleY * 80 + consoleX)] = *str;
			vidmem[2 * (consoleY * 80 + consoleX) + 1] = 0x07;
			
			if ((++consoleX) == 80)
			{
				consoleX = 0;
				consoleY++;
			};
		};
	};
};

void readBlock(qword_t index, void *buffer)
{
	dap.lba = blockBase + index;
	biosRead();
	memcpy(buffer, sectorBuffer, 512);
};

void bmain()
{
	consoleX = 0;
	consoleY = 0;
	memset(vidmem, 0, 80*25*2);
	
	dtermput("Validating superblock...\n");
	blockBase = (qword_t) part_start + 0x1000;
	dap.offset = (word_t) (dword_t) sectorBuffer;
	dap.numSectors = 1;
	
	Superblock sb;
	readBlock(0, &sb);
	
	if (memcmp(sb.sbMagic, "GLIDIXFS", 8) != 0)
	{
		termput("ERROR: Superblock invalid (no GLIDIXFS magic value)\n");
		return;
	};
	
	qword_t sum = 0;
	qword_t *scan;
	
	for (scan=(qword_t*)&sb; scan<=&sb.sbChecksum; scan++)
	{
		sum += *scan;
	};
	
	if (sum != 0)
	{
		termput("ERROR: Superblock invalid (bad checksum)\n");
		return;
	};
	
	dtermput("Superblock OK\n");
};

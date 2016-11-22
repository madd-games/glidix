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

void bmain()
{
	consoleX = 0;
	consoleY = 0;
	memset(vidmem, 0, 80*25*2);
	
	dtermput("Validating superblock... ");
	blockBase = (qword_t) part_start + 0x1000;
	dap.offset = (word_t) (dword_t) sectorBuffer;
	dap.numSectors = 1;
	
	Superblock sb;
	readBlock(0, &sb);
	
	if (memcmp(sb.sbMagic, "GLIDIXFS", 8) != 0)
	{
		dtermput("FAILED\n");
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
		dtermput("FAILED\n");
		termput("ERROR: Superblock invalid (bad checksum)\n");
		return;
	};
	
	dtermput("OK\n");
	
	dtermput("Finding /boot/vmglidix.tar... ");
	FileHandle initrd;
	if (openFile(&initrd, "/boot/vmglidix.tar") != 0)
	{
		dtermput("FAILED\n");
		termput("ERROR: Cannot find /boot/vmglidix.tar\n");
		return;
	};
	
	dtermput("OK\n");
	
	dtermput("Looking for kernel.so... ");
	qword_t pos = 0;
	qword_t size;
	int found = 0;
	
	while (pos < initrd.inode.inoSize)
	{
		TARFileHeader header;
		
		readFile(&initrd, &header, sizeof(TARFileHeader), pos);
		
		size = 0;
		const char *scan;
		for (scan=header.size; *scan!=0; scan++)
		{
			size = (size << 3) + ((*scan)-'0');
		};
		
		if (strcmp(header.filename, "kernel.so") == 0)
		{
			found = 1;
			break;
		};
		
		size = (size + 511) & ~511;
		pos += 512 * (size + 1);
	};
	
	pos += 512;
	if (!found)
	{
		dtermput("FAILED\n");
		termput("ERROR: Cannot find kernel.so in /boot/vmglidix.tar!\n");
		return;
	};
	
	dtermput("OK\n");
	
	Elf64_Ehdr header;
	readFile(&initrd, &header, sizeof(Elf64_Ehdr), pos);
	
	dtermput("Validating ELF64 header... ");
	if (memcmp(header.e_ident, "\x7f" "ELF", 4) != 0)
	{
		dtermput("FAILED\n");
		termput("ERROR: Invalid ELF64 signature in kernel.so\n");
		return;
	};

	if (header.e_ident[EI_CLASS] != ELFCLASS64)
	{
		dtermput("FAILED\n");
		termput("ERROR: kernel.so is not an ELF64 file");
		return;
	};

	if (header.e_ident[EI_DATA] != ELFDATA2LSB)
	{
		dtermput("FAILED\n");
		termput("ERROR: kernel.so is not little endian");
		return;
	};

	if (header.e_ident[EI_VERSION] != 1)
	{
		dtermput("FAILED\n");
		termput("kernel.so is not ELF64 version 1");
		return;
	};

	if (header.e_type != ET_EXEC)
	{
		dtermput("FAILED\n");
		termput("ERROR: kernel.so is not an executable");
		return;
	};

	if (header.e_phentsize != sizeof(Elf64_Phdr))
	{
		dtermput("FAILED\n");
		termput("ERROR: kernel.so program header size unexpected");
		return;
	};
	
	dtermput("OK\n");
	
	dtermput("Loading initrd to memory... ");
	Elf64_Phdr phead;
	void *initrdStart = NULL;
	
	int i;
	for (i=0; i<header.e_phnum; i++)
	{
		readFile(&initrd, &phead, sizeof(Elf64_Phdr), pos + header.e_phoff + i * sizeof(Elf64_Phdr));
		
		if (phead.p_type == PT_GLIDIX_INITRD)
		{
			initrdStart = (void*) (dword_t) phead.p_paddr;
			break;
		};
	};
	
	if (initrdStart == NULL)
	{
		dtermput("FAILED\n");
		termput("ERROR: no initrd segment in kernel.so");
		return;
	};
	
	readFile(&initrd, initrdStart, size, 0);
	dtermput("OK\n");
	
	char *elfPtr = (char*) initrdStart + pos;
	Elf64_Ehdr *elfHeader = (Elf64_Ehdr*) elfPtr;
	
};

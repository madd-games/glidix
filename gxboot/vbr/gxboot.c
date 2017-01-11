/*
	Glidix bootloader (gxboot)

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

void termputd(dword_t num)
{
	char buffer[64];
	char *put = &buffer[63];
	*put = 0;
	
	do
	{
		*--put = '0' + (num % 10);
		num /= 10;
	} while (num != 0);
	
	termput(put);
};

void termputp64(qword_t addr)
{
	termput("0x");
	char buffer[64];
	char *put = &buffer[63];
	*put = 0;
	
	do
	{
		qword_t digit = (addr % 16);
		if (digit < 10)
		{
			*--put = '0' + digit;
		}
		else
		{
			*--put = 'A' + digit - 10;
		};
		addr /= 16;
	} while (addr != 0);
	
	termput(put);
};

dword_t placement;
void *balloc(dword_t align, dword_t size)
{
	placement = (placement + align - 1) & ~(align-1);
	dword_t result = placement;
	placement += size;
	return (void*) result;
};

qword_t *pml4;

qword_t *getPageEntry(qword_t addr)
{
	qword_t pageIndex = (addr >> 12) & 0x1FFULL;
	qword_t ptIndex = (addr >> 21) & 0x1FFULL;
	qword_t pdIndex = (addr >> 30) & 0x1FFULL;
	qword_t pdptIndex = (addr >> 39) & 0x1FFULL;
	
	if (pdptIndex == 511)
	{
		termput("ERROR: Attempting to access the top of 64-bit virtual memory, reserved for recursive mapping!");
		while (1) asm volatile ("cli; hlt");
	};
	
	qword_t *pdpt;
	if (pml4[pdptIndex] == 0)
	{
		pdpt = (qword_t*) balloc(0x1000, 0x1000);
		memset(pdpt, 0, 0x1000);
		pml4[pdptIndex] = (qword_t)(dword_t)pdpt | PT_PRESENT | PT_WRITE;
	}
	else
	{
		pdpt = (qword_t*) ((dword_t) (pml4[pdptIndex] & ~0xFFFULL));
	};
	
	qword_t *pd;
	if (pdpt[pdIndex] == 0)
	{
		pd = (qword_t*) balloc(0x1000, 0x1000);
		memset(pd, 0, 0x1000);
		pdpt[pdIndex] = (qword_t)(dword_t)pd | PT_PRESENT | PT_WRITE;
	}
	else
	{
		pd = (qword_t*) ((dword_t) (pdpt[pdIndex] & ~0xFFFULL));
	};
	
	qword_t *pt;
	if (pd[ptIndex] == 0)
	{
		pt = (qword_t*) balloc(0x1000, 0x1000);
		memset(pt, 0, 0x1000);
		pd[ptIndex] = (qword_t)(dword_t)pt | PT_PRESENT | PT_WRITE;
	}
	else
	{
		pt = (qword_t*) ((dword_t) (pd[ptIndex] & ~0xFFF));
	};
	
	return &pt[pageIndex];
};

void mmap(qword_t vaddr, dword_t paddr, dword_t size)
{
	size = (size + 0xFFF) & ~0xFFF;
	
	termput("mmap ");
	termputp64(vaddr);
	termput(" -> ");
	termputp64(paddr);
	termput(" (");
	termputp64((qword_t) size);
	termput(")\n");
	
	qword_t i;
	for (i=0; i<size; i+=0x1000)
	{	
		qword_t *pte = getPageEntry(vaddr+i);
		*pte = (paddr+i) | PT_PRESENT | PT_WRITE;
	};
};

const char *strings;
Elf64_Sym *symtab;
int numSyms;

qword_t getSymbol(const char *name)
{
	int i;
	for (i=0; i<numSyms; i++)
	{
		const char *symname = &strings[symtab[i].st_name];
		if (strcmp(symname, name) == 0)
		{
			return symtab[i].st_value;
		};
	};
	
	return 0;
};

void* virt2phys(qword_t virt)
{
	qword_t *pte = getPageEntry(virt);
	if (*pte == 0)
	{
		return NULL;
	};
	
	dword_t addr = ((dword_t)(*pte) & (~0xFFF)) | (virt & 0xFFF);
	return (void*) addr;
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
	
	readFile(&initrd, initrdStart, initrd.inode.inoSize, 0);
	dtermput("OK\n");
	
	char *elfPtr = (char*) initrdStart + pos;
	Elf64_Ehdr *elfHeader = (Elf64_Ehdr*) elfPtr;
	Elf64_Phdr *pheads = (Elf64_Phdr*) &elfPtr[elfHeader->e_phoff];
	
	// first find the top address
	qword_t topAddr = 0;
	for (i=0; i<elfHeader->e_phnum; i++)
	{
		if ((pheads[i].p_type == PT_LOAD) || (pheads[i].p_type == PT_GLIDIX_INITRD))
		{
			if ((pheads[i].p_paddr + pheads[i].p_memsz) > topAddr)
			{
				topAddr = pheads[i].p_paddr + pheads[i].p_memsz;
			};
		};
	};
	
	// initialize placement allocator
	placement = (dword_t) topAddr;
	
	// allocate space for the PML4
	pml4 = (qword_t*) balloc(0x1000, 0x1000);
	memset(pml4, 0, 0x1000);
	
	// recursive mapping
	pml4[511] = (qword_t) (dword_t) pml4 | PT_WRITE | PT_PRESENT;
	
	// now load the segments into memory
	for (i=0; i<elfHeader->e_phnum; i++)
	{
		if (pheads[i].p_type == PT_LOAD)
		{
			if (pheads[i].p_paddr & 0xFFF)
			{
				termput("ERROR: kernel.so contains a program header with non-page-aligned physical address\n");
				return;
			};
			
			memset((void*) (dword_t) pheads[i].p_paddr, 0, pheads[i].p_memsz);
			memcpy((void*) (dword_t) pheads[i].p_paddr, elfPtr + pheads[i].p_offset, pheads[i].p_filesz);
			
			mmap(pheads[i].p_vaddr, pheads[i].p_paddr, pheads[i].p_memsz);
		}
		else if ((pheads[i].p_type == PT_GLIDIX_MMAP) || (pheads[i].p_type == PT_GLIDIX_INITRD))
		{
			if (pheads[i].p_paddr & 0xFFF)
			{
				termput("ERROR: kernel.so contains a program header with non-page-aligned physical address!");
				return;
			};
			
			mmap(pheads[i].p_vaddr, pheads[i].p_paddr, pheads[i].p_memsz);
		};
	};
	
	// find the string and symbol table sections
	Elf64_Shdr *sections = (Elf64_Shdr*) (elfPtr + elfHeader->e_shoff);
	
	// find the symbol table
	symtab = NULL;
	numSyms = 0;
	strings = NULL;

	for (i=0; i<elfHeader->e_shnum; i++)
	{
		if (sections[i].sh_type == SHT_SYMTAB)
		{
			symtab = (Elf64_Sym*) (elfPtr + sections[i].sh_offset);
			numSyms = sections[i].sh_size / sections[i].sh_entsize;
			strings = elfPtr + sections[sections[i].sh_link].sh_offset;
		};
	};
	
	if (symtab == NULL)
	{
		termput("ERROR: kernel.so does not contain a symbol table!");
		return;
	};
	
	// get the bottom of the stack
	qword_t stack = getSymbol("_stackBottom");
	termput("_stackBottom at ");
	termputp64(stack);
	termput("\n");
	
	if (stack == 0)
	{
		termput("ERROR: kernel.so does not contain the _stackBottom symbol!");
		return;
	};
	
	// allocate 2KB for the memory map
	qword_t mmapBase = (stack - 0x800ULL) & ~0xFULL;
	dword_t mmapSize = 0;
	
	// build the memory map
	MemoryMap *mmapPut = (MemoryMap*) virt2phys(mmapBase);
	if (mmapPut == NULL)
	{
		termput("ERROR: cannot resolve address for memory map! ");
		termputp64(mmapBase);
		return;
	};
	
	dword_t entIndex = 0;

	dtermput("Loading memory map... ");
	do
	{
		int ok = 1;
		entIndex = biosGetMap(entIndex, &mmapPut->baseAddr, &ok);
		if (!ok) break;
		
		mmapPut->size = 20;
		mmapPut++;
		mmapSize += 24;
	} while (entIndex != 0);
	
	dtermput("OK\n");
	
	// now allocate the information structure
	qword_t kernelInfoVirt = (mmapBase - sizeof(KernelInfo)) & (~0xFULL);
	KernelInfo *kinfo = (KernelInfo*) virt2phys(kernelInfoVirt);
	
	if (kinfo == NULL)
	{
		termput("ERROR: cannot resolve address for kernel info!");
		return;
	};
	
	kinfo->features = 0;
	kinfo->kernelMain = getSymbol("kmain");
	if (kinfo->kernelMain == 0)
	{
		termput("ERROR: kernel.so does not contain the kmain symbol!");
		return;
	};
	
	kinfo->gdtPointerVirt = getSymbol("GDTPointer");
	if (kinfo->gdtPointerVirt == 0)
	{
		termput("ERROR: kernel.so does not contain the GDTPointer symbol!");
		return;
	};
	
	kinfo->pml4Phys = (dword_t) pml4;
	kinfo->mmapSize = mmapSize;
	kinfo->mmapVirt = mmapBase;
	kinfo->initrdSize = initrd.inode.inoSize;
	kinfo->end = (qword_t) (dword_t) balloc(0x1000, 0);
	kinfo->initrdSymtabOffset = (qword_t) (dword_t) ((char*) symtab - (char*) initrdStart);
	kinfo->initrdStrtabOffset = (qword_t) (dword_t) ((char*) strings - (char*) initrdStart);
	kinfo->numSymbols = numSyms;
	
	// identity-map our area
	// we allocate this AFTER passing the end pointer to the kernel, because it's temporary.
	// the kernel will destroy those page tables once we pass control to it, and they are no
	// longer needed, so we save some memory
	mmap(0x5000, 0x5000, 0xFB000);
	
	// go to 64-bit mode
	go64(kinfo, kernelInfoVirt);
};

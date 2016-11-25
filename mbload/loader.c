/*
	Glidix MBLoader -- Tool for loading Glidix from a multiboot module

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

#include "loader.h"

#define	LOAD_ADDR_MIN				0x100000
#define	LOAD_ADDR_MAX				0xE00000

void strcpy(char *dest, const char *src)
{
	while (*src != 0)
	{
		*dest++ = *src++;
	};
};

int strcmp(const char *a, const char *b)
{
	while ((*a != 0) && (*b != 0))
	{
		if (*a != *b) return 1;
		a++;
		b++;
	};
	
	return (*a) - (*b);
};

int memcmp(const void *a_, const void *b_, size_t sz)
{
	const char *a = (const char*) a_;
	const char *b = (const char*) b_;
	
	while (sz--)
	{
		if (*a != *b) return 1;
		a++;
		b++;
	};
	
	return 0;
};

void memmove(void *dest_, const void *src_, size_t sz)
{
	char *dest = (char*) dest_;
	const char *src = (const char*) src_;
	
	if (dest < src)
	{
		while (sz--)
		{
			*dest++ = *src++;
		};
	}
	else
	{
		dest += sz;
		src += sz;
		
		while (sz--)
		{
			*--dest = *--src;
		};
	};
};

void memcpy(void *dest_, const void *src_, size_t sz)
{
	char *dest = (char*) dest_;
	const char *src = (char*) src_;
	
	while (sz--)
	{
		*dest++ = *src++;
	};
};

void memset(void *dest_, int byte, size_t sz)
{
	char *dest = (char*) dest_;
	
	while (sz--)
	{
		*dest++ = (char) byte;
	};
};

void mb_hang()
{
	while (1)
	{
		asm volatile ("cli; hlt");
	};
};

void mb_crash(const char *message, uint64_t errcode)
{
	static char hex[16] = "0123456789ABCDEF";
	char *videoram = (char*) 0xB8000;
	
	strcpy(videoram, "!\7 \7B\7O\7O\7T\7 \7F\7A\7I\7L\7E\7D\7 \7!\7");
	
	char *put = &videoram[160];
	while (*message != 0)
	{
		*put++ = *message++;
		*put++ = 0x07;
	};
	
	put = &videoram[320];
	*put++ = '0';
	*put++ = 0x07;
	*put++ = 'x';
	*put++ = 0x07;
	
	put += 32;
	int count;
	for (count=0; count<8; count++)
	{
		uint8_t lo = (uint8_t) errcode & 0xF;
		uint8_t hi = (uint8_t) (errcode >> 4) & 0xF;
		
		errcode >>= 8;
		
		*--put = 0x07;
		*--put = hex[lo];
		*--put = 0x07;
		*--put = hex[hi];
	};
	
	mb_hang();
};

const char *mb_find_file(const char *scan, const char *initrdEnd, const char *filename)
{
	const TARFileHeader *header = (const TARFileHeader*) scan;
	while (scan < initrdEnd)
	{
		if (strcmp(header->filename, filename) == 0)
		{
			return scan + 512;
		};
		
		uint32_t fileSize = 0;
		const char *fetch = (const char*) header->size;
		
		while (*fetch != 0)
		{
			fileSize <<= 3;
			fileSize += ((*fetch)-'0');
		};
		
		fileSize = (fileSize + 511) & (~511);
		scan += fileSize + 512;
		header = (const TARFileHeader*) scan;
	};
	
	return NULL;
};

uint32_t placement;
uint64_t *pml4;

void *mb_alloc(uint32_t size, uint32_t align)
{
	placement = (placement + align - 1) & ~(align-1);
	void *out = (void*) placement;
	placement += size;
	
	if (placement > LOAD_ADDR_MAX)
	{
		mb_crash("Ran out of heap space!", placement);
	};
	
	return out;
};

uint64_t *mb_get_page_entry(uint64_t addr)
{
	uint64_t pageIndex = (addr >> 12) & 0x1FF;
	uint64_t ptIndex = (addr >> 21) & 0x1FF;
	uint64_t pdIndex = (addr >> 30) & 0x1FF;
	uint64_t pdptIndex = (addr >> 39) & 0x1FF;
	
	if (pdptIndex == 511)
	{
		mb_crash("Attempting to access the top of 64-bit virtual memory, reserved for recursive mapping!", addr);
	};
	
	uint64_t *pdpt;
	if (pml4[pdptIndex] == 0)
	{
		pdpt = (uint64_t*) mb_alloc(0x1000, 0x1000);
		memset(pdpt, 0, 0x1000);
		pml4[pdptIndex] = (uint64_t)(uint32_t)pdpt | PT_PRESENT | PT_WRITE;
	}
	else
	{
		pdpt = (uint64_t*) ((uint32_t) (pml4[pdptIndex] & ~0xFFF));
	};
	
	uint64_t *pd;
	if (pdpt[pdIndex] == 0)
	{
		pd = (uint64_t*) mb_alloc(0x1000, 0x1000);
		memset(pd, 0, 0x1000);
		pdpt[pdIndex] = (uint64_t)(uint32_t)pd | PT_PRESENT | PT_WRITE;
	}
	else
	{
		pd = (uint64_t*) ((uint32_t) (pdpt[pdIndex] & ~0xFFF));
	};
	
	uint64_t *pt;
	if (pd[ptIndex] == 0)
	{
		pt = (uint64_t*) mb_alloc(0x1000, 0x1000);
		memset(pt, 0, 0x1000);
		pd[ptIndex] = (uint64_t)(uint32_t)pt | PT_PRESENT | PT_WRITE;
	}
	else
	{
		pt = (uint64_t*) ((uint32_t) (pd[ptIndex] & ~0xFFF));
	};
	
	return &pt[pageIndex];
};

void mb_mmap(uint64_t vaddr, uint32_t paddr, uint32_t size)
{
	size = (size + 0xFFF) & ~0xFFF;
	
	uint64_t i;
	for (i=0; i<size; i+=0x1000)
	{
		uint64_t *pte = mb_get_page_entry(vaddr+i);
		*pte = (paddr+i) | PT_PRESENT | PT_WRITE;
	};
};

const char *strings;
Elf64_Sym *symtab;
int numSyms;

uint64_t mb_symbol(const char *name)
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

void* mb_virt_to_phys(uint64_t virt)
{
	uint64_t *pte = mb_get_page_entry(virt);
	if (*pte == 0)
	{
		return NULL;
	};
	
	uint32_t addr = ((uint32_t)(*pte) & (~0xFFF)) | (virt & 0xFFF);
	return (void*) addr;
};

void mb_go64(uint64_t kernelInfoVirt, KernelInfo *kinfo);

void mb_main(MultibootInfo *info)
{	
	if (info->modsCount != 1)
	{
		mb_crash("Module count is not 1", 0);
	};
	
	MultibootModule *mod = (MultibootModule*) info->modsAddr;
	const char *initrdStart = (const char*) mod->modStart;
	const char *initrdEnd = (const char*) mod->modEnd;
	
	const char *kernelData = mb_find_file(initrdStart, initrdEnd, "kernel.so");
	if (kernelData == NULL)
	{
		mb_crash("Cannot find kernel.so!", 0);
	};
	
	Elf64_Ehdr *elfHeader = (Elf64_Ehdr*) kernelData;
	
	if (memcmp(elfHeader->e_ident, "\x7f" "ELF", 4) != 0)
	{
		mb_crash("Invalid ELF64 signature in kernel.so", 0);
	};

	if (elfHeader->e_ident[EI_CLASS] != ELFCLASS64)
	{
		mb_crash("kernel.so is not an ELF64 file", 0);
	};

	if (elfHeader->e_ident[EI_DATA] != ELFDATA2LSB)
	{
		mb_crash("kernel.so is not little endian", 0);
	};

	if (elfHeader->e_ident[EI_VERSION] != 1)
	{
		mb_crash("kernel.so is not ELF64 version 1", 0);
	};

	if (elfHeader->e_type != ET_EXEC)
	{
		mb_crash("kernel.so is not an executable", 0);
	};

	if (elfHeader->e_phentsize != sizeof(Elf64_Phdr))
	{
		mb_crash("kernel.so program header size unexpected", 0);
	};
	
	// find the PT_GLIDIX_INITRD header
	Elf64_Phdr *phdrs = (Elf64_Phdr*) (kernelData + elfHeader->e_phoff);
	Elf64_Phdr *phInitrd = NULL;
	int i;
	for (i=0; i<elfHeader->e_phnum; i++)
	{
		if (phdrs[i].p_type == PT_GLIDIX_INITRD)
		{
			phInitrd = &phdrs[i];
			break;
		};
	};
	
	if (phInitrd == NULL)
	{
		mb_crash("No initrd area defined in kernel.so!", 0);
	};
	
	size_t initrdSize = initrdEnd - initrdStart;
	if (phInitrd->p_memsz < initrdSize)
	{
		mb_crash("kernel.so does not reserve enough memory for the initrd!", initrdSize);
	};
	
	if ((phInitrd->p_paddr < LOAD_ADDR_MIN) || (phInitrd->p_paddr+initrdSize > LOAD_ADDR_MAX))
	{
		mb_crash("initrd load address is outside of allowed range!", phInitrd->p_paddr);
	};
	
	if ((phInitrd->p_paddr & 0xFFF) != 0)
	{
		mb_crash("initrd load address is not page-aligned!", phInitrd->p_paddr);
	};
	
	char *newInitrd = (char*)(uint32_t)phInitrd->p_paddr;
	memmove(newInitrd, initrdStart, initrdSize);
	
	// now that we've moved the initrd, find the kernel again, and this time load it
	initrdStart = newInitrd;
	initrdEnd = initrdStart + initrdSize;
	
	kernelData = mb_find_file(initrdStart, initrdEnd, "kernel.so");
	if (kernelData == NULL)
	{
		mb_crash("Cannot find kernel.so after moving the initrd! This is a bug!", 0);
	};
	
	elfHeader = (Elf64_Ehdr*) kernelData;
	
	// check the validity of all program headers, and find the top address as we go along
	phdrs = (Elf64_Phdr*) (kernelData + elfHeader->e_phoff);
	
	uint32_t topAddr = 0;
	
	for (i=0; i<elfHeader->e_phnum; i++)
	{
		if ((phdrs[i].p_paddr & 0xFFF) != (phdrs[i].p_vaddr & 0xFFF))
		{
			mb_crash("Virtual and physical addresses of a program header are not congruent!", i);
		};
		
		if ((phdrs[i].p_paddr & 0xFFF) != (phdrs[i].p_offset & 0xFFF))
		{
			mb_crash("Address and offset of a program header are not congruent!", i);
		};
		
		if ((phdrs[i].p_type == PT_LOAD) || (phdrs[i].p_type == PT_GLIDIX_INITRD))
		{
			if ((phdrs[i].p_paddr < LOAD_ADDR_MIN) || (phdrs[i].p_paddr+phdrs[i].p_memsz > LOAD_ADDR_MAX))
			{
				mb_crash("Physical address of a program header is outside the allowed range!", phdrs[i].p_paddr);
			};
		
			uint32_t top = (uint32_t) phdrs[i].p_paddr + (uint32_t) phdrs[i].p_memsz;
			if (top > topAddr) topAddr = top;
		};
	};
	
	placement = topAddr;
	
	pml4 = (uint64_t*) mb_alloc(0x1000, 0x1000);
	memset(pml4, 0, 0x1000);
	
	// recursive mapping
	pml4[511] = (uint64_t) (uint32_t) pml4 | PT_PRESENT | PT_WRITE;
	
	// do the loading
	for (i=0; i<elfHeader->e_phnum; i++)
	{
		if (phdrs[i].p_type == PT_LOAD)
		{
			if (phdrs[i].p_paddr & 0xFFF)
			{
				mb_crash("kernel.so contains a program header with non-page-aligned physical address!", phdrs[i].p_paddr);
			};
			
			memset((void*) (uint32_t) phdrs[i].p_paddr, 0, phdrs[i].p_memsz);
			memcpy((void*) (uint32_t) phdrs[i].p_paddr, kernelData + phdrs[i].p_offset, phdrs[i].p_filesz);
			
			mb_mmap(phdrs[i].p_vaddr, phdrs[i].p_paddr, phdrs[i].p_memsz);
		}
		else if ((phdrs[i].p_type == PT_GLIDIX_MMAP) || (phdrs[i].p_type == PT_GLIDIX_INITRD))
		{
			if (phdrs[i].p_paddr & 0xFFF)
			{
				mb_crash("kernel.so contains a program header with non-page-aligned physical address!", phdrs[i].p_paddr);
			};
			
			mb_mmap(phdrs[i].p_vaddr, phdrs[i].p_paddr, phdrs[i].p_memsz);
		};
	};
	
	// find the string and symbol table sections
	Elf64_Shdr *sections = (Elf64_Shdr*) (kernelData + elfHeader->e_shoff);
	
	// find the symbol table
	symtab = NULL;
	numSyms = 0;
	strings = NULL;

	for (i=0; i<elfHeader->e_shnum; i++)
	{
		if (sections[i].sh_type == SHT_SYMTAB)
		{
			symtab = (Elf64_Sym*) (kernelData + sections[i].sh_offset);
			numSyms = sections[i].sh_size / sections[i].sh_entsize;
			strings = kernelData + sections[sections[i].sh_link].sh_offset;
		};
	};
	
	if (symtab == NULL)
	{
		mb_crash("kernel.so does not contain a symbol table!", 0);
	};
	
	uint64_t initStackPtr = mb_symbol("_stackBottom");
	if (initStackPtr == 0)
	{
		mb_crash("Cannot find the _stackBottom symbol in kernel.so!", 0);
	};
	
	// allocate memory map on the 64-bit stack
	uint64_t mmapVirt = (initStackPtr - info->mmapLen) & (~0xF);
	void *mmapPhys = mb_virt_to_phys(mmapVirt);
	if (mmapPhys == NULL)
	{
		mb_crash("Failed to resolve address for memory map!", mmapVirt);
	};
	
	memcpy(mmapPhys, (void*)info->mmapAddr, info->mmapLen);
	
	// allocate the KernelInfo struct on the 64-bit stack
	uint64_t kernelInfoVirt = (mmapVirt - sizeof(KernelInfo)) & (~0xF);
	KernelInfo *kinfo = (KernelInfo*) mb_virt_to_phys(kernelInfoVirt);
	if (kinfo == NULL)
	{
		mb_crash("Failed to resolve stack virtual address!", kernelInfoVirt);
	};
	
	memset(kinfo, 0, sizeof(KernelInfo));
	kinfo->features = 0;
	kinfo->kernelMain = mb_symbol("kmain");
	
	if (kinfo->kernelMain == 0)
	{
		mb_crash("Cannot find kmain in kernel.so!", 0);
	};
	
	// identity-map the loader area
	mb_mmap(0xE00000, 0xE00000, 0x100000);
	
	uint64_t gdt = mb_symbol("GDTPointer");
	
	if (gdt == 0)
	{
		mb_crash("Cannot find symbol GDTPointer in kernel.so!", 0);
	};
	
	kinfo->gdtPointerVirt = gdt;
	kinfo->pml4Phys = (uint32_t) pml4;
	kinfo->mmapSize = info->mmapLen;
	kinfo->mmapVirt = mmapVirt;
	kinfo->initrdSize = initrdSize;
	kinfo->end = (uint64_t) (uint32_t) mb_alloc(0, 0x1000);
	kinfo->initrdSymtabOffset = (uint64_t) (uint32_t) symtab - (uint64_t) (uint32_t) initrdStart;
	kinfo->initrdStrtabOffset = (uint64_t) (uint32_t) strings - (uint64_t) (uint32_t) initrdStart;
	kinfo->numSymbols = numSyms;
	
	mb_go64(kernelInfoVirt, kinfo);
};

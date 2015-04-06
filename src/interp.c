/*
	Glidix kernel

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

#include <glidix/interp.h>
#include <glidix/common.h>
#include <glidix/elf64.h>
#include <glidix/console.h>
#include <glidix/vfs.h>
#include <glidix/isp.h>
#include <glidix/memory.h>
#include <glidix/procmem.h>
#include <glidix/string.h>

#define	LIBC_BASE	0x100000000
#define	LIBC_TEXTPAGE	(LIBC_BASE / 0x1000)

static uint64_t		libcDynamicOffset;
static void*		libcDataSection;
static uint64_t		libcNextLoadAddr;
static uint64_t		libcDataOffset;
static uint64_t		libcInterpStack;
static uint64_t		libcInterpMain;
static uint64_t		libcDataPages;
static FrameList*	libcTextFrames;
static Elf64_Sym*	libcSymbolTable;
static char*		libcStringTable;
static uint64_t		libcDataSize;
static uint64_t		libcSymbolCount;

static int isSymbolNeededFor(Elf64_Xword relocType)
{
	switch (relocType)
	{
	case R_X86_64_RELATIVE:
		return 0;
	default:
		return 1;
	};
};

static int relocate(char *tmpBuffer, char *strtab, Elf64_Sym *symtab, Elf64_Rela *table, size_t num)
{
	size_t i;
	for (i=0; i<num; i++)
	{
		Elf64_Rela *rela = &table[i];
		Elf64_Xword type = ELF64_R_TYPE(rela->r_info);
		Elf64_Xword symidx = ELF64_R_SYM(rela->r_info);

		Elf64_Sym *symbol = &symtab[symidx];
		const char *symname = &strtab[symbol->st_name];
		if (symbol->st_name == 0) symname = "<noname>";

		void *symaddr = NULL;
		if ((symbol->st_shndx == 0) && isSymbolNeededFor(type))
		{
			FAILED();
			panic("/initrd/lib/libc.so: undefined reference to '%s'", symname);
		}
		else
		{
			symaddr = (void*) (LIBC_BASE + symbol->st_value);
		};

		void *reladdr = (void*) (tmpBuffer + rela->r_offset);
		switch (type)
		{
		case R_X86_64_64:
			//printf("64 (%p) = '%s' + %d (%p)\n", reladdr, symname, rela->r_addend, (void*)((uint64_t)symaddr + rela->r_addend));
			*((uint64_t*)reladdr) = (uint64_t) symaddr + rela->r_addend;
			break;
		case R_X86_64_JUMP_SLOT:
			//printf("JUMP_SLOT (%p) = '%s' (%p)\n", reladdr, symname, symaddr);
			*((uint64_t*)reladdr) = (uint64_t) symaddr;
			break;
		case R_X86_64_RELATIVE:
			//printf("RELATIVE (%p) = %p\n", reladdr, (void*)(lib->loadAddr + rela->r_addend));
			*((uint64_t*)reladdr) = (uint64_t) (LIBC_BASE + rela->r_addend);
			break;
		case R_X86_64_GLOB_DAT:
			//printf("GLOB_DAT (%p) = '%s' (%p)\n", reladdr, symname, symaddr);
			*((uint64_t*)reladdr) = (uint64_t) symaddr;
			break;
		default:
			FAILED();
			panic("/initrd/lib/libc.so: contains an invalid relocation entry");
			return 1;
		};
	};

	return 0;
};

void initInterp()
{
	kprintf("Loading the ELF interpreter... ");

	int error;
	File *fp = vfsOpen("/initrd/lib/libc.so", 0, &error);
	if (fp == NULL)
	{
		FAILED();
		panic("failed to open /initrd/lib/libc.so (error %d)", error);
	};

	Elf64_Ehdr elfHeader;
	if (vfsRead(fp, &elfHeader, sizeof(Elf64_Ehdr)) < sizeof(Elf64_Ehdr))
	{
		FAILED();
		panic("/initrd/lib/libc.so: ELF header too short");
	};

	if (memcmp(elfHeader.e_ident, "\x7f" "ELF", 4) != 0)
	{
		FAILED();
		panic("/initrd/lib/libc.so: invalid ELF magic");
	};

	if (elfHeader.e_ident[EI_CLASS] != ELFCLASS64)
	{
		FAILED();
		panic("/initrd/lib/libc.so: this is ELF32, not ELF64");
	};

	if (elfHeader.e_ident[EI_DATA] != ELFDATA2LSB)
	{
		FAILED();
		panic("/initrd/lib/libc.so: ELF data not little-endian");
	};

	if (elfHeader.e_ident[EI_VERSION] != 1)
	{
		FAILED();
		panic("/initrd/lib/libc.so: ELF version not 1");
	};

	if (elfHeader.e_type != ET_DYN)
	{
		FAILED();
		panic("/initrd/lib/libc.so: not a dynamic library");
	};

	size_t progSize = elfHeader.e_phentsize * elfHeader.e_phnum;
	Elf64_Phdr *progHeads = (Elf64_Phdr*) kmalloc(progSize);
	fp->seek(fp, elfHeader.e_phoff, SEEK_SET);
	if (vfsRead(fp, progHeads, progSize) < progSize)
	{
		FAILED();
		panic("/initrd/lib/libc.so: failed to read all program headers");
	};

	Elf64_Phdr *hdrText = NULL;
	Elf64_Phdr *hdrData = NULL;
	Elf64_Phdr *hdrDynamic = NULL;

	size_t i;
	for (i=0; i<elfHeader.e_phnum; i++)
	{
		Elf64_Phdr *phdr = &progHeads[i];
		if ((phdr->p_type == PT_LOAD) && (phdr->p_flags & PF_W))
		{
			hdrData = phdr;
		}
		else if (phdr->p_type == PT_LOAD)
		{
			hdrText = phdr;
		}
		else if (phdr->p_type == PT_DYNAMIC)
		{
			hdrDynamic = phdr;
		};
	};

	if ((hdrText == NULL) || (hdrData == NULL) || (hdrDynamic == NULL))
	{
		FAILED();
		panic("/initrd/lib/libc.so: expected text, data and dynamic program headers");
	};

	libcDynamicOffset = hdrDynamic->p_vaddr - hdrData->p_vaddr;
	libcDataOffset = hdrData->p_vaddr;
	libcDataSection = kmalloc(hdrData->p_memsz);
	memset(libcDataSection, 0, hdrData->p_memsz);

	// we must temporarily buffer all sections, with the correct layout, so that we can perform
	// relocations on the data sections while reading all the tables from the text section.
	char *tmpBuffer = (char*) kmalloc(hdrData->p_vaddr + hdrData->p_memsz);
	memset(tmpBuffer, 0, hdrData->p_vaddr + hdrData->p_memsz);

	fp->seek(fp, hdrText->p_offset, SEEK_SET);
	vfsRead(fp, &tmpBuffer[hdrText->p_vaddr], hdrText->p_filesz);
	fp->seek(fp, hdrData->p_offset, SEEK_SET);
	vfsRead(fp, &tmpBuffer[hdrData->p_vaddr], hdrData->p_filesz);

	// now perform all relocations
	Elf64_Rela *rela = NULL;
	Elf64_Rela *pltRela = NULL;
	Elf64_Word *hashtab;
	size_t szRela = 0;
	size_t szRelaEntry = 0;
	size_t pltRelaSize = 0;

	Elf64_Sym *symtab;
	char *strtab;

	Elf64_Dyn *dyn = (Elf64_Dyn*) ((uint64_t)tmpBuffer + hdrDynamic->p_vaddr);
	while (dyn->d_tag != DT_NULL)
	{
		if (dyn->d_tag == DT_RELA)
		{
			rela = (Elf64_Rela*) (tmpBuffer + dyn->d_un.d_ptr);
		}
		else if (dyn->d_tag == DT_RELASZ)
		{
			szRela = dyn->d_un.d_val;
		}
		else if (dyn->d_tag == DT_RELAENT)
		{
			szRelaEntry = dyn->d_un.d_val;
		}
		else if (dyn->d_tag == DT_JMPREL)
		{
			pltRela = (Elf64_Rela*) (tmpBuffer + dyn->d_un.d_ptr);
		}
		else if (dyn->d_tag == DT_PLTRELSZ)
		{
			pltRelaSize = dyn->d_un.d_val;
		}
		else if (dyn->d_tag == DT_STRTAB)
		{
			strtab = (char*) (tmpBuffer + dyn->d_un.d_ptr);
		}
		else if (dyn->d_tag == DT_SYMTAB)
		{
			symtab = (Elf64_Sym*) (tmpBuffer + dyn->d_un.d_ptr);
		}
		else if (dyn->d_tag == DT_HASH)
		{
			hashtab = (Elf64_Word*) (tmpBuffer + dyn->d_un.d_ptr);
		}
		else if (dyn->d_tag == DT_SYMENT)
		{
			if (dyn->d_un.d_val != sizeof(Elf64_Sym))
			{
				FAILED();
				panic("/initrd/lib/libc.so: symbol size mismatch");
			};
		}
		else if (dyn->d_tag == DT_NEEDED)
		{
			FAILED();
			panic("/initrd/lib/libc.so: requesting additional libraries");
		};

		dyn++;
	};

	if (rela != NULL) relocate(tmpBuffer, strtab, symtab, rela, szRela / szRelaEntry);
	if (pltRela != NULL) relocate(tmpBuffer, strtab, symtab, pltRela, pltRelaSize / sizeof(Elf64_Rela));

	libcInterpMain = 0;
	libcInterpStack = 0;

	if ((symtab == NULL) || (hashtab == NULL) || (strtab == NULL))
	{
		FAILED();
		panic("/initrd/lib/libc.so: missing tables");
	};

	size_t numSymbols = hashtab[1];
	libcSymbolCount = numSymbols;
	for (i=0; i<numSymbols; i++)
	{
		Elf64_Sym *sym = &symtab[i];
		const char *name = &strtab[sym->st_name];

		if (strcmp(name, "__interp_stack") == 0)
		{
			libcInterpStack = LIBC_BASE + sym->st_value + 4090;
		}
		else if (strcmp(name, "__interp_main") == 0)
		{
			libcInterpMain = LIBC_BASE + sym->st_value;
		};
	};

	if (libcInterpMain == 0)
	{
		FAILED();
		panic("/initrd/lib/libc.so: could not find symbol __interp_main");
	};

	if (libcInterpStack == 0)
	{
		FAILED();
		panic("/initrd/lib/libc.so: could not find symbol __interp_stack");
	};

	// copy the data into the data section
	memcpy(libcDataSection, &tmpBuffer[hdrData->p_vaddr], hdrData->p_memsz);

	// calculate the number of data pages
	libcDataPages = ((hdrData->p_vaddr + hdrData->p_memsz) / 0x1000) - (hdrData->p_vaddr / 0x1000) + 1;

	// make the text pages
	uint64_t textPages = ((hdrText->p_vaddr + hdrText->p_memsz) / 0x1000) - (hdrText->p_vaddr / 0x1000) + 1;

	libcTextFrames = palloc(textPages);
	libcTextFrames->flags = FL_SHARED;
	ispLock();

	for (i=0; i<textPages; i++)
	{
		ispSetFrame(libcTextFrames->frames[i]);
		memcpy(ispGetPointer(), &tmpBuffer[hdrText->p_vaddr + i * 0x1000], 0x1000);
	};

	ispUnlock();

	// OK, cleanup time.
	libcDataSize = hdrData->p_memsz;
	libcNextLoadAddr = LIBC_BASE + hdrData->p_vaddr + hdrData->p_memsz;
	libcSymbolTable = (Elf64_Sym*) ((uint64_t)symtab - (uint64_t)tmpBuffer + LIBC_BASE);
	libcStringTable = strtab - tmpBuffer + (char*)LIBC_BASE;
	kfree(progHeads);
	kfree(tmpBuffer);
	vfsClose(fp);
	DONE();
};

void linkInterp(Regs *regs, Elf64_Dyn *dynamic, ProcMem *pm)
{
	uint64_t entry = regs->rip;

	if (AddSegment(pm, LIBC_TEXTPAGE, libcTextFrames, PROT_READ | PROT_EXEC) != 0)
	{
		kprintf_debug("could not load text pages\n");
		regs->rip = 0;
		return;
	};

	FrameList *fl = palloc(libcDataPages);
	uint64_t index = (LIBC_BASE + libcDataOffset) / 0x1000;
	if (AddSegment(pm, index, fl, PROT_READ | PROT_WRITE) != 0)
	{
		kprintf_debug("could not load data pages\n");
		regs->rip = 0;
		return;
	};

	SetProcessMemory(pm);
	memcpy((void*)(LIBC_BASE + libcDataOffset), libcDataSection, libcDataSize);

	regs->rip = libcInterpMain;
	regs->rsp = libcInterpStack;
	regs->rdi = (uint64_t) dynamic;
	regs->rsi = (uint64_t) libcSymbolTable;
	regs->rdx = libcSymbolCount;
	regs->rcx = libcNextLoadAddr;
	regs->r8  = (uint64_t) libcStringTable;
	regs->r9  = entry;
};

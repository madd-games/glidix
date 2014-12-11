/*
	Glidix kernel

	Copyright (c) 2014, Madd Games.
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

#include <glidix/elf64.h>
#include <glidix/vfs.h>
#include <glidix/console.h>
#include <glidix/string.h>
#include <glidix/sched.h>
#include <glidix/memory.h>

typedef struct
{
	uint64_t		index;
	int			count;
	uint64_t		fileOffset;
	uint64_t		memorySize;
	uint64_t		fileSize;
	uint64_t		loadAddr;
	int			flags;
} ProgramSegment;

int elfExec(Regs *regs, const char *path)
{
	// TODO: permissions!
	// TODO: don't panic on errors.

	File *fp = vfsOpen(path, 0);
	if (fp == NULL)
	{
		panic("exec: failed to open %s", path);
	};

	if (fp->seek == NULL)
	{
		vfsClose(fp);
		panic("exec: the filesystem containing the executable cannot seek");
	};

	Elf64_Ehdr elfHeader;
	if (vfsRead(fp, &elfHeader, sizeof(Elf64_Ehdr)) < sizeof(Elf64_Ehdr))
	{
		vfsClose(fp);
		panic("exec: not an ELF64 file: too small to read header");
	};

	if (memcmp(elfHeader.e_ident, "\x7f" "ELF", 4) != 0)
	{
		vfsClose(fp);
		panic("exec: invalid ELF64 file: bad magic");
	};

	if (elfHeader.e_ident[EI_CLASS] != ELFCLASS64)
	{
		vfsClose(fp);
		panic("exec: invalid ELF64 file: this is an ELF32 file");
	};

	if (elfHeader.e_ident[EI_DATA] != ELFDATA2LSB)
	{
		vfsClose(fp);
		panic("exec: invalid ELF64: data is big-endian");
	};

	if (elfHeader.e_ident[EI_VERSION] != 1)
	{
		vfsClose(fp);
		panic("exec: invalid ELF64: EI_VERSION should be 1 but is %d", elfHeader.e_ident[EI_VERSION]);
	};

	if (elfHeader.e_type != ET_EXEC)
	{
		vfsClose(fp);
		panic("exec: this ELF64 is not executable");
	};

	if (elfHeader.e_phentsize < sizeof(Elf64_Phdr))
	{
		vfsClose(fp);
		panic("exec: e_phentsize is too small (%d), should be at least %d", elfHeader.e_phentsize, sizeof(Elf64_Phdr));
	};

	//kprintf("number of program headers: %d\n", elfHeader.e_phnum);
	ProgramSegment *segments = (ProgramSegment*) kmalloc(sizeof(ProgramSegment)*(elfHeader.e_phnum));

	unsigned int i;
	for (i=0; i<elfHeader.e_phnum; i++)
	{
		fp->seek(fp, elfHeader.e_phoff + i * elfHeader.e_phentsize, SEEK_SET);
		Elf64_Phdr proghead;
		if (vfsRead(fp, &proghead, sizeof(Elf64_Phdr)) < sizeof(Elf64_Phdr))
		{
			panic("exec: EOF while reading program header");
		};

		if (proghead.p_type == PT_NULL)
		{
			continue;
		}
		else if (proghead.p_type == PT_LOAD)
		{
			if (proghead.p_vaddr < 0x8000000000)
			{
				vfsClose(fp);
				panic("exec: a program header has an address that's too low");
			};

			if ((proghead.p_vaddr+proghead.p_memsz) > 0x10000000000)
			{
				vfsClose(fp);
				panic("exec: a program header has an address that's too high");
			};

			uint64_t start = proghead.p_vaddr;
			segments[i].index = (start-0x8000000000)/0x1000;

			uint64_t end = proghead.p_vaddr + proghead.p_memsz;
			uint64_t size = end - start;
			uint64_t numPages = size / 0x1000;
			if (size % 0x1000) numPages++;

			segments[i].count = (int) numPages;
			segments[i].fileOffset = proghead.p_offset;
			segments[i].memorySize = proghead.p_memsz;
			segments[i].fileSize = proghead.p_filesz;
			segments[i].loadAddr = proghead.p_vaddr;
			segments[i].flags = 0;

			if (proghead.p_flags & PF_R)
			{
				segments[i].flags |= PROT_READ;
			};

			if (proghead.p_flags & PF_W)
			{
				segments[i].flags |= PROT_WRITE;
			};

			if (proghead.p_flags & PF_X)
			{
				segments[i].flags |= PROT_EXEC;
			};
		}
		else
		{
			panic("exec: unrecognised program header type: %d", proghead.p_type);
		};
	};

	// delete the current addr space
	Thread *thread = getCurrentThread();
	ProcMem *pm = thread->pm;
	ASM("cli");
	thread->pm = NULL;
	ASM("sti");

	DownrefProcessMemory(pm);

	// create a new one
	pm = CreateProcessMemory();

	// pass 1: allocate the frames and map them
	for (i=0; i<(elfHeader.e_phnum); i++)
	{
		FrameList *fl = palloc(segments[i].count);
		if (AddSegment(pm, segments[i].index, fl, segments[i].flags) != 0)
		{
			pdownref(fl);
			DownrefProcessMemory(pm);
			panic("exec: AddSegment failed");
		};
		pdownref(fl);
	};

	// switch the memory space
	ASM("cli");
	thread->pm = pm;
	ASM("sti");
	SetProcessMemory(pm);

	// pass 2: load segments into memory
	for (i=0; i<(elfHeader.e_phnum); i++)
	{
		fp->seek(fp, segments[i].fileOffset, SEEK_SET);
		memset((void*) segments[i].loadAddr, 0, segments[i].memorySize);
		vfsRead(fp, (void*) segments[i].loadAddr, segments[i].fileSize);
	};

	// close the ELF64 file.
	vfsClose(fp);

	// make sure we jump to the entry upon return
	regs->rip = elfHeader.e_entry;

	return 0;
};

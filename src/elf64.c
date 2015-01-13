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

#include <glidix/elf64.h>
#include <glidix/vfs.h>
#include <glidix/console.h>
#include <glidix/string.h>
#include <glidix/sched.h>
#include <glidix/memory.h>
#include <glidix/errno.h>

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

int elfExec(Regs *regs, const char *path, const char *pars, size_t parsz)
{
	getCurrentThread()->therrno = ENOEXEC;

	struct stat st;
	if (vfsStat(path, &st) != 0)
	{
		return -1;
	};

	if (!vfsCanCurrentThread(&st, 1))
	{
		return -1;
	};

	int error;
	File *fp = vfsOpen(path, VFS_CHECK_ACCESS, &error);
	if (fp == NULL)
	{
		return -1;
	};

	if (fp->seek == NULL)
	{
		vfsClose(fp);
		return -1;
	};

	Elf64_Ehdr elfHeader;
	if (vfsRead(fp, &elfHeader, sizeof(Elf64_Ehdr)) < sizeof(Elf64_Ehdr))
	{
		vfsClose(fp);
		return -1;
	};

	if (memcmp(elfHeader.e_ident, "\x7f" "ELF", 4) != 0)
	{
		vfsClose(fp);
		return -1;
	};

	if (elfHeader.e_ident[EI_CLASS] != ELFCLASS64)
	{
		vfsClose(fp);
		return -1;
	};

	if (elfHeader.e_ident[EI_DATA] != ELFDATA2LSB)
	{
		vfsClose(fp);
		return -1;
	};

	if (elfHeader.e_ident[EI_VERSION] != 1)
	{
		vfsClose(fp);
		return -1;
	};

	if (elfHeader.e_type != ET_EXEC)
	{
		vfsClose(fp);
		return -1;
	};

	if (elfHeader.e_phentsize < sizeof(Elf64_Phdr))
	{
		vfsClose(fp);
		return -1;
	};

	ProgramSegment *segments = (ProgramSegment*) kmalloc(sizeof(ProgramSegment)*(elfHeader.e_phnum));

	unsigned int i;
	for (i=0; i<elfHeader.e_phnum; i++)
	{
		fp->seek(fp, elfHeader.e_phoff + i * elfHeader.e_phentsize, SEEK_SET);
		Elf64_Phdr proghead;
		if (vfsRead(fp, &proghead, sizeof(Elf64_Phdr)) < sizeof(Elf64_Phdr))
		{
			kfree(segments);
			return -1;
		};

		if (proghead.p_type == PT_NULL)
		{
			continue;
		}
		else if (proghead.p_type == PT_LOAD)
		{
			if (proghead.p_vaddr < 0x1000)
			{
				vfsClose(fp);
				kfree(segments);
				return -1;
			};

			if ((proghead.p_vaddr+proghead.p_memsz) > 0x8000000000)
			{
				vfsClose(fp);
				kfree(segments);
				return -1;
			};

			uint64_t start = proghead.p_vaddr;
			segments[i].index = (start)/0x1000;

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
			kfree(segments);
			return -1;
		};
	};

	// set the signal handler to default.
	getCurrentThread()->rootSigHandler = 0;

	// set the execPars
	Thread *thread = getCurrentThread();
	thread->execPars = (char*) kmalloc(parsz);
	thread->szExecPars = parsz;
	memcpy(thread->execPars, pars, parsz);

	// delete the current addr space
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
			// the memory is now severely broken, but still try running the program because YOLO
			// TODO: perhaps we should send a signal like SIGILL?
			break;
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

	// suid/sgid stuff
	if (st.st_mode & VFS_MODE_SETUID)
	{
		thread->euid = st.st_uid;
		thread->flags |= THREAD_REBEL;
	};

	if (st.st_mode & VFS_MODE_SETGID)
	{
		thread->egid = st.st_gid;
		thread->flags |= THREAD_REBEL;
	};

	return 0;
};

/*
	Glidix kernel

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

#include <glidix/elf64.h>
#include <glidix/vfs.h>
#include <glidix/console.h>
#include <glidix/string.h>
#include <glidix/sched.h>
#include <glidix/memory.h>
#include <glidix/errno.h>
#include <glidix/interp.h>

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

int sysOpenErrno(int error);			// syscall.c

int execScript(File *fp, const char *path, const char *pars, size_t parsz)
{
	char intline[1024];
	ssize_t bufsz = vfsRead(fp, intline, 1023);
	intline[bufsz] = 0;
	
	if (bufsz < 4)
	{
		vfsClose(fp);
		ERRNO = ENOEXEC;
		return -1;
	};
	
	// adjust the parameters
	char *newPars = (char*) kalloca(strlen(path)+2+bufsz+parsz);
	size_t newParsz = 0;
	
	// first argument is the interpreter path itself
	char *put = newPars;
	const char *scan = intline + 2;
	
	while (isspace(*scan)) scan++;
	while (!isspace(*scan))
	{
		*put++ = *scan++;
		newParsz++;
	};
	
	*put++ = 0;
	
	// copy over the next parameter if necessary
	while (isspace(*scan) && (*scan != '\n')) scan++;
	char nextPar[1024];
	char *parput = nextPar;
	while (!isspace(*scan))
	{
		*parput++ = *scan++;
	};
	*parput = 0;
	
	if (strlen(nextPar) != 0)
	{
		strcpy(put, nextPar);
		put += strlen(nextPar) + 1;
		newParsz += strlen(nextPar) + 1;
	};
	
	// next parameter is the script path; do not place the terminator
	// because that will be copied from the origin execpars
	strcpy(put, path);
	put += strlen(path);
	newParsz += strlen(path);
	
	// copy the rest from the original
	while (*pars != 0)
	{
		pars++;
		parsz--;
	};
	
	memcpy(put, pars, parsz);
	newParsz += parsz;
	
	// do it
	vfsClose(fp);
	return elfExec(newPars, newPars, newParsz);
};

int elfExec(const char *path, const char *pars, size_t parsz)
{
	//getCurrentThread()->therrno = ENOEXEC;
	Regs regs;
	initUserRegs(&regs);
	
	vfsLockCreation();
	struct stat st;
	int error = vfsStat(path, &st);
	if (error != 0)
	{
		vfsUnlockCreation();
		return sysOpenErrno(error);
	};

	if (!vfsCanCurrentThread(&st, 1))
	{
		vfsUnlockCreation();
		getCurrentThread()->therrno = EPERM;
		return -1;
	};

	File *fp = vfsOpen(path, VFS_CHECK_ACCESS, &error);
	if (fp == NULL)
	{
		vfsUnlockCreation();
		return sysOpenErrno(error);
	};
	vfsUnlockCreation();

	if (fp->seek == NULL)
	{
		vfsClose(fp);
		getCurrentThread()->therrno = EIO;
		return -1;
	};
	
	char shebang[2];
	if (vfsRead(fp, shebang, 2) < 2)
	{
		vfsClose(fp);
		ERRNO = ENOEXEC;
		return -1;
	};
	
	fp->seek(fp, 0, SEEK_SET);	// seek back to start
	
	if (memcmp(shebang, "#!", 2) == 0)
	{
		// executable script
		return execScript(fp, path, pars, parsz);
	};

	Elf64_Ehdr elfHeader;
	if (vfsRead(fp, &elfHeader, sizeof(Elf64_Ehdr)) < sizeof(Elf64_Ehdr))
	{
		vfsClose(fp);
		getCurrentThread()->therrno = ENOEXEC;
		return -1;
	};

	if (memcmp(elfHeader.e_ident, "\x7f" "ELF", 4) != 0)
	{
		vfsClose(fp);
		getCurrentThread()->therrno = ENOEXEC;
		return -1;
	};

	if (elfHeader.e_ident[EI_CLASS] != ELFCLASS64)
	{
		vfsClose(fp);
		getCurrentThread()->therrno = ENOEXEC;
		return -1;
	};

	if (elfHeader.e_ident[EI_DATA] != ELFDATA2LSB)
	{
		vfsClose(fp);
		getCurrentThread()->therrno = ENOEXEC;
		return -1;
	};

	if (elfHeader.e_ident[EI_VERSION] != 1)
	{
		vfsClose(fp);
		getCurrentThread()->therrno = ENOEXEC;
		return -1;
	};

	if (elfHeader.e_type != ET_EXEC)
	{
		vfsClose(fp);
		getCurrentThread()->therrno = ENOEXEC;
		return -1;
	};

	if (elfHeader.e_phentsize < sizeof(Elf64_Phdr))
	{
		vfsClose(fp);
		getCurrentThread()->therrno = ENOEXEC;
		return -1;
	};

	ProgramSegment *segments = (ProgramSegment*) kmalloc(sizeof(ProgramSegment)*(elfHeader.e_phnum));
	memset(segments, 0, sizeof(ProgramSegment) * elfHeader.e_phnum);

	int interpNeeded = 0;
	Elf64_Dyn *dynamic;

	unsigned int i;
	for (i=0; i<elfHeader.e_phnum; i++)
	{
		fp->seek(fp, elfHeader.e_phoff + i * elfHeader.e_phentsize, SEEK_SET);
		Elf64_Phdr proghead;
		if (vfsRead(fp, &proghead, sizeof(Elf64_Phdr)) < sizeof(Elf64_Phdr))
		{
			kfree(segments);
			getCurrentThread()->therrno = ENOEXEC;
			return -1;
		};

		if (proghead.p_type == PT_PHDR)
		{
			continue;
		}
		else if (proghead.p_type == PT_NULL)
		{
			continue;
		}
		else if (proghead.p_type == PT_LOAD)
		{
			if (proghead.p_vaddr < 0x400000)
			{
				vfsClose(fp);
				kfree(segments);
				getCurrentThread()->therrno = ENOEXEC;
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
			uint64_t numPages = ((start + size) / 0x1000) - segments[i].index + 1; 
			//if (size % 0x1000) numPages++;

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
		else if (proghead.p_type == PT_INTERP)
		{
			interpNeeded = 1;
		}
		else if (proghead.p_type == PT_DYNAMIC)
		{
			dynamic = (Elf64_Dyn*) proghead.p_vaddr;
		}
		else
		{
			kfree(segments);
			getCurrentThread()->therrno = ENOEXEC;
			return -1;
		};
	};

	// signal dispositions
	getCurrentThread()->sigdisp = sigdispExec(getCurrentThread()->sigdisp);

	// thread name
	strcpy(getCurrentThread()->name, path);

	// set the execPars
	Thread *thread = getCurrentThread();
	if (thread->execPars != NULL) kfree(thread->execPars);
	thread->execPars = (char*) kmalloc(parsz);
	thread->szExecPars = parsz;
	memcpy(thread->execPars, pars, parsz);

	// create a new address space
	ProcMem *pm = CreateProcessMemory();

	// switch the address space, so that AddSegment() can optimize mapping
	cli();
	lockSched();
	ProcMem *oldPM = thread->pm;
	thread->pm = pm;
	unlockSched();
	sti();
	SetProcessMemory(pm);
	DownrefProcessMemory(oldPM);

	// allocate the frames and map them
	for (i=0; i<(elfHeader.e_phnum); i++)
	{
		if (segments[i].count > 0)
		{
			FrameList *fl = palloc_later(segments[i].count, segments[i].fileOffset, segments[i].fileSize);
			if (AddSegment(pm, segments[i].index, fl, segments[i].flags) != 0)
			{
				getCurrentThread()->therrno = ENOEXEC;
				pdownref(fl);
				DownrefProcessMemory(pm);
				break;
			};
			pdownref(fl);
		};
	};
	
	// allocate a 2MB stack
	FrameList *flStack = palloc(0x200);
	if (AddSegment(pm, 0x200, flStack, PROT_READ | PROT_WRITE) != 0)
	{
		kprintf_debug("ERROR: failed to map stack for some reason\n");
	};
	pdownref(flStack);
	
	// change the fpexec
	if (thread->fpexec != NULL)
	{
		if (thread->fpexec->close != NULL) thread->fpexec->close(thread->fpexec);
		kfree(thread->fpexec);
	};
	thread->fpexec = fp;

	// make sure we jump to the entry upon return
	regs.rip = elfHeader.e_entry;

	// the errnoptr is now invalid
	thread->errnoptr = NULL;

	// close all files marked with O_CLOEXEC (on glidix a.k.a. FD_CLOEXEC)
	spinlockAcquire(&getCurrentThread()->ftab->spinlock);
	for (i=0; i<MAX_OPEN_FILES; i++)
	{
		File *fp = getCurrentThread()->ftab->entries[i];
		if (fp != NULL)
		{
			if (fp->oflag & O_CLOEXEC)
			{
				getCurrentThread()->ftab->entries[i] = NULL;
				vfsClose(fp);
			};
		};
	};
	spinlockRelease(&getCurrentThread()->ftab->spinlock);
	
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

	if (interpNeeded)
	{
		linkInterp(&regs, dynamic, pm);
	};
	
	regs.rsp = 0x400000;
	regs.rbp = 0;
	refreshAddrSpace();
	
	memset((void*)0x200000, 0, 0x200000);
	switchContext(&regs);
	return 0;
};

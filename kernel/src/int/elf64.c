/*
	Glidix kernel

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

#include <glidix/int/elf64.h>
#include <glidix/fs/vfs.h>
#include <glidix/display/console.h>
#include <glidix/util/string.h>
#include <glidix/thread/sched.h>
#include <glidix/util/memory.h>
#include <glidix/util/errno.h>
#include <glidix/int/syscall.h>
#include <glidix/int/trace.h>

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

static int validateElfHeader(Elf64_Ehdr *header)
{
	if (memcmp(header->e_ident, "\x7f" "ELF", 4) != 0)
	{
		return -1;
	};

	if (header->e_ident[EI_CLASS] != ELFCLASS64)
	{
		return -1;
	};

	if (header->e_ident[EI_DATA] != ELFDATA2LSB)
	{
		return -1;
	};

	if (header->e_ident[EI_VERSION] != 1)
	{
		return -1;
	};

	if (header->e_type != ET_EXEC)
	{
		return -1;
	};

	if (header->e_machine != EM_X86_64)
	{
		return -1;
	};
	
	if (header->e_phentsize < sizeof(Elf64_Phdr))
	{
		return -1;
	};
	
	return 0;
};

int elfExec(const char *path, const char *pars, size_t parsz)
{
	if (parsz < 4)
	{
		ERRNO = EINVAL;
		return -1;
	};
	
	if ((pars[parsz-1] != 0) || (pars[parsz-2] != 0))
	{
		ERRNO = EINVAL;
		return -1;
	};

	// auxiliary vector; up to 2 entries (AT_EXECFD and AT_NULL or just
	// 2 AT_NULLs)
	Elf64_Auxv auxv[2];
	memset(auxv, 0, sizeof(Elf64_Auxv)*2);

	// offset to each argument and argument count
	uint64_t argOffsets[2048];
	
	// we must zero it out because it's going to userspace
	memset(argOffsets, 0, 2048 * 8);
	
	int stillArgs = 1;
	uint64_t argc = 0;
	uint64_t totalTokens = 0;
	uint64_t uptrPars = (0x400000 - parsz) & ~0xF;
	
	uint64_t scan;
	for (scan=0; scan<parsz; scan++)
	{
		if (pars[scan] == 0)
		{
			argOffsets[totalTokens++] = 0;
			
			if (stillArgs)
			{
				stillArgs = 0;
			}
			else
			{
				break;
			};
		}
		else
		{
			argOffsets[totalTokens++] = uptrPars + scan;
			if (stillArgs) argc++;
			
			while (pars[scan] != 0) scan++;
		};
	};
	
	Regs regs;
	initUserRegs(&regs);

	int error;
	File *fp = vfsOpen(VFS_NULL_IREF, path, O_RDONLY, 0, &error);
	if (fp == NULL)
	{
		ERRNO = error;
		return -1;
	};

	if (fp->iref.inode->ft == NULL)
	{
		vfsClose(fp);
		ERRNO = EACCES;
		return -1;
	};
	
	semWait(&fp->iref.inode->lock);
	int execAllowed = vfsIsAllowed(fp->iref.inode, VFS_ACE_EXEC);
	mode_t execmode = fp->iref.inode->mode;
	if (fp->iref.inode->fs->flags & VFS_ST_NOSUID) execmode &= 0777;	// no suid/sgid
	uid_t execuid = fp->iref.inode->uid;
	gid_t execgid = fp->iref.inode->gid;
	uint64_t exec_ixperm = fp->iref.inode->ixperm;
	uint64_t exec_oxperm = fp->iref.inode->oxperm;
	uint64_t exec_dxperm = fp->iref.inode->dxperm;
	semSignal(&fp->iref.inode->lock);
	
	if (!execAllowed)
	{
		vfsClose(fp);
		ERRNO = EACCES;
		return -1;
	};
	
	char shebang[2];
	if (vfsRead(fp, shebang, 2) < 2)
	{
		vfsClose(fp);
		ERRNO = ENOEXEC;
		return -1;
	};
	
	vfsSeek(fp, 0, VFS_SEEK_SET);	// seek back to start
	
	if (memcmp(shebang, "#!", 2) == 0)
	{
		// executable script
		return execScript(fp, path, pars, parsz);
	};

	Elf64_Ehdr elfHeader;
	if (vfsRead(fp, &elfHeader, sizeof(Elf64_Ehdr)) < sizeof(Elf64_Ehdr))
	{
		vfsClose(fp);
		ERRNO = ENOEXEC;
		return -1;
	};

	if (validateElfHeader(&elfHeader) != 0)
	{
		vfsClose(fp);
		ERRNO = ENOEXEC;
		return -1;
	};

	ProgramSegment *segments = (ProgramSegment*) kmalloc(sizeof(ProgramSegment)*(elfHeader.e_phnum));
	memset(segments, 0, sizeof(ProgramSegment) * elfHeader.e_phnum);

	unsigned int i;
	for (i=0; i<elfHeader.e_phnum; i++)
	{
		vfsSeek(fp, elfHeader.e_phoff + i * elfHeader.e_phentsize, VFS_SEEK_SET);
		Elf64_Phdr proghead;
		if (vfsRead(fp, &proghead, sizeof(Elf64_Phdr)) < sizeof(Elf64_Phdr))
		{
			kfree(segments);
			vfsClose(fp);
			ERRNO = ENOEXEC;
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
				ERRNO = ENOEXEC;
				return -1;
			};

			if ((proghead.p_vaddr+proghead.p_memsz) > 0x8000000000)
			{
				vfsClose(fp);
				kfree(segments);
				ERRNO = ENOEXEC;
				return -1;
			};

			uint64_t start = proghead.p_vaddr;
			segments[i].index = (start)/0x1000;

			uint64_t end = proghead.p_vaddr + proghead.p_memsz;
			uint64_t size = end - start;
			uint64_t numPages = ((start + size) / 0x1000) - segments[i].index + 1; 

			segments[i].count = (int) numPages;
			segments[i].fileOffset = proghead.p_offset & ~0xFFF;
			segments[i].memorySize = proghead.p_memsz + (proghead.p_offset & 0xFFF);
			segments[i].fileSize = proghead.p_filesz + (proghead.p_offset & 0xFFF);
			segments[i].loadAddr = proghead.p_vaddr & ~0xFFF;
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
			// execute the interpreter instead of the requested executable
			char interpPath[256];
			memset(interpPath, 0, 256);
			if (proghead.p_filesz >= 256)
			{
				vfsClose(fp);
				kfree(segments);
				ERRNO = ENOEXEC;
				return -1;
			};
			
			vfsSeek(fp, proghead.p_offset, VFS_SEEK_SET);
			if (vfsRead(fp, interpPath, proghead.p_filesz) != proghead.p_filesz)
			{
				vfsClose(fp);
				kfree(segments);
				ERRNO = ENOEXEC;
				return -1;
			};
			
			File *execfp = fp;
			fp = vfsOpen(VFS_NULL_IREF, interpPath, O_RDONLY, 0, &error);
			if (fp == NULL)
			{
				vfsClose(execfp);
				kfree(segments);
				ERRNO = ENOEXEC;
				return -1;
			};
			
			if (fp->iref.inode->ft == NULL)
			{
				vfsClose(execfp);
				vfsClose(fp);
				kfree(segments);
				ERRNO = ENOEXEC;
				return -1;
			};
			
			if (vfsRead(fp, &elfHeader, sizeof(Elf64_Ehdr)) != sizeof(Elf64_Ehdr))
			{
				vfsClose(execfp);
				vfsClose(fp);
				kfree(segments);
				ERRNO = ENOEXEC;
				return -1;
			};
			
			if (validateElfHeader(&elfHeader) != 0)
			{
				vfsClose(execfp);
				vfsClose(fp);
				kfree(segments);
				ERRNO = ENOEXEC;
				return -1;
			};
			
			kfree(segments);
			segments = (ProgramSegment*) kmalloc(sizeof(ProgramSegment)*(elfHeader.e_phnum));
			memset(segments, 0, sizeof(ProgramSegment) * elfHeader.e_phnum);

			for (i=0; i<elfHeader.e_phnum; i++)
			{
				vfsSeek(fp, elfHeader.e_phoff + i * elfHeader.e_phentsize, VFS_SEEK_SET);
				Elf64_Phdr proghead;
				if (vfsRead(fp, &proghead, sizeof(Elf64_Phdr)) < sizeof(Elf64_Phdr))
				{
					kfree(segments);
					vfsClose(fp);
					vfsClose(execfp);
					ERRNO = ENOEXEC;
					return -1;
				};
				
				if (proghead.p_type == PT_LOAD)
				{
					if (proghead.p_vaddr < 0x400000)
					{
						vfsClose(fp);
						kfree(segments);
						ERRNO = ENOEXEC;
						return -1;
					};

					if ((proghead.p_vaddr+proghead.p_memsz) > 0x8000000000)
					{
						vfsClose(fp);
						kfree(segments);
						ERRNO = ENOEXEC;
						return -1;
					};

					uint64_t start = proghead.p_vaddr;
					segments[i].index = (start)/0x1000;

					uint64_t end = proghead.p_vaddr + proghead.p_memsz;
					uint64_t size = end - start;
					uint64_t numPages = ((start + size) / 0x1000) - segments[i].index + 1; 

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
					vfsClose(fp);
					vfsClose(execfp);
					ERRNO = ENOEXEC;
					return -1;
				};
			};

			int i = ftabAlloc(getCurrentThread()->ftab);
			if (i == -1)
			{
				kfree(segments);
				vfsClose(fp);
				vfsClose(execfp);
				ERRNO = EMFILE;
				return -1;
			};
			
			ftabSet(getCurrentThread()->ftab, i, execfp, 0);
			auxv[0].a_type = AT_EXECFD;
			auxv[0].a_un.a_val = i;

			break;
		}
		else if (proghead.p_type == PT_DYNAMIC)
		{
			// ignore
		}
		else
		{
			vfsClose(fp);
			kfree(segments);
			ERRNO = ENOEXEC;
			return -1;
		};
	};

	// signal dispositions
	getCurrentThread()->sigdisp = sigdispExec(getCurrentThread()->sigdisp);

	// thread name
	if (strlen(path) < 256)
	{
		strcpy(getCurrentThread()->name, path);
	}
	else
	{
		strcpy(getCurrentThread()->name, "<TOO LONG>");
	};

	// /proc/<pid>/exe
	DentryRef dref = vfsGetDentry(VFS_NULL_IREF, path, 0, NULL);
	if (dref.dent != NULL)
	{
		char *rpath = vfsRealPath(dref);
		dref = vfsGetDentry(VFS_NULL_IREF, "/proc/self/exe", 0, NULL);
		if (dref.dent != NULL)
		{
			InodeRef iref = vfsGetInode(dref, 0, NULL);
			if (iref.inode != NULL)
			{
				semWait(&iref.inode->lock);
				kfree(iref.inode->target);
				iref.inode->target = rpath;
				semSignal(&iref.inode->lock);
			}
			else
			{
				kfree(rpath);
			};
		}
		else
		{
			kfree(rpath);
		};
	};
	
	// set the execPars
	Thread *thread = getCurrentThread();
	if (thread->execPars != NULL) kfree(thread->execPars);
	thread->execPars = (char*) kmalloc(parsz);
	thread->szExecPars = parsz;
	memcpy(thread->execPars, pars, parsz);

	// create a new address space
	vmNew();

	uint8_t zeroPage[0x1000];
	memset(zeroPage, 0, 0x1000);
	
	// allocate the frames and map them
	for (i=0; i<(elfHeader.e_phnum); i++)
	{
		if (segments[i].loadAddr > 0)
		{
			vmMap(segments[i].loadAddr, segments[i].memorySize, segments[i].flags,
				MAP_PRIVATE | MAP_ANON | MAP_FIXED, NULL, 0);
			vmMap(segments[i].loadAddr, segments[i].fileSize, segments[i].flags,
				MAP_PRIVATE | MAP_FIXED, fp, segments[i].fileOffset);
			
			uint64_t zeroPos = segments[i].loadAddr + segments[i].fileSize;
			uint64_t toZero = (0x1000 - (zeroPos & 0xFFF)) & 0xFFF;
			
			if (segments[i].flags & PROT_WRITE)
			{
				if (toZero != 0)
				{
					memcpy_k2u((void*)zeroPos, zeroPage, toZero);
				};
			};
		};
	};
	
	// we can close the main handle now
	vfsClose(fp);
	
	// allocate a 2MB stack
	vmMap(0x200000, 0x200000, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON | MAP_FIXED, NULL, 0);

	// make sure we jump to the entry upon return
	regs.rip = elfHeader.e_entry;

	// the errnoptr is now invalid
	thread->errnoptr = NULL;

	// kill all threads in the same process and wait for them to fully detach
	killOtherThreads();
	
	// close all files marked with O_CLOEXEC (on glidix a.k.a. FD_CLOEXEC)
	ftabCloseOnExec(getCurrentThread()->ftab);
	
	// suid/sgid stuff
	if (execmode & VFS_MODE_SETUID)
	{
		thread->debugFlags = 0;
		thread->creds->euid = execuid;
		thread->flags |= THREAD_REBEL;
	};

	if (execmode & VFS_MODE_SETGID)
	{
		thread->debugFlags = 0;
		thread->creds->egid = execgid;
		thread->flags |= THREAD_REBEL;
	};
	
	// permissions
	getCurrentThread()->oxperm = (getCurrentThread()->oxperm & (getCurrentThread()->dxperm & exec_ixperm)) | exec_oxperm;
	getCurrentThread()->dxperm = exec_dxperm;
	
	if (memcpy_k2u((void*)uptrPars, pars, parsz) != 0)
	{
		vmDump(getCurrentThread()->pm, uptrPars);
		panic("memcpy_k2u failed unexpectedly (0x%016lx)", uptrPars);
	};
	
	uint64_t uptrAuxv = uptrPars - sizeof(Elf64_Auxv)*2;
	
	// totalTokens must be odd in order to break the 16-byte alignment of uptrList
	// such that the stack is 16-byte aligned. if, however, it is even, subtract 8 bytes
	// to fix that
	if ((totalTokens & 1) == 0)
	{
		uptrAuxv -= 8;
	};
	
	if (memcpy_k2u((void*)uptrAuxv, auxv, sizeof(Elf64_Auxv)*2) != 0)
	{
		vmDump(getCurrentThread()->pm, uptrPars);
		panic("memcpy_k2u failed unexpectedly (0x%016lx)", uptrPars);
	};
	
	uint64_t uptrList = uptrAuxv - 8 * totalTokens;
	if (memcpy_k2u((void*)uptrList, argOffsets, 8 * totalTokens) != 0)
	{
		vmDump(getCurrentThread()->pm, uptrPars);
		panic("memcpy_k2u failed unexpectedly (0x%016lx)", uptrPars);
	};
	
	uint64_t stack = uptrList - 8;
	if (memcpy_k2u((void*)stack, &argc, 8) != 0)
	{
		vmDump(getCurrentThread()->pm, uptrPars);
		panic("memcpy_k2u failed unexpectedly (0x%016lx)", uptrPars);
	};
	
	regs.rsp = stack;
	regs.rbp = 0;
	regs.rdx = 0;

	// initialize MXCSR
	FPURegs fregs;
	fpuSave(&fregs);
	fregs.mxcsr = MX_PM | MX_UM | MX_OM | MX_ZM | MX_DM | MX_IM;
	fpuLoad(&fregs);
	
	// do not block any signals in a new executable by default
	getCurrentThread()->sigmask = 0;
	
	if (getCurrentThread()->debugFlags & DBG_STOP_ON_EXEC)
	{
		traceTrap(&regs, TR_EXEC);
	};
	
	switchContext(&regs);
	return 0;
};

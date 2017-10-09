/*
	Glidix Debugger

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

#include <sys/call.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <sys/debug.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <pthread.h>
#include <unistd.h>
#include <termios.h>
#include <dlfcn.h>

#include "gxdbg.h"
#include "core.h"

/**
 * A stack frame read from a coredump's memory.
 */
typedef struct
{
	uint64_t rbp;
	uint64_t rip;
} StackFrame;

/**
 * Read data from the virtual address space in a core file. Returns 0 on success, -1 if
 * the address is not valid.
 */
int readCore(void *buffer, uint64_t addr, size_t size, CoreSegment *seglist, size_t numSegs, CoreHeader *header, FILE *fp)
{
	char *put = (char*) buffer;
	while (size > 0)
	{
		// try reading it from the segment list first
		size_t i;
		int ok = 0;
		int foundSegment = 0;
		for (i=0; i<numSegs; i++)
		{
			CoreSegment *seg = &seglist[i];
			if (seg->cs_base <= addr && seg->cs_base+seg->cs_size > addr)
			{
				if (seg->cs_offset == 0)
				{
					foundSegment = 1;
					break;
				}
				else
				{
					uint64_t offset = addr - seg->cs_base;
					size_t readNow = seg->cs_size - offset;
					if (readNow > size) readNow = size;
				
					fseek(fp, seg->cs_offset + offset, SEEK_SET);
					fread(put, 1, readNow, fp);
				
					put += readNow;
					size -= readNow;
					addr += readNow;
				
					ok = 1;
					break;
				};
			};
		};
		
		if (ok) continue;
		if (!foundSegment) return -1;
		
		if (header->ch_pagetree == 0)
		{
			// doesn't exist
			return -1;
		};
		
		uint64_t entries[512];
		uint64_t indexC = (addr >> 12) & 0x1FF;
		uint64_t indexB = (addr >> 21) & 0x1FF;
		uint64_t indexA = (addr >> 30) & 0x1FF;
		uint64_t offset = addr & 0xFFF;
		
		fseek(fp, header->ch_pagetree, SEEK_SET);
		fread(entries, 8, 512, fp);
		int foundData = 0;
		if (entries[indexA] != 0)
		{
			fseek(fp, entries[indexA], SEEK_SET);
			fread(entries, 8, 512, fp);
			if (entries[indexB] != 0)
			{
				fseek(fp, entries[indexB], SEEK_SET);
				fread(entries, 8, 512, fp);
				if (entries[indexC] != 0)
				{
					fseek(fp, entries[indexC] + offset, SEEK_SET);
					foundData = 1;
				};
			};	
		};
		
		
		size_t readNow = 0x1000 - offset;
		if (readNow > size) readNow = size;
		
		if (foundData) fread(put, 1, readNow, fp);
		else memset(put, 0, readNow);
		
		put += readNow;
		size -= readNow;
		addr += readNow;
	};
	
	return 0;
};

/**
 * Figure out which library image and offset the given address corresponds to in a core file.
 * If finding the library fails, the path is set to an empty string.
 */
void getAddrInfo(FILE *fp, uint64_t pointer, char *pathOut, uint64_t *offsetOut)
{
	CoreHeader header;
	fseek(fp, 0, SEEK_SET);
	fread(&header, 1, sizeof(CoreHeader), fp);

	CoreSegment *seglist = (CoreSegment*) malloc(sizeof(CoreSegment) * header.ch_segnum);
	fseek(fp, header.ch_segoff, SEEK_SET);
	fread(seglist, sizeof(CoreSegment), header.ch_segnum, fp);
	
	*pathOut = 0;		// empty by default
	
	Dl_Library lib;
	uint64_t addr;
	for (addr=(uint64_t) dlopen(NULL, RTLD_LAZY); addr!=0; addr=(uint64_t) lib.next)
	{
		if (readCore(&lib, addr, sizeof(Dl_Library), seglist, header.ch_segnum, &header, fp) != 0)
		{
			break;
		};
		
		size_t i;
		for (i=0; i<lib.numSegs; i++)
		{
			Dl_Segment *seg = &lib.segs[i];
			if ((uint64_t)seg->base <= pointer && (uint64_t)seg->base+seg->size > pointer)
			{
				// it is this object
				strcpy(pathOut, lib.path);
				*offsetOut = pointer - (uint64_t) lib.base;
				break;
			};
		};
	};

	free(seglist);
};

int inspectCore(const char *filename)
{
	FILE *fp = fopen(filename, "rb");
	if (fp == NULL)
	{
		printf("failed to open core file %s: %s\n", filename, strerror(errno));
		return 1;
	};
	
	// validate the header
	CoreHeader header;
	fread(&header, 1, sizeof(CoreHeader), fp);
	
	if (header.ch_magic != CORE_MAGIC)
	{
		printf("failed to load core file %s: invalid magic\n", filename);
		fclose(fp);
		return 1;
	};
	
	if (header.ch_size != sizeof(CoreHeader))
	{
		printf("failed to load core file %s: incompatible header size\n", filename);
		fclose(fp);
		return 1;
	};
	
	if (header.ch_segentsz != sizeof(CoreSegment))
	{
		printf("failed to load core file %s: incompatible segment descriptor size\n", filename);
		fclose(fp);
		return 1;
	};
	
	if (header.ch_thentsz != sizeof(CoreThread))
	{
		printf("failed to load core file %s: incompatible thread descriptor size\n", filename);
		fclose(fp);
		return 1;
	};
	
	if (header.ch_thnum == 0)
	{
		printf("failed to load core file %s: no threads are listed\n", filename);
		fclose(fp);
		return 1;
	};
	
	CoreThread *thlist = (CoreThread*) malloc(sizeof(CoreThread) * header.ch_thnum);
	fseek(fp, header.ch_thoff, SEEK_SET);
	fread(thlist, sizeof(CoreThread), header.ch_thnum, fp);
	
	CoreThread *currentThread = thlist;
	
	printf("successfully loaded core file '%s'\n", filename);
	printf("the failing thread has been selected.\n");
	
	char *cmdline = NULL;
	while (1)
	{
		free(cmdline);
		
		cmdline = prompt();
		char *cmd = strtok(cmdline, SPACE_CHARS);
		if (cmd == NULL) continue;
		
		if (strcmp(cmd, "exit") == 0 || strcmp(cmd, "quit") == 0)
		{
			printf("exiting debugger.\n");
			break;
		}
		else if (strcmp(cmd, "segs") == 0 || strcmp(cmd, "segments") == 0)
		{
			CoreSegment *seglist = (CoreSegment*) malloc(sizeof(CoreSegment) * header.ch_segnum);
			fseek(fp, header.ch_segoff, SEEK_SET);
			fread(seglist, sizeof(CoreSegment), header.ch_segnum, fp);
			
			printf("%-20s%-20sProt Flags\n", "Start", "End");
			
			size_t i;
			for (i=0; i<header.ch_segnum; i++)
			{
				CoreSegment *seg = &seglist[i];
				printf("0x%016lX  0x%016lX  %c%c%c  %c%c%c\n",
					seg->cs_base, seg->cs_base + seg->cs_size - 1,
					(seg->cs_prot & PROT_READ) ? 'R':'r',
					(seg->cs_prot & PROT_WRITE) ? 'W':'w',
					(seg->cs_prot & PROT_EXEC) ? 'X':'x',
					(seg->cs_flags & MAP_PRIVATE) ? 'P':'S',
					(seg->cs_flags & MAP_ANONYMOUS) ? 'A':'a',
					(seg->cs_flags & MAP_THREAD) ? 'T':'t'
				);
			};
			
			free(seglist);
		}
		else if (strcmp(cmd, "info") == 0)
		{
			printf("Threads:               %lu\n", header.ch_thnum);
			printf("PID:                   %d\n", header.ch_pid);
			printf("Reason of failure:     %s\n", strsignal(header.ch_sig));
			printf("Address of failure:    0x%016lX\n", header.ch_addr);
			printf("Real user ID:          %lu\n", header.ch_uid);
			printf("Real group ID:         %lu\n", header.ch_gid);
			printf("Effective user ID:     %lu\n", header.ch_euid);
			printf("Effective group ID:    %lu\n", header.ch_egid);
		}
		else if (strcmp(cmd, "regs") == 0)
		{
			printf("Thread ID '%d' (nice value %d)\n", currentThread->ct_thid, currentThread->ct_nice);
			printf("RAX: 0x%016lX   RCX: 0x%016lX\n", currentThread->ct_rax, currentThread->ct_rcx);
			printf("RDX: 0x%016lX   RBX: 0x%016lX\n", currentThread->ct_rdx, currentThread->ct_rbx);
			printf("RSP: 0x%016lX   RBP: 0x%016lX\n", currentThread->ct_rsp, currentThread->ct_rbp);
			printf("RSI: 0x%016lX   RDI: 0x%016lX\n", currentThread->ct_rsi, currentThread->ct_rdi);
			printf("R8 : 0x%016lX   R9 : 0x%016lX\n", currentThread->ct_r8, currentThread->ct_r9);
			printf("R10: 0x%016lX   R11: 0x%016lX\n", currentThread->ct_r10, currentThread->ct_r11);
			printf("R12: 0x%016lX   R13: 0x%016lX\n", currentThread->ct_r12, currentThread->ct_r13);
			printf("R14: 0x%016lX   R15: 0x%016lX\n", currentThread->ct_r14, currentThread->ct_r15);
			printf("RIP: 0x%016lX RFLAGS 0x%016lX\n", currentThread->ct_rip, currentThread->ct_rflags);
			printf("FS : 0x%016lX   GS : 0x%016lX\n", currentThread->ct_fsbase, currentThread->ct_gsbase);
			printf("\n'errno' location: 0x%016lX\n", currentThread->ct_errnoptr);
		}
		else if (strcmp(cmd, "thread") == 0)
		{
			char *indexSpec = strtok(NULL, SPACE_CHARS);
			if (indexSpec == NULL)
			{
				printf("%-20s%-20s\n", "Index", "Thread ID");
				
				size_t i;
				for (i=0; i<header.ch_thnum; i++)
				{
					printf("%-20lu%-20d\n", i, thlist[i].ct_thid);
				};
			}
			else
			{
				char *endptr;
				unsigned long index = strtoul(indexSpec, &endptr, 10);
				if (*endptr != 0 || index >= header.ch_thnum)
				{
					printf("invalid thread index\n");
				}
				else
				{
					currentThread = &thlist[index];
				};
			};
		}
		else if (strcmp(cmd, "libs") == 0)
		{
			printf("%-20s%-20s%-5s%-7s%s\n", "Base", "Name", "Bind", "Scope", "Image");
			CoreSegment *seglist = (CoreSegment*) malloc(sizeof(CoreSegment) * header.ch_segnum);
			fseek(fp, header.ch_segoff, SEEK_SET);
			fread(seglist, sizeof(CoreSegment), header.ch_segnum, fp);
			
			Dl_Library lib;
			uint64_t addr;
			for (addr=(uint64_t) dlopen(NULL, RTLD_LAZY); addr!=0; addr=(uint64_t) lib.next)
			{
				if (readCore(&lib, addr, sizeof(Dl_Library), seglist, header.ch_segnum, &header, fp) != 0)
				{
					printf("<bad pointer 0x%016lX>\n", addr);
					break;
				};
				
				const char *binding = "LAZY";
				if (lib.flags & RTLD_NOW) binding = "NOW";
				
				const char *scope = "LOCAL";
				if (lib.flags & RTLD_GLOBAL) scope = "GLOBAL";
				
				printf("0x%016lX  %-20s%-5s%-7s%s\n", lib.base, lib.soname, binding, scope, lib.path);
			};
			
			free(seglist);
		}
		else if (strcmp(cmd, "where") == 0 || strcmp(cmd, "stack") == 0)
		{
			CoreSegment *seglist = (CoreSegment*) malloc(sizeof(CoreSegment) * header.ch_segnum);
			fseek(fp, header.ch_segoff, SEEK_SET);
			fread(seglist, sizeof(CoreSegment), header.ch_segnum, fp);

			StackFrame frame;
			frame.rbp = currentThread->ct_rbp;
			frame.rip = currentThread->ct_rip;
			
			int number = 0;
			while (1)
			{
				char path[256];
				uint64_t offset;
				getAddrInfo(fp, frame.rip, path, &offset);
				
				char *funcname;
				char *lineinfo;
				if (path[0] == 0)
				{
					funcname = strdup("??");
					lineinfo = strdup("??");
				}
				else
				{
					funcname = __dbg_getsym(path, offset);
					lineinfo = __dbg_addr2line(path, offset);
				};
				
				printf("#%-4d 0x%lx in %s() at %s\n", number++, frame.rip, funcname, lineinfo);
				free(funcname);
				free(lineinfo);
				
				if (frame.rbp == 0) break;
				if (readCore(&frame, frame.rbp, sizeof(StackFrame), seglist, header.ch_segnum, &header, fp) != 0)
				{
					printf("<bad base pointer 0x%016lX>\n", frame.rbp);
					break;
				};
			};
			
			free(seglist);
		}
		else if (strcmp(cmd, "addr") == 0 || strcmp(cmd, "address") == 0)
		{
			char *addrspec = strtok(NULL, SPACE_CHARS);
			if (addrspec == NULL)
			{
				printf("USAGE: %s <address>\n", cmd);
			}
			else
			{
				char *endptr;
				uint64_t addr = strtoul(addrspec, &endptr, 0);
				if (*endptr != 0)
				{
					printf("invalid address given\n");
				}
				else
				{
					char path[256];
					uint64_t offset;
					getAddrInfo(fp, addr, path, &offset);
					
					printf("Address 0x%016lX:\n", addr);
					if (path[0] == 0)
					{
						printf("\tNon-static memory\n");
					}
					else
					{
						printf("\tStatic memory at %s+0x%lx\n", path, offset);
					};
				};
			};
		}
		else
		{
			printf("unknown command `%s'\n", cmd);
		};
	};
	
	free(cmdline);
	fclose(fp);
	return 0;
};

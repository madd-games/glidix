/*
	Glidix dynamic linker

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

#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include "dynld.h"

char dynld_errmsg[1024];
Library chainHead;
const char *libraryPath = "";
int debugMode = 0;
int bindNow = 1;		/* TODO: lazy binding doesn't work; fix it and make it lazy by default */
int dynld_errno;

void dynld_enter(void *retstack, Elf64_Addr entry);
int dynld_main(int argc, char *argv[], char *envp[], void *retstack)
{
	if (sizeof(Library) > 0x1000)
	{
		dynld_printf("sizeof(Library) exceeds page size!\n");
		return 1;
	};
	
	char **envscan;
	for (envscan=envp; *envscan!=NULL; envscan++)
	{
		if (memcmp(*envscan, "LD_LIBRARY_PATH=", strlen("LD_LIBRARY_PATH=")) == 0)
		{
			libraryPath = (*envscan) + strlen("LD_LIBRARY_PATH=");
		};
		
		if (strcmp(*envscan, "LD_DEBUG=1") == 0)
		{
			debugMode = 1;
		};
		
		if (strcmp(*envscan, "LD_BIND_NOW=1") == 0)
		{
			bindNow = 1;
		};
	};
	
	// get the auxiliary vector
	char **auxvp;
	for (auxvp=envp; *auxvp!=NULL; auxvp++);
	Elf64_Auxv *auxv = (Elf64_Auxv*) &auxvp[1];
	
	// see if we can get the executable file descriptor
	for (; auxv->a_type != AT_NULL; auxv++)
	{
		if (auxv->a_type == AT_EXECFD) break;
	};
	
	const char *progname;
	int execfd;
	if (auxv->a_type == AT_EXECFD)
	{
		execfd = (int) auxv->a_un.a_val;
		progname = argv[0];
	}
	else
	{
		if (argc < 2)
		{
			dynld_printf("dynld: no AT_EXECFD and no arguments given, see dynld(1)\n");
			return 1;
		};
		
		execfd = open(argv[1], O_RDONLY);
		if (execfd == -1)
		{
			dynld_printf("dynld: failed to open %s\n", argv[1]);
			return 1;
		};
		
		progname = argv[1];
	};
	
	struct stat st;
	if (fstat(execfd, &st) == 0)
	{
		if ((st.st_mode & 06000) != 0)
		{
			if (debugMode)
			{
				dynld_printf("dynld: program is suid/sgid; running in safe-execution mode\n");
			};
			
			libraryPath = "";
		};
		
		if (st.st_oxperm != 0)
		{
			if (debugMode)
			{
				dynld_printf("dynld: program has own permissions; running in safe-execution mode\n");
			};
			
			libraryPath = "";
		};
	};
	
	chainHead.prev = chainHead.next = NULL;
	if (dynld_mapobj(&chainHead, execfd, 0, progname, RTLD_LAZY | RTLD_GLOBAL) == 0)
	{
		dynld_printf("dynld: failed to load executable: %s\n", dynld_errmsg);
		return 1;
	};
	
	Elf64_Ehdr elfHeader;
	if (pread(execfd, &elfHeader, sizeof(Elf64_Ehdr), 0) != sizeof(Elf64_Ehdr))
	{
		dynld_printf("dynld: failed to read ELF header of executable\n");
		return 1;
	};
	
	close(execfd);
	
	dynld_initlib(&chainHead);
	
	if (debugMode)
	{
		dynld_printf("dynld: passing control to program\n");
	};
	
	dynld_enter(retstack, elfHeader.e_entry);
	return 0;
};

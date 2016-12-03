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

#include <unistd.h>
#include <fcntl.h>
#include "dynld.h"

int dynld_main(int argc, char *argv[], char *envp[])
{
	// get the auxiliary vector
	char **auxvp;
	for (auxvp=envp; *auxvp!=NULL; auxvp++);
	Elf64_Auxv *auxv = (Elf64_Auxv*) &envp[1];
	
	// see if we can get the executable file descriptor
	for (; auxv->a_type != AT_NULL; auxv++)
	{
		if (auxv->a_type == AT_EXECFD) break;
	};
	
	int execfd;
	if (auxv->a_type == AT_EXECFD)
	{
		execfd = (int) auxv->a_un.a_val;
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
	};
	
	return 0;
};

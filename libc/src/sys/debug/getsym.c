/*
	Glidix Runtime

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

#include <sys/debug.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>
#include <signal.h>
#include <__elf64.h>

char* __dbg_getsym(const char *path, uint64_t offset)
{
	// map the file into memory first
	int fd = open(path, O_RDONLY);
	if (fd == -1)
	{
		return strdup("??");
	};
	
	struct stat st;
	if (fstat(fd, &st) != 0)
	{
		close(fd);
		return strdup("??");
	};
	
	void *map = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
	if (map == MAP_FAILED)
	{
		close(fd);
		return strdup("??");
	};
	
	close(fd);
	
	// validate ELF64 header
	char *data = (char*) map;
	Elf64_Ehdr *elfHeader = (Elf64_Ehdr*) data;
	
	if (memcmp(elfHeader->e_ident, "\x7f" "ELF", 4) != 0)
	{
		munmap(map, st.st_size);
		return strdup("??");
	};

	if (elfHeader->e_ident[EI_CLASS] != ELFCLASS64)
	{
		munmap(map, st.st_size);
		return strdup("??");
	};

	if (elfHeader->e_ident[EI_DATA] != ELFDATA2LSB)
	{
		munmap(map, st.st_size);
		return strdup("??");
	};

	if (elfHeader->e_ident[EI_VERSION] != 1)
	{
		munmap(map, st.st_size);
		return strdup("??");
	};

	if (elfHeader->e_machine != EM_X86_64)
	{
		munmap(map, st.st_size);
		return strdup("??");
	};
	
	if (elfHeader->e_shentsize != sizeof(Elf64_Shdr))
	{
		munmap(map, st.st_size);
		return strdup("??");
	};
	
	if (elfHeader->e_shnum == 0)
	{
		munmap(map, st.st_size);
		return strdup("??");
	};
	
	// look for symbol table sections
	Elf64_Shdr *sections = (Elf64_Shdr*) (data + elfHeader->e_shoff);
	size_t i;
	for (i=0; i<elfHeader->e_shnum; i++)
	{
		Elf64_Shdr *sect = &sections[i];
		if (sect->sh_type == SHT_SYMTAB)
		{
			// symbol table found, look for a symbol containing the address
			Elf64_Sym *symtab = (Elf64_Sym*) (data + sect->sh_offset);
			size_t numSymbols = sect->sh_size / sizeof(Elf64_Sym);
			
			size_t j;
			for (j=0; j<numSymbols; j++)
			{
				Elf64_Sym *sym = &symtab[j];
				if (sym->st_shndx == SHN_UNDEF) continue;
				
				uint64_t addr = sym->st_value;
				
				if (offset >= addr && offset < (addr+sym->st_size))
				{
					// this is the symbol
					Elf64_Shdr *strSection = &sections[sect->sh_link];
					char *strings = data + strSection->sh_offset;
					char *result = strdup(strings + sym->st_name);
					
					munmap(map, st.st_size);
					return result;
				};
			};
		};
	};
	
	// done; nothing found
	munmap(map, st.st_size);
	return strdup("??");
};

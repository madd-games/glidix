/*
	Glidix dynamic linker

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

#include <sys/mman.h>
#include <unistd.h>
#include <signal.h>
#include "dynld.h"

extern Library chainHead;
uint64_t addrPlacement;

void dynld_diag(void *ptr);

Library* dynld_getlib(const char *soname)
{
	Library *lib;
	for (lib=&chainHead; lib!=NULL; lib=lib->next)
	{
		if (strcmp(soname, lib->soname) == 0)
		{
			lib->refcount++;
			return lib;
		};
	};
	
	return NULL;
};

void* dynld_libsym(Library *lib, const char *symname, unsigned char binding)
{
	size_t i;
	for (i=0; i<lib->numSymbols; i++)
	{
		Elf64_Sym *symbol = &lib->symtab[i];
		const char *name = &lib->strtab[symbol->st_name];
		
		if ((ELF64_S_BINDING(symbol->st_info) == binding) && (symbol->st_shndx != 0))
		{
			if (strcmp(symname, name) == 0)
			{
				return (void*) (lib->base + symbol->st_value);
			};
		};
	};
	
	return NULL;
};

Library* dynld_dlopen(const char *soname, int mode)
{
	Library *dep = dynld_getlib(soname);
	if (dep == NULL)
	{
		int depfd = dynld_open(soname, "", libraryPath, "", "/usr/local/lib:/usr/lib:/lib", NULL);
		if (depfd == -1)
		{
			strcpy(dynld_errmsg, soname);
			strcpy(&dynld_errmsg[strlen(soname)], ": library not found");
			
			return NULL;
		};
		
		dep = mmap(NULL, 0x1000, PROT_READ | PROT_WRITE,
				MAP_PRIVATE | MAP_ANON, -1, 0);
		if (dep == MAP_FAILED)
		{
			close(depfd);
			
			strcpy(dynld_errmsg, soname);
			strcpy(&dynld_errmsg[strlen(soname)], ": cannot create library description");
			
			return NULL;
		};

		if (mode & RTLD_GLOBAL)
		{
			Library *last = &chainHead;
			while (last->next != NULL) last = last->next;
			
			dep->next = NULL;
			last->next = dep;
			dep->prev = last;
		}
		else
		{
			dep->prev = dep->next = NULL;
		};
		
		if (dynld_mapobj(dep, depfd, addrPlacement, soname, RTLD_LAZY | RTLD_GLOBAL) == 0)
		{
			// leave dynld_errmsg as is; we forward the error from the recursive
			// invocation
			close(depfd);
			
			if (dep->prev != NULL)
			{
				dep->prev->next = NULL;
			};
			
			munmap(dep, 0x1000);
			
			return NULL;
		};
		
		dynld_initlib(dep);
		close(depfd);
	};
	
	return dep;
};

void* dynld_dlsym(Library *lib, const char *symname)
{
	void *val = dynld_libsym(lib, symname, STB_GLOBAL);
	if (val != NULL) return val;
	
	return dynld_libsym(lib, symname, STB_WEAK);
};

char* dynld_dlerror()
{
	return dynld_errmsg;
};

void* dynld_globsym(const char *symname, Library *lastLib)
{
	Library *lib;
	
	// try global symbols first
	for (lib=&chainHead; lib!=NULL; lib=lib->next)
	{
		void *val = dynld_libsym(lib, symname, STB_GLOBAL);
		if (val != NULL) return val;
	};

	// now internal symbols
	if (strcmp(symname, "dlopen") == 0)
	{
		return dynld_dlopen;
	};
	
	if (strcmp(symname, "dlsym") == 0)
	{
		return dynld_dlsym;
	};
	
	if (strcmp(symname, "dlclose") == 0)
	{
		return dynld_libclose;
	};
	
	if (strcmp(symname, "dlerror") == 0)
	{
		return dynld_dlerror;
	};

	// now try the weak symbols
	for (lib=&chainHead; lib!=NULL; lib=lib->next)
	{
		void *val = dynld_libsym(lib, symname, STB_WEAK);
		if (val != NULL) return val;
	};
	
	// now the library itself (in case it was RTLD_LOCAL)
	void *val = dynld_libsym(lastLib, symname, STB_GLOBAL);
	if (val != NULL) return val;

	val = dynld_libsym(lastLib, symname, STB_WEAK);
	if (val != NULL) return val;

	return NULL;
};

void* dynld_globdat(const char *name)
{
	size_t i;
	for (i=0; i<chainHead.numRela; i++)
	{
		Elf64_Rela *rela = &chainHead.rela[i];
		Elf64_Xword type = ELF64_R_TYPE(rela->r_info);
		Elf64_Xword symidx = ELF64_R_SYM(rela->r_info);

		if (type == R_X86_64_COPY)
		{
			Elf64_Sym *sym = &chainHead.symtab[symidx];
			char *symname = &chainHead.strtab[sym->st_name];

			if (strcmp(name, symname) == 0)
			{
				return (void*) sym->st_value;
			};
		};
	};
	
	return NULL;
};

void* dynld_globdef(const char *name)
{
	Library *lib;
	
	// try global symbols first
	for (lib=chainHead.next; lib!=NULL; lib=lib->next)
	{
		void *val = dynld_libsym(lib, name, STB_GLOBAL);
		if (val != NULL) return val;
	};
	
	// now try the weak symbols
	for (lib=chainHead.next; lib!=NULL; lib=lib->next)
	{
		void *val = dynld_libsym(lib, name, STB_WEAK);
		if (val != NULL) return val;
	};
	
	return NULL;
};

// perform a PLT relocation with the given index on the given library, and return the resulting
// address (as well as filling it in the GOT). returns NULL on error.
void* dynld_pltreloc(Library *lib, uint64_t index)
{
	Elf64_Rela *rela = &lib->pltRela[index];
	Elf64_Xword symidx = ELF64_R_SYM(rela->r_info);
	
	Elf64_Sym *symbol = &lib->symtab[symidx];
	const char *symname = &lib->strtab[symbol->st_name];
	
	void *symaddr = dynld_globsym(symname, lib);
	if (symaddr == NULL)
	{
		strcpy(dynld_errmsg, lib->soname);
		strcpy(&dynld_errmsg[strlen(dynld_errmsg)], ": undefined reference to `");
		strcpy(&dynld_errmsg[strlen(dynld_errmsg)], symname);
		strcpy(&dynld_errmsg[strlen(dynld_errmsg)], "'");
		return NULL;
	};
	
	void **relput = (void**) (lib->base + rela->r_offset);

	if (debugMode)
	{
		dynld_printf("dynld: BIND `%s' at %s+0x%p (0x%p) -> 0x%p\n",
				symname, lib->soname, rela->r_offset, relput, symaddr);
	};
	
	*relput = symaddr;
	return symaddr;
};
extern char dynld_lazybind[];

// called by dynld_lazybind if the binding failed
void dynld_abort()
{
	dynld_printf("dynld: lazy binding failed: %s\n", dynld_errmsg);
	raise(SIGABRT);
};

void dynld_initlib(Library *lib)
{
	if (lib->initDone) return;
	lib->initDone = 1;
	
	// call it recursively on dependencies
	size_t i;
	for (i=0; i<lib->numDeps; i++)
	{
		dynld_initlib(lib->deps[i]);
	};
	
	// call initialization functions
	if (debugMode)
	{
		dynld_printf("dynld: invoking constructors of %s\n", lib->soname);
	};
	
	if (lib->initFunc != NULL) lib->initFunc();
	for (i=0; i<lib->numInit; i++)
	{
		if (lib->initVec[i] != NULL) lib->initVec[i]();
	};
};

uint64_t dynld_mapobj(Library *lib, int fd, uint64_t base, const char *name, int flags)
{
	if (debugMode)
	{
		dynld_printf("dynld: mapping object `%s' into memory\n", name);
	};
	
	Elf64_Ehdr elfHeader;
	if (pread(fd, &elfHeader, sizeof(Elf64_Ehdr), 0) != sizeof(Elf64_Ehdr))
	{
		strcpy(dynld_errmsg, "failed to read the ELF header");
		return 0;
	};
	
	if (memcmp(elfHeader.e_ident, "\x7f" "ELF", 4) != 0)
	{
		strcpy(dynld_errmsg, "invalid ELF64 magic number");
		return 0;
	};

	if (elfHeader.e_ident[EI_CLASS] != ELFCLASS64)
	{
		strcpy(dynld_errmsg, "not an ELF64 file");
		return 0;
	};

	if (elfHeader.e_ident[EI_DATA] != ELFDATA2LSB)
	{
		strcpy(dynld_errmsg, "not litte-endian");
		return 0;
	};

	if (elfHeader.e_ident[EI_VERSION] != 1)
	{
		strcpy(dynld_errmsg, "ELF64 version not set to 1");
		return 0;
	};

	if (elfHeader.e_type == ET_EXEC)
	{
		if (base != 0)
		{
			strcpy(dynld_errmsg, "attempting to load executable at nonzero base");
			return 0;
		};
	}
	else if (elfHeader.e_type != ET_DYN)
	{
		strcpy(dynld_errmsg, "not a shared library");
		return 0;
	};

	if (elfHeader.e_machine != EM_X86_64)
	{
		strcpy(dynld_errmsg, "object does not target x86_64");
		return 0;
	};
	
	if (elfHeader.e_phentsize != sizeof(Elf64_Phdr))
	{
		strcpy(dynld_errmsg, "unexpected program header size");
		return 0;
	};
	
	if (elfHeader.e_phnum == 0)
	{
		strcpy(dynld_errmsg, "no segments in this object");
		return 0;
	};
	
	// fill in the library description
	lib->refcount = 1;
	lib->base = base;
	strcpy(lib->soname, name);
	lib->numSegs = 0;
	lib->numDeps = 0;
	lib->flags = flags;
	lib->dyn = NULL;
	lib->symtab = NULL;
	lib->strtab = NULL;
	lib->numSymbols = 0;
	lib->rela = NULL;
	lib->numRela = 0;
	lib->pltRela = NULL;
	lib->hashtab = NULL;
	lib->pltgot = NULL;
	lib->initFunc = NULL;
	lib->numInit = 0;
	lib->finiFunc = NULL;
	lib->numFini = 0;
	lib->initDone = 0;
	
	// map the segments
	Elf64_Phdr phdr;
	size_t i;
	uint64_t topAddr = 0;
	for (i=0; i<elfHeader.e_phnum; i++)
	{
		if (pread(fd, &phdr, sizeof(Elf64_Phdr), elfHeader.e_phoff + sizeof(Elf64_Phdr) * i) != sizeof(Elf64_Phdr))
		{
			strcpy(dynld_errmsg, "failed to read segment");
			
			for (i=0; i<lib->numSegs; i++)
			{
				munmap(lib->segs[i].base, lib->segs[i].size);
			};
			
			return 0;
		};
		
		if (phdr.p_type == PT_LOAD)
		{
			if ((phdr.p_vaddr + phdr.p_memsz) > topAddr)
			{
				topAddr = phdr.p_vaddr + phdr.p_memsz;
			};
			
			int index = lib->numSegs++;
			lib->segs[index].base = (void*) ((phdr.p_vaddr & ~0xFFF) + base);
			lib->segs[index].size = phdr.p_filesz + (phdr.p_vaddr & 0xFFF);
			
			int prot = 0;
			if (phdr.p_flags & PF_R)
			{
				prot |= PROT_READ;
			};
			
			if (phdr.p_flags & PF_W)
			{
				prot |= PROT_WRITE;
			};
			
			if (phdr.p_flags & PF_X)
			{
				prot |= PROT_EXEC;
			};
			
			if (mmap(lib->segs[index].base, lib->segs[index].size,
					prot, MAP_PRIVATE | MAP_FIXED,
					fd, phdr.p_offset & ~0xFFF) == MAP_FAILED)
			{
				strcpy(dynld_errmsg, "failed to map segment into memory");
				for (i=0; i<lib->numSegs-1; i++)
				{
					munmap(lib->segs[i].base, lib->segs[i].size);
				};
				
				return 0;
			};
			
			uint64_t zeroAddr = (uint64_t) lib->segs[index].base + lib->segs[index].size;
			uint64_t toZero = (0x1000 - (zeroAddr & 0xFFF)) & 0xFFF;
			
			if (prot & PROT_WRITE)
			{
				if (toZero != 0)
				{
					memset((void*)zeroAddr, 0, toZero);
				};
			};
			
			// calculate the top page-aligned address based on file and memory
			// sizes, to see if we need to map extra anonymous pages 
			uint64_t fileTop = base + ((phdr.p_vaddr + phdr.p_filesz + 0xFFF) & ~0xFFF);
			uint64_t memTop = base + ((phdr.p_vaddr + phdr.p_memsz + 0xFFF) & ~0xFFF);
			uint64_t anonLen = memTop - fileTop;
			
			if (anonLen > 0)
			{
				index = lib->numSegs++;
				lib->segs[index].base = (void*) fileTop;
				lib->segs[index].size = anonLen;
				
				if (mmap((void*)fileTop, anonLen, prot, MAP_PRIVATE | MAP_FIXED | MAP_ANON, -1, 0) == MAP_FAILED)
				{
					strcpy(dynld_errmsg, "failed to map anonymous pages");
					for (i=0; i<lib->numSegs-1; i++)
					{
						munmap(lib->segs[i].base, lib->segs[i].size);
					};
					
					return 0;
				};
			};
		}
		else if (phdr.p_type == PT_DYNAMIC)
		{
			lib->dyn = (Elf64_Dyn*) (phdr.p_vaddr + lib->base);
		};
	};
	
	// memory-align the top address
	topAddr = (topAddr + 0xFFF) & ~0xFFF;
	
	if (base == 0)
	{
		addrPlacement = 0x200000000;	/* 8GB */
	}
	else
	{
		addrPlacement += topAddr;
	};
	
	// if there is no dynamic information, we don't link
	if (lib->dyn == NULL)
	{
		return topAddr;
	};
	
	// get the symbol table, string table and relocation table
	Elf64_Dyn *dyn;
	size_t numPltRela = 0;
	for (dyn=lib->dyn; dyn->d_tag!=DT_NULL; dyn++)
	{
		switch (dyn->d_tag)
		{
		case DT_RELA:
			lib->rela = (Elf64_Rela*) (base + dyn->d_un.d_ptr);
			break;
		case DT_RELASZ:
			lib->numRela = dyn->d_un.d_val / sizeof(Elf64_Rela);
			break;
		case DT_JMPREL:
			lib->pltRela = (Elf64_Rela*) (base + dyn->d_un.d_ptr);
			break;
		case DT_STRTAB:
			lib->strtab = (char*) (base + dyn->d_un.d_ptr);
			break;
		case DT_SYMTAB:
			lib->symtab = (Elf64_Sym*) (base + dyn->d_un.d_ptr);
			break;
		case DT_HASH:
			lib->hashtab = (Elf64_Word*) (base + dyn->d_un.d_ptr);
			break;
		case DT_PLTGOT:
			lib->pltgot = (void**) (base + dyn->d_un.d_ptr);
			break;
		case DT_PLTRELSZ:
			numPltRela = dyn->d_un.d_val / sizeof(Elf64_Rela);
			break;
		case DT_INIT:
			lib->initFunc = (InitFunc) (base + dyn->d_un.d_ptr);
			break;
		case DT_INIT_ARRAY:
			lib->initVec = (InitFunc*) (base + dyn->d_un.d_ptr);
			break;
		case DT_INIT_ARRAYSZ:
			lib->numInit = dyn->d_un.d_val >> 3;
			break;
		case DT_FINI:
			lib->finiFunc = (FiniFunc) (base + dyn->d_un.d_ptr);
			break;
		case DT_FINI_ARRAY:
			lib->finiVec = (FiniFunc*) (base + dyn->d_un.d_ptr);
			break;
		case DT_FINI_ARRAYSZ:
			lib->numFini = dyn->d_un.d_val >> 3;
			break;
		};
	};
	
	lib->numSymbols = lib->hashtab[1];
	
	// additional library paths
	const char *rpath = "";
	const char *runpath = "";

	for (dyn=lib->dyn; dyn->d_tag!=DT_NULL; dyn++)
	{
		switch (dyn->d_tag)
		{
		case DT_RPATH:
			rpath = &lib->strtab[dyn->d_un.d_val];
			break;
		case DT_RUNPATH:
			runpath = &lib->strtab[dyn->d_un.d_val];
			break;
		};
	};
	
	// load dependencies
	for (dyn=lib->dyn; dyn->d_tag!=DT_NULL; dyn++)
	{
		if (dyn->d_tag == DT_NEEDED)
		{
			const char *depname = &lib->strtab[dyn->d_un.d_val];
			
			Library *dep = dynld_getlib(depname);
			if (dep == NULL)
			{
				int depfd = dynld_open(depname, rpath, libraryPath, runpath, "/usr/local/lib:/usr/lib:/lib", NULL);
				if (depfd == -1)
				{
					strcpy(dynld_errmsg, depname);
					strcpy(&dynld_errmsg[strlen(depname)], ": library not found");
					
					for (i=0; i<lib->numDeps; i++)
					{
						dynld_libclose(lib->deps[i]);
					};
					
					for (i=0; i<lib->numSegs; i++)
					{
						munmap(lib->segs[i].base, lib->segs[i].size);
					};
					
					return 0;
				};
				
				dep = mmap((void*)addrPlacement, 0x1000, PROT_READ | PROT_WRITE,
						MAP_PRIVATE | MAP_FIXED | MAP_ANON, -1, 0);
				if (dep == MAP_FAILED)
				{
					close(depfd);
					
					strcpy(dynld_errmsg, depname);
					strcpy(&dynld_errmsg[strlen(depname)], ": cannot create library description");
					
					for (i=0; i<lib->numDeps; i++)
					{
						dynld_libclose(lib->deps[i]);
					};
					
					for (i=0; i<lib->numSegs; i++)
					{
						munmap(lib->segs[i].base, lib->segs[i].size);
					};
					
					return 0;
				};

				dep->next = lib->next;
				dep->prev = lib;
				lib->next = dep;
				addrPlacement += 0x1000;
				
				if (dynld_mapobj(dep, depfd, addrPlacement, depname, RTLD_LAZY | RTLD_GLOBAL) == 0)
				{
					// leave dynld_errmsg as is; we forward the error from the recursive
					// invocation
					close(depfd);
					
					for (i=0; i<lib->numDeps; i++)
					{
						dynld_libclose(lib->deps[i]);
					};
					
					for (i=0; i<lib->numSegs; i++)
					{
						munmap(lib->segs[i].base, lib->segs[i].size);
					};
					
					lib->next = dep->next;
					munmap(dep, 0x1000);
					
					return 0;
				};
				
				close(depfd);
			};
			
			lib->deps[lib->numDeps++] = dep;
		};
	};
	
	// perform relocations on the object
	if (debugMode)
	{
		dynld_printf("dynld: performing relocations on object `%s' (0x%p relocations)\n", name, lib->numRela);
	};
	
	for (i=0; i<lib->numRela; i++)
	{
		Elf64_Rela *rela = &lib->rela[i];
		Elf64_Xword type = ELF64_R_TYPE(rela->r_info);
		Elf64_Xword symidx = ELF64_R_SYM(rela->r_info);
		
		Elf64_Sym *symbol = &lib->symtab[symidx];
		const char *symname = &lib->strtab[symbol->st_name];
		if (symbol->st_name == 0) symname = "<noname>";
		
		void *symaddr = NULL;
		if (((symbol->st_shndx == 0) && (type != R_X86_64_RELATIVE)) && (type != R_X86_64_COPY))
		{
			symaddr = dynld_globsym(symname, lib);
			if (symaddr == NULL)
			{
				strcpy(dynld_errmsg, name);
				strcpy(&dynld_errmsg[strlen(dynld_errmsg)], ": undefined reference to `");
				strcpy(&dynld_errmsg[strlen(dynld_errmsg)], symname);
				strcpy(&dynld_errmsg[strlen(dynld_errmsg)], "'");
				
				return 0;
			};
		}
		else
		{
			symaddr = (void*) (lib->base + symbol->st_value);
			
			if (type == R_X86_64_COPY)
			{
				// if no libraries define the symbol, we leave 'symaddr' as-is, it will point
				// to the data in the executable. otherwise, we must point 'symaddr' to the
				// location to copy from. to find this location, we search all libraries for
				// for a symbol definition, but not the executable.
				
				void *srcaddr = dynld_globdef(symname);
				if (srcaddr != NULL)
				{
					symaddr = srcaddr;
				};
			};
		};
		
		// for R_X86_64_GLOB_DAT relocations, use the normal symbol value unless the executable contained
		// an R_X86_64_COPY relocation for this symbol, in which case use that instead.
		if (type == R_X86_64_GLOB_DAT)
		{
			void *globaddr = dynld_globdat(symname);
			if (globaddr != NULL)
			{
				symaddr = globaddr;
			};
		};
		
		// fill in the resulting value
		uint64_t *relput = (uint64_t*) (lib->base + rela->r_offset);
		switch (type)
		{
		case R_X86_64_64:
			if (debugMode) dynld_printf("R_X86_64_64 against `%s' at %s+0x%p (0x%p) -> 0x%p\n",
							symname, name, rela->r_offset, relput, (uint64_t) symaddr + rela->r_addend);
			*relput = (uint64_t) symaddr + rela->r_addend;
			break;
		case R_X86_64_RELATIVE:
			if (debugMode) dynld_printf("R_X86_64_RELATIVE against `%s' at %s+0x%p (0x%p) -> 0x%p\n",
							symname, name, rela->r_offset, relput, lib->base + rela->r_addend);
			*relput = lib->base + rela->r_addend;
			break;
		case R_X86_64_GLOB_DAT:
			if (debugMode) dynld_printf("R_X86_64_GLOB_DAT against `%s' at %s+0x%p (0x%p) -> 0x%p\n",
							symname, name, rela->r_offset, relput, symaddr);
			*relput = (uint64_t) symaddr;
			break;
		case R_X86_64_COPY:
			if (debugMode) dynld_printf("R_X86_64_COPY against `%s' at %s+0x%p (0x%p) <- 0x%p (0x%p bytes)\n",
							symname, name, rela->r_offset, relput, symaddr, symbol->st_size);
			memcpy(relput, symaddr, symbol->st_size);
			break;
		default:
			strcpy(dynld_errmsg, name);
			strcpy(&dynld_errmsg[strlen(dynld_errmsg)], ": invalid relocation");
			dynld_libclose(lib);
			return 0;
		};
	};

	// PLT resolution or lazy binding time
	if (lib->pltRela != NULL)
	{
		if ((flags & RTLD_NOW) || (bindNow))
		{
			// we must do all bindings immediately
			if (debugMode)
			{
				dynld_printf("dynld: performing PLT on object `%s' (0x%p relocations)\n", name, numPltRela);
			};

			for (i=0; i<numPltRela; i++)
			{
				if (dynld_pltreloc(lib, i) == NULL)
				{
					// dynld_errmsg already set to the appropriate message
					return 0;
				};
			};
		}
		else
		{			
			for (i=0; i<numPltRela; i++)
			{
				lib->pltgot[i+3] += lib->base;
			};
			
			lib->pltgot[1] = lib;
			lib->pltgot[2] = dynld_lazybind;
		};
	};
	
	return topAddr;
};

int dynld_libclose(Library *lib)
{
	if ((--lib->refcount) == 0)
	{
		// call destructors
		if (lib->finiFunc != NULL) lib->finiFunc();
		size_t i;
		for (i=0; i<lib->numFini; i++)
		{
			if (lib->finiVec[i] != NULL) lib->finiVec[i]();
		};
		
		for (i=0; i<lib->numDeps; i++)
		{
			dynld_libclose(lib->deps[i]);
		};
		
		for (i=0; i<lib->numSegs; i++)
		{
			munmap(lib->segs[i].base, lib->segs[i].size);
		};
		
		if (lib->prev != NULL)
		{
			lib->prev->next = lib->next;
		};
		
		if (lib->next != NULL)
		{
			lib->next->prev = lib->prev;
		};
		
		munmap(lib, 0x1000);
	};
	
	return 0;
};

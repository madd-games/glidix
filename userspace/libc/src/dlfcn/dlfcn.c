/*
	Glidix Runtime

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

#include <sys/glidix.h>
#include <stdio.h>
#include <__elf64.h>
#include <string.h>
#include <dlfcn.h>
#include <stdlib.h>
#include <sys/stat.h>

#define	DEFAULT_LIBRARY_PATH			"/initrd/lib:/lib:/usr/lib"
#define	LIBC_BASE				0x100000000

static uint64_t nextLoadAddr = 0x100000000;

typedef struct _Library
{
	char					soname[256];
	Elf64_Rela*				rela;
	size_t					szRela;
	size_t					szRelaEntry;
	Elf64_Rela*				pltRela;
	size_t					pltRelaSize;
	Elf64_Sym*				symtab;
	char*					strtab;
	uint64_t				loadAddr;
	Elf64_Word*				hashtab;
	unsigned int				symbolCount;
	int					mode;
	int					refcount;
	_glidix_libinfo				info;
	int					numDeps;
	struct _Library**			deps;
	struct _Library*			prev;
	struct _Library*			next;
} Library;

static Library *globalResolutionTable = NULL;
static char libdlError[256];
static Library __libc_handle;
static Library __main_handle;

uint64_t __alloc_pages(size_t len)
{
	if (nextLoadAddr & 0xFFF)
	{
		nextLoadAddr &= ~0xFFF;
		nextLoadAddr += 0x1000;
	};

	uint64_t out = nextLoadAddr;
	nextLoadAddr += len;
	return out;
};

void* __get_glob_data(const char *name)
{
	if (__main_handle.rela == NULL) return NULL;

	size_t numRela = __main_handle.szRela / __main_handle.szRelaEntry;
	size_t i;

	for (i=0; i<numRela; i++)
	{
		Elf64_Rela *rela = &__main_handle.rela[i];
		Elf64_Xword type = ELF64_R_TYPE(rela->r_info);
		Elf64_Xword symidx = ELF64_R_SYM(rela->r_info);

		if (type == R_X86_64_COPY)
		{
			Elf64_Sym *sym = &__main_handle.symtab[symidx];
			char *symname = &__main_handle.strtab[sym->st_name];

			if (strcmp(name, symname) == 0)
			{
				return (void*) sym->st_value;
			};
		};
	};

	return NULL;
};

int __dlclose(Library *lib);
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

void* __dlsym(Library *lib, const char *name);
const char *__dlsymname_inlib(Library *lib, void *ptr);
void* __dlsym_global(const char *name)
{
	Library *lib = globalResolutionTable;
	while (lib != NULL)
	{
		if (lib->mode & RTLD_GLOBAL)
		{
			void *sym = __dlsym(lib, name);
			if (sym != NULL) return sym;
			lib = lib->next;
		};
	};
	return NULL;
};

const char *__dlsymname(void *ptr)
{
	Library *lib = globalResolutionTable;
	while (lib != NULL)
	{
		const char *name = __dlsymname_inlib(lib, ptr);
		if (name != NULL) return name;
		lib = lib->next;
	};
	return "?";
};

static int relocate(Library *lib, Elf64_Rela *table, size_t num)
{
	size_t i;
	for (i=0; i<num; i++)
	{
		Elf64_Rela *rela = &table[i];
		Elf64_Xword type = ELF64_R_TYPE(rela->r_info);
		Elf64_Xword symidx = ELF64_R_SYM(rela->r_info);

		Elf64_Sym *symbol = &lib->symtab[symidx];
		const char *symname = &lib->strtab[symbol->st_name];
		if (symbol->st_name == 0) symname = "<noname>";

		void *symaddr = NULL;
		if (((symbol->st_shndx == 0) && isSymbolNeededFor(type)) || (type == R_X86_64_COPY))
		{
			symaddr = __dlsym_global(symname);
			if (symaddr == NULL)
			{
				//strcpy(libdlError, "undefined reference");
				sprintf(libdlError, "undefined reference to '%s'", symname);
				return -1;
			};
		}
		else
		{
			symaddr = (void*) (lib->loadAddr + symbol->st_value);
		};

		// for R_X86_64_GLOB_DAT relocations, if the symbol was copied by the main module
		// via R_X86_64_COPY relocations, we must relocate to that address, else we relocate
		// to this library's value.
		if (type == R_x86_64_GLOB_DAT)
		{
			void *globaddr = __get_glob_data(symname);
			if (globaddr != NULL)
			{
				symaddr = globaddr;
			};
		};

		void *reladdr = (void*) (lib->loadAddr + rela->r_offset);
		switch (type)
		{
		case R_X86_64_64:
			//printf("64 (%p) = '%s' + %d (%p)\n", reladdr, symname, (int)rela->r_addend, (void*)((uint64_t)symaddr + rela->r_addend));
			*((uint64_t*)reladdr) = (uint64_t) symaddr + rela->r_addend;
			break;
		case R_X86_64_JUMP_SLOT:
			//printf("JUMP_SLOT (%p) = '%s' (%p)\n", reladdr, symname, symaddr);
			*((uint64_t*)reladdr) = (uint64_t) symaddr;
			break;
		case R_X86_64_RELATIVE:
			//printf("RELATIVE (%p) = %p\n", reladdr, (void*)(lib->loadAddr + rela->r_addend));
			*((uint64_t*)reladdr) = (uint64_t) (lib->loadAddr + rela->r_addend);
			break;
		case R_x86_64_GLOB_DAT:
			//printf("GLOB_DAT (%p) = '%s' (%p)\n", reladdr, symname, symaddr);
			*((uint64_t*)reladdr) = (uint64_t) symaddr;
			break;
		case R_X86_64_COPY:
			//printf("COPY (%p <- %p), '%s' + %d bytes\n", reladdr, symaddr, symname, (int)symbol->st_size);
			memcpy(reladdr, symaddr, symbol->st_size);
			break;       
		default:
			strcpy(libdlError, "bad relocation");
			return 1;
		};
	};

	return 0;
};

Library* __libopen_withpaths(const char *soname, const char *rpath, const char *runpath, int mode);
static int __lib_process(Library *lib, _glidix_libinfo info)
{
	Elf64_Dyn *dyn = (Elf64_Dyn*) info.dynSection;
	lib->loadAddr = info.loadAddr;

	char *rpath = NULL;
	char *runpath = NULL;

	// pass 1: get most information, including number of needed libraries
	while (dyn->d_tag != DT_NULL)
	{
		if (dyn->d_tag == DT_RELA)
		{
			lib->rela = (Elf64_Rela*) (info.loadAddr + dyn->d_un.d_ptr);
		}
		else if (dyn->d_tag == DT_RELASZ)
		{
			lib->szRela = dyn->d_un.d_val;
		}
		else if (dyn->d_tag == DT_RELAENT)
		{
			lib->szRelaEntry = dyn->d_un.d_val;
		}
		else if (dyn->d_tag == DT_JMPREL)
		{
			lib->pltRela = (Elf64_Rela*) (info.loadAddr + dyn->d_un.d_ptr);
		}
		else if (dyn->d_tag == DT_PLTRELSZ)
		{
			lib->pltRelaSize = dyn->d_un.d_val;
		}
		else if (dyn->d_tag == DT_STRTAB)
		{
			lib->strtab = (char*) (info.loadAddr + dyn->d_un.d_ptr);
		}
		else if (dyn->d_tag == DT_SYMTAB)
		{
			lib->symtab = (Elf64_Sym*) (info.loadAddr + dyn->d_un.d_ptr);
		}
		else if (dyn->d_tag == DT_HASH)
		{
			lib->hashtab = (Elf64_Word*) (info.loadAddr + dyn->d_un.d_ptr);
		}
		else if (dyn->d_tag == DT_SYMENT)
		{
			if (dyn->d_un.d_val != sizeof(Elf64_Sym))
			{
				strcpy(libdlError, "symbol size mismatch");
				return -1;
			};
		}
		else if (dyn->d_tag == DT_NEEDED)
		{
			lib->numDeps++;
		};

		dyn++;
	};

	// pass 2: get RPATH and RUNPATH
	dyn = (Elf64_Dyn*) info.dynSection;
	while (dyn->d_tag != DT_NULL)
	{
		if (dyn->d_tag == DT_RPATH)
		{
			rpath = &lib->strtab[dyn->d_un.d_val];
		}
		else if (dyn->d_tag == DT_RUNPATH)
		{
			runpath = &lib->strtab[dyn->d_un.d_val];
		};

		dyn++;
	};

	// pass 3: load dependencies
	lib->deps = (Library**) malloc(lib->numDeps * sizeof(Library*));
	memset(lib->deps, 0, sizeof(Library*)*lib->numDeps);

	int i=0;
	dyn = (Elf64_Dyn*) info.dynSection;
	while (dyn->d_tag != DT_NULL)
	{
		if (dyn->d_tag == DT_NEEDED)
		{
			char *depsoname = &lib->strtab[dyn->d_un.d_val];
			Library *dep = __libopen_withpaths(depsoname, rpath, runpath, RTLD_NOW | RTLD_GLOBAL);

			if (dep == NULL)
			{
				for (i=0; i<lib->numDeps; i++)
				{
					if (lib->deps[i] != NULL) __dlclose(lib->deps[i]);
				};

				free(lib->deps);
				return -1;
			};

			lib->deps[i++] = dep;
		};

		dyn++;
	};

	if (lib->hashtab != NULL)
	{
		lib->symbolCount = (unsigned int) lib->hashtab[1];
	};

	if (lib->pltRela != NULL)
	{
		if (relocate(lib, lib->pltRela, lib->pltRelaSize / sizeof(Elf64_Rela)))
		{
			return -1;
		};
	};

	if (lib->rela != NULL)
	{
		if (relocate(lib, lib->rela, lib->szRela / lib->szRelaEntry))
		{
			return -1;
		};
	};

	return 0;
};

Library* __libopen_found(const char *path, const char *soname, int mode)
{
	_glidix_libinfo info;
	int status = _glidix_libopen(path, nextLoadAddr, &info);

	if (status == -1)
	{
		strcpy(libdlError, "_glidix_libopen failed");
		return NULL;
	};

	Library *lib = (Library*) malloc(sizeof(Library));
	memset(lib, 0, sizeof(Library));
	strcpy(lib->soname, soname);
	lib->mode = mode;
	nextLoadAddr = (info.nextLoadAddr & ~0xFFFL) + 0x1000;
	
	if (__lib_process(lib, info) != 0)
	{
		_glidix_libclose(&info);
		free(lib);
		return NULL;
	};

	lib->refcount = 1;
	memcpy(&lib->info, &info, sizeof(_glidix_libinfo));
	return lib;
};

void* __dlsym(Library *lib, const char *name)
{
	// we pass through global symbols then go to weak symbols
	unsigned char pass;
	for (pass=1; pass<3; pass++)
	{
		size_t i;
		for (i=0; i<lib->symbolCount; i++)
		{
			Elf64_Sym *sym = &lib->symtab[i];
			const char *thisName = &lib->strtab[sym->st_name];

			if (strcmp(thisName, name) == 0)
			{
				unsigned char binding = (sym->st_info >> 4) & 0xF;
				if (binding == pass)
				{
					return (void*)(lib->loadAddr + sym->st_value);
				};
			};
		};
	};

	return NULL;
};

const char *__dlsymname_inlib(Library *lib, void *ptr)
{
	uint64_t addr = (uint64_t) ptr;

	size_t i;
	for (i=0; i<lib->symbolCount; i++)
	{
		Elf64_Sym *sym = &lib->symtab[i];
		const char *thisName = &lib->strtab[sym->st_name];

		uint64_t symaddr = lib->loadAddr + sym->st_value;
		uint64_t symend = symaddr + sym->st_size;
		
		if ((symaddr <= addr) && (symend > addr))
		{
			return thisName;
		};
	};
	
	return NULL;
};

int __dlclose(Library *lib)
{
	lib->refcount--;

	if (lib->refcount == 0)
	{
		_glidix_libclose(&lib->info);
		if (lib->prev != NULL) lib->prev->next = lib->next;
		if (lib->next != NULL) lib->next->prev = lib->prev;

		int i;
		for (i=0; i<lib->numDeps; i++)
		{
			if (lib->deps[i] != NULL) __dlclose(lib->deps[i]);
		};

		free(lib);
	};

	return 0;
};

char *__dlerror()
{
	return libdlError;
};

int __find_lib_in_path(char *path, const char *soname, const char *searchPath)
{
	char *search = strdup(searchPath);
	char *token = strtok(search, ":");

	do
	{
		strcpy(path, token);
		if (path[strlen(path)-1] != '/') strcat(path, "/");
		strcat(path, soname);

		struct stat st;
		if (stat(path, &st) == 0)
		{
			free(search);
			return 0;
		};
		token = strtok(NULL, ":");
	} while (token != NULL);

	free(search);
	return -1;
};

int __find_lib(char *libpath, const char *soname, const char *rpath, const char *runpath)
{
	const char *scan = soname;
	while (*scan != 0)
	{
		if (*scan == '/')
		{
			strcpy(libpath, soname);
			return 0;
		};
		scan++;
	};

	char *ldpath = getenv("LD_LIBRARY_PATH");
	if (ldpath != NULL)
	{
		if (__find_lib_in_path(libpath, soname, ldpath) == 0) return 0;
	};

	if (rpath != NULL)
	{
		if (__find_lib_in_path(libpath, soname, rpath) == 0) return 0;
	};

	if (runpath != NULL)
	{
		if (__find_lib_in_path(libpath, soname, runpath) == 0) return 0;
	};

	if (__find_lib_in_path(libpath, soname, DEFAULT_LIBRARY_PATH) == 0) return 0;

	return -1;
};

Library* __libopen_withpaths(const char *soname, const char *rpath, const char *runpath, int mode)
{
	if (strcmp(soname, "libg.so") == 0)
	{
		// TODO: we should really do it so that libraries are identified by inode/dev pairs,
		// to allow symbolic links to point to libraries.
		soname = "libc.so";
	};

	Library *grt = globalResolutionTable;
	while (grt != NULL)
	{
		if (strcmp(grt->soname, soname) == 0)
		{
			grt->refcount++;
			return grt;
		};

		grt = grt->next;
	};

	char libpath[256];
	if (__find_lib(libpath, soname, rpath, runpath) != 0)
	{
		sprintf(libdlError, "could not find library '%s'", soname);
		//strcpy(libdlError, "could not find library\n");
		return NULL;
	};

	Library *lib = __libopen_found(libpath, soname, mode);
	if (lib == NULL) return NULL;

	// link into the global resolution table
	if (globalResolutionTable == NULL)
	{
		globalResolutionTable = grt;
	}
	else
	{
		Library *last = globalResolutionTable;
		while (last->next != NULL) last = last->next;
		lib->prev = last;
		last->next = lib;
	};

	return lib;
};

Library *__dlopen(const char *soname, int mode)
{
	if (soname == NULL)
	{
		__main_handle.refcount++;
		return &__main_handle;
	};

	return __libopen_withpaths(soname, NULL, NULL, mode);
};

extern Elf64_Dyn _DYNAMIC;
void __do_init();
typedef void (*__entry_t)(void);
char __interp_stack[4096];
void _exit(int status);
void __interp_main(Elf64_Dyn *execDyn, Elf64_Sym *mySymbols, unsigned int mySymbolCount, uint64_t firstLoadAddr, char *myStringTable, uint64_t entry)
{
	nextLoadAddr = firstLoadAddr;
	__do_init();

	// create a handle for the C library. it's already relocated.
	memset(&__libc_handle, 0, sizeof(Library));
	strcpy(__libc_handle.soname, "libc.so");
	__libc_handle.symtab = mySymbols;
	__libc_handle.strtab = myStringTable;
	__libc_handle.loadAddr = LIBC_BASE;
	__libc_handle.symbolCount = mySymbolCount;
	__libc_handle.mode = RTLD_NOW | RTLD_GLOBAL;
	__libc_handle.refcount = 1;

	// parse the dynamic information for the executable.
	memset(&__main_handle, 0, sizeof(Library));
	Elf64_Dyn *dyn = execDyn;
	while (dyn->d_tag != DT_NULL)
	{
		if (dyn->d_tag == DT_SYMTAB)
		{
			__main_handle.symtab = (Elf64_Sym*) dyn->d_un.d_ptr;
		}
		else if (dyn->d_tag == DT_STRTAB)
		{
			__main_handle.strtab = (char*) dyn->d_un.d_ptr;
		}
		else if (dyn->d_tag == DT_RELA)
		{
			__main_handle.rela = (Elf64_Rela*) (dyn->d_un.d_ptr);
		}
		else if (dyn->d_tag == DT_RELASZ)
		{
			__main_handle.szRela = dyn->d_un.d_val;
		}
		else if (dyn->d_tag == DT_RELAENT)
		{
			__main_handle.szRelaEntry = dyn->d_un.d_val;
		};

		dyn++;
	};

	// set up the global resolution table
	// first we'll only have the C library on it. this is so that the executable
	// does not relocate its 'copy' relocations to itself.
	globalResolutionTable = &__libc_handle;

	__main_handle.info.dynSection = execDyn;
	__main_handle.info.loadAddr = 0;
	if (__lib_process(&__main_handle, __main_handle.info) != 0)
	{
		fprintf(stderr, "failed to process executable: %s\n", libdlError);
		_exit(1);
	};

	// OK, next link the executable in.
	__main_handle.next = globalResolutionTable;
	globalResolutionTable->prev = &__main_handle;
	globalResolutionTable = &__main_handle;

	// now the executable has copies of some of our data. we must now self-relocate, to reconnect with
	// this data.
	__libc_handle.info.dynSection = &_DYNAMIC;
	__libc_handle.info.loadAddr = LIBC_BASE;
	if (__lib_process(&__libc_handle, __libc_handle.info) != 0)
	{
		fprintf(stderr, "failed to second-pass-relocate the C library\n");
		_exit(1);
	};

	// jump to the entry point by pretending it's a function
	__entry_t entryFunc = (__entry_t) entry;
	entryFunc();

	while (1);
};

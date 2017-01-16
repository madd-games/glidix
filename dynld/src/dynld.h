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

#ifndef DYNLD_H_
#define DYNLD_H_

#include <sys/types.h>
#include <stdarg.h>
#include <inttypes.h>
#include <dlfcn.h>		/* RTLD_* */

#include "elf64.h"

/**
 * Describes a library segment mapped into memory.
 */
typedef struct
{
	/**
	 * Base address.
	 */
	void*					base;
	
	/**
	 * Size in bytes.
	 */
	size_t					size;
} Segment;

/**
 * Initialization/termination function.
 */
typedef void (*InitFunc)(void);
typedef void (*FiniFunc)(void);

/**
 * Describes a library loaded into memory.
 */
typedef struct Library_
{
	/**
	 * Links to the previous and next library in the chain.
	 */
	struct Library_*			prev;
	struct Library_*			next;
	
	/**
	 * Reference count - how many times this library was dlopen()'ed. dlclose()
	 * decrements this and releases the library once this reaches zero.
	 */
	int					refcount;
	
	/**
	 * Load address of this object.
	 */
	uint64_t				base;
	
	/**
	 * Name of this library.
	 */
	char					soname[128];
	
	/**
	 * Loading flags (RTLD_*)
	 */
	int					flags;
	
	/**
	 * Points to the dynamic linker information.
	 */
	Elf64_Dyn*				dyn;
	
	/**
	 * Points to the symbol table and string table.
	 */
	Elf64_Sym*				symtab;
	char*					strtab;
	size_t					numSymbols;
	
	/**
	 * Relocation table, and its size.
	 */
	Elf64_Rela*				rela;
	size_t					numRela;
	
	/**
	 * PLT relocation table.
	 */
	Elf64_Rela*				pltRela;
	
	/**
	 * Hash table.
	 */
	Elf64_Word*				hashtab;
	
	/**
	 * PLT GOT.
	 */
	void**					pltgot;
	
	/**
	 * Initialization function (or NULL), extra initialization functions, and
	 * the number of them.
	 */
	InitFunc				initFunc;
	InitFunc*				initVec;
	size_t					numInit;
	
	/**
	 * Same except destructors.
	 */
	FiniFunc				finiFunc;
	FiniFunc*				finiVec;
	size_t					numFini;
	
	/**
	 * Set to 1 if initialization has been performed.
	 */
	int					initDone;
	
	/**
	 * Segments constituting this library (max 64) and the actual amount.
	 */
	int					numSegs;
	Segment					segs[64];
	
	/**
	 * Pointers to libraries which this one depends on.
	 */
	int					numDeps;
	struct Library_*			deps[64];
} Library;

/**
 * A very simplistic string-formatting function.
 */
void dynld_vprintf(const char *fmt, va_list ap);
void dynld_printf(const char *fmt, ...);

/**
 * String operations.
 */
size_t strlen(const char *s);
void strcpy(char *dst, const char *src);
int memcmp(const void *a_, const void *b_, size_t sz);
void memset(void *put_, int val, size_t sz);
char* strchr(const char *s, char c);
void memcpy(void *dst_, const void *src_, size_t sz);
int strcmp(const char *a, const char *b);

/**
 * Linker error message buffer.
 */
extern char dynld_errmsg[];

/**
 * Value of the LD_LIBRARY_PATH, if present (else empty string).
 */
extern const char *libraryPath;

/**
 * Set to 1 if we are debugging.
 */
extern int debugMode;

/**
 * Set to 1 if PLT relocations msut be resolved immediately.
 */
extern int bindNow;

/**
 * Error number (from system calls).
 */
extern int dynld_errno;

/**
 * Get a library handle, and increase its reference count, if the specified object is already loaded.
 * Otherwise, return NULL.
 */
Library* dynld_getlib(const char *soname);

/**
 * Find a symbol by name in the specified library. Returns NULL if the symbol is not present.
 * 'binding' specified whether global (STB_GLOBAL) or weak (STB_WEAK) symbols should be searched for.
 */
void* dynld_libsym(Library *lib, const char *symname, unsigned char binding);

/**
 * Search for the named symbol in the global namespace. Returns NULL if not found.
 */
void* dynld_globsym(const char *symname);

/**
 * Map an object from a file into memory. Returns the amount of bytes to advance the load address
 * by on success, or 0 on error; in which case dynld_errmsg is set to an error string.
 */
uint64_t dynld_mapobj(Library *lib, int fd, uint64_t base, const char *name, int flags);

/**
 * Open a library file. If 'soname' contains a slash, it is interpreted as a direct path name; otherwise,
 * the remaining arguments are a NULL-terminated lsit of strings, which may contain multiple paths separated
 * by the ":" character. The paths are searches left-to-right, to find the desired library.
 *
 * On success, returns a file descriptor referring to the library; on error returns -1 and dynld_errmsg is filled
 * with an error message.
 */
int dynld_open(const char *soname, ...);

/**
 * Close a library. This may result in unmapping all its segments if it is no longer in use.
 */
void dynld_libclose(Library *lib);

/**
 * Initialize a library (and all its dependencies).
 */
void dynld_initlib(Library *lib);

#endif

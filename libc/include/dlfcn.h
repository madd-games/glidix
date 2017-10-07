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

#ifndef _DLFCN_H
#define _DLFCN_H

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/types.h>
#include <inttypes.h>

#define	RTLD_LAZY				(1 << 0)
#define	RTLD_NOW				(1 << 1)
#define	RTLD_LOCAL				(1 << 2)
#define	RTLD_GLOBAL				(1 << 3)

#ifdef _GLIDIX_SOURCE
#include <__elf64.h>

/**
 * Glidix-specific structure, specifying which object an address came from, its "raw"
 * (unrelocated) address, and some other information, returned by dladdrinfo().
 */
typedef struct
{
	/**
	 * The object that this address came from. Can be passed to dlsym(), but must
	 * never be passed to dlclose()!
	 *
	 * Note that this becomes invalidated if the object gets unloaded.
	 */
	void*					dl_object;
	
	/**
	 * Real (absolute) address.
	 */
	void*					dl_addr;
	
	/**
	 * Raw (unrelocated) address; that is, offset from the base of the object.
	 */
	uint64_t				dl_offset;
	
	/**
	 * Base address of the object.
	 */
	uint64_t				dl_base;
	
	/**
	 * Name of the object.
	 */
	const char*				dl_soname;
	
	/**
	 * Path to the object's image.
	 */
	const char*				dl_path;
} Dl_AddrInfo;

/**
 * Describes a library segment mapped into memory.
 */
typedef struct
{
	/**
	 * Absolute base address.
	 */
	void*					base;
	
	/**
	 * Size in bytes.
	 */
	size_t					size;
} Dl_Segment;

/**
 * Initialization/termination function.
 */
typedef void (*Dl_InitFunc)(void);
typedef void (*Dl_FiniFunc)(void);

/**
 * Describes a library loaded into memory.
 */
typedef struct Dl_Library_
{
	/**
	 * Links to the previous and next library in the chain.
	 */
	struct Dl_Library_*			prev;
	struct Dl_Library_*			next;
	
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
	 * Path to the library's image.
	 */
	char					path[256];
	
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
	Dl_InitFunc				initFunc;
	Dl_InitFunc*				initVec;
	size_t					numInit;
	
	/**
	 * Same except destructors.
	 */
	Dl_FiniFunc				finiFunc;
	Dl_FiniFunc*				finiVec;
	size_t					numFini;
	
	/**
	 * Set to 1 if initialization has been performed.
	 */
	int					initDone;
	
	/**
	 * Segments constituting this library (max 64) and the actual amount.
	 */
	int					numSegs;
	Dl_Segment				segs[64];
	
	/**
	 * Pointers to libraries which this one depends on.
	 */
	int					numDeps;
	struct Dl_Library_*			deps[64];
} Dl_Library;
#endif	/* _GLIDIX_SOURCE */

void *dlopen(const char *path, int mode);
void *dlsym(void *lib, const char *name);
int   dlclose(void *lib);
char *dlerror();

#ifdef _GLIDIX_SOURCE
size_t dladdrinfo(void *addr, Dl_AddrInfo *info, size_t infoSize);
#endif

#ifdef __cplusplus
}	/* extern "C" */
#endif

#endif

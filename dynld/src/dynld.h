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

#ifndef DYNLD_H_
#define DYNLD_H_

#include <sys/types.h>
#include <stdarg.h>
#include <inttypes.h>

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
	 * Name of this library.
	 */
	char					soname[128];
	
	/**
	 * Segments constituing this library (max 64) and they actual amount.
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

#endif

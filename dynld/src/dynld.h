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
Dl_Library* dynld_getlib(const char *soname);

/**
 * Find a symbol by name in the specified library. Returns NULL if the symbol is not present.
 * 'binding' specifies whether global (STB_GLOBAL) or weak (STB_WEAK) symbols should be searched for.
 */
void* dynld_libsym(Dl_Library *lib, const char *symname, unsigned char binding);

/**
 * Search for the named symbol in the global namespace, and if not found, then in 'lib' (makes a difference
 * when the library was loaded with RTLD_LOCAL). Returns NULL if not found.
 */
void* dynld_globsym(const char *symname, Dl_Library *lib);

/**
 * Map an object from a file into memory. Returns the amount of bytes to advance the load address
 * by on success, or 0 on error; in which case dynld_errmsg is set to an error string.
 */
uint64_t dynld_mapobj(Dl_Library *lib, int fd, uint64_t base, const char *name, int flags, const char *path);

/**
 * Open a library file. If 'soname' contains a slash, it is interpreted as a direct path name; otherwise,
 * the remaining arguments are a NULL-terminated lsit of strings, which may contain multiple paths separated
 * by the ":" character. The paths are searches left-to-right, to find the desired library.
 *
 * 'path' is the buffer into which the asbolute path of the library is written.
 *
 * On success, returns a file descriptor referring to the library; on error returns -1 and dynld_errmsg is filled
 * with an error message.
 */
int dynld_open(char *path, const char *soname, ...);

/**
 * Close a library. This may result in unmapping all its segments if it is no longer in use.
 */
int dynld_libclose(Dl_Library *lib);

/**
 * Initialize a library (and all its dependencies).
 */
void dynld_initlib(Dl_Library *lib);

/**
 * Get our PID.
 */
int dynld_getpid();

#endif

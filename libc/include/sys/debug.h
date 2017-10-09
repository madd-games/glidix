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

#ifndef _SYS_DEBUG_H
#define _SYS_DEBUG_H

#include <inttypes.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Represents a stack frame.
 */
struct stack_frame
{
	struct stack_frame*		sf_next;
	void*				sf_ip;
};

/**
 * Get the line, source file and function name corresponding to the specified raw (unrelocated)
 * address in the given object file. Returns a string on the heap, which must later be freed using
 * free(), in the form "<function>@<source-file>:<source-line>". Always returns something; even
 * if all fields are "??".
 *
 * This function is only successful if "objdump" from "binutils" is installed.
 */
char* __dbg_addr2line(const char *path, uint64_t offset);

/**
 * Get the symbol containing the specified raw (unrelocated) address in the given object file.
 * Returns a string on the heap (which may just be "??") and free() must be called on it later.
 */
char* __dbg_getsym(const char *path, uint64_t offset);

#ifdef __cplusplus
};	/* extern "C" */
#endif

#endif

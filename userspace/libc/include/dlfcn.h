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

#ifndef _DLFCN_H
#define _DLFCN_H

#ifdef __cplusplus
extern "C" {
#endif

#include <inttypes.h>

#define	RTLD_LAZY				(1 << 0)
#define	RTLD_NOW				(1 << 1)
#define	RTLD_LOCAL				(1 << 2)
#define	RTLD_GLOBAL				(1 << 3)

#define	__DLFCN_MAX_SEGMENTS			16

typedef struct
{
	void*				addr;
	size_t				len;
} __libinfo_segment;

typedef struct
{
	uint64_t			dynSize;
	void*				dynSection;
	uint64_t			loadAddr;
	uint64_t			nextLoadAddr;
	int				numSegments;
	__libinfo_segment		segments[__DLFCN_MAX_SEGMENTS];
} __libinfo;

void *dlopen(const char *path, int mode);
void *dlsym(void *lib, const char *name);
int   dlclose(void *lib);
char *dlerror();

#ifdef __cplusplus
}	/* extern "C" */
#endif

#endif

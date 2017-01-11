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

#ifndef _SYS_MMAN_H
#define _SYS_MMAN_H

#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

#define	PROT_NONE			0
#define	PROT_READ			(1 << 0)
#define	PROT_WRITE			(1 << 1)
#define	PROT_EXEC			(1 << 2)

#define	MAP_PRIVATE			(1 << 0)
#define	MAP_SHARED			(1 << 1)
#define	MAP_ANONYMOUS			(1 << 2)
#define	MAP_FIXED			(1 << 3)

#ifdef _GLIDIX_SOURCE
#define	MAP_THREAD			(1 << 4)
#define	MAP_UN				(1 << 5)
#endif

#define	MAP_ANON			MAP_ANONYMOUS

#define	MAP_FAILED			((void*)-1)

/* implemented by libglidix directly */
int mprotect(void *addr, size_t len, int prot);
int munmap(void *addr, size_t len);
void* mmap(void *addr, size_t len, int prot, int flags, int fd, off_t offset);

#ifdef __cplusplus
}	/* extern "C" */
#endif

#endif

/*
	Glidix Runtime

	Copyright (c) 2014, Madd Games.
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

#ifndef _FCNTL_H
#define _FCNTL_H

#include <sys/types.h>
#include <sys/stat.h>

#define	O_WRONLY			(1 << 0)
#define	O_RDONLY			(1 << 1)
#define	O_RDWR				(O_WRONLY | O_RDONLY)
#define	O_APPEND			(1 << 2)
#define	O_CREAT				(1 << 3)
#define	O_EXCL				(1 << 4)
#define	O_NOCTTY			(1 << 5)
#define	O_TRUNC				(1 << 6)
#define	O_DSYNC				(1 << 7)
#define	O_NONBLOCK			(1 << 8)
#define	O_RSYNC				(1 << 9)
#define	O_SYNC				(1 << 10)
#define	O_ACCMODE			(O_RDWR)

#ifdef __cplusplus
extern "C" {
#endif

/* implemented by the runtime */
int open(const char *path, int oflag, ...);

#ifdef __cplusplus
}	/* extern "C" */
#endif

#endif

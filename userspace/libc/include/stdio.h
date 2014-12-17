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

#ifndef _STDIO_H
#define _STDIO_H

#include <stdarg.h>
#include <stddef.h>

#define SEEK_SET 0
#define	SEEK_END 1
#define	SEEK_CUR 2

#define	BUFSIZ				1024
#define	FILENAME_MAX			128
#define	FOPEN_MAX			32

#define	TMP_MAX				10000

#define	EOF				-1

typedef uint64_t			fpos_t;

typedef struct __file
{
	/**
	 * The buffer for this FILE. bufsiz is the remaining number of bytes
	 * in the buffer. The 'trigger' is a character which when encountered,
	 * forces the buffer to be flushed.
	 */
	void				*_buf;
	const char			*_rdbuf;
	char				*_wrbuf;
	size_t				_bufsiz;
	size_t				_bufsiz_org;
	char				_trigger;

	/**
	 * This is called when the buffer overflows or an explicit flush or a flush
	 * caused by the trigger is needed.
	 */
	void (*_flush)(struct __file *fp);

	/**
	 * The output file descriptor, if applicable.
	 */
	int _fd;
} FILE;

#ifdef __cplusplus
extern "C" {
#endif

extern FILE* stdin;
extern FILE* stdout;
extern FILE* stderr;
#define stderr stderr

int fclose(FILE*);
int fflush(FILE*);
FILE* fopen(const char*, const char*);
int fprintf(FILE*, const char*, ...);
size_t fread(void*, size_t, size_t, FILE*);
int fseek(FILE*, long, int);
long ftell(FILE*);
size_t fwrite(const void*, size_t, size_t, FILE*);
void setbuf(FILE*, char*);
int vfprintf(FILE*, const char*, va_list);

#ifdef __cplusplus
}
#endif

#endif

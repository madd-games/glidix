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

#ifndef _STDIO_H
#define _STDIO_H

#include <sys/types.h>
#include <stdarg.h>
#include <stddef.h>
#include <pthread.h>

#define SEEK_SET 0
#define	SEEK_END 1
#define	SEEK_CUR 2

#define	_IOFBF				0
#define	_IOLBF				1
#define	_IONBF				2

#define	BUFSIZ				1024
#define	FILENAME_MAX			128
#define	FOPEN_MAX			32
#define	PATH_MAX			256

#define	TMP_MAX				10000

#define	EOF				-1

#define	L_cuserid			128

#define	__FILE_FERROR			(1 << 0)
#define	__FILE_READ			(1 << 1)
#define	__FILE_WRITE			(1 << 2)
#define	__FILE_EOF			(1 << 3)

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
	 * The file descriptor, if applicable.
	 */
	int _fd;
	
	/**
	 * This is used as a one-byte buffer when "no buffer" is used.
	 */
	char _nanobuf;

	/**
	 * The character pushed by ungetc().
	 */
	int _ungot;

	/**
	 * Flags.
	 */
	int _flags;
	
	/**
	 * pid of the child process if this stream was opened with popen().
	 */
	int _pid;
} FILE;

#ifdef __cplusplus
extern "C" {
#endif

extern FILE* stdin;
extern FILE* stdout;
extern FILE* stderr;
#define stderr stderr

int	fclose(FILE*);
int	fflush(FILE*);
FILE*	fopen(const char*, const char*);
FILE*	__fopen_strm(FILE *fp, const char*, const char*);
FILE*	freopen(const char*, const char*, FILE *fp);
int	fprintf(FILE*, const char*, ...);
size_t	fread(void*, size_t, size_t, FILE*);
int	fseek(FILE*, long, int);
long	ftell(FILE*);
size_t	fwrite(const void*, size_t, size_t, FILE*);
int	setvbuf(FILE *fp, char *buf, int type, size_t size);
void	setbuf(FILE*, char*);
int	vfprintf(FILE*, const char*, va_list);
int	fprintf(FILE*, const char*, ...);
int	vprintf(const char*, va_list);
int	printf(const char*, ...);
int	vsprintf(char *s, const char *fmt, va_list ap);
int	sprintf(char *s, const char *fmt, ...);
int	vsnprintf(char *s, size_t n, const char *fmt, va_list ap);
int	snprintf(char *s, size_t n, const char *fmt, ...);
int	asprintf(char **strp, const char *fmt, ...);
int	vasprintf(char **strp, const char *fmt, va_list ap);
int	fputs(const char *s, FILE *stream);
int	fputc(int, FILE*);
int	fgetc(FILE*);
int	putchar(int);
int	puts(const char *s);
int	ferror(FILE *fp);
int	feof(FILE *fp);
void	perror(const char *s);
FILE*	fdopen(int fd, const char *mode);
int	fileno(FILE *fp);
int	rename(const char *oldpath, const char *newpath);
int	ungetc(int c, FILE *fp);
int	vfscanf(FILE *stream, const char *format, va_list arg);
int	vscanf(const char *format, va_list arg);
int	vsscanf(const char *s, const char *format, va_list arg);
int	fscanf(FILE *stream, const char *format, ... );
int	scanf(const char *format, ... );
int	sscanf(const char *s, const char *format, ... );
char*	fgets(char *s, int size, FILE *stream);
void	clearerr(FILE *fp);
void	rewind(FILE *fp);
int	getchar();
int     getc_unlocked(FILE *stream);
int     getchar_unlocked(void);
int     putc_unlocked(int c, FILE *stream);
int     putchar_unlocked(int c);
int	fgetpos(FILE *fp, fpos_t *pos);
int	fsetpos(FILE *fp, const fpos_t *pos);
int	getc(FILE *fp);
int	putc(int c, FILE *fp);
int	remove(const char *path);
FILE*	tmpfile();
FILE*	popen(const char *cmd, const char *mode);
int	pclose(FILE *fp);
char*	cuserid(char *buffer);

/* off_t is the same as long on Glidix */
#define	fseeko					fseek
#define	ftello					ftell

#ifdef __cplusplus
}
#endif

#endif

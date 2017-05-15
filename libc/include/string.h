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

#ifndef _STRING_H
#define _STRING_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* copied from PDCLib and not really cleaned up */
#define _PDCLIB_restrict
void * memcpy( void *  s1, const void *  s2, size_t n );
void * memmove( void * s1, const void * , size_t n );
char * strcpy( char *  s1, const char *  s2 );
char * strncpy( char *  s1, const char *  s2, size_t n );
char * strcat( char *  s1, const char *  s2 );
char * strncat( char *  s1, const char *  s2, size_t n );
int memcmp( const void * s1, const void * s2, size_t n );
int strcmp( const char * s1, const char * s2 );
int strcoll( const char * s1, const char * s2 );
int strncmp( const char * s1, const char * s2, size_t n );
size_t strxfrm( char *  s1, const char *  s2, size_t n );
void * memchr( const void * s, int c, size_t n );
char * strchr( const char * s, int c );
size_t strcspn( const char * s1, const char * s2 );
char * strpbrk( const char * s1, const char * s2 );
char * strrchr( const char * s, int c );
size_t strspn( const char * s1, const char * s2 );
char * strstr( const char * s1, const char * s2 );
char * strtok( char *  s1, const char *  s2 );
char * strtok_r(char *s1, const char *s2, char **saveptr);
void * memset( void * s, int c, size_t n );
char * strerror( int errnum );
size_t strlen( const char * s );
size_t strnlen( const char * s, size_t maxlen );
char * strdup( const char* src );
char * strndup( const char* src, size_t n );
int    strerror_r(int errnum, char *strerrbuf, size_t buflen);
char *strsignal(int signum);

#ifdef __cplusplus
}	/* extern "C" */
#endif

#endif

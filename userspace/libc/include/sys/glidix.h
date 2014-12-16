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

#ifndef _SYS_GLIDIX_H
#define _SYS_GLIDIX_H

/**
 * Non-standard declarations for libglidix functions are here.
 */

#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

int	_glidix_exec(const char *path, const char *pars, size_t parsz);
int	_glidix_open(const char *path, int flags, mode_t mode);
uid_t	_glidix_getsuid();
gid_t	_glidix_getsgid();
void	_glidix_sighandler(void *handler);
void	_glidix_sigret(void *ret);
size_t	_glidix_getparsz();
void	_glidix_getpars(char *buffer, size_t size);
int	_glidix_geterrno();
void	_glidix_seterrno(int _errno);

#ifdef __cplusplus
}	/* extern "C" */
#endif

#endif

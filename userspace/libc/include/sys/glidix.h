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
#include <stddef.h>

typedef struct
{
	uint64_t rdi, rsi, rbp, rbx, rdx, rcx, rax;
	uint64_t r8, r9, r10, r11, r12, r13, r14, r15;
	uint64_t rip, rsp;
} __attribute__ ((packed)) _glidix_mstate;

#ifdef __cplusplus
extern "C" {
#endif

#define	_GLIDIX_CLONE_SHARE_MEMORY		(1 << 0)
#define	_GLIDIX_CLONE_SHARE_FTAB		(1 << 1)

#define	_GLIDIX_INSMOD_VERBOSE			(1 << 0)

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
pid_t	_glidix_clone(int flags, const _glidix_mstate *state);
pid_t	_glidix_pollpid(pid_t pid, int *stat_loc, int flags);
int	_glidix_insmod(const char *modname, const char *path, const char *opt, int flags);
int	_glidix_ioctl(int fd, unsigned long cmd, void *argp);
int	_glidix_fdopendir(const char *dirname);
void	_glidix_diag();

#ifdef __cplusplus
}	/* extern "C" */
#endif

#endif

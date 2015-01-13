/*
	Glidix Runtime

	Copyright (c) 2014-2015, Madd Games.
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

#ifndef _UNISTD_H
#define _UNISTD_H

#include <sys/types.h>
#include <stdio.h>		/* SEEK_* */

#ifdef __cplusplus
extern "C" {
#endif

/* implemented by the runtime */
int	execv(const char*, char* const[]);
int	execve(const char*, char* const[], char* const[]);
int	execvp(const char*, char* const[]);
pid_t	fork(void);
int	truncate(const char *path, off_t length);

/* implemented by libglidix directly */
ssize_t	write(int fildes, const void *buf, size_t nbyte);
ssize_t	read(int fildes, void *buf, size_t nbytes);
int	close(int fildes);
off_t	lseek(int fildes, off_t pos, int whence);
void	_exit(int status);
int	pause();
int	chdir(const char *path);
char*	getcwd(char*, size_t);
int	fsync(int fd);
int	chown(const char *path, uid_t uid, gid_t gid);
int	fchown(int fd, uid_t uid, gid_t gid);
int	ftruncate(int fd, off_t length);
int	unlink(const char *path);

#ifdef __cplusplus
}	/* extern "C" */
#endif

#endif

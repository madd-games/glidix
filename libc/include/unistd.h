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

#ifndef _UNISTD_H
#define _UNISTD_H

#include <sys/types.h>
#include <stdio.h>				/* SEEK_* */

#ifdef __cplusplus
extern "C" {
#endif

#define	_SC_GETPW_R_SIZE_MAX 			0
#define	_SC_GETGR_R_SIZE_MAX			1
#define	_SC_LOGIN_NAME_MAX			2
#define	_SC_PAGESIZE				3 // { Synonims
#define	_SC_PAGE_SIZE				3 // {
#define	_SC_NGROUPS_MAX				4
#define	_SC_OPEN_MAX				5

#define	LOGIN_NAME_MAX				127
#define	PAGESIZE				0x1000
#define	PAGE_SIZE				0x1000
#define	NGROUPS_MAX				64
#define	OPEN_MAX				256
#define	PATH_MAX				256

#define	_PC_LINK_MAX				0
#define	_PC_MAX_CANON				1
#define	_PC_MAX_INPUT				2
#define	_PC_NAME_MAX				3
#define	_PC_PATH_MAX				4

#define	_POSIX_LINK_MAX				255
#define	_POSIX_MAX_CANON			4095
#define	_POSIX_MAX_INPUT			4095
#define	_POSIX_NAME_MAX				127
#define	_POSIX_PATH_MAX				255
#define	_POSIX_MAPPED_FILES			1

#define	X_OK					(1 << 0)
#define	W_OK					(1 << 1)
#define	R_OK					(1 << 2)
#define	F_OK					0

#define	F_LOCK					0
#define	F_TLOCK					1
#define	F_ULOCK					2
#define	F_TEST					3

extern char* optarg;
extern int optind, opterr, optopt;

/* implemented by the runtime */
int		execv(const char*, char* const[]);
int		execve(const char*, char* const[], char* const[]);
int		execvp(const char*, char* const[]);
int		execl(const char *, const char *arg0, ...);
int		execle(const char *, const char *arg0, ...);
int		execlp(const char *, const char *arg0, ...);
pid_t		fork(void);
int		truncate(const char *path, off_t length);
long		fpathconf(int fd, int name);
long		pathconf(const char *path, int name);
long		sysconf(int name);
int		access(const char *path, int amode);
int		getdtablesize(void);
int		getopt(int argc, char* const argv[], const char* optstring);
int		getpagesize();
int		setpgrp();
pid_t		getpgrp();
pid_t		tcgetpgrp(int fd);
int		tcsetpgrp(int fd, pid_t pgrp);
int		lockf(int fd, int cmd, off_t len);
char*		getlogin();
int		getlogin_r(char *buf, size_t bufsize);
int		isatty(int fd);
char*		getcwd(char*, size_t);
ssize_t		readlink(const char *path, char *buf, size_t bufsiz);

/* implemented by libglidix directly */
ssize_t		write(int fildes, const void *buf, size_t nbyte);
ssize_t		read(int fildes, void *buf, size_t nbytes);
int		close(int fildes);
off_t		lseek(int fildes, off_t pos, int whence);
void		_exit(int status);
int		pause();
int		chdir(const char *path);
int		fsync(int fd);
int		chown(const char *path, uid_t uid, gid_t gid);
int		fchown(int fd, uid_t uid, gid_t gid);
int		ftruncate(int fd, off_t length);
int		unlink(const char *path);
int		dup(int fd);
int		dup2(int oldfd, int newfd);
int		dup3(int oldfd, int newfd, int flags);
int		pipe(int pipefd[2]);
int		pipe2(int pipefd[2], int flags);
uid_t		geteuid();
uid_t		getuid();
gid_t		getegid();
gid_t		getgid();
int		seteuid(uid_t);
int		setuid(uid_t);
int		setreuid(uid_t, uid_t);
int		setegid(gid_t);
int		setgid(gid_t);
int		setregid(gid_t, gid_t);
int		link(const char *oldname, const char *newname);
int		symlink(const char *oldname, const char *newname);
unsigned	sleep(unsigned seconds);
int		getgroups(int count, gid_t *out);
int		getpid();
void		_exit(int status);
int		getppid();
unsigned	alarm(unsigned sec);
pid_t		setsid();
int		setpgid(pid_t pid, pid_t pgid);
pid_t		getsid(pid_t pid);
pid_t		getpgid(pid_t pid);
ssize_t		pread(int fd, void *buf, size_t count, off_t offset);
ssize_t		pwrite(int fd, const void *buf, size_t count, off_t offset);
void		sync();
int		nice(int incr);
int		chroot(const char *path);
int		rmdir(const char *path);

/* libcrypt */
char*		crypt(const char *key, const char *salt);

#ifdef __cplusplus
}	/* extern "C" */
#endif

#endif

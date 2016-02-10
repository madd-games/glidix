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

#ifndef _SYS_TIME_H
#define _SYS_TIME_H

#include <sys/types.h>

struct timeval
{
	time_t				tv_sec;
	suseconds_t			tv_usec;
};

struct itimerval
{
	struct timeval			it_interval;
	struct timeval			it_value;
};

typedef struct
{
	long				fds_bits[1];
} fd_set;

enum
{
	ITIMER_REAL,
	ITIMER_VIRTUAL,
	ITIMER_PROF
};

#define	FD_CLR(fd, set) (set)->fds_bits[0] &= ~(1 << fd)
#define	FD_ISSET(fd, set) (!!((set)->fds_bits[0] & (1 << fd)))
#define	FD_SET(fd, set) (set)->fds_bits[0] |= (1 << fd)
#define	FD_ZERO(set) (set)->fds_bits[0] = 0
#define	FD_SETSIZE 32

#ifdef __cplusplus
extern "C" {
#endif

int   getitimer(int, struct itimerval *);
int   setitimer(int, const struct itimerval *, struct itimerval *);
int   gettimeofday(struct timeval *, void *);
int   select(int, fd_set *, fd_set *, fd_set *, struct timeval *);
int   utimes(const char *, const struct timeval [2]);

#ifdef __cplusplus
};	/* extern "C" */
#endif

#endif

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

#ifndef	_ERRNO_H
#define	_ERRNO_H

#include <sys/glidix.h>
#define	errno (*_glidix_geterrnoptr())

#define E2BIG                                     1
#define EACCES                                    2
#define EADDRINUSE                                3
#define EADDRNOTAVAIL                             4
#define EAFNOSUPPORT                              5
#define EAGAIN                                    6
#define EALREADY                                  7
#define EBADF                                     8
#define EBADMSG                                   9
#define EBUSY                                     10
#define ECANCELED                                 11
#define ECHILD                                    12
#define ECONNABORTED                              13
#define ECONNREFUSED                              14
#define ECONNRESET                                15
#define EDEADLK                                   16
#define EDESTADDRREQ                              17
#define EDOM                                      18
#define EDQUOT                                    19
#define EEXIST                                    20
#define EFAULT                                    21
#define EFBIG                                     22
#define EHOSTUNREACH                              23
#define EIDRM                                     24
#define EILSEQ                                    25
#define EINPROGRESS                               26
#define EINTR                                     27
#define EINVAL                                    28
#define EIO                                       29
#define EISCONN                                   30
#define EISDIR                                    31
#define ELOOP                                     32
#define EMFILE                                    33
#define EMLINK                                    34
#define EMSGSIZE                                  35
#define EMULTIHOP                                 36
#define ENAMETOOLONG                              37
#define ENETDOWN                                  38
#define ENETRESET                                 39
#define ENETUNREACH                               40
#define ENFILE                                    41
#define ENOBUFS                                   42
#define ENODATA                                   43
#define ENODEV                                    45
#define ENOENT                                    46
#define ENOEXEC                                   47
#define ENOLCK                                    48
#define ENOLINK                                   49
#define ENOMEM                                    50
#define ENOMSG                                    51
#define ENOPROTOOPT                               52
#define ENOSPC                                    53
#define ENOSR                                     54
#define ENOSTR                                    56
#define ENOSYS                                    58
#define ENOTCONN                                  59
#define ENOTDIR                                   60
#define ENOTEMPTY                                 61
#define ENOTSOCK                                  62
#define ENOTSUP                                   63
#define ENOTTY                                    64
#define ENXIO                                     65
#define EOPNOTSUPP                                66
#define EOVERFLOW                                 67
#define EPERM                                     68
#define EPIPE                                     69
#define EPROTO                                    70
#define EPROTONOSUPPORT                           71
#define EPROTOTYPE                                72
#define ERANGE                                    73
#define EROFS                                     74
#define ESPIPE                                    75
#define ESRCH                                     76
#define ESTALE                                    77
#define ETIME                                     78
#define ETIMEDOUT                                 80
#define ETXTBSY                                   81
#define EWOULDBLOCK                               EAGAIN
#define EXDEV                                     83

#endif

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

#ifndef _SYS_TYPES_H
#define _SYS_TYPES_H

#include <stdint.h>
#include <stddef.h>

#ifndef UINT8_MAX
#define	UINT8_MAX			0xFF
#endif

#ifndef UINT16_MAX
#define	UINT16_MAX			0xFFFF
#endif

#ifndef UINT32_MAX
#define UINT32_MAX			0xFFFFFFFF
#endif

typedef	uint64_t			dev_t;
typedef	uint64_t			ino_t;
typedef	uint64_t			mode_t;
typedef	uint64_t			nlink_t;
typedef	uint64_t			uid_t;
typedef	uint64_t			gid_t;
typedef	uint64_t			blksize_t;
typedef	uint64_t			blkcnt_t;
typedef int64_t				clock_t;
typedef	int64_t				time_t;
typedef	int64_t				off_t;
typedef	int64_t				ssize_t;
typedef int				pid_t;
typedef	int64_t				suseconds_t;
typedef	uint64_t			socklen_t;
typedef	uint16_t			sa_family_t;
typedef uint16_t			in_port_t;
typedef uint32_t			in_addr_t;
typedef void*				pthread_t;		/* actually points to a struct defined in pthread.h */
typedef	uint8_t				pthread_spinlock_t;

typedef struct
{
} pthread_attr_t;

#endif

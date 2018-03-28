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

#ifndef _SYS_XPERM_H
#define _SYS_XPERM_H

#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

#define	XP_RAWSOCK			(1 << 0)
#define	XP_NETCONF			(1 << 1)
#define	XP_MODULE			(1 << 2)
#define	XP_MOUNT			(1 << 3)
#define	XP_CHXPERM			(1 << 4)
#define	XP_NICE				(1 << 5)
#define	XP_DISPCONF			(1 << 6)
#define	XP_FSADMIN			(1 << 7)

#define	XP_ALL				0x7FFFFFFFFFFFFFFF
#define	XP_NCHG				0xFFFFFFFFFFFFFFFF

/* implemented by libglidix directly */
xperm_t _glidix_oxperm();
xperm_t _glidix_dxperm();
int     _glidix_chxperm(const char *path, xperm_t ixperm, xperm_t oxperm, xperm_t dxperm);
int     _glidix_fchxperm(int fd, xperm_t ixperm, xperm_t oxperm, xperm_t dxperm);
int	_glidix_haveperm(xperm_t perm);

#ifdef __cplusplus
}	/* extern "C" */
#endif

#endif

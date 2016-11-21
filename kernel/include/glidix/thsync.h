/*
	Glidix kernel

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

#ifndef __glidix_thsync_h
#define __glidix_thsync_h

#include <glidix/common.h>
#include <glidix/ioctl.h>
#include <glidix/vfs.h>

#define	IOCTL_MUTEX_LOCK		IOCTL_NOARG(IOCTL_INT_THSYNC, 0)
#define	IOCTL_MUTEX_UNLOCK		IOCTL_NOARG(IOCTL_INT_THSYNC, 1)
#define	IOCTL_SEMA_WAIT			IOCTL_ARG(int, IOCTL_INT_THSYNC, 2)
#define	IOCTL_SEMA_SIGNAL		IOCTL_ARG(int, IOCTL_INT_THSYNC, 3)

// synchronisation primitive types for thsync()
#define	THSYNC_MUTEX			0
#define	THSYNC_SEMA			1

/**
 * Provides thread synchronisation services to userspace. The _glidix_thsync()
 * function in libglidix may be called to create a file descriptor representing
 * a synchronisation primitive, and ioctl() calls may be used to lock and unlock
 * it.
 */
File *thsync(int type, int par);

#endif

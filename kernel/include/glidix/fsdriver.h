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

#ifndef __glidix_fsdriver_h
#define __glidix_fsdriver_h

/**
 * An interface for filesystem drivers.
 */

#include <glidix/vfs.h>
#include <glidix/mount.h>
#include <glidix/errno.h>

typedef struct _FSDriver
{
	/**
	 * This is called when mounting a filesystem. It has to fill the passed FileSystem structure with file pointers
	 * to appropriate driver functions. 0 is returned on success, -1 on error.
	 * 'image' is the path to the filesystem image, which could be a physical drive.
	 * 'szfs' is sizeof(FileSystem).
	 */
	int (*onMount)(const char *image, FileSystem *fs, size_t szfs);

	/**
	 * The name of this filesystem, as use by sys_mount().
	 */
	const char *name;

	struct _FSDriver *prev;
	struct _FSDriver *next;
} FSDriver;

void initFSDrivers();
void registerFSDriver(FSDriver *drv);
int sys_mount(const char *fsname, const char *image, const char *mountpoint, int flags);

#endif

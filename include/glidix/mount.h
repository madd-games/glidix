/*
	Glidix kernel

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

#ifndef __glidix_mount_h
#define __glidix_mount_h

/**
 * Managing a list of mount points.
 */

#include <glidix/vfs.h>

/**
 * Errors returned by mount() and unmount().
 */
#define	MOUNT_BAD_FLAGS		-1
#define MOUNT_BAD_PREFIX	-2

typedef struct _MountPoint
{
	// mountpoint prefix (the directory in which the filesystem is mounted), must start and end
	// with "/".
	char			prefix[512];

	// The filesystem mounted here.
	FileSystem		*fs;

	// Previous and next mountpoint.
	// They are sorted from longest prefix to shortest prefix btw.
	struct _MountPoint	*prev;
	struct _MountPoint	*next;
} MountPoint;

typedef struct
{
	FileSystem		*fs;
	char			filename[512];
} SplitPath;

void initMount();
int mount(const char *prefix, FileSystem *fs, int flags);
void unmount(const char *prefix);
int resolveMounts(const char *path, SplitPath *out);
void dumpMountTable();

#endif

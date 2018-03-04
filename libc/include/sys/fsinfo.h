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

#ifndef _SYS_FSINFO_H
#define _SYS_FSINFO_H

#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

struct fsinfo
{
	dev_t					fs_dev;
	char					fs_image[256];
	char					fs_mntpoint[256];
	char					fs_name[64];
	size_t					fs_usedino;
	size_t					fs_inodes;
	size_t					fs_usedblk;
	size_t					fs_blocks;
	size_t					fs_blksize;
	uint8_t					fs_bootid[16];
	char					fs_pad[968];
};

struct __fsinfo_record
{
	char					__image[256];
	char					__mntpoint[256];
};

/* implemented by the runtime */
size_t _glidix_fsinfo(struct fsinfo *list, size_t count);

#ifdef __cplusplus
}	/* extern "C" */
#endif

#endif

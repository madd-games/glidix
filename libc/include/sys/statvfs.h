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

#ifndef _SYS_STATVFS_H
#define	_SYS_STATVFS_H_

#define	ST_RDONLY			(1 << 0)
#define	ST_NOSUID			(1 << 1)

struct statvfs
{
	union
	{
		struct
		{
			unsigned long	f_bsize;
			unsigned long	f_frsize;
			fsblkcnt_t	f_blocks;
			fsblkcnt_t	f_bfree;
			fsblkcnt_t	f_bavail;
			fsfilcnt_t	f_files;
			fsfilcnt_t	f_ffree;
			fsfilcnt_t	f_favail;
			unsigned long	f_fsid;
			unsigned long	f_flag;
			unsigned long	f_namemax;
#ifdef _GLIDIX_SOURCE
			char		f_fstype[16];
			char		f_bootid[16];
#endif
		};
		
		char __pad[1024];
	};
};

#ifdef __cplusplus
extern "C" {
#endif

int statvfs(const char *path, struct statvfs *buf);
int fstatvfs(int fd, struct statvfs *buf);

#ifdef __cplusplus
}	/* extern "C" */
#endif

#endif

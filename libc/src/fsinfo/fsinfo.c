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

#include <sys/fsinfo.h>
#include <sys/statvfs.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

size_t _glidix_fsinfo(struct fsinfo *list, size_t count)
{
	struct statvfs st;
	size_t countOut = 0;

	int fd = open("/run/fsinfo", O_RDONLY);
	if (fd == -1)
	{
		errno = EAGAIN;
		return 0;
	};
	
	struct flock lock;
	memset(&lock, 0, sizeof(struct flock));
	lock.l_type = F_RDLCK;
	lock.l_whence = SEEK_SET;
	lock.l_start = 0;
	lock.l_len = 0;
	
	int status;
	do
	{
		status = fcntl(fd, F_SETLKW, &lock);
	} while (status != 0 && errno == EINTR);
	
	if (status != 0)
	{
		close(fd);
		errno = EAGAIN;
		return 0;
	};
	
	while (count--)
	{
		struct __fsinfo_record record;
		if (read(fd, &record, sizeof(struct __fsinfo_record)) != sizeof(struct __fsinfo_record))
		{
			break;
		};
		
		if (statvfs(record.__mntpoint, &st) != 0)
		{
			continue;
		};
		
		memset(&list[countOut], 0, sizeof(struct fsinfo));
		list[countOut].fs_dev = st.f_fsid;
		strcpy(list[countOut].fs_image, record.__image);
		strcpy(list[countOut].fs_mntpoint, record.__mntpoint);
		strcpy(list[countOut].fs_name, st.f_fstype);
		list[countOut].fs_usedino = st.f_files - st.f_ffree;
		list[countOut].fs_inodes = st.f_files;
		list[countOut].fs_usedblk = st.f_blocks - st.f_bfree;
		list[countOut].fs_blocks = st.f_blocks;
		list[countOut].fs_blksize = st.f_frsize;
		memcpy(list[countOut].fs_bootid, st.f_bootid, 16);
		
		countOut++;
	};
	
	close(fd);
	errno = EAGAIN;	// in case we didn't return anything for some reason
	return countOut;
};

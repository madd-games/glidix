/*
	Glidix Shell Utilities

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

#define	_GLIDIX_SOURCE
#include <sys/mount.h>
#include <sys/call.h>
#include <sys/fsinfo.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>

int main(int argc, char *argv[])
{
	if (argc != 2)
	{
		fprintf(stderr, "USAGE:\t%s block-device\n", argv[0]);
		fprintf(stderr, "\tSecurely mount the specified block device. This mounts it under\n");
		fprintf(stderr, "\t/media, and does not require root privileges. The mountpoint is\n");
		fprintf(stderr, "\tprinted to standard output on success.\n");
		return 1;
	};
	
	char *mediaName;
	char *slashPos = strrchr(argv[1], '/');
	if (slashPos == NULL)
	{
		mediaName = argv[1];
	}
	else
	{
		mediaName = slashPos+1;
	};
	
	char *drive = argv[1];
	
	char *mountPoint;
	if (asprintf(&mountPoint, "/media/%s", mediaName) == -1)
	{
		fprintf(stderr, "%s: fatal error: %s\n", argv[0], strerror(errno));
		return 1;
	};
	
	char fsnames[256*16];
	int drvcount = (int) __syscall(__SYS_fsdrv, fsnames, 256);
	if (drvcount == -1)
	{
		fprintf(stderr, "%s: cannot get filesystem driver list: %s\n", argv[0], strerror(errno));
		return 1;
	};
	
	const char *scan = fsnames;
	while (drvcount--)
	{
		const char *fstype = scan;
		scan += 16;
		
		if (mount(fstype, drive, mountPoint, MNT_TEMP, NULL, 0) == 0)
		{
			int fd = open("/run/fsinfo", O_WRONLY | O_APPEND);
			if (fd == -1)
			{
				fprintf(stderr, "%s: failed to add to /run/fsinfo: cannot open: %s\n", argv[0], strerror(errno));
				return 1;
			};
	
			struct flock lock;
			memset(&lock, 0, sizeof(struct flock));
			lock.l_type = F_WRLCK;
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
				fprintf(stderr, "%s: failed to add to /run/fsinfo: cannot acquire lock: %s\n",
							argv[0], strerror(errno));
				close(fd);
				return 1;
			};
	
			struct __fsinfo_record record;
			memset(&record, 0, sizeof(struct __fsinfo_record));
			strcpy(record.__image, drive);
			strcpy(record.__mntpoint, mountPoint);
			write(fd, &record, sizeof(struct __fsinfo_record));
			close(fd);
			
			printf("%s\n", mountPoint);
			return 0;
		};
	};
	
	fprintf(stderr, "%s: failed to mount or find suitable filesystem driver for `%s'\n", argv[0], drive);
	return 1;
};

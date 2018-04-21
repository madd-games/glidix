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

#include <sys/glidix.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/fsinfo.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>

const char *progName;
int force = 0;

struct __fsinfo_record *records;
size_t numRecords;

void processSwitches(const char *sw)
{
	const char *scan;
	for (scan=&sw[1]; *scan!=0; scan++)
	{
		switch (*scan)
		{
		case 'f':
			force = 1;
			break;
		default:
			fprintf(stderr, "%s: unrecognised command-line option: -%c\n", progName, *scan);
			break;
		};
	};
};

void usage()
{
	fprintf(stderr, "USAGE:\t%s [-f] mountpoint [mountpoint2...]\n", progName);
	fprintf(stderr, "\tUnmount filesystems.\n");
};

void doUnmount(const char *prefix)
{
	const char *mountpoint = realpath(prefix, NULL);
	if (mountpoint == NULL)
	{
		fprintf(stderr, "%s: cannot find %s: %s\n", progName, prefix, strerror(errno));
		if (!force) exit(1);
	};

	if (_glidix_unmount(mountpoint, 0) != 0)
	{
		perror(prefix);
		if (!force) exit(1);
	};
	
	size_t i;
	for (i=0; i<numRecords; i++)
	{
		if (strcmp(records[i].__mntpoint, mountpoint) == 0)
		{
			memset(&records[i], 0, sizeof(struct __fsinfo_record));
		};
	};
};

int main(int argc, char *argv[])
{
	progName = argv[0];
	int unmountsDone = 0;

	int fd = open("/run/fsinfo", O_WRONLY | O_APPEND);
	if (fd == -1)
	{
		fprintf(stderr, "%s: failed to access /run/fsinfo: cannot open: %s\n", argv[0], strerror(errno));
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
		fprintf(stderr, "%s: failed to access /run/fsinfo: cannot acquire lock: %s\n",
					argv[0], strerror(errno));
		close(fd);
		return 1;
	};

	struct stat st;
	if (fstat(fd, &st) != 0)
	{
		fprintf(stderr, "%s: failed to access /run/fsinfo: fstat failed: %s\n", argv[0], strerror(errno));
		close(fd);
		return 1;
	};
	
	void *map = mmap(NULL, st.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (map == MAP_FAILED)
	{
		fprintf(stderr, "%s: failed to access /run/fsinfo: mmap failed: %s\n", argv[0], strerror(errno));
		close(fd);
		return 1;
	};
	
	records = (struct __fsinfo_record*) map;
	numRecords = st.st_size / sizeof(struct __fsinfo_record);
	
	int i;
	for (i=1; i<argc; i++)
	{
		if (argv[i][0] == '-')
		{
			processSwitches(argv[i]);
		}
		else
		{
			unmountsDone++;
			doUnmount(argv[i]);
		};
	};

	close(fd);
	if ((unmountsDone == 0) && (!force))
	{
		usage();
		return 1;
	};

	return 0;
};

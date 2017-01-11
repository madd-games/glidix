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
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pwd.h>
#include <grp.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

const char *sizeScales[] = {
	"B", "KB", "MB", "GB", "TB", "PB", "EB"
};

void print_size(size_t sz)
{
	int idx = 0;
	while ((sz >= 1024) && (idx < 7))
	{
		sz /= 1024;
		idx++;
	};

	printf("%lu %s", sz, sizeScales[idx]);
};

const char *getFileType(mode_t mode)
{
	switch (mode & S_IFMT)
	{
	case S_IFBLK:
		return "Block device";
		break;
	case S_IFCHR:
		return "Character device";
		break;
	case S_IFIFO:
		return "FIFO";
		break;
	case S_IFREG:
		return "Regular file";
		break;
	case S_IFDIR:
		return "Directory";
		break;
	case S_IFLNK:
		return "Symbolic link";
		break;
	default:
		return "???";
		break;
	};
};

const char *binary64(uint64_t x)
{
	static char binary[64];
	int i;
	
	for (i=0; i<63; i++)
	{
		uint64_t whichBit = 62UL - (uint64_t)i;
		uint64_t mask = (1UL << whichBit);
		
		if (x & mask)
		{
			binary[i] = '1';
		}
		else
		{
			binary[i] = '0';
		};
	};
	
	binary[63] = 0;
	return binary;
};

int main(int argc, char *argv[])
{
	if ((argc != 2) && (argc != 3))
	{
		fprintf(stderr, "USAGE:\t%s [-f] filename\n\n", argv[0]);
		fprintf(stderr, "\tDisplays information about a file.\n");
		fprintf(stderr, "\t-f causes the file to be opened and fstat() called\n");
		return 1;
	};

	const char *filename = NULL;
	int do_fstat = 0;
	int i;
	for (i=1; i<argc; i++)
	{
		if (strcmp(argv[i], "-f") == 0)
		{
			do_fstat = 1;
		}
		else
		{
			filename = argv[i];
		};
	};
	
	if (filename == NULL)
	{
		fprintf(stderr, "%s: filename not specified\n", argv[0]);
		return 1;
	};
	
	struct stat st;
	if (!do_fstat)
	{
		if (lstat(filename, &st) != 0)
		{
			fprintf(stderr, "%s: %s: %s\n", argv[0], filename, strerror(errno));
			return 1;
		};
	}
	else
	{
		int fd = open(filename, O_RDONLY);
		if (fd == -1)
		{
			fprintf(stderr, "%s: cannot open %s: %s\n", argv[0], filename, strerror(errno));
			return 1;
		};
		
		if (fstat(fd, &st) != 0)
		{
			fprintf(stderr, "%s: fstat failed: %s\n", argv[0], strerror(errno));
			close(fd);
			return 1;
		};
		
		close(fd);
	};

	struct passwd *pwd = getpwuid(st.st_uid);
	const char *name = "error";
	if (pwd != NULL) name = pwd->pw_name;

	struct group *grp = getgrgid(st.st_gid);
	const char *grpname = "error";
	if (grp != NULL) grpname = grp->gr_name;

	printf("Device:     %lu\n", st.st_dev);
	printf("Inode:      %lu\n", st.st_ino);
	printf("Mode:       %04lo\n", st.st_mode & 07777);
	printf("Links:      %lu\n", st.st_nlink);
	printf("Owner:      %lu <%s>\n", st.st_uid, name);
	printf("Group:      %lu <%s>\n", st.st_gid, grpname);
	printf("Type:       %s\n", getFileType(st.st_mode));
	if (S_ISLNK(st.st_mode))
	{
	char target[256];
	ssize_t sz;
	if ((sz = readlink(argv[1], target, 255)) == -1) strcpy(target, "???");
	else target[sz] = 0;
	printf("Target:     %s\n", target);
	};
	printf("Size:       "); print_size(st.st_size); printf("\n");
	printf("A-time:     %s", ctime(&st.st_atime));
	printf("M-time:     %s", ctime(&st.st_mtime));
	printf("C-time:     %s", ctime(&st.st_ctime));
	printf("B-time:     %s", ctime(&st.st_btime));
	printf("Block size: "); print_size(st.st_blksize); printf("\n");
	printf("Blocks:     %lu\n", st.st_blocks);
	printf("I. Perms:   %s\n", binary64(st.st_ixperm));
	printf("O. Perms:   %s\n", binary64(st.st_oxperm));
	printf("D. Perms:   %s\n", binary64(st.st_dxperm));
	
	return 0;
};

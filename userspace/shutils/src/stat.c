/*
	Glidix Shell Utilities

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

#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pwd.h>
#include <grp.h>

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

	printf("%d %s", sz, sizeScales[idx]);
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

int main(int argc, char *argv[])
{
	if (argc != 2)
	{
		fprintf(stderr, "USAGE:\tstat filename\n\n");
		fprintf(stderr, "\tDisplays information about a file.\n");
		return 1;
	};

	struct stat st;
	if (lstat(argv[1], &st) != 0)
	{
		fprintf(stderr, "%s: ", argv[0]);
		perror(argv[1]);
		return 1;
	};

	struct passwd *pwd = getpwuid(st.st_uid);
	const char *name = "error";
	if (pwd != NULL) name = pwd->pw_name;

	struct group *grp = getgrgid(st.st_gid);
	const char *grpname = "error";
	if (grp != NULL) grpname = grp->gr_name;

	printf("Device:     %d\n", st.st_dev);
	printf("Inode:      %d\n", st.st_ino);
	printf("Mode:       %o\n", st.st_mode & 07777);
	printf("Links:      %d\n", st.st_nlink);
	printf("Owner:      %d <%s>\n", st.st_uid, name);
	printf("Group:      %d <%s>\n", st.st_gid, grpname);
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
	printf("A-time:     %d\n", st.st_atime);
	printf("M-time:     %d\n", st.st_mtime);
	printf("C-time:     %d\n", st.st_ctime);
	printf("Block size: "); print_size(st.st_blksize); printf("\n");
	printf("Blocks:     %d\n", st.st_blocks);
	return 0;
};

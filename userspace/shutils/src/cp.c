/*
	Glidix Shell Utilities

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

#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <dirent.h>
#include <string.h>

const char *progName;
char transferBuffer[2048];
int recur = 0;

void usage()
{
	fprintf(stderr, "USAGE:\t%s src dst\n", progName);
	fprintf(stderr, "\tCopy the file 'src' into the new name 'dst'.\n");
};

void copyFile(const char *src, const char *dst);

void copyDirRecur(const char *src, const char *dst)
{
	struct stat st;
	if (stat(dst, &st) != 0)
	{
		if (!S_ISDIR(st.st_mode))
		{
			fprintf(stderr, "%s: %s exists and is not a directory\n", progName, dst);
			exit(1);
		};
	}
	else
	{
		if (mkdir(dst, 0755) != 0)
		{
			fprintf(stderr, "%s: mkdir %s", progName, dst);
			perror("");
			exit(1);
		};
	};

	DIR *dp = opendir(src);
	if (dp == NULL)
	{
		perror(src);
		exit(1);
	};

	struct dirent *ent;
	while ((ent = readdir(dp)) != NULL)
	{
		if (strcmp(ent->d_name, ".") == 0)
		{
			continue;
		};

		if (strcmp(ent->d_name, "..") == 0)
		{
			continue;
		};

		char srcpath[256];
		strcpy(srcpath, src);
		if (srcpath[strlen(srcpath)-1] != '/') strcat(srcpath, "/");
		strcat(srcpath, ent->d_name);

		char dstpath[256];
		strcpy(dstpath, src);
		if (dstpath[strlen(dstpath)-1] != '/') strcat(dstpath, "/");
		strcat(dstpath, ent->d_name);

		copyFile(srcpath, dstpath);
	};

	closedir(dp);
};

void copyFile(const char *src, const char *dst)
{
	struct stat st;
	if (stat(src, &st) != 0)
	{
		fprintf(stderr, "%s: stat %s: ", progName, src);
		perror(NULL);
		exit(1);
	};

	if (S_ISDIR(st.st_mode))
	{
		if (recur)
		{
			copyDirRecur(src, dst);
		}
		else
		{
			fprintf(stderr, "%s: %s: is a directory\n", progName, src);
			exit(1);
		};
	};

	int fdSrc = open(src, O_RDONLY);
	if (fdSrc == -1)
	{
		fprintf(stderr, "%s: open %s for read: ", progName, src);
		perror(NULL);
		exit(1);
	};

	int fdDst = open(dst, O_WRONLY | O_TRUNC | O_CREAT, st.st_mode);
	if (fdDst == -1)
	{
		close(fdSrc);
		fprintf(stderr, "%s: open %s for write: ", progName, dst);
		perror(NULL);
		exit(1);
	};

	while (1)
	{
		ssize_t count = read(fdSrc, transferBuffer, 2048);
		if (count == -1)
		{
			close(fdSrc);
			close(fdDst);
			unlink(dst);
			fprintf(stderr, "%s: read %s: ", progName, src);
			perror(NULL);
			exit(1);
		};

		write(fdDst, transferBuffer, count);
		if (count < 2048)
		{
			break;
		};
	};

	close(fdSrc);
	close(fdDst);
};

void processSwitches(const char *sw)
{
	const char *scan;
	for (scan=&sw[1]; *scan!=0; scan++)
	{
		char c = *scan;
		switch (c)
		{
		case 'r':
			recur = 1;
			break;
		default:
			fprintf(stderr, "%s: unrecognised command-line option: -%c\n", progName, c);
			exit(1);
			break;
		};
	};
};

int main(int argc, char *argv[])
{
	progName = argv[0];

	const char *src = NULL;
	const char *dst = NULL;

	int i;
	for (i=1; i<argc; i++)
	{
		if (argv[i][0] == '-')
		{
			processSwitches(&argv[i][1]);
		}
		else if (src == NULL)
		{
			src = argv[i];
		}
		else if (dst == NULL)
		{
			dst = argv[i];
		}
		else
		{
			usage();
			return 1;
		};
	};

	if ((src == NULL) || (dst == NULL))
	{
		usage();
		return 1;
	};

	copyFile(src, dst);
	return 0;
};

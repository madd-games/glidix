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

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <string.h>
#include "mip.h"

void writeDir(FILE *fp, const char *pkgname, const char *fsname)
{
	DIR *dp = opendir(fsname);
	struct dirent *ent;

	while ((ent = readdir(dp)) != NULL)
	{
		if (ent->d_name[0] == '.') continue;

		char fullpath[256];
		strcpy(fullpath, fsname);
		strcat(fullpath, "/");
		strcat(fullpath, ent->d_name);

		char pkgpath[256];
		strcpy(pkgpath, pkgname);
		strcat(pkgpath, "/");
		strcat(pkgpath, ent->d_name);

		struct stat st;
		if (stat(fullpath, &st) != 0)
		{
			fprintf(stderr, "stat failed on %s", fullpath);
			perror("");
			continue;
		};

		if (S_ISDIR(st.st_mode))
		{
			MIPFile fileinfo;
			strcpy(fileinfo.filename, pkgpath);
			fileinfo.mode = ((uint16_t)st.st_mode & 0xFFF) | 0x8000;
			fileinfo.size = 0;
			fwrite(&fileinfo, 1, sizeof(MIPFile), fp);
			writeDir(fp, pkgpath, fullpath);
		}
		else
		{
			MIPFile fileinfo;
			strcpy(fileinfo.filename, pkgpath);
			fileinfo.mode = ((uint16_t)st.st_mode & 0xFFF);
			fileinfo.size = st.st_size;
			fwrite(&fileinfo, 1, sizeof(MIPFile), fp);

			FILE *infile = fopen(fullpath, "rb");
			int c;
			while ((c = fgetc(infile)) != EOF) fputc(c, fp);
			fclose(infile);
		};
	};

	closedir(dp);
};

static int startsWith(const char *str, const char *with)
{
	if (strlen(str) >= strlen(with))
	{
		if (memcmp(str, with, strlen(with)) == 0)
		{
			return 1;
		};
	};
	
	return 0;
};

int main(int argc, char *argv[])
{
	if (argc < 3)
	{
		fprintf(stderr, "USAGE:\t%s sysroot filename [--option=value]...\n", argv[0]);
		fprintf(stderr, "\tCreate a MIP package using the specified\n");
		fprintf(stderr, "\tsysroot, in the given filename.\n");
		return 1;
	};

	FILE *fp = fopen(argv[2], "wb");
	if (fp == NULL)
	{
		perror("fopen");
		return 1;
	};
	
	MIPHeader header;
	memcpy(&header.magic, "MIP", 3);
	header.version = 2;
	header.setupFile[0] = 0;

	int i;
	for (i=3; i<argc; i++)
	{
		if (startsWith(argv[i], "--setup="))
		{
			strcpy(header.setupFile, &argv[i][8]);
		}
		else
		{
			fprintf(stderr, "%s: unknown option: %s\n", argv[0], argv[i]);
			fclose(fp);
			return 1;
		};
	};
	
	fwrite(&header, 1, sizeof(MIPHeader), fp);

	writeDir(fp, "", argv[1]);

	fclose(fp);
	return 0;
};

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

#include <sys/wait.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include "mip.h"

#define	COPY_BUFFER_SIZE			0x8000

char destDir[256];
int installPackage(const char *path)
{
	FILE *fp = fopen(path, "rb");
	if (fp == NULL)
	{
		perror(path);
		return 1;
	};

	MIPHeader head;
	fread(&head, 1, sizeof(MIPHeader), fp);

	if (memcmp(head.magic, "MIP", 3) != 0)
	{
		fprintf(stderr, "this is not a valid MIP file\n");
		fclose(fp);
		return 1;
	};

	char setupFile[256] = "";
	if (head.version == 1)
	{
		fseek(fp, 4, SEEK_SET);
	}
	else if (head.version == 2)
	{
		strcpy(setupFile, head.setupFile);
	}
	else
	{
		fprintf(stderr, "unsupported MIP version (%d)\n", head.version);
		fclose(fp);
		return 1;
	};

	while (!feof(fp))
	{
		MIPFile fileinfo;
		if (fread(&fileinfo, 1, sizeof(MIPFile), fp) == 0) break;

		char destname[256];
		sprintf(destname, "%s%s", destDir, fileinfo.filename);
		
		if ((fileinfo.mode & MIP_MODE_TYPEMASK) == MIP_MODE_DIRECTORY)
		{
			printf("mkdir %s (%o)\n", destname, fileinfo.mode & 0xFFF);
			struct stat st;
			if (stat(destname, &st) == 0)
			{
				continue;
			};

			if (mkdir(destname, fileinfo.mode & 0xFFF) != 0)
			{
				perror("mkdir");
				fclose(fp);
				return 1;
			};
		}
		else if ((fileinfo.mode & MIP_MODE_TYPEMASK) == MIP_MODE_SYMLINK)
		{
			char target[fileinfo.size];
			fread(target, 1, fileinfo.size, fp);
			
			printf("symlink %s -> %s\n", destname, target);
			unlink(destname);		// if it already exists
			if (symlink(target, destname) != 0)
			{
				perror("symlink");
				fclose(fp);
				return 1;
			};
		}
		else if ((fileinfo.mode & MIP_MODE_TYPEMASK) == MIP_MODE_REGULAR)
		{
			printf("unpack %s (%o)\n", destname, fileinfo.mode);
			int fd = open(destname, O_CREAT | O_WRONLY | O_TRUNC, fileinfo.mode);
			if (fd == -1)
			{
				perror("open");
				fclose(fp);
				return 1;
			};
			
			char buffer[COPY_BUFFER_SIZE];
			uint64_t togo = fileinfo.size;
			
			while (togo > 0)
			{
				uint64_t doNow = togo;
				if (doNow > COPY_BUFFER_SIZE) doNow = COPY_BUFFER_SIZE;
				
				fread(buffer, 1, doNow, fp);
				write(fd, buffer, doNow);
				togo -= doNow;
			};

			close(fd);
		}
		else
		{
			fprintf(stderr, "file %s has invalid type value (mode=0x%04hX)\n", fileinfo.filename, fileinfo.mode);
			fclose(fp);
			return 1;
		}
	};

	if (setupFile[0] != 0)
	{
		printf("running setup script %s\n", setupFile);
		
		char fullpath[256];
		sprintf(fullpath, "%s%s", destDir, setupFile);
		
		char dirname[256];
		strcpy(dirname, fullpath);
		
		char *endslash = strrchr(dirname, '/');
		*endslash = 0;
		
		if (chdir(dirname) != 0)
		{
			fprintf(stderr, "failed to switch directory to %s\n", dirname);
			return 1;
		};
		
		pid_t pid = fork();
		if (pid == -1)
		{
			fprintf(stderr, "fork failed: %s\n", strerror(errno));
			return 1;
		}
		else if (pid == 0)
		{
			execl("/bin/sh", "sh", fullpath, NULL);
			perror("exec");
		}
		else
		{
			waitpid(pid, NULL, 0);
		};
	};
	
	printf("installation of %s complete\n", path);
	return 0;
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
	destDir[0] = 0;
	
	if (argc < 2)
	{
		fprintf(stderr, "USAGE:\t%s mip-files... [--option=value...]\n", argv[0]);
		fprintf(stderr, "\tInstall MIP files.\n");
		return 1;
	};

	if (geteuid() != 0)
	{
		fprintf(stderr, "you must be root\n");
		return 1;
	};

	int i;
	for (i=1; i<argc; i++)
	{
		if (startsWith(argv[i], "--dest="))
		{
			strcpy(destDir, &argv[i][7]);
		}
		else if (startsWith(argv[i], "--"))
		{
			printf("invalid command-line option: %s\n", argv[i]);
			return 1;
		};
	};
	
	for (i=1; i<argc; i++)
	{
		if (!startsWith(argv[i], "--"))
		{
			int status = installPackage(argv[i]);
			if (status != 0)
			{
				fprintf(stderr, "installation aborted\n");
				return status;
			};
		};
	};

	printf("installation complete\n");
	return 0;
};

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
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include "mip.h"

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

	if (head.version != 1)
	{
		fprintf(stderr, "unsupported MIP version (%d)\n", head.version);
		fclose(fp);
		return 1;
	};

	while (!feof(fp))
	{
		MIPFile fileinfo;
		if (fread(&fileinfo, 1, sizeof(MIPFile), fp) == 0) break;

		if (fileinfo.mode & 0x8000)
		{
			printf("mkdir %s (%o)\n", fileinfo.filename, fileinfo.mode & 0xFFF);
			struct stat st;
			if (stat(fileinfo.filename, &st) == 0)
			{
				continue;
			};

			if (mkdir(fileinfo.filename, fileinfo.mode & 0xFFF) != 0)
			{
				perror("mkdir");
				fclose(fp);
				return 1;
			};
		}
		else
		{
			printf("unpack %s (%o)\n", fileinfo.filename, fileinfo.mode);
			int fd = open(fileinfo.filename, O_CREAT | O_WRONLY | O_TRUNC, fileinfo.mode);
			if (fd == -1)
			{
				perror("open");
				fclose(fp);
				return 1;
			};

			//void *buffer = malloc(fileinfo.size);
			//if (buffer == NULL)
			//{
			//	fprintf(stderr, "out of memory!\n");
			//	return 1;
			//};
			char buffer[4096];
			uint64_t togo = fileinfo.size;
			
			while (togo > 0)
			{
				uint64_t doNow = togo;
				if (doNow > 4096) doNow = 4096;
				
				fread(buffer, 1, doNow, fp);
				write(fd, buffer, doNow);
				togo -= doNow;
			};

			close(fd);
		};
	};

	printf("installation of %s complete\n", path);
	return 0;
};

int main(int argc, char *argv[])
{
	if (argc < 2)
	{
		fprintf(stderr, "USAGE:\t%s mip-files...\n", argv[0]);
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
		int status = installPackage(argv[i]);
		if (status != 0)
		{
			fprintf(stderr, "installation aborted\n");
			return status;
		};
	};

	printf("installation complete\n");
	return 0;
};

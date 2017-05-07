/*
	Glidix GUI

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

#include <sys/mman.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <__elf64.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>

int findCommand(char *path, const char *cmd)
{
	const char *scan = cmd;
	while (*scan != 0)
	{
		if (*scan == '/')
		{
			strcpy(path, cmd);
			return 0;
		};
		scan++;
	};

	char *search = strdup(getenv("PATH"));
	char *token = strtok(search, ":");

	do
	{
		strcpy(path, token);
		if (path[strlen(path)-1] != '/') strcat(path, "/");
		strcat(path, cmd);

		struct stat st;
		if (stat(path, &st) == 0)
		{
			free(search);
			return 0;
		};
		token = strtok(NULL, ":");
	} while (token != NULL);

	free(search);
	return -1;
};

int main(int argc, char *argv[])
{
	if (argc < 2)
	{
		fprintf(stderr, "USAGE:\t%s <command> <args...>\n", argv[0]);
		fprintf(stderr, "\tRuns a command, opening a terminal if necessary.\n");
		return 1;
	};
	
	char path[PATH_MAX];
	if (findCommand(path, argv[1]) != 0)
	{
		// TODO: message box?
		fprintf(stderr, "%s: %s: command not found\n", argv[0], argv[1]);
		return 1;
	};
	
	// get the application type
	const char *runWith = "/usr/bin/terminal";
	int fd = open(path, O_RDONLY);
	if (fd == -1)
	{
		fprintf(stderr, "%s: cannot open %s: %s\n", argv[0], path, strerror(errno));
		return 1;
	};
	
	struct stat st;
	if (fstat(fd, &st) != 0)
	{
		fprintf(stderr, "%s: cannot fstat %s: %s\n", argv[0], path, strerror(errno));
		close(fd);
		return 1;
	};
	
	char *filedata = (char*) mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
	if (filedata == MAP_FAILED)
	{
		fprintf(stderr, "%s: cannot map %s: %s\n", argv[0], path, strerror(errno));
		close(fd);
		return 1;
	};
	
	close(fd);
	
	Elf64_Ehdr *ehdr = (Elf64_Ehdr*) filedata;
	Elf64_Shdr *sections = (Elf64_Shdr*) (filedata + ehdr->e_shoff);
	char *strings = filedata + sections[ehdr->e_shstrndx].sh_offset;
	
	int i;
	for (i=0; i<ehdr->e_shnum; i++)
	{
		if (strcmp(&strings[sections[i].sh_name], ".glidix.annot") == 0)
		{
			Elf64_GlidixAnnot *annot = (Elf64_GlidixAnnot*) (filedata + sections[i].sh_offset);
			int count = sections[i].sh_size / sizeof(Elf64_GlidixAnnot);
			
			int j;
			for (j=0; j<count; j++)
			{
				if (annot[j].gan_type == GAT_APPTYPE)
				{
					if (annot[j].gan_val == APT_GUI)
					{
						runWith = "/usr/bin/env";
					};
				};
			};
		};
	};
	
	munmap(filedata, st.st_size);
	
	close(0);
	close(1);
	close(2);
	dup(open("/dev/null", O_RDWR));
	dup(1);
	setsid();
	
	if (fork() == 0)
	{
		execv(runWith, argv);
		return 1;
	};
	
	return 0;
};

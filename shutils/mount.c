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

#ifndef __glidix__
#	error This program works on Glidix only!
#endif

#include <sys/glidix.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>

const char *progName;

void usage()
{
	fprintf(stderr, "USAGE:\t%s [-t <type>] <device> <mountpoint>\n", progName);
	fprintf(stderr, "\tMount a filesystem.\n");
};

int parseFstabLine(char *line)
{
	char *saveptr;
	
	char *mountpoint = strtok_r(line, " \t", &saveptr);
	if (mountpoint == NULL) return 1;
	
	char *type = strtok_r(NULL, " \t", &saveptr);
	if (type == NULL) return 1;
	
	char *device = strtok_r(NULL, " \t", &saveptr);
	if (device == NULL) return 1;
	
	char mountPrefix[256];
	strcpy(mountPrefix, mountpoint);
	if (mountPrefix[strlen(mountPrefix)-1] != '/') strcat(mountPrefix, "/");

	if (_glidix_mount(type, device, mountPrefix, 0, NULL, 0) != 0)
	{
		perror(progName);
		return 1;
	};
	
	return 0;
};

int doMountAll()
{
	if (geteuid() != 0)
	{
		fprintf(stderr, "%s: only root can do this\n", progName);
		return 1;
	};
	
	struct stat st;
	if (stat("/etc/fstab", &st) != 0)
	{
		fprintf(stderr, "%s: cannot stat /etc/fstab: %s\n", progName, strerror(errno));
		return 1;
	};
	
	int fd = open("/etc/fstab", O_RDONLY);
	if (fd == -1)
	{
		fprintf(stderr, "%s: cannot open /etc/fstab: %s\n", progName, strerror(errno));
		return 1;
	};
	
	char *data = (char*) malloc(st.st_size+1);
	data[st.st_size] = 0;
	read(fd, data, st.st_size);
	
	close(fd);
	
	char *saveptr;
	char *line;
	
	for (line=strtok_r(data, "\n", &saveptr); line!=NULL; line=strtok_r(NULL, "\n", &saveptr))
	{
		if (line[0] == '#')
		{
			continue;
		}
		else
		{
			if (parseFstabLine(line) != 0)
			{
				fprintf(stderr, "%s: fs table parse error at:\n`%s`\n", progName, line);
				return 1;
			};
		};
	};
	
	return 0;
};

int main(int argc, char *argv[])
{
	progName = argv[0];

	const char *fstype = NULL;
	const char *mountpoint = NULL;
	const char *device = NULL;
	int mountAll = 0;
	
	int i;
	for (i=1; i<argc; i++)
	{
		if (memcmp(argv[i], "-t", 2) == 0)
		{
			if (strlen(argv[i]) > 2)
			{
				fstype = &argv[i][2];
			}
			else
			{
				i++;
				if (i == argc)
				{
					usage();
					return 1;
				};
				fstype = argv[i];
			};
		}
		else if (strcmp(argv[i], "-a") == 0)
		{
			mountAll = 1;
		}
		else if (device == NULL)
		{
			device = argv[i];
		}
		else
		{
			mountpoint = argv[i];
		};
	};

	if (mountAll)
	{
		return doMountAll();
	};
	
	if ((mountpoint == NULL) || (device == NULL))
	{
		usage();
		return 1;
	};

	if (fstype == NULL)
	{
		// TODO
		fstype = "gxfs";
	};

	struct stat st;
	if (stat(mountpoint, &st) == 0)
	{
		if (!S_ISDIR(st.st_mode))
		{
			fprintf(stderr, "%s: %s is not a directory\n", argv[0], mountpoint);
			return 1;
		};
	}
	else
	{
		fprintf(stderr, "%s: %s: %s\n", argv[0], mountpoint, strerror(errno));
		return 1;
	};

	char mountPrefix[256];
	strcpy(mountPrefix, mountpoint);
	if (mountPrefix[strlen(mountPrefix)-1] != '/') strcat(mountPrefix, "/");

	if (_glidix_mount(fstype, device, mountPrefix, 0, NULL, 0) != 0)
	{
		perror(argv[0]);
		return 1;
	};

	return 0;
};

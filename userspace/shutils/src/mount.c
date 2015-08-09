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

#ifndef __glidix__
#	error This program works on Glidix only!
#endif

#include <sys/glidix.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

const char *progName;

void usage()
{
	fprintf(stderr, "USAGE:\t%s [-t <type>] <device> <mountpoint>\n", progName);
	fprintf(stderr, "\tMount a filesystem.\n");
};

int main(int argc, char *argv[])
{
	progName = argv[0];

	const char *fstype = NULL;
	const char *mountpoint = NULL;
	const char *device = NULL;

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
		else if (device == NULL)
		{
			device = argv[i];
		}
		else
		{
			mountpoint = argv[i];
		};
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

	if (_glidix_mount(fstype, device, mountPrefix, 0) != 0)
	{
		perror(argv[0]);
		return 1;
	};

	return 0;
};

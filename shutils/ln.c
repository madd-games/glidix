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
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

const char *progName;
int (*linkfunc)(const char *src, const char *dst);		// the function we'll call to make links (etiher link() or symlink())
int force = 0;

enum
{
	LINK_TARGET,
	LINK_DIR
};

void usage()
{
	fprintf(stderr, "USAGE:\t%s [-sf] source_file dest_file\n", progName);
	fprintf(stderr, "\t%s [-sf] source_file [source_file2] ... dest_dir\n", progName);
	fprintf(stderr, "\tCreate hard or symbolic (-s) links.\n");
};

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
		case 's':
			linkfunc = symlink;
			break;
		default:
			fprintf(stderr, "%s: unrecognised command-line option: -%c\n", progName, *scan);
			break;
		};
	};
};

int main(int argc, char *argv[])
{
	linkfunc = link;
	progName = argv[0];

	if (argc < 3)
	{
		usage();
		return 1;
	};

	char **sourceFiles;
	int numSourceFiles;

	if (argv[1][0] == '-')
	{
		processSwitches(argv[1]);
		if (argc < 4)
		{
			usage();
			return 1;
		};

		sourceFiles = &argv[2];
		numSourceFiles = argc - 3;
	}
	else
	{
		sourceFiles = &argv[1];
		numSourceFiles = argc - 2;
	};

	const char *dest = argv[argc-1];
	int kind = LINK_TARGET;
	struct stat st;

	if (stat(dest, &st) == 0)
	{
		if (S_ISDIR(st.st_mode))
		{
			kind = LINK_DIR;
		};
	};

	if ((kind == LINK_TARGET) && (numSourceFiles != 1))
	{
		fprintf(stderr, "%s: more than one source file but destination is not a directory!\n", argv[0]);
		return 1;
	};

	if (kind == LINK_TARGET)
	{
		if (linkfunc(sourceFiles[0], dest) != 0)
		{
			if (!force)
			{
				perror(progName);
				return 1;
			};
		};
	}
	else
	{
		int i;
		for (i=0; i<numSourceFiles; i++)
		{
			char destName[256];
			strcpy(destName, dest);
			if (destName[strlen(destName)-1] != '/') strcat(destName, "/");

			char *ptrslash = strrchr(sourceFiles[i], '/');
			if (ptrslash == NULL)
			{
				strcat(destName, sourceFiles[i]);
			}
			else
			{
				strcat(destName, ptrslash+1);
			};

			if (linkfunc(sourceFiles[i], destName) != 0)
			{
				if (!force)
				{
					fprintf(stderr, "%s: %s->%s: %s\n", progName, sourceFiles[i], destName, strerror(errno));
					return 1;
				};
			};
		};
	};

	return 0;
};

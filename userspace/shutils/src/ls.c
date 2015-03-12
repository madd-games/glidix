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
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const char *progname;
int showAll = 0;

void usage()
{
	fprintf(stderr, "USAGE:\t%s [-a] [dirname]\n\n", progname);
	fprintf(stderr, "\tList the contents of a directory. If no dirname is specified, it defaults\n");
	fprintf(stderr, "\tto the current working directory (`.`).\n");
	fprintf(stderr, "\t-a\tShows also entries with names starting with '.'\n");
};

void processSwitch(const char *sw)
{
	sw++;
	while (*sw != 0)
	{
		char c = *sw++;

		switch (c)
		{
		case 'a':
			showAll = 1;
			break;
		default:
			fprintf(stderr, "%s: unknown command-line option: -%c\n", progname, c);
			exit(1);
			break;
		};
	};
};

int main(int argc, char *argv[])
{
	progname = argv[0];
	const char *dirname = NULL;

	int i;
	for (i=1; i<argc; i++)
	{
		if (argv[i][0] == '-')
		{
			processSwitch(argv[i]);
		}
		else
		{
			if (dirname == NULL)
			{
				dirname = argv[i];
			}
			else
			{
				fprintf(stderr, "%s: only 1 directory should be specified\n", argv[0]);
				usage();
				return 1;
			};
		};
	};

	if (dirname == NULL)
	{
		dirname = ".";
	};

	DIR *dp = opendir(dirname);
	if (dp == NULL)
	{
		printf("%s ", argv[0]);
		perror(dirname);
		return 1;
	};

	struct dirent *ent;
	while ((ent = readdir(dp)) != NULL)
	{
		if ((ent->d_name[0] != '.') || (showAll))
		{
			printf("%s\n", ent->d_name);
		};
	};

	closedir(dp);
	return 0;
};

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
#include <fcntl.h>
#include <dirent.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

const char *progName;
int recur = 0;
int force = 0;
int verbose = 0;

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
		case 'f':
			force = 1;
			break;
		case 'v':
			verbose = 1;
			break;
		default:
			fprintf(stderr, "%s: unrecognised command-line option: -%c\n", progName, c);
			exit(1);
			break;
		};
	};
};

void doRemove(const char *name);

void doRemoveRecur(const char *name)
{
	DIR *dp = opendir(name);
	if (dp == NULL)
	{
		if ((errno != ENOENT) || (!force))
		{
			perror(name);
			exit(1);
		}
		else
		{
			return;
		};
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

		char fullpath[256];
		strcpy(fullpath, name);
		if (fullpath[strlen(fullpath)-1] != '/') strcat(fullpath, "/");
		strcat(fullpath, ent->d_name);

		doRemove(fullpath);
	};

	closedir(dp);
};

void doRemove(const char *name)
{
	if (verbose)
	{
		printf("delete %s\n", name);
	};

	struct stat st;
	if (lstat(name, &st) != 0)
	{
		if ((errno != ENOENT) || (!force))
		{
			perror(name);
			exit(1);
		}
		else
		{
			return;
		};
	};

	if (S_ISDIR(st.st_mode))
	{
		if (recur)
		{
			doRemoveRecur(name);
		};
	};

	if (unlink(name) != 0)
	{
		if ((errno != ENOENT) || (!force))
		{
			perror(name);
			exit(1);
		};
	};
};

int main(int argc, char *argv[])
{
	progName = argv[0];
	if (argc == 1)
	{
		fprintf(stderr, "USAGE:\t%s [-rf] names...\n", argv[0]);
		fprintf(stderr, "\tDelete files or directories. If some of the names refer to\n");
		fprintf(stderr, "\tdirectories, and -r is not passed, the directories must be empty.\n");
		fprintf(stderr, "\n\t-r\n");
		fprintf(stderr, "\t\tRemove recursively - directories and their contents are deleted.\n");
		fprintf(stderr, "\n\t-f\n");
		fprintf(stderr, "\t\tIf an given name does not exit, ignore silently, still returning success.\n");
		return 1;
	};

	int i;
	for (i=1; i<argc; i++)
	{
		if (argv[i][0] == '-')
		{
			processSwitches(argv[i]);
		}
		else
		{
			doRemove(argv[i]);
		};
	};

	return 0;
};

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
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <dirent.h>

const char *progName;
int recur = 0;
int force = 0;
int verbose = 0;

/**
 * The new mode of each file shall be (st_mode & modeAnd | modeOr).
 */
mode_t modeAnd = 07777;
mode_t modeOr  = 0;

void usage()
{
	fprintf(stderr, "USAGE:\t%s [-Rvf] mode file1 [file2...]\n", progName);
	fprintf(stderr, "\tChange access modes of files.\n");
	fprintf(stderr, "\n\t-R\n");
	fprintf(stderr, "\t\tApply changes to directory contents (recursively).\n");
	fprintf(stderr, "\n\t-v\n");
	fprintf(stderr, "\t\tBe verbose.\n");
	fprintf(stderr, "\n\t-f\n");
	fprintf(stderr, "\t\tIgnore errors.\n");
};

void processSwitches(const char *sw)
{
	const char *scan;
	for (scan=&sw[1]; *scan!=0; scan++)
	{
		switch (*scan)
		{
		case 'R':
			recur = 1;
			break;
		case 'f':
			force = 1;
			break;
		case 'v':
			verbose = 1;
			break;
		default:
			fprintf(stderr, "%s: unrecognised command-line option: -%c\n", progName, *scan);
			exit(1);
			break;
		};
	};
};

void doModeChange(const char *filename);

void doModeChangeRecur(const char *name)
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

		doModeChange(fullpath);
	};

	closedir(dp);
};

void doModeChange(const char *filename)
{
	struct stat st;
	if (stat(filename, &st) != 0)
	{
		if ((!force) || (verbose))
		{
			fprintf(stderr, "%s: stat failed on %s: %s\n", progName, filename, strerror(errno));
		};

		if (!force)
		{
			exit(1);
		};
	}
	else
	{
		if (S_ISDIR(st.st_mode))
		{
			if (recur)
			{
				doModeChangeRecur(filename);
			};
		};

		mode_t newMode = ((st.st_mode & modeAnd) | modeOr) & 07777;
		if (verbose)
		{
			printf("chmod %lo %s\n", newMode, filename);
		};

		if (chmod(filename, newMode) != 0)
		{
			if ((!force) || (verbose))
			{
				fprintf(stderr, "%s: chmod failed on %s: %s\n", progName, filename, strerror(errno));
			};

			if (!force)
			{
				exit(1);
			};
		};
	};
};

int main(int argc, char *argv[])
{
	progName = argv[0];

	const char *modespec = NULL;
	const char **files = NULL;
	int numFiles = 0;

	int i;
	for (i=1; i<argc; i++)
	{
		if (argv[i][0] == '-')
		{
			processSwitches(argv[i]);
		}
		else if (modespec == NULL)
		{
			modespec = argv[i];
		}
		else
		{
			files = realloc(files, sizeof(const char*)*(++numFiles));
			files[numFiles-1] = argv[i];
		};
	};

	if (numFiles == 0)		// if no mode was specified, there are no files either, so this test is enough.
	{
		usage();
		return 1;
	};

	mode_t classMask = 07777;
	char op;			// +, - or =
	mode_t modesToAffect = 0;

	char c = *modespec;
	if ((c >= '0') && (c <= '7'))
	{
		mode_t newMode;
		if (sscanf(modespec, "%lo", &newMode) != 1)
		{
			usage();
			return 1;
		};
		
		if (newMode > 07777)
		{
			usage();
			return 1;
		};

		modeAnd = 0;
		modeOr = newMode;
	}
	else
	{
		if ((c != '+') && (c != '-') && (c != '='))
		{
			classMask = 0;
		};

		while (((*modespec) != '+') && ((*modespec) != '-') && ((*modespec) != '='))
		{
			switch (*modespec)
			{
			case 0:
				fprintf(stderr, "%s: invalid format for symbolic mode\n", progName);
				break;
			case 'u':
				classMask |= 05700;
				break;
			case 'g':
				classMask |= 03070;
				break;
			case 'o':
				classMask |= 01007;
				break;
			case 'a':
				classMask = 07777;
				break;
			default:
				fprintf(stderr, "%s: unknown reference '%c'\n", progName, *modespec);
				return 1;
				break;
			};

			modespec++;
		};

		op = *modespec++;

		while (*modespec != 0)
		{
			switch (*modespec)
			{
			case 'r':
				modesToAffect |= 0444;
				break;
			case 'w':
				modesToAffect |= 0222;
				break;
			case 'x':
			case 'X':
				modesToAffect |= 0111;
				break;
			case 's':
				modesToAffect |= 06000;
				break;
			case 't':
				modesToAffect |= 01000;
				break;
			default:
				fprintf(stderr, "%s: unknown mode specifier '%c'\n", progName, *modespec);
				break;
			};

			modespec++;
		};

		switch (op)
		{
		case '+':
			modeAnd = 07777;			// keep current modes
			modeOr = modesToAffect & classMask;	// set the specified modes for the specified classes
			break;
		case '-':
			modeAnd = ~(modesToAffect & classMask);	// clear all permissions specified
			modeOr = 0;				// set no new permissions
			break;
		default: // '='
			modeAnd = ~classMask;			// clear all current modes for classes we are affecting
			modeOr = modesToAffect & classMask;	// set only the specified modes for the specified classes
			break;
		};
	};

	for (i=0; i<numFiles; i++)
	{
		doModeChange(files[i]);
	};

	return 0;
};

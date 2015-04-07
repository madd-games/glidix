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
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pwd.h>
#include <grp.h>
#include <errno.h>
#include <dirent.h>

const char *progName;
int recur = 0;
int force = 0;
int verbose = 0;
uid_t newUID = (uid_t)-1;
gid_t newGID = (gid_t)-1;

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

void usage()
{
	fprintf(stderr, "USAGE:\t%s [-Rvf] owner[:group] file1 [file2...]\n", progName);
	fprintf(stderr, "\tChange the owner and possibly associated group of files.\n");
	fprintf(stderr, "\n\t-R\n");
	fprintf(stderr, "\t\tApply changes to directory contents (recursively).\n");
	fprintf(stderr, "\n\t-v\n");
	fprintf(stderr, "\t\tBe verbose.\n");
	fprintf(stderr, "\n\t-f\n");
	fprintf(stderr, "\t\tIgnore errors.\n");
};

void doChangeOwner(const char *filename);

void doChangeOwnerRecur(const char *name)
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

		doChangeOwner(fullpath);
	};

	closedir(dp);
};

void doChangeOwner(const char *filename)
{
	struct stat st;
	if (stat(filename, &st) != 0)
	{
		if ((!force) || (verbose))
		{
			fprintf(stderr, "%s: cannot stat %s: %s\n", progName, filename, strerror(errno));
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
			doChangeOwnerRecur(filename);
		};

		if (verbose)
		{
			printf("chown %u:%u %s\n", newUID, newGID, filename);
		};

		if (chown(filename, newUID, newGID) != 0)
		{
			if ((!force) || (verbose))
			{
				fprintf(stderr, "%s: cannot chown %s: %s\n", progName, filename, strerror(errno));
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

	char *chspec = NULL;
	const char **files = NULL;
	int numFiles = 0;

	int i;
	for (i=1; i<argc; i++)
	{
		if (argv[i][0] == '-')
		{
			processSwitches(argv[i]);
		}
		else if (chspec == NULL)
		{
			chspec = argv[i];
		}
		else
		{
			files = realloc(files, sizeof(const char*)*(++numFiles));
			files[numFiles-1] = argv[i];
		};
	};

	if (numFiles == 0)
	{
		usage();
		return 1;
	};

	const char *newOwner = chspec;
	const char *group = NULL;

	char *colonAt = strchr(chspec, ':');
	if (colonAt != NULL)
	{
		*colonAt = 0;
		group = &colonAt[1];
	};

	struct passwd *pwd = getpwnam(newOwner);
	if (pwd == NULL)
	{
		fprintf(stderr, "%s: user '%s' not found\n", argv[0], newOwner);
		return 1;
	};

	newUID = pwd->pw_uid;

	if (group != NULL)
	{
		struct group *grp = getgrnam(group);
		if (grp == NULL)
		{
			fprintf(stderr, "%s: group '%s' not found\n", argv[0], group);
			return 1;
		};

		newGID = grp->gr_gid;
	};

	for (i=0; i<numFiles; i++)
	{
		doChangeOwner(files[i]);
	};

	return 0;
};

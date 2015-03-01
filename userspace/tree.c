/*
	Glidix Runtime

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

#include <stdio.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>
#include <string.h>

void usage(const char *pname)
{
	fprintf(stderr, "%s [<dirname>]\n", pname);
	fprintf(stderr, "  Displays a directory tree, starting at <dirname>.\n");
	fprintf(stderr, "  If dirname is not specified, then it defaults to '.'\n");
};

void displayTree(const char *dirname, int depth)
{
	DIR *dp = opendir(dirname);
	if (dp == NULL) return;

	struct dirent *ent;
	struct stat st;
	while ((ent = readdir(dp)) != NULL)
	{
		if (ent->d_name[0] == '.') continue;

		int count = depth;
		while (count--)
		{
			printf("\xB3");
		};

		printf("\xC3");

		char fullpath[256];
		strcpy(fullpath, dirname);
		if (dirname[strlen(dirname)-1] != '/') strcat(fullpath, "/");
		strcat(fullpath, ent->d_name);

		if (stat(fullpath, &st) != 0)
		{
			printf("%s <stat failed>\n", ent->d_name);
			continue;
		};

		if (S_ISDIR(st.st_mode))
		{
			printf("%s\n", ent->d_name);
			displayTree(fullpath, depth+1);
		}
		else
		{
			printf("%s (%d bytes)\n", ent->d_name, st.st_size);
		};
	};

	closedir(dp);
};

int main(int argc, char *argv[])
{
	if ((argc != 1) && (argc != 2))
	{
		usage(argv[0]);
		return 1;
	};

	const char *dirname = ".";
	if (argc == 2)
	{
		dirname = argv[1];
	};

	if (strcmp(dirname, "-h") == 0)
	{
		usage(argv[0]);
		return 1;
	};

	if (strcmp(dirname, "--help") == 0)
	{
		usage(argv[0]);
		return 1;
	};

	printf("%s\n", dirname);
	displayTree(dirname, 0);
	return 0;
};

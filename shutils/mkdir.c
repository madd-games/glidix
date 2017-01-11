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

#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

int verbose = 0;
int parents = 0;
mode_t mode = 0755;

const char *progName;

// returns 1 if it consumed the nextParam, 0 otherwise
int processSwitches(const char *sw, const char *nextParam)
{
	for (sw++; *sw!=0; sw++)
	{
		switch (*sw)
		{
		case 'v':
			verbose = 1;
			break;
		case 'p':
			parents = 1;
			break;
		case 'm':
			if (sw[1] == 0)
			{
				if (nextParam == NULL)
				{
					fprintf(stderr, "%s: -m switch requires a parameter\n", progName);
					exit(1);
				};
				
				if (sscanf(nextParam, "%lo", &mode) != 1)
				{
					fprintf(stderr, "%s: invalid access mode: %s\n", progName, nextParam);
					exit(1);
				};
				
				return 1;
			}
			else
			{
				if (sscanf(&sw[1], "%lo", &mode) != 1)
				{
					fprintf(stderr, "%s: invalid access mode: %s\n", progName, &sw[1]);
					exit(1);
				};
				
				return 0;
			};
		default:
			fprintf(stderr, "%s: invalid command-line option: -%c\n", progName, *sw);
			exit(1);
		};
	};
	
	return 0;
};

void processLongSwitch(const char *sw)
{
	mode_t modeTmp;
	
	if (strcmp(sw, "--parents") == 0)
	{
		parents = 1;
	}
	else if (strcmp(sw, "--verbose") == 0)
	{
		verbose = 1;
	}
	else if (sscanf(sw, "--mode=%lo", &modeTmp) == 1)
	{
		mode = modeTmp;
	}
	else
	{
		fprintf(stderr, "%s: invalid command-line option: %s\n", progName, sw);
		exit(1);
	};
};

void makeDir(const char *dirname)
{
	if (mkdir(dirname, mode) == 0)
	{
		if (verbose)
		{
			printf("%s: created directory '%s'\n", progName, dirname);
		};
	}
	else
	{
		if ((errno != EEXIST) || (!parents))
		{
			fprintf(stderr, "%s: cannot create %s: %s\n", progName, dirname, strerror(errno));
			exit(1);
		};
	};
};

void makeDirParents(const char *dirname)
{
	if (strlen(dirname) >= PATH_MAX)
	{
		fprintf(stderr, "%s: directory name too long: %s\n", progName, dirname);
		exit(1);
	};
	
	char path[PATH_MAX];
	memset(path, 0, PATH_MAX);
	
	char *put = path;
	*put++ = *dirname++;
	
	do
	{
		while ((*dirname != 0) && (*dirname != '/'))
		{
			*put++ = *dirname++;
		};
		
		makeDir(path);
		*put++ = *dirname++;
	} while (*dirname != 0);
};

void feedDir(const char *dirname)
{
	if (parents)
	{
		makeDirParents(dirname);
	}
	else
	{
		makeDir(dirname);
	};
};

int main(int argc, char *argv[])
{
	progName = argv[0];
	
	// process switches
	int i;
	for (i=1; i<argc; i++)
	{
		if (strcmp(argv[i], "--") == 0)
		{
			// no more options past this point
			break;
		}
		else if (memcmp(argv[i], "--", 2) == 0)
		{
			processLongSwitch(argv[i]);
		}
		else if (argv[i][0] == '-')
		{
			if (processSwitches(argv[i], argv[i+1]))
			{
				argv[i+1] = "-";
				i++;
			};
		};
	};
	
	if (mode > 0xFFF)
	{
		fprintf(stderr, "%s: invalid access mode: %lo\n", argv[0], mode);
		return 1;
	};
	
	// directory creation time
	int eatAll = 0;
	for (i=1; i<argc; i++)
	{
		if (eatAll)
		{
			feedDir(argv[i]);
		}
		else if (strcmp(argv[i], "--") == 0)
		{
			eatAll = 1;
		}
		else if (argv[i][0] != '-')
		{
			feedDir(argv[i]);
		};
	};
	
	return 0;
};

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
#include <time.h>
#include <fcntl.h>

int main(int argc, char *argv[])
{
	const char *infile = NULL;
	const char *outfile = NULL;
	size_t ibs = 512;
	size_t obs = 512;
	size_t count = 0;
	int limited = 0;
	size_t skip = 0;
	size_t seek = 0;
	
	int i;
	for (i=1; i<argc; i++)
	{
		char *equalPos = strchr(argv[i], '=');
		if (equalPos == NULL)
		{
			fprintf(stderr, "%s: unrecognised command-line option: `%s'\n", argv[0], argv[i]);
			return 1;
		};
		
		*equalPos = 0;
		char *paramName = argv[i];
		char *valueStr = equalPos+1;
		
		if (strcmp(paramName, "if") == 0)
		{
			infile = valueStr;
		}
		else if (strcmp(paramName, "of") == 0)
		{
			outfile = valueStr;
		}
		else if (strcmp(paramName, "ibs") == 0)
		{
			sscanf(valueStr, "%lu", &ibs);
		}
		else if (strcmp(paramName, "obs") == 0)
		{
			sscanf(valueStr, "%lu", &obs);
		}
		else if (strcmp(paramName, "bsize") == 0)
		{
			sscanf(valueStr, "%lu", &ibs);
			obs = ibs;
		}
		else if (strcmp(paramName, "count") == 0)
		{
			sscanf(valueStr, "%lu", &count);
			limited = 1;
		}
		else if (strcmp(paramName, "skip") == 0)
		{
			sscanf(valueStr, "%lu", &skip);
		}
		else if (strcmp(paramName, "seek") == 0)
		{
			sscanf(valueStr, "%lu", &seek);
		}
		else
		{
			fprintf(stderr, "%s: unrecognised option: `%s'\n", argv[0], paramName);
			return 1;
		};
	};
	
	int infd;
	int outfd;
	
	if (infile == NULL)
	{
		infd = 0;
	}
	else
	{
		infd = open(infile, O_RDONLY);
		if (infd == -1)
		{
			fprintf(stderr, "%s: cannot open %s for reading: %s\n", argv[0], infile, strerror(errno));
			return 1;
		};
	};
	
	if (outfile == NULL)
	{
		outfd = 1;
	}
	else
	{
		outfd = open(outfile, O_WRONLY);
		if (outfd == -1)
		{
			fprintf(stderr, "%s: cannot open %s for writing: %s\n", argv[0], outfile, strerror(errno));
			return 1;
		};
	};
	
	lseek(infd, ibs*skip, SEEK_SET);
	lseek(outfd, obs*seek, SEEK_SET);
	
	if (ibs > obs) ibs = obs;
	
	char buffer[obs];
	while ((count--) || (!limited))
	{
		memset(buffer, 0, obs);
		ssize_t sz = read(infd, buffer, ibs);
		
		if (sz == -1)
		{
			fprintf(stderr, "%s: cannot read: %s\n", argv[0], strerror(errno));
			return 1;
		};
		
		if (sz == 0) break;
		
		write(outfd, buffer, sz);
	};
	
	close(infd);
	close(outfd);
	return 0;
};

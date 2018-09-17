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

#include <sys/wait.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <libgpm.h>

char *progName;

void usage()
{
	fprintf(stderr, "USAGE:\t%s <command> [arguments...]\n", progName);
	fprintf(stderr, "\tGlidix Package Manager. Manipulates the package system.\n");
	fprintf(stderr, "\tFor help on a specific command, type '%s help <command>'\n", progName);
	fprintf(stderr, "\tSupported commands:\n\n");
	fprintf(stderr, "\treindex	info\n");
};

void usage_reindex()
{
	fprintf(stderr, "USAGE:\t%s reindex\n", progName);
	fprintf(stderr, "\tForce a re-build of the repository index.\n\n");
	
	fprintf(stderr, "\tThis reads the file `$GPM_ROOT/etc/gpm/repos.conf`, downloads each\n");
	fprintf(stderr, "\trepository index, and compiles them into a single index file located at\n");
	fprintf(stderr, "\t`$GPM_ROOT/etc/gpm/repos.index`.\n");
};

int main(int argc, char *argv[])
{
	progName = argv[0];
	
	if (argc < 2)
	{
		usage();
		return 1;
	};
	
	const char *cmd = argv[1];
	if (strcmp(cmd, "help") == 0)
	{
		if (argc != 3)
		{
			fprintf(stderr, "USAGE:\t%s help <command>\n", argv[0]);
			return 1;
		};
		
		const char *subject = argv[2];
		if (strcmp(subject, "reindex") == 0)
		{
			usage_reindex();
			return 1;
		}
		else
		{
			usage();
			return 1;
		};
	}
	else if (strcmp(cmd, "reindex") == 0)
	{
		int error;
		GPMContext *ctx = gpmCreateContext(getenv("GPM_ROOT"), &error);
		if (ctx == NULL)
		{
			fprintf(stderr, "%s: failed to create GPM context: error %d\n", argv[0], error);
			return 1;
		};
		
		if (gpmReindex(ctx) != 0)
		{
			fprintf(stderr, "%s: reindexing failed; see GPM log for more info.\n", argv[0]);
			return 1;
		};
		
		return 0;
	}
	else
	{
		fprintf(stderr, "%s: unknown command: %s\n", argv[0], cmd);
		usage();
		return 1;
	};
};

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

int main(int argc, char *argv[])
{
	char *dest = NULL;
	
	int i;
	int havepkg = 0;
	for (i=1; i<argc; i++)
	{
		if (memcmp(argv[i], "--dest=", 7) == 0)
		{
			if (dest != NULL)
			{
				fprintf(stderr, "%s: destination specified multiple times\n", argv[0]);
				return 1;
			};
			
			dest = &argv[i][7];
		}
		else if (argv[i][0] == '-')
		{
			fprintf(stderr, "%s: unrecognised command-line option: `%s'\n", argv[0], argv[i]);
			return 1;
		}
		else
		{
			havepkg = 1;
		};
	};
	
	if (!havepkg)
	{
		fprintf(stderr, "USAGE:\t%s mip-list... [--option=value...]\n", argv[0]);
		fprintf(stderr, "\tInstall the specified MIP files.\n");
		fprintf(stderr, "\n\t--dest=DESTINATION\n");
		fprintf(stderr, "\t\tSpecifies the system root to install under.\n");
		return 1;
	};
	
	int error;
	GPMContext *ctx = gpmCreateContext(dest, &error);
	if (ctx == NULL)
	{
		fprintf(stderr, "%s: failed to open package database: error %d\n", argv[0], error);
		return 1;
	};
	
	GPMInstallRequest *req = gpmCreateInstallRequest(ctx, &error);
	if (req == NULL)
	{
		fprintf(stderr, "%s: failed to create installation request: error %d\n", argv[0], error);
		return 1;
	};
	
	for (i=1; i<argc; i++)
	{
		if (argv[i][0] != '-')
		{
			if (gpmInstallRequestMIP(req, argv[i], &error) != 0)
			{
				fprintf(stderr, "%s: failed to add `%s' to installation request: error %d\n", argv[0], argv[i], error);
				return 1;
			};
		};
	};
	
	GPMTaskProgress *task = gpmRunInstall(req);
	if (task == NULL)
	{
		fprintf(stderr, "%s: failed to execute the installation task\n", argv[0]);
		return 1;
	};
	
	while (!gpmIsComplete(task))
	{
		float prog = gpmGetProgress(task);
		printf("\r%d%%      ", (int)(prog*100));
		fflush(stdout);
		sleep(1);
	};
	
	printf("\r100%%           ");
	fflush(stdout);
	
	const char *status = gpmWait(task);
	if (status == NULL)
	{
		printf("\n");
		return 0;
	}
	else
	{
		printf("\r             ");
		printf("\r%s: installation failed: %s\n", argv[0], status);
		return 1;
	};
};

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
	fprintf(stderr, "\treindex	info	install\n");
};

void usage_reindex()
{
	fprintf(stderr, "USAGE:\t%s reindex\n", progName);
	fprintf(stderr, "\tForce a re-build of the repository index.\n\n");
	
	fprintf(stderr, "\tThis reads the file `$GPM_ROOT/etc/gpm/repos.conf`, downloads each\n");
	fprintf(stderr, "\trepository index, and compiles them into a single index file located at\n");
	fprintf(stderr, "\t`$GPM_ROOT/etc/gpm/repos.index`.\n");
};

void usage_info()
{
	fprintf(stderr, "USAGE:\t%s info <package>\n", progName);
	fprintf(stderr, "\tDisplay information about the specified package.\n");
};

void usage_install()
{
	fprintf(stderr, "USAGE:\t%s install <packages...>\n", progName);
	fprintf(stderr, "\tInstall the named package(s) or update if necessary.\n");
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
		else if (strcmp(subject, "info") == 0)
		{
			usage_info();
			return 1;
		}
		else if (strcmp(subject, "install") == 0)
		{
			usage_install();
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
	else if (strcmp(cmd, "info") == 0)
	{
		if (argc != 3)
		{
			usage_info();
			return 1;
		};
		
		int error;
		GPMContext *ctx = gpmCreateContext(getenv("GPM_ROOT"), &error);
		if (ctx == NULL)
		{
			fprintf(stderr, "%s: failed to create GPM context: error %d\n", argv[0], error);
			return 1;
		};

		if (gpmMaybeReindex(ctx) != 0)
		{
			fprintf(stderr, "%s: failed to check repository index\n", argv[0]);
			return 1;
		};

		const char *pkg = argv[2];
		uint32_t currentVer = gpmGetPackageVersion(ctx, pkg);
		uint32_t latestVer = gpmGetLatestVersion(ctx, pkg);
		char *url = NULL;
		if (latestVer != 0) url = gpmGetPackageURL(ctx, pkg, latestVer);
		
		printf("Information about `%s':\n", pkg);
		
		if (currentVer == 0)
		{
			printf("  Currently not installed.\n");
		}
		else
		{
			char verbuf[64];
			gpmFormatVersion(verbuf, currentVer);
			printf("  Current version: %s\n", verbuf);
		};
		
		if (latestVer == 0)
		{
			printf("  Not available in repositories.\n");
		}
		else
		{
			char verbuf[64];
			gpmFormatVersion(verbuf, latestVer);
			printf("  Latest version:  %s\n", verbuf);
		};
		
		if (url == NULL)
		{
			printf("  No known download URL.\n");
		}
		else
		{
			printf("  Latest URL:      %s\n", url);
		};
		
		return 0;
	}
	else if (strcmp(cmd, "install") == 0)
	{
		if (argc < 3)
		{
			usage_install();
			return 1;
		};
		
		int error;
		GPMContext *ctx = gpmCreateContext(getenv("GPM_ROOT"), &error);
		if (ctx == NULL)
		{
			fprintf(stderr, "%s: failed to create GPM context: error %d\n", argv[0], error);
			return 1;
		};
		
		if (gpmMaybeReindex(ctx) != 0)
		{
			fprintf(stderr, "%s: failed to check repository index\n", argv[0]);
			return 1;
		};
		
		GPMInstallRequest *req = gpmCreateInstallRequest(ctx, &error);
		if (req == NULL)
		{
			fprintf(stderr, "%s: failed to create installation request: error %d\n", argv[0], error);
			return 1;
		};
		
		int i;
		for (i=2; i<argc; i++)
		{
			GPMTaskProgress *task = gpmInstallRequestAdd(req, argv[i], 0);
			if (task == NULL)
			{
				gpmDestroyContext(ctx);
				fprintf(stderr, "%s: failed to create download task\n", argv[0]);
				return 1;
			};
			
			while (!gpmIsComplete(task))
			{
				float prog = gpmGetProgress(task);
				printf("\rFetching %s... %d%%      ", argv[i], (int)(prog*100));
				fflush(stdout);
				sleep(1);
			};
			
			const char *status = gpmWait(task);
			if (status == NULL)
			{
				printf("\rFetching %s... Done              \n", argv[i]);
			}
			else
			{
				printf("\rFetching %s... Failed            \n", argv[i]);
				printf("%s\n", status);
				gpmDestroyContext(ctx);
				return 1;
			};
		};
		
		GPMTaskProgress *task = gpmRunInstall(req);
		if (task == NULL)
		{
			fprintf(stderr, "%s: failed to execute the installation task\n", argv[0]);
			gpmDestroyContext(ctx);
			return 1;
		};
		
		while (!gpmIsComplete(task))
		{
			float prog = gpmGetProgress(task);
			printf("\rInstalling... %d%%      ", (int)(prog*100));
			fflush(stdout);
			sleep(1);
		};
		
		const char *status = gpmWait(task);
		if (status == NULL)
		{
			printf("\rInstalling... Done           \n");
			gpmDestroyContext(ctx);
			return 0;
		}
		else
		{
			printf("\rInstalling... Failed         \n");
			printf("%s\n", status);
			gpmDestroyContext(ctx);
			return 1;
		};
	}
	else
	{
		fprintf(stderr, "%s: unknown command: %s\n", argv[0], cmd);
		usage();
		return 1;
	};
};

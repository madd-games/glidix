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
#include <sys/wait.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>

#define	NUM_LEVELS				3

char *progName;

int startService(const char *name)
{
	int state;
	for (state=1; state<=NUM_LEVELS; state++)
	{
		char startPath[PATH_MAX];
		sprintf(startPath, "/etc/services/%d/%s.start", state, name);
		
		struct stat st;
		if (stat(startPath, &st) == 0)
		{
			printf("service %s starting\n", name);
			execl(startPath, startPath, NULL);
			perror("exec");
			return 1;
		};
	};
	
	fprintf(stderr, "%s: %s: service not found\n", progName, name);
	return 1;
};

int stopService(const char *name)
{
	int state;
	for (state=1; state<=NUM_LEVELS; state++)
	{
		char stopPath[PATH_MAX];
		sprintf(stopPath, "/etc/services/%d/%s.stop", state, name);
		
		struct stat st;
		if (stat(stopPath, &st) == 0)
		{
			printf("service %s stopping\n", name);
			execl(stopPath, stopPath, NULL);
			perror("exec");
			return 1;
		};
	};
	
	fprintf(stderr, "%s: %s: service not found\n", progName, name);
	return 1;
};

int serviceState(const char *val)
{
	int level;
	if (sscanf(val, "%d", &level) != 1)
	{
		fprintf(stderr, "%s: invalid system state: %s\n", progName, val);
		return 1;
	};
	
	int state;
	for (state=0; state<NUM_LEVELS; state++)
	{
		char dirname[PATH_MAX];
		snprintf(dirname, PATH_MAX, "/etc/services/%d", state);
	
		DIR *dirp = opendir(dirname);
		if (dirp == NULL)
		{
			fprintf(stderr, "%s: could not scan %s: %s\n", progName, dirname, strerror(errno));
			continue;
		};
		
		const char *op;
		if (state <= level)
		{
			op = ".start";
		}
		else
		{
			op = ".stop";
		};
		
		pid_t *pidList = NULL;
		int numPids = 0;
	
		struct dirent *ent;
		while ((ent = readdir(dirp)) != NULL)
		{
			if (ent->d_name[0] != '.')
			{
				if (strlen(ent->d_name) > strlen(op))
				{
					if (strcmp(&ent->d_name[strlen(ent->d_name)-strlen(op)], op) == 0)
					{
						pid_t pid = fork();
						if (pid == 0)
						{
							char fullpath[PATH_MAX];
							sprintf(fullpath, "/etc/services/%d/%s", state, ent->d_name);
							execl(fullpath, fullpath, NULL);
							perror("execl");
							_exit(1);
						}
						else
						{
							int index = numPids++;
							pidList = (pid_t*) realloc(pidList, sizeof(pid_t)*numPids);
							pidList[index] = pid;
						};
					};
				};
			};
		};
	
		int i;
		for (i=0; i<numPids; i++)
		{
			waitpid(pidList[i], NULL, 0);
		};
	
		closedir(dirp);
	};

	return 0;
};

int main(int argc, char *argv[])
{
	progName = argv[0];
	
	if (geteuid() != 0)
	{
		fprintf(stderr, "%s: you must be root\n", argv[0]);
		return 1;
	};
	
	setuid(0);
	setgid(0);
	
	if (argc != 3)
	{
		fprintf(stderr, "%s: incorrect arguments\n", argv[0]);
		fprintf(stderr, "See service(1) for more information.\n");
		return 0;
	};
	
	if (strcmp(argv[1], "start") == 0)
	{
		return startService(argv[2]);
	}
	else if (strcmp(argv[1], "stop") == 0)
	{
		return stopService(argv[2]);
	}
	else if (strcmp(argv[1], "restart") == 0)
	{
		if (stopService(argv[2]) != 0) return 1;
		return startService(argv[2]);
	}
	else if (strcmp(argv[1], "state") == 0)
	{
		return serviceState(argv[2]);
	}
	else
	{
		fprintf(stderr, "%s: unknown command: %s\n", argv[0], argv[1]);
		return 1;
	};
};

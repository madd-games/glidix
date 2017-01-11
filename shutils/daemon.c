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

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>

const char *pidfile = NULL;
const char *infile = "/dev/null";
const char *logfile = "/dev/null";

int main(int argc, char *argv[])
{
	int i;
	for (i=1; i<argc; i++)
	{
		if (argv[i][0] != '-')
		{
			break;
		}
		else if (strcmp(argv[i], "-p") == 0)
		{
			i++;
			if (i == argc)
			{
				fprintf(stderr, "%s: the -p option expects a parameter\n", argv[0]);
				return 1;
			};
			
			pidfile = argv[i];
		}
		else if (strcmp(argv[i], "-i") == 0)
		{
			i++;
			if (i == argc)
			{
				fprintf(stderr, "%s: the -i option expects a parameter\n", argv[0]);
				return 1;
			};
			
			infile = argv[i];
		}
		else if (strcmp(argv[i], "-o") == 0)
		{
			i++;
			if (i == argc)
			{
				fprintf(stderr, "%s: the -o option expects a parameter\n", argv[0]);
				return 1;
			};
			
			logfile = argv[i];
		}
		else
		{
			fprintf(stderr, "%s: unrecognised command-line option: %s\n", argv[0], argv[i]);
			return 1;
		};
	};
	
	char **dargs = &argv[i];
	
	if (dargs[0] == NULL)
	{
		fprintf(stderr, "%s: no executable specified\n", argv[0]);
		return 1;
	};
	
	int pidfd = -1;
	if (pidfile != NULL)
	{
		pidfd = open(pidfile, O_RDWR | O_CREAT, 0600);
		if (pidfd == -1)
		{
			fprintf(stderr, "%s: could not open pidfile %s: %s\n", argv[0], pidfile, strerror(errno));
			return 1;
		};
	};
	
	int infd = open(infile, O_RDONLY);
	if (infd == -1)
	{
		fprintf(stderr, "%s: could not open input file %s: %s\n", argv[0], infile, strerror(errno));
		return 1;
	};
	
	int logfd = open(logfile, O_WRONLY | O_CREAT | O_APPEND, 0644);
	if (logfd == -1)
	{
		fprintf(stderr, "%s: could not open log file %s: %s\n", argv[0], logfile, strerror(errno));
		return 1;
	};
	
	pid_t pid = fork();
	if (pid == -1)
	{
		fprintf(stderr, "%s: fork failed: %s\n", argv[0], strerror(errno));
		return 1;
	}
	else if (pid == 0)
	{
		// intermediate process; start a new session
		setsid();
		
		// create the daemon process (it must not be the process group leader)
		pid_t pid = fork();
		if (pid == -1)
		{
			_exit(1);
		}
		else if (pid == 0)
		{
			// the daemon process
			if (pidfd != -1)
			{
				if (lockf(pidfd, F_TLOCK, 0) != 0)
				{
					// pidfile already in use
					_exit(1);
				};
				
				char buffer[256];
				sprintf(buffer, "%d\n", getpid());
				write(pidfd, buffer, strlen(buffer));
			};
			
			close(0);
			close(1);
			close(2);
			
			dup(infd);
			dup(logfd);
			dup(logfd);
			
			// close all other files
			for (i=0; i<sysconf(_SC_OPEN_MAX); i++)
			{
				if (i != pidfd)
				{
					close(i);
				};
			};
			
			// execute
			execv(dargs[0], dargs);
			_exit(1);
		}
		else
		{
			// exit, forcing the daemon to become owned by init
			_exit(0);
		};
	}
	else
	{
		// wait for the intermediate process to terminate
		waitpid(pid, NULL, 0);
	};
	
	return 0;
};

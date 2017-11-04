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
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <termios.h>
#include <signal.h>
#include <pwd.h>
#include <dirent.h>

int main(int argc, char *argv[])
{
	if (geteuid())
	{
		fprintf(stderr, "%s: must be run as root\n", argv[0]);
		return 1;
	};
	
	int rmhome = 0;
	int kick = 0;
	const char *username = NULL;
	
	int i;
	for (i=1; i<argc; i++)
	{
		if (argv[i][0] != '-')
		{
			if (username != NULL)
			{
				fprintf(stderr, "%s: only one user should be specified\n", argv[0]);
				return 1;
			};
			
			username = argv[i];
		}
		else if (argv[i][1] != '-')
		{
			// short option
			const char *scan;
			for (scan=&argv[i][1]; *scan!=0; scan++)
			{
				switch (*scan)
				{
				case 'r':
					rmhome = 1;
					break;
				case 'k':
					kick = 1;
					break;
				default:
					fprintf(stderr, "%s: unrecognized command-line switch `-%c'\n", argv[0], *scan);
					return 1;
				};
			};
		}
		else if (strcmp(argv[i], "--remove-home") == 0)
		{
			rmhome = 1;
		}
		else if (strcmp(argv[i], "--kick") == 0)
		{
			kick = 1;
		}
		else
		{
			fprintf(stderr, "%s: unrecognized command-line option `%s'\n", argv[0], argv[i]);
			return 1;
		};
	};
	
	if (username == NULL)
	{
		fprintf(stderr, "USAGE:\t%s [-r|--remove-home] [-k|--kick] <username>\n", argv[0]);
		fprintf(stderr, "\tDelete the user with the given username.\n\n");
		fprintf(stderr, "\t-r, --remove-home\n");
		fprintf(stderr, "\t\tDelete the user's home directory. If not passed, then\n");
		fprintf(stderr, "\t\tby default the directory's owner and group are set to root.\n\n");
		fprintf(stderr, "\t-k, --kick\n");
		fprintf(stderr, "\t\tKill all processes running for this user.\n");
		return 1;
	};
	
	fprintf(stderr, "Removing user account `%s'...\n", username);
	struct passwd *pwd = getpwnam(username);
	if (pwd == NULL)
	{
		fprintf(stderr, "User account not found.\n");
		return 1;
	};
	
	int pipefd[2];
	if (pipe(pipefd) != 0)
	{
		perror("pipe");
		return 1;
	};
	
	pid_t pid = fork();
	if (pid == -1)
	{
		perror("fork");
		return 1;
	}
	else if (pid == 0)
	{
		close(0);
		dup(pipefd[0]);
		close(pipefd[0]);
		close(pipefd[1]);
		execl("/usr/bin/userctl", "rmuser", NULL);
		perror("exec userctl");
		_exit(1);
	}
	else
	{
		FILE *cmdout = fdopen(pipefd[1], "w");
		close(pipefd[0]);
		fprintf(cmdout, "rmuser %s\n", username);
		fprintf(cmdout, "save\n");
		fclose(cmdout);
		
		int wstatus;
		waitpid(pid, &wstatus, 0);
		
		if (wstatus != 0)
		{
			return 1;
		};
	};
	
	if (rmhome)
	{
		fprintf(stderr, "Deleting home directory `%s'...\n", pwd->pw_dir);
		pid = fork();
		if (pid == -1)
		{
			perror("fork");
			return 1;
		}
		else if (pid == 0)
		{
			execl("/usr/bin/rm", "rm", "-r", pwd->pw_dir, NULL);
			_exit(1);
		}
		else
		{
			waitpid(pid, NULL, 0);
		};
	}
	else
	{
		fprintf(stderr, "Delegating home directory `%s' to root...\n", pwd->pw_dir);
		chown(pwd->pw_dir, 0, 0);
	};
	
	if (kick)
	{
		fprintf(stderr, "Kicking the user...\n");
		
		int gotAny;
		do
		{
			gotAny = 0;
			
			DIR *dirp = opendir("/proc");
			if (dirp == NULL)
			{
				perror("opendir /proc");
				return 1;
			};
		
			struct dirent *ent;
			while ((ent = readdir(dirp)) != NULL)
			{
				char *endptr;
				pid_t pid = (pid_t) strtoul(ent->d_name, &endptr, 10);
				if (*endptr == 0)
				{
					char path[256];
					sprintf(path, "/proc/%s", ent->d_name);
				
					struct stat st;
					if (stat(path, &st) == 0)
					{
						if (st.st_uid == pwd->pw_uid)
						{
							kill(pid, SIGKILL);
							gotAny = 1;
						};
					};
				};
			};
		
			closedir(dirp);
			sleep(1);
		} while (gotAny);
	};
	
	fprintf(stderr, "User deleted.\n");
	return 0;
};

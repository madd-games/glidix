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
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <termios.h>
#include <signal.h>
#include <pwd.h>

int main(int argc, char *argv[])
{
	if (argc != 2)
	{
		fprintf(stderr, "USAGE:\tadduser <username>\n");
		fprintf(stderr, "\tCreate a new user account.\n");
		return 1;
	};
	
	if (strlen(argv[1]) >= 128)
	{
		fprintf(stderr, "User name too long.\n");
		return 1;
	};
	
	if (geteuid() != 0)
	{
		fprintf(stderr, "%s: must be run as root\n", argv[0]);
		return 1;
	};
		
	struct termios tc;
	tcgetattr(0, &tc);
	tc.c_lflag &= ~(ECHO);
	tcsetattr(0, TCSANOW, &tc);

	char passwd1[1024];
	fprintf(stderr, "Creating user account `%s'...\n", argv[1]);
	fprintf(stderr, "Password for new user: "); fflush(stderr);
	if (fgets(passwd1, 1024, stdin) == NULL)
	{
		tc.c_lflag |= ECHO;
		tcsetattr(0, TCSANOW, &tc);
		return 1;
	};
	
	char *newline = strchr(passwd1, '\n');
	if (newline != NULL) *newline = 0;
	
	char passwd2[1024];
	fprintf(stderr, "Re-type password: "); fflush(stderr);
	if (fgets(passwd2, 1024, stdin) == NULL)
	{
		tc.c_lflag |= ECHO;
		tcsetattr(0, TCSANOW, &tc);
		return 1;
	};
	
	newline = strchr(passwd2, '\n');
	if (newline != NULL) *newline = 0;
	
	if (strcmp(passwd1, passwd2) != 0)
	{
		fprintf(stderr, "Passwords do not match.\n");
		tc.c_lflag |= ECHO;
		tcsetattr(0, TCSANOW, &tc);
		return 1;
	};

	tc.c_lflag |= ECHO;
	tcsetattr(0, TCSANOW, &tc);

	char fullname[1024];
	fprintf(stderr, "Full name (optional): "); fflush(stderr);
	if (fgets(fullname, 1024, stdin) == NULL)
	{
		fullname[0] = 0;
	};
	
	fprintf(stderr, "Creating the account...\n");
	int pipefd[2];
	if (pipe(pipefd) != 0)
	{
		perror("pipe");
		return 1;
	};
	
	char salt[2];
	int fd = open("/dev/urandom", O_RDONLY);
	if (fd == -1)
	{
		perror("open");
		return 1;
	};
	read(fd, salt, 2);
	close(fd);
	
	char *hash = crypt(passwd1, salt);
	
	pid_t pid = fork();
	if (pid == -1)
	{
		perror("fork");
		return 1;
	}
	else if (pid == 0)
	{
		close(pipefd[1]);
		close(0);
		dup(pipefd[0]);
		close(pipefd[0]);
		execl("/usr/bin/userctl", "adduser", NULL);
		perror("exec userctl");
		_exit(1);
	}
	else
	{
		close(pipefd[0]);
		FILE *cmdout = fdopen(pipefd[1], "w");
		fprintf(cmdout, "adduser %s %s /home/%s /bin/sh %s\n", argv[1], hash, argv[1], fullname);
		fprintf(cmdout, "save\n");
		fclose(cmdout);
		
		int wstatus;
		waitpid(pid, &wstatus, 0);
		if (wstatus != 0)
		{
			return 1;
		};
		
		char homedir[1024];
		sprintf(homedir, "/home/%s", argv[1]);
		
		fprintf(stderr, "Creating home directory `%s'\n", homedir);
		if (mkdir(homedir, 0700) != 0)
		{
			perror("mkdir");
			return 1;
		};
		
		pid = fork();
		if (pid == -1)
		{
			perror("fork");
			return 1;
		}
		else if (pid == 0)
		{
			char spec[512];
			sprintf(spec, "%s:%s", argv[1], argv[1]);
			execl("/usr/bin/chown", "chown", spec, homedir, NULL);
			perror("exec chown");
			_exit(1);
		}
		else
		{
			waitpid(pid, &wstatus, 0);
			if (wstatus != 0)
			{
				return 1;
			};
		};
		
		fprintf(stderr, "Account created.\n");
		return 0;
	};
};

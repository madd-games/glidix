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

void usage(const char *name)
{
	fprintf(stderr, "USAGE:\t%s\n", name);
	fprintf(stderr, "\tChanges the password of the current user.\n");
	fprintf(stderr, "USAGE:\t%s <username>\n", name);
	fprintf(stderr, "\tChanges the password of the given user; must be run as root\n");
};

int main(int argc, char *argv[])
{
	if (geteuid() != 0)
	{
		fprintf(stderr, "%s: must be run with effective root (suid bit should be set)\n", argv[0]);
		return 1;
	};
	
	if (argc != 1 && argc != 2)
	{
		usage(argv[0]);
		return 1;
	};
	
	if (argc == 2)
	{
		if (strcmp(argv[1], "--help") == 0)
		{
			usage(argv[0]);
			return 1;
		};
	};
	
	struct passwd *pwd = getpwuid(getuid());
	if (pwd == NULL)
	{
		fprintf(stderr, "%s: fatal error: could not determine current user (UID %lu)\n", argv[0], getuid());
		return 1;
	};

	const char *currentUserName = pwd->pw_name;
	
	int cmdpipe[2];
	if (pipe(cmdpipe) != 0)
	{
		fprintf(stderr, "%s: cannot create command pipe: %s\n", argv[0], strerror(errno));
		return 1;
	};
	
	int resultpipe[2];
	if (pipe(resultpipe) != 0)
	{
		fprintf(stderr, "%s: cannot create result pipe: %s\n", argv[0], strerror(errno));
		return 1;
	};
	
	pid_t pid = fork();
	if (pid == -1)
	{
		fprintf(stderr, "%s: cannot fork: %s\n", argv[0], strerror(errno));
		return 1;
	}
	else if (pid == 0)
	{
		// stdin = read side of command pipe
		close(0);
		dup(cmdpipe[0]);
		close(cmdpipe[0]);
		close(cmdpipe[1]);
		
		// stdout = write side of result pipe
		close(1);
		dup(resultpipe[1]);
		close(resultpipe[0]);
		close(resultpipe[1]);
		
		execl("/usr/bin/userctl", "passwd", NULL);
		fprintf(stderr, "%s: failed to execute userctl: %s\n", argv[0], strerror(errno));
		_exit(1);
	}
	else
	{
		FILE *cmdout = fdopen(cmdpipe[1], "w");
		close(cmdpipe[0]);
		
		FILE *resin = fdopen(resultpipe[0], "r");
		close(resultpipe[1]);
		
		struct termios tc;
		tcgetattr(0, &tc);
		tc.c_lflag &= ~(ECHO);
		tcsetattr(0, TCSANOW, &tc);

		const char *username;
		if (argc == 1)
		{
			fprintf(cmdout, "gethash %s\n", currentUserName);
			char hash[1024];
			if (fgets(hash, 1024, resin) == NULL)
			{
				waitpid(pid, NULL, 0);
				tc.c_lflag |= ECHO;
				tcsetattr(0, TCSANOW, &tc);
				return 1;
			};
			
			char *newline = strchr(hash, '\n');
			if (newline != NULL) *newline = 0;
			
			char linebuf[1024];
			fprintf(stderr, "Current password: "); fflush(stderr);
			if (fgets(linebuf, 1024, stdin) == NULL)
			{
				kill(pid, SIGKILL);
				waitpid(pid, NULL, 0);
				tc.c_lflag |= ECHO;
				tcsetattr(0, TCSANOW, &tc);
				return 1;
			};
			
			newline = strchr(linebuf, '\n');
			if (newline != NULL) *newline = 0;
			
			char *enteredHash = crypt(linebuf, hash);
			if (strcmp(enteredHash, hash) != 0)
			{
				fprintf(stderr, "%s: wrong password\n", argv[0]);
				tc.c_lflag |= ECHO;
				tcsetattr(0, TCSANOW, &tc);
				return 1;
			};
			
			username = currentUserName;
		}
		else
		{
			if (getuid() != 0)
			{
				fprintf(stderr, "%s: only root can set passwords of other users\n", argv[0]);
				tc.c_lflag |= ECHO;
				tcsetattr(0, TCSANOW, &tc);
				return 1;
			};
			
			username = argv[1];
		};

		char passwd1[1024];
		char passwd2[1024];
		fprintf(stderr, "New password: "); fflush(stderr);
		if (fgets(passwd1, 1024, stdin) == NULL)
		{
			kill(pid, SIGKILL);
			waitpid(pid, NULL, 0);
			return 1;
		};
		
		fprintf(stderr, "Re-type new password: "); fflush(stderr);
		if (fgets(passwd2, 1024, stdin) == NULL)
		{
			kill(pid, SIGKILL);
			waitpid(pid, NULL, 0);
			tc.c_lflag |= ECHO;
			tcsetattr(0, TCSANOW, &tc);
			return 1;
		};
		
		char *newline = strchr(passwd1, '\n');
		if (newline != NULL) *newline = 0;
		newline = strchr(passwd2, '\n');
		if (newline != NULL) *newline = 0;
		
		if (strcmp(passwd1, passwd2) != 0)
		{
			fprintf(stderr, "%s: passwords do not match\n", argv[0]);
			kill(pid, SIGKILL);
			waitpid(pid, NULL, 0);
			tc.c_lflag |= ECHO;
			tcsetattr(0, TCSANOW, &tc);
			return 1;
		};
		
		char salt[2];
		int fd = open("/dev/urandom", O_RDONLY);
		if (fd == -1)
		{
			fprintf(stderr, "%s: fatal error: failed to open /dev/urandom: %s\n", argv[0], strerror(errno));
			tc.c_lflag |= ECHO;
			tcsetattr(0, TCSANOW, &tc);
			return 1;
		};
		read(fd, salt, 2);
		close(fd);

		tc.c_lflag |= ECHO;
		tcsetattr(0, TCSANOW, &tc);

		char *newHash = crypt(passwd1, salt);
		fprintf(cmdout, "hash %s %s\n", username, newHash);
		fprintf(cmdout, "save\n");
		fclose(cmdout);
		
		int wstatus;
		waitpid(pid, &wstatus, 0);
		if (wstatus == 0)
		{
			fprintf(stderr, "Password updated.\n");
			return 0;
		};
		
		return 1;
	};
};

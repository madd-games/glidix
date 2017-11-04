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
#include <sys/glidix.h>
#include <stdio.h>
#include <pwd.h>
#include <termios.h>
#include <stdlib.h>
#include <unistd.h>
#include <grp.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>

char username[128];
char password[128];
char passcrypt[128];
char shname[128];

void getline(char *buffer)
{
	ssize_t count = read(0, buffer, 128);
	if (count == -1)
	{
		perror("read");
		exit(1);
	};

	if (count > 127)
	{
		fprintf(stderr, "input too long\n");
		exit(1);
	};

	buffer[count-1] = 0;
};

int nextShadowEntry(FILE *fp)
{
	char *put = shname;
	while (1)
	{
		int c = fgetc(fp);
		if (c == EOF)
		{
			return -1;
		};

		if (c == ':')
		{
			*put = 0;
			break;
		};

		*put++ = (char) c;
	};
	*put = 0;

	put = passcrypt;
	while (1)
	{
		int c = fgetc(fp);
		if (c == EOF)
		{
			return -1;
		};

		if (c == ':')
		{
			*put = 0;
			break;
		};

		*put++ = (char) c;
	};
	*put = 0;

	while (1)
	{
		int c = fgetc(fp);
		if (c == '\n')
		{
			break;
		};

		if (c == EOF)
		{
			return -1;
		};
	};

	return 0;
};

int findPassword(const char *username)
{
	FILE *fp = fopen("/etc/shadow", "r");
	if (fp == NULL)
	{
		perror("open /etc/shadow");
		exit(1);
	};

	struct flock lock;
	memset(&lock, 0, sizeof(struct flock));
	lock.l_type = F_RDLCK;
	lock.l_whence = SEEK_SET;
	lock.l_start = 0;
	lock.l_len = 0;
	
	int status;
	do
	{
		status = fcntl(fileno(fp), F_SETLKW, &lock);
	} while (status != 0 && errno == EINTR);

	while (nextShadowEntry(fp) != -1)
	{
		if (strcmp(shname, username) == 0)
		{
			fclose(fp);
			return 0;
		};
	};

	fclose(fp);
	return -1;
};

int main(int argc, char *argv[])
{
	if ((geteuid() != 0) || (getuid() != 0))
	{
		fprintf(stderr, "%s: you must be root\n", argv[0]);
		return 1;
	};

	while (1)
	{
		printf("Username: "); fflush(stdout); getline(username);

		struct termios tc;
		tcgetattr(0, &tc);
		tc.c_lflag &= ~(ECHO);
		tcsetattr(0, TCSANOW, &tc);

		printf("Password: "); fflush(stdout); getline(password);

		tc.c_lflag |= ECHO;
		tcsetattr(0, TCSANOW, &tc);

		struct passwd *pwd = getpwnam(username);
		if (pwd == NULL)
		{
			sleep(2);
			fprintf(stderr, "Sorry, please try again.\n");
			continue;
		};

		if (findPassword(username) != 0)
		{
			fprintf(stderr, "Fatal error: this user is not in /etc/shadow\n");
			continue;
		};

		if (strcmp(crypt(password, passcrypt), passcrypt) == 0)
		{
			// search all groups to find which ones we are in
			gid_t *groups = NULL;
			size_t numGroups = 0;
			
			struct group *grp;
			while ((grp = getgrent()) != NULL)
			{
				int i;
				for (i=0; grp->gr_mem[i]!=NULL; i++)
				{
					if (strcmp(grp->gr_mem[i], username) == 0)
					{
						groups = realloc(groups, sizeof(gid_t)*(numGroups+1));
						groups[numGroups++] = grp->gr_gid;
						break;
					};
				};
			};
			
			if (_glidix_setgroups(numGroups, groups) != 0)
			{
				perror("_glidix_setgroups");
				return 1;
			};
			
			if (setgid(pwd->pw_gid) != 0)
			{
				perror("setgid");
				return 1;
			};

			if (setuid(pwd->pw_uid) != 0)
			{
				perror("setuid");
				return 1;
			};

			// at this point, we are logged in as the user.
			const char *home;
			if (chdir(pwd->pw_dir) != 0)
			{
				perror("chdir");
				fprintf(stderr, "Using / as home.\n");
				chdir("/");
				home = "/";
			}
			else
			{
				home = pwd->pw_dir;
			};

			// set up the environment
			setenv("HOME", home, 1);
			setenv("SHELL", pwd->pw_shell, 1);
			setenv("USERNAME", pwd->pw_name, 1);

			// try finding the login script.
			const char *loginScript;
			struct stat st;
			if (stat("/etc/init/login.sh", &st) == 0)
			{
				if ((st.st_uid != 0) || (st.st_gid != 0) || ((st.st_mode & 0444) != 0444))
				{
					fprintf(stderr, "ERROR: Bad permissions set on /etc/init/login.sh! Refusing to log in.\n");
					return 1;
				};

				loginScript = "/etc/init/login.sh";
			}
			else
			{
				fprintf(stderr, "Cannot find login script (/etc/init/login.sh), dropping to shell.\n");
				loginScript = NULL;
			};

			printf("Welcome, %s\n", pwd->pw_gecos);
			if (execl(pwd->pw_shell, "-sh", loginScript, NULL) != 0)
			{
				perror(pwd->pw_shell);
				return 1;
			};
		}
		else
		{
			sleep(2);
			fprintf(stderr, "Sorry, please try again.\n");
		};
	};

	return 0;
};

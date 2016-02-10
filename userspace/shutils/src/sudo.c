/*
	Glidix Shell Utilities

	Copyright (c) 2014-2016, Madd Games.
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

#include <stdio.h>
#include <pwd.h>
#include <termios.h>
#include <stdlib.h>
#include <unistd.h>

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

int isAdmin()
{
	if ((getegid() == 0) || (getegid() == 1))
	{
		return 1;
	};
	
	int numGroups = getgroups(0, NULL);
	gid_t *groups = (gid_t*) malloc(sizeof(gid_t)*numGroups);
	getgroups(numGroups, groups);
	
	int i;
	for (i=0; i<numGroups; i++)
	{
		if ((groups[i] == 0) || (groups[i] == 1))
		{
			free(groups);
			return 1;
		};
	};
	
	return 0;
};

// TODO: sudoers file!
int main(int argc, char *argv[])
{
	if (geteuid() != 0)
	{
		fprintf(stderr, "%s: not running as effective root! probably the setuid flag is clear!\n", argv[0]);
		return 1;
	};

	if (!isAdmin())
	{
		fprintf(stderr, "%s: you are not an admin\n", argv[0]);
		return 1;
	};

	// root running sudo should just be like running env.
	if (getuid() != 0)
	{
		struct passwd *pwd = getpwuid(getuid());
		if (pwd == NULL)
		{
			fprintf(stderr, "%s: cannot resolve current UID (%u)\n", argv[0], getuid());
			return 1;
		};

		if (findPassword(pwd->pw_name) != 0)
		{
			fprintf(stderr, "%s: user %s is not in the shadow file!\n", argv[0], pwd->pw_name);
			return 1;
		};

		while (1)
		{
			struct termios tc;
			tcgetattr(0, &tc);
			tc.c_lflag &= ~(ECHO);
			tcsetattr(0, TCSANOW, &tc);

			printf("[%s] password for %s: ", argv[0], pwd->pw_name); fflush(stdout); getline(password);

			tc.c_lflag |= ECHO;
			tcsetattr(0, TCSANOW, &tc);

			if (strcmp(crypt(password, passcrypt), passcrypt) != 0)
			{
				fprintf(stderr, "Sorry, please try again.\n");
				continue;
			}
			else
			{
				break;
			};
		};
	};

	// ok, now try switching to being root.
	if (setgid(0) != 0)
	{
		perror("setgid");
		return 1;
	};

	if (setuid(0) != 0)
	{
		perror("setuid");
		return 1;
	};

	// now run
	argv[0] = "env";
	execv("/usr/bin/env", argv);
	perror("execv");
	return 1;
};

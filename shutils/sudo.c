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

#define _GLIDIX_SOURCE
#include <stdio.h>
#include <pwd.h>
#include <grp.h>
#include <termios.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <time.h>
#include <errno.h>

char password[128];
char passcrypt[128];
char shname[128];

int canSudo()
{
	if (getuid() == 0) return 1;

	FILE *fp = fopen("/etc/sudoers", "r");
	if (fp == NULL)
	{
		fprintf(stderr, "CRITICAL: cannot open /etc/sudoers for reading: %s\n", strerror(errno));
		abort();
	};
	
	char linebuf[1024];
	int lineno = 0;
	while (fgets(linebuf, 1024, fp) != NULL)
	{
		// remove comments
		lineno++;
		char *comment = strchr(linebuf, '#');
		if (comment != NULL) *comment = 0;
		char *endline = strchr(linebuf, '\n');
		if (endline != NULL) *endline = 0;
		
		char *dir = strtok(linebuf, " \t");
		if (dir == NULL) continue;
		
		char *name = strtok(NULL, " \t");
		if (name == NULL)
		{
			fprintf(stderr, "CRITICAL: syntax error in /etc/sudoers on line %d\n", lineno);
			abort();
		};
		
		if (strcmp(dir, "user") == 0)
		{
			struct passwd *info = getpwnam(name);
			if (info == NULL) continue;
			if (info->pw_uid != getuid()) continue;
		}
		else if (strcmp(dir, "group") == 0)
		{
			struct group *info = getgrnam(name);
			if (info == NULL) continue;

			int numGroups = getgroups(0, NULL);
			gid_t *groups = (gid_t*) malloc(sizeof(gid_t)*numGroups);
			getgroups(numGroups, groups);
	
			int i;
			int found = 0;
			for (i=0; i<numGroups; i++)
			{
				if (groups[i] == info->gr_gid)
				{
					found = 1;
					break;
				};
			};
			
			free(groups);
			if (!found)
			{
				if (info->gr_gid != getgid())
				{
					continue;
				};
			};
		}
		else
		{
			fprintf(stderr, "CRITICAL: invalid directive in /etc/sudoers on line %d\n", lineno);
			abort();
		};
		
		int def;
		char *deftok = strtok(NULL, " \t");
		if (deftok == NULL)
		{
			fprintf(stderr, "CRITICAL: syntax error in /etc/sudoers on line %d\n", lineno);
			abort();
		};
		
		if (strcmp(deftok, "allow") == 0)
		{
			def = 1;
		}
		else if (strcmp(deftok, "deny") == 0)
		{
			def = 0;
		}
		else
		{
			fprintf(stderr, "CRITICAL: syntax error in /etc/sudoers on line %d\n", lineno);
			abort();
		};
		
		// TODO: exceptions
		fclose(fp);
		return def;
	};
	
	fclose(fp);
	return 0;	// do not allow by default
};

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

int hasAuthed()
{
	// check if the current user has recently (in the last 10 minutes) authenticated, and return
	// nonzero if so. this is to avoid constantly asking for the password
	
	// first make sure the auth directories exist
	mkdir("/run/sudo", 0755);
	mkdir("/run/sudo/auth", 0700);
	
	char path[256];
	sprintf(path, "/run/sudo/auth/%lu", getuid());
	
	struct stat st;
	if (stat(path, &st) != 0)
	{
		unlink(path);	// in case it exists but something's wrong with it
		return 0;
	};
	
	// check if it was created less than 10 minutes ago
	time_t earliestAllowed = time(NULL) - 10 * 60;
	if (st.st_btime < earliestAllowed)
	{
		unlink(path);	// too old
		return 0;
	};
	
	return 1;
};

void authOk()
{
	// report successful authentication, so that the user doesn't have to re-authenticate until
	// a timeout.
	
	// first make sure the auth directories exist
	mkdir("/run/sudo", 0755);
	mkdir("/run/sudo/auth", 0700);
	
	char path[256];
	sprintf(path, "/run/sudo/auth/%lu", getuid());
	close(open(path, O_CREAT | O_WRONLY, 0600));
};

int main(int argc, char *argv[])
{
	if (geteuid() != 0)
	{
		fprintf(stderr, "%s: not running as effective root! probably the setuid flag is clear!\n", argv[0]);
		return 1;
	};

	if (!canSudo())
	{
		fprintf(stderr, "%s: you do not have sudo permission\n", argv[0]);
		return 1;
	};

	// root running sudo should just be like running env.
	if (getuid() != 0 && !hasAuthed())
	{
		struct passwd *pwd = getpwuid(getuid());
		if (pwd == NULL)
		{
			fprintf(stderr, "%s: cannot resolve current UID (%lu)\n", argv[0], getuid());
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
				sleep(2);
				fprintf(stderr, "Sorry, please try again.\n");
				continue;
			}
			else
			{
				authOk();
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

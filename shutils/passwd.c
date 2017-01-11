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

#ifndef __glidix__
#	error This program assumes it runs on Glidix!
#endif

#include <sys/stat.h>
#include <pwd.h>
#include <grp.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <string.h>

const char *progName;

char oldpass[128];
char newpass[128];
char newpass2[128];

char getchfd(int fd)
{
	char c;
	if (read(fd, &c, 1) != 1)
	{
		return 0xFF;
	}
	else
	{
		return c;
	};
};

int nextShadowEntry(int fd, char *username, off_t *cryptoff)
{
	while (1)
	{
		char c = getchfd(fd);
		if (c == 0xFF)
		{
			return -1;
		};

		if (c == ':')
		{
			*username = 0;
			*cryptoff = lseek(fd, 0, SEEK_CUR);
			while (1)
			{
				c = getchfd(fd);
				if ((c == '\n') || (c == 0xFF)) break;
			};
			return 0;
		};

		*username++ = c;
	};

	return -1;
};

int getShadowEntryFor(const char *username, int fd, off_t *cryptoff)
{
	char bufusr[128];
	while (nextShadowEntry(fd, bufusr, cryptoff) != -1)
	{
		if (strcmp(bufusr, username) == 0)
		{
			return 0;
		};
	};

	return -1;
};

void usage()
{
	fprintf(stderr, "USAGE:\t%s\n", progName);
	fprintf(stderr, "\t%s username\n", progName);
	fprintf(stderr, "\tChange the password of the current user, or the user with the given name.\n");
	fprintf(stderr, "\tTo change the password of another user, you must be root.\n");
};

void getline(char *buffer)
{
	struct termios oldtc, tc;
	tcgetattr(0, &tc);
	oldtc = tc;
	tc.c_lflag &= ~(ECHO);
	tcsetattr(0, TCSANOW, &tc);

	ssize_t count = read(0, buffer, 128);
	if (count == -1)
	{
		perror("read");
		tcsetattr(0, TCSANOW, &oldtc);
		exit(1);
	};

	if (count > 127)
	{
		fprintf(stderr, "input too long\n");
		tcsetattr(0, TCSANOW, &oldtc);
		exit(1);
	};

	tcsetattr(0, TCSANOW, &oldtc);
	buffer[count-1] = 0;
};

char saltchars[64] = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ./";

int main(int argc, char *argv[])
{
	progName = argv[0];

	if (geteuid() != 0)
	{
		fprintf(stderr, "%s: the effective UID is not root, the executable probably doesn't have the setuid bit!\n", argv[0]);
		return 1;
	};

	if ((argc != 1) && (argc != 2))
	{
		usage();
		return 1;
	};

	if (argc == 2)
	{
		if (argv[1][0] == '-')
		{
			usage();
			return 1;
		};
	};

	if ((argc == 2) && (getuid() != 0))
	{
		fprintf(stderr, "%s: you must be root to change another user's password\n", argv[0]);
		return 1;
	};

	struct passwd *pwd;
	if (argc == 1)
	{
		pwd = getpwuid(getuid());
	}
	else
	{
		pwd = getpwnam(argv[1]);
	};

	if (pwd == NULL)
	{
		fprintf(stderr, "%s: could not find the passwd entry\n", argv[0]);
		return 1;
	};

	int fd = open("/etc/shadow", O_RDWR);
	if (fd == -1)
	{
		perror("open /etc/shadow");
		return 1;
	};

	off_t cryptoff;
	if (getShadowEntryFor(pwd->pw_name, fd, &cryptoff) != 0)
	{
		fprintf(stderr, "%s: could not find shadow entry for %s\n", argv[0], pwd->pw_name);
		return 1;
	};

	char passcrypt[67];
	passcrypt[66] = 0;

	if (argc == 1)
	{
		lseek(fd, cryptoff, SEEK_SET);
		read(fd, passcrypt, 66);

		printf("Current password: "); fflush(stdout); getline(oldpass);

		if (strcmp(crypt(oldpass, passcrypt), passcrypt) != 0)
		{
			fprintf(stderr, "Wrong password!\n");
			return 1;
		};
	};

	printf("New password: "); fflush(stdout); getline(newpass);
	printf("Retype new password: "); fflush(stdout); getline(newpass2);

	if (strcmp(newpass, newpass2) != 0)
	{
		fprintf(stderr, "Passwords do not match!\n");
		return 1;
	};

	char salt[2] = "AB";
	int randfd = open("/dev/random", O_RDONLY);
	if (randfd != -1)
	{
		read(randfd, salt, 2);
		salt[0] = saltchars[(int)salt[0] * 63 / 255];
		salt[1] = saltchars[(int)salt[1] * 63 / 255];
		close(randfd);
	};

	char *newcrypt = crypt(newpass, salt);
	lseek(fd, cryptoff, SEEK_SET);
	write(fd, newcrypt, 66);
	close(fd);

	printf("Password updated.\n");
	return 0;
};

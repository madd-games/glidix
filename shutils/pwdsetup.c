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
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>

char username[128];
char fullname[128];
char password[128];
char password2[128];
char homedir[256];

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

void ensureDirectory(const char *dirname)
{
	struct stat st;
	if (stat(dirname, &st) != 0)
	{
		if (mkdir(dirname, 0755) != 0)
		{
			perror("mkdir");
			exit(1);
		};
	};
};

int main(int argc, char *argv[])
{
	if (geteuid() != 0)
	{
		fprintf(stderr, "%s: only root can do this\n", argv[0]);
		return 1;
	};

	printf("Admin username: "); fflush(stdout); getline(username);
	const char *scan = username;
	while (*scan != 0)
	{
		char c = *scan++;
		if ((c == ' ') || (c == '/') || (c == ':'))
		{
			fprintf(stderr, "illegal characters in username\n");
			return 1;
		};
	};

	printf("Full name: "); fflush(stdout); getline(fullname);
	scan = fullname;
	while (*scan != 0)
	{
		char c = *scan++;
		if ((c == '/') || (c == ':'))
		{
			fprintf(stderr, "illegal characters in username\n");
			return 1;
		};
	};

	struct termios tc;
	tcgetattr(0, &tc);
	tc.c_lflag &= ~(ECHO);
	tcsetattr(0, TCSANOW, &tc);

	printf("Admin password: "); fflush(stdout); getline(password);
	printf("Retype password: "); fflush(stdout); getline(password2);

	tc.c_lflag |= ECHO;
	tcsetattr(0, TCSANOW, &tc);

	if (strcmp(password, password2) != 0)
	{
		fprintf(stderr, "passwords do not match\n");
		return 1;
	};

	printf("Ensuring /etc exists...\n");
	ensureDirectory("/etc");

	printf("Ensuring /root exists...\n");
	ensureDirectory("/root");

	printf("Ensuring /home exists...\n");
	ensureDirectory("/home");

	strcpy(homedir, "/home/");
	strcat(homedir, username);

	printf("Ensuring %s exists...\n", homedir);
	ensureDirectory(homedir);
	if (chown(homedir, 1000, 1000) != 0)
	{
		perror("chown");
		return 1;
	};

	if (chmod(homedir, 0700) != 0)
	{
		perror("chmod");
		return 1;
	};

	printf("Creating /etc/passwd...\n");
	int fd = open("/etc/passwd", O_WRONLY | O_CREAT | O_TRUNC, 0644);
	if (fd == -1)
	{
		perror("open /etc/passwd");
		return 1;
	};

	FILE *fp = fdopen(fd, "w");
	fprintf(fp, "root:x:0:0:root:/root:/bin/sh\n");
	fprintf(fp, "%s:x:1000:1000:%s:%s:/bin/sh\n", username, fullname, homedir);
	fclose(fp);		// this closes fd too!

	char salt[2];
	char *hexd = "0123456789abcdef";
	unsigned char c;
	fd = open("/dev/random", O_RDONLY);
	if (fd == -1)
	{
		perror("generate salt: open /dev/random");
		return 1;
	};
	read(fd, &c, 1);
	salt[0] = hexd[c >> 4];
	salt[1] = hexd[c & 0xF];

	printf("Creating /etc/shadow...\n");
	fd = open("/etc/shadow", O_WRONLY | O_CREAT | O_TRUNC, 0600);
	if (fd == -1)
	{
		perror("open /etc/shadow");
		return 1;
	};

	fp = fdopen(fd, "w");
	fprintf(fp, "root:*:0:0:0:0:0:0\n");
	fprintf(fp, "%s:%s:0:0:0:0:0:0\n", username, crypt(password, salt));
	fclose(fp);

	printf("Creating /etc/group...\n");
	fd = open("/etc/group", O_WRONLY | O_CREAT | O_TRUNC, 0644);
	if (fd == -1)
	{
		perror("open /etc/group");
		return 1;
	};

	fp = fdopen(fd, "w");
	fprintf(fp, "root:x:0:root\n");
	fprintf(fp, "admin:x:1:root,%s\n", username);
	fprintf(fp, "%s:x:1000:%s\n", username, username);
	fclose(fp);

	printf("setup complete.\n");
	return 0;
};

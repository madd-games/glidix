/*
	Glidix Runtime

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

#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>

static int __pwnextent(FILE *fp, struct passwd *pwd, char *buffer)
{
	pwd->pw_name = buffer;
	size_t nameSize = 0;

	// read the name
	while (1)
	{
		int c = fgetc(fp);
		if (c == EOF)
		{
			return -1;
		};

		if (c == ':')
		{
			*buffer++ = 0;
			break;
		};

		*buffer++ = (char) c;
		if ((++nameSize) == 128)		// one more than is allowed
		{
			return -1;
		};
	};

	// expecting "x" then ":"
	if (fgetc(fp) != 'x') return -1;
	if (fgetc(fp) != ':') return -1;

	char numbuf[8];
	char *put = numbuf;

	// read the UID
	while (1)
	{
		int c = fgetc(fp);
		if (c == EOF)
		{
			return -1;
		};

		if (c == ':')
		{
			*put++ = 0;
			break;
		};

		if ((c < '0') || (c > '9'))
		{
			return -1;
		};

		*put++ = (char) c;
		if (put == &numbuf[7])
		{
			return -1;
		};
	};

	pwd->pw_uid = strtoul(numbuf, NULL, 10);
	put = numbuf;

	// read the GID
	while (1)
	{
		int c = fgetc(fp);
		if (c == EOF)
		{
			return -1;
		};

		if (c == ':')
		{
			*put++ = 0;
			break;
		};

		if ((c < '0') || (c > '9'))
		{
			return -1;
		};

		*put++ = (char) c;
		if (put == &numbuf[7])
		{
			return -1;
		};
	};

	pwd->pw_gid = strtoul(numbuf, NULL, 10);

	// read the gecos
	pwd->pw_gecos = buffer;
	nameSize = 0;
	while (1)
	{
		int c = fgetc(fp);
		if (c == EOF)
		{
			return -1;
		};

		if (c == ':')
		{
			*buffer++ = 0;
			break;
		};

		*buffer++ = (char) c;
		if ((++nameSize) == 256)		// one more than is allowed
		{
			return -1;
		};
	};

	// read the home directory
	pwd->pw_dir = buffer;
	nameSize = 0;
	while (1)
	{
		int c = fgetc(fp);
		if (c == EOF)
		{
			return -1;
		};

		if (c == ':')
		{
			*buffer++ = 0;
			break;
		};

		*buffer++ = (char) c;
		if ((++nameSize) == 256)		// one more than is allowed
		{
			return -1;
		};
	};

	// read the shell
	pwd->pw_shell = buffer;
	nameSize = 0;
	while (1)
	{
		int c = fgetc(fp);
		if (c == EOF)
		{
			return -1;
		};

		if (c == '\n')
		{
			*buffer++ = 0;
			break;
		};

		*buffer++ = (char) c;
		if ((++nameSize) == 256)		// one more than is allowed
		{
			return -1;
		};
	};

	pwd->pw_passwd = "x";
	return 0;
};

int getpwnam_r(const char *username, struct passwd *pwd, char *buffer, size_t bufsiz, struct passwd **result)
{
	if (bufsiz < 0x1000)
	{
		return ERANGE;
	};

	FILE *fp = fopen("/etc/passwd", "r");
	if (fp == NULL)
	{
		return EIO;
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
	
	while (__pwnextent(fp, pwd, buffer) != -1)
	{
		if (strcmp(pwd->pw_name, username) == 0)
		{
			*result = pwd;
			fclose(fp);
			return 0;
		};
	};

	fclose(fp);
	*result = NULL;
	return 0;
};

static struct passwd __passwd_static;
static char          __passwd_static_buf[0x1000];

struct passwd* getpwnam(const char *username)
{
	struct passwd *out;
	int status = getpwnam_r(username, &__passwd_static, __passwd_static_buf, 0x1000, &out);
	if (status != 0)
	{
		out = NULL;
		errno = status;
	};
	return out;
};

int getpwuid_r(uid_t uid, struct passwd *pwd, char *buffer, size_t bufsiz, struct passwd **result)
{
	if (bufsiz < 0x1000)
	{
		return ERANGE;
	};

	FILE *fp = fopen("/etc/passwd", "r");
	if (fp == NULL)
	{
		return EIO;
	};

	while (__pwnextent(fp, pwd, buffer) != -1)
	{
		if (pwd->pw_uid == uid)
		{
			*result = pwd;
			fclose(fp);
			return 0;
		};
	};

	fclose(fp);
	*result = NULL;
	return 0;
};

struct passwd* getpwuid(uid_t uid)
{
	struct passwd *out;
	int status = getpwuid_r(uid, &__passwd_static, __passwd_static_buf, 0x1000, &out);
	if (status != 0)
	{
		out = NULL;
		errno = status;
	};
	return out;
};

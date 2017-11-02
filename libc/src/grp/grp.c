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

#include <grp.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>

int __grpnextent(FILE *fp, struct group *grp, char *buffer, char **newbuf)
{
	grp->gr_name = buffer;
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

	grp->gr_gid = strtoul(numbuf, NULL, 10);
	put = numbuf;

	grp->_gr_memraw = buffer;
	nameSize = 0;

	// read the members list
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
		if ((++nameSize) == 2048)		// one more than is allowed
		{
			return -1;
		};
	};

	*newbuf = buffer;
	return 0;
};

static void __grpmemparse(struct group *grp, char *buffer)
{
	size_t i;
	size_t len = strlen(grp->_gr_memraw);
	size_t commas = 0;
	for (i=0; i<len; i++)
	{
		if (grp->_gr_memraw[i] == ',') commas++;
	};

	grp->gr_mem = (char**) buffer;
	grp->gr_mem[commas+1] = NULL;

	char **put = grp->gr_mem;
	*put++ = grp->_gr_memraw;
	char *scan = grp->_gr_memraw;

	while (*scan != 0)
	{
		if (*scan == ',')
		{
			*scan = 0;
			*put++ = (scan+1);
		};

		scan++;
	};
};

int getgrnam_r(const char *name, struct group *grp, char *buffer, size_t bufsiz, struct group **result)
{
	if (bufsiz < 0x1000)
	{
		return ERANGE;
	};

	FILE *fp = fopen("/etc/group", "r");
	if (fp == NULL)
	{
		return EIO;
	};

	char *nextbuf;
	while (__grpnextent(fp, grp, buffer, &nextbuf) != -1)
	{
		if (strcmp(grp->gr_name, name) == 0)
		{
			__grpmemparse(grp, nextbuf);
			*result = grp;
			fclose(fp);
			return 0;
		};
	};

	fclose(fp);
	*result = NULL;
	return 0;
};

int getgrgid_r(gid_t gid, struct group *grp, char *buffer, size_t bufsiz, struct group **result)
{
	if (bufsiz < 0x1000)
	{
		return ERANGE;
	};

	FILE *fp = fopen("/etc/group", "r");
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

	char *nextbuf;
	while (__grpnextent(fp, grp, buffer, &nextbuf) != -1)
	{
		if (grp->gr_gid == gid)
		{
			__grpmemparse(grp, nextbuf);
			*result = grp;
			fclose(fp);
			return 0;
		};
	};

	fclose(fp);
	*result = NULL;
	return 0;
};

static struct group __grp_static;
static char __grp_static_buf[0x1000];

struct group* getgrnam(const char *name)
{
	struct group *out;
	int status = getgrnam_r(name, &__grp_static, __grp_static_buf, 0x1000, &out);
	if (status != 0)
	{
		errno = status;
	};
	return out;
};

struct group* getgrgid(gid_t gid)
{
	struct group *out;
	int status = getgrgid_r(gid, &__grp_static, __grp_static_buf, 0x1000, &out);
	if (status != 0)
	{
		errno = status;
	};
	return out;
};

static FILE *fpGroups = NULL;
void setgrent()
{
	endgrent();
};

void endgrent()
{
	if (fpGroups != NULL)
	{
		fclose(fpGroups);
		fpGroups = NULL;
	};
};

struct group *getgrent()
{
	if (fpGroups == NULL)
	{
		fpGroups = fopen("/etc/group", "r");
		if (fpGroups == NULL)
		{
			errno = EIO;
			return NULL;
		};
	};
	
	char *nextbuf;
	if (__grpnextent(fpGroups, &__grp_static, __grp_static_buf, &nextbuf) != -1)
	{
		__grpmemparse(&__grp_static, nextbuf);
		return &__grp_static;
	};
	
	errno = 0;
	return NULL;
};

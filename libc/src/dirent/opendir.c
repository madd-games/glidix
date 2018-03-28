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

#include <dirent.h>
#include <sys/glidix.h>
#include <stdlib.h>
#include <limits.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

DIR *fdopendir(int fd)
{
	struct stat st;
	if (fstat(fd, &st) != 0)
	{
		int errnum = errno;
		close(fd);
		errno = errnum;
		return NULL;
	};
	
	if (!S_ISDIR(st.st_mode))
	{
		close(fd);
		errno = ENOTDIR;
		return NULL;
	};
	
	DIR *dirp = (DIR*) malloc(sizeof(DIR));
	if (dirp == NULL)
	{
		close(fd);
		errno = ENOMEM;
		return NULL;
	};
	
	dirp->__fd = fd;
	dirp->__current = NULL;
	dirp->__key = 0;
	
	return dirp;
};

DIR *opendir(const char *dirname)
{
	int fd = open(dirname, O_RDONLY | O_CLOEXEC);
	if (fd == -1)
	{
		return NULL;
	};
	
	return fdopendir(fd);
};

/*
	Glidix Runtime

	Copyright (c) 2014, Madd Games.
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
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/glidix.h>

/* internal/file.c */
void __fd_flush(FILE *fp);

FILE *fopen(const char *path, const char *mode)
{
	int oflags = 0;
	int fpflags;
	if (*mode == 'r')
	{
		oflags = O_RDONLY;
		fpflags = __FILE_READ;
	}
	else if (*mode == 'w')
	{
		oflags = O_WRONLY | O_TRUNC | O_CREAT;
		fpflags = __FILE_WRITE;
	}
	else if (*mode == 'a')
	{
		oflags = O_WRONLY | O_APPEND | O_CREAT;
		fpflags = __FILE_WRITE;
	}
	else
	{
		_glidix_seterrno(EINVAL);
		return NULL;
	};

	mode++;
	if (*mode == 'b')
	{
		mode++;
	};

	if (*mode == '+')
	{
		oflags |= O_RDWR;
		fpflags = __FILE_READ | __FILE_WRITE;
	};

	int fd = open(path, oflags, 0644);
	if (fd < 0)
	{
		return NULL;
	};

	FILE *fp = (FILE*) malloc(sizeof(FILE));
	fp->_buf = &fp->_nanobuf;
	fp->_rdbuf = fp->_buf;
	fp->_wrbuf = fp->_wrbuf;
	fp->_bufsiz = 1;
	fp->_bufsiz_org = 1;
	fp->_trigger = 0;
	fp->_flush = __fd_flush;
	fp->_fd = fd;
	fp->_flags = fpflags;
	return fp;
};

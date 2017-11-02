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

#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/glidix.h>

/* internal/file.c */
void __fd_flush(FILE *fp);

FILE *fdopen(int fd, const char *mode)
{
	int fpflags;
	if (*mode == 'r')
	{
		fpflags = __FILE_READ;
	}
	else if (*mode == 'w')
	{
		fpflags = __FILE_WRITE;
	}
	else if (*mode == 'a')
	{
		fpflags = __FILE_WRITE;
	}
	else
	{
		errno = EINVAL;
		return NULL;
	};

	mode++;
	if (*mode == 'b')
	{
		mode++;
	};

	if (*mode == '+')
	{
		fpflags = __FILE_READ | __FILE_WRITE;
	};

	FILE *fp = (FILE*) malloc(sizeof(FILE));
	fp->_buf = &fp->_nanobuf;
	fp->_rdbuf = fp->_buf;
	fp->_wrbuf = fp->_buf;
	fp->_bufsiz = 1;
	fp->_bufsiz_org = 1;
	fp->_trigger = 0;
	fp->_flush = __fd_flush;
	fp->_fd = fd;
	fp->_flags = fpflags;
	fp->_ungot = -1;
	return fp;
};

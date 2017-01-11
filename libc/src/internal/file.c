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
#include <unistd.h>
#include <stdlib.h>

static char _buf_stdout[128];
static char _buf_stderr[128];

void __fd_flush(FILE *fp)
{
	if (fp->_wrbuf < fp->_rdbuf)
	{
		fprintf(stderr, "libc: consistency check failed: buffer write is behind a buffer read\n");
		fprintf(stderr, "fileno: %d; read=%p, write=%p\n", fp->_fd, fp->_rdbuf, fp->_wrbuf);
		abort();
	};

	write(fp->_fd, fp->_rdbuf, (size_t)(fp->_wrbuf - fp->_rdbuf));
	fp->_wrbuf = fp->_buf;
	fp->_rdbuf = fp->_buf;
	fp->_bufsiz = fp->_bufsiz_org;
};

static FILE _file_stdin = {
	._buf = NULL,
	._rdbuf = NULL,
	._wrbuf = NULL,
	._bufsiz = 0,
	._bufsiz_org = 0,
	._trigger = 0,
	._flush = NULL,
	._fd = 0,
	._flags = __FILE_READ,
	._ungot = -1
};

static FILE _file_stdout = {
	._buf = _buf_stdout,
	._rdbuf = _buf_stdout,
	._wrbuf = _buf_stdout,
	._bufsiz = 128,
	._bufsiz_org = 128,
	._trigger = '\n',
	._flush = &__fd_flush,
	._fd = 1,
	._flags = __FILE_WRITE,
	._ungot = -1
};

static FILE _file_stderr = {
	._buf = _buf_stderr,
	._rdbuf = _buf_stderr,
	._wrbuf = _buf_stderr,
	._bufsiz = 128,
	._bufsiz_org = 128,
	._trigger = '\n',
	._flush = &__fd_flush,
	._fd = 2,
	._flags = __FILE_WRITE,
	._ungot = -1
};

FILE *stdin  = &_file_stdin;
FILE *stdout = &_file_stdout;
FILE *stderr = &_file_stderr;

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

size_t fread(void *buf, size_t a, size_t b, FILE *fp)
{
	if ((fp->_flags & __FILE_READ) == 0)
	{
		fp->_flags |= __FILE_FERROR;
		return 0;
	};

	size_t size = a * b;
	if (size == 0)
	{
		return 0;
	};
	
	ssize_t addret = 0;
	if (fp->_ungot != -1)
	{
		char *out = (char*) buf;
		*out++ = (char) fp->_ungot;
		fp->_ungot = -1;
		buf = out;
		size--;
		addret++;
	};
	
	if (size == 0)
	{
		return addret;
	};

	if (fp->_flags & __FILE_WRITE)
	{
		// flush before reading!
		fflush(fp);
	};
	
	ssize_t ret = read(fp->_fd, buf, size);
	if (ret >= 0)
	{
		if (ret < size)
		{
			fp->_flags |= __FILE_EOF;
		};
		return (ret+addret) / a;
	}
	else
	{
		fp->_flags |= __FILE_FERROR;
		return 0;
	};
};

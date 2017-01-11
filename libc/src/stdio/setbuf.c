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

int setvbuf(FILE *fp, char *buf, int type, size_t size)
{
	fflush(fp);
	fp->_buf = buf;
	fp->_rdbuf = buf;
	fp->_wrbuf = buf;
	fp->_bufsiz = size;
	fp->_bufsiz_org = size;
	
	switch (type)
	{
	case _IOFBF:
		fp->_trigger = 0;
		break;
	case _IOLBF:
		fp->_trigger = '\n';
		break;
	case _IONBF:
		fp->_buf = &fp->_nanobuf;
		fp->_rdbuf = fp->_buf;
		fp->_wrbuf = fp->_buf;
		fp->_bufsiz = 1;
		fp->_bufsiz_org = 1;
		break;
	};
	
	return 0;
};

void setbuf(FILE *fp, char *buf)
{
	if (buf != NULL)
	{
		setvbuf(fp, buf, _IOFBF, BUFSIZ);
	}
	else
	{
		setvbuf(fp, buf, _IONBF, BUFSIZ);
	};
};

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
#include <stdarg.h>

int vsnprintf(char *s, size_t n, const char *fmt, va_list ap)
{
	FILE temp;
	temp._buf = s;
	temp._wrbuf = s;
	temp._rdbuf = s;
	temp._bufsiz = n-1;
	temp._bufsiz_org = n-1;
	temp._trigger = 0;
	temp._flush = NULL;
	temp._fd = -1;
	temp._flags = __FILE_WRITE;
	int out = vfprintf(&temp, fmt, ap);
	*(temp._wrbuf) = 0;
	return out;
};

int snprintf(char *s, size_t n, const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	int ret = vsnprintf(s, n, fmt, ap);
	va_end(ap);
	return ret;
};

int vsprintf(char *s, const char *fmt, va_list ap)
{
	return vsnprintf(s, 0xFFFFFFFFFFFFFFFF, fmt, ap);
};

int sprintf(char *s, const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	int ret = vsprintf(s, fmt, ap);
	va_end(ap);
	return ret;
};

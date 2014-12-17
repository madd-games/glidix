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

int fputc(int ch, FILE *fp)
{
	char c = (char) ch;
	return fwrite(&c, 1, 1, fp);
};

int vfprintf(FILE *fp, const char *fmt, va_list ap)
{
	while (*fmt != 0)
	{
		char c = *fmt++;
		if (c != '%')
		{
			fputc((int) c, fp);
		}
		else
		{
			c = *fmt++;
			if (c == 's')
			{
				const char *s = va_arg(ap, const char*);
				fputs(s, fp);
			};
		};
	};
};

int vprintf(const char *fmt, va_list ap)
{
	return vfprintf(stdout, fmt, ap);
};

int fprintf(FILE *fp, const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	int ret = vfprintf(fp, fmt, ap);
	va_end(ap);
	return ret;
};

int printf(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	int ret = vprintf(fmt, ap);
	va_end(ap);
	return ret;
};

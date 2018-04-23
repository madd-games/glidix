/*
	Glidix kernel

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

#include <glidix/util/string.h>
#include <glidix/util/common.h>
#include <stdarg.h>

static char* strformat_d(char *buffer, size_t *bufsizeptr, int num)
{
	char tmp[256];
	char *put = &tmp[255];
	*--put = 0;
	
	int sign = 0;
	if (num < 0)
	{
		sign = 1;
		num = -num;
	};
	
	do
	{
		*--put = (num % 10)+'0';
		num /= 10;
	} while (num != 0);
	
	if (sign)
	{
		*--put = '-';
	};
	
	if (strlen(put) >= (*bufsizeptr))
	{
		return NULL;
	};
	
	(*bufsizeptr) -= strlen(put);
	strcpy(buffer, put);
	return buffer + strlen(put);
};

static char *strformat_hex(char *buffer, size_t *bufsizeptr, uint64_t num, size_t numNibbles)
{
	if ((2*numNibbles) >= (*bufsizeptr))
	{
		return NULL;
	};
	
	char tmp[256];
	char *put = &tmp[255];
	*--put = 0;
	
	static char hexdigits[16] = "0123456789ABCDEF";
	while (numNibbles--)
	{
		*--put = hexdigits[num & 0xF];
		num >>= 4;
	};
	
	(*bufsizeptr) -= strlen(put);
	strcpy(buffer, put);
	return buffer + strlen(put);
};

int strformat(char *buffer, size_t bufsize, const char *format, ...)
{
	va_list ap;
	va_start(ap, format);
	
	while ((*format) != 0)
	{
		char c = *format++;
		if (c != '%')
		{
			if (bufsize == 1) return -1;
			*buffer++ = c;
			bufsize--;
		}
		else
		{
			const char *str;
			char c = *format++;
			switch (c)
			{
			case 0:
				return -1;
			case '%':
				if (bufsize == 1) return -1;
				*buffer++ = '%';
				bufsize--;
				break;
			case 'c':
				if (bufsize == 1) return -1;
				*buffer++ = (char) va_arg(ap, int);
				bufsize--;
				break;
			case 's':
				str = va_arg(ap, const char*);
				if (strlen(str) >= bufsize) return -1;
				while ((*str) != 0)
				{
					*buffer++ = *str++;
					bufsize--;
				};
				break;
			case 'd':
				buffer = strformat_d(buffer, &bufsize, va_arg(ap, int));
				if (buffer == NULL) return -1;
				break;
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7':
			case '8':
			case '9':
				buffer = strformat_hex(buffer, &bufsize, va_arg(ap, uint64_t), (size_t) (c-'0'));
				if (buffer == NULL) return -1;
				break;
			default:
				return -1;
			};
		};
	};
	
	va_end(ap);
	*buffer = 0;
	return 0;
};

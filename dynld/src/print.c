/*
	Glidix dynamic linker

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

#include <unistd.h>

#include "dynld.h"

void dynld_printf(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	dynld_vprintf(fmt, ap);
	va_end(ap);
};

void dynld_print_p(uint64_t val)
{
	static char hexd[16] = "0123456789abcdef";
	
	char buffer[64];
	char *put = buffer + 64;
	size_t sz = 0;
	
	do
	{
		*--put = hexd[val & 0xF];
		val >>= 4;
		sz++;
	} while (val != 0);
	
	write(2, put, sz);
};

void dynld_print_d(int val)
{
	char buffer[64];
	char *put = buffer + 64;
	size_t sz = 0;
	
	do
	{
		*--put = '0' + (val % 10);
		val /= 10;
		sz++;
	} while (val != 0);
	
	write(2, put, sz);
};

void dynld_vprintf(const char *fmt, va_list ap)
{
	while (*fmt != 0)
	{
		if (*fmt != '%')
		{
			write(2, fmt, 1);
			fmt++;
		}
		else
		{
			fmt++;
			char spec = *fmt++;
			
			const char *sarg;
			uint64_t uarg;
			int iarg;
			
			switch (spec)
			{
			case '%':
				write(2, "%", 1);
				break;
			case 's':
				sarg = va_arg(ap, const char*);
				write(2, sarg, strlen(sarg));
				break;
			case 'p':
				uarg = va_arg(ap, uint64_t);
				dynld_print_p(uarg);
				break;
			case 'd':
				iarg = va_arg(ap, int);
				dynld_print_d(iarg);
				break;
			};
		};
	};
};

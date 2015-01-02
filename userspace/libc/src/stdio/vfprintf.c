/*
	Glidix Runtime

	Copyright (c) 2014-2015, Madd Games.
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

/* INTERNAL SECTION */
static int _put_d(int d, FILE *fp)
{
	char buffer[20];
	char *dput = &buffer[19];
	*dput-- = 0;

	if (d < 0)
	{
		fputc((int) '-', fp);
		d = -d;
	};

	do
	{
		*dput-- = ((d % 10)+'0');
		d /= 10;
	} while (d != 0);

	dput++;
	return fputs(dput, fp);
};

static int _put_u(unsigned int u, FILE *fp)
{
	char buffer[20];
	char *dput = &buffer[19];
	*dput-- = 0;

	do
	{
		*dput-- = ((u % 10)+'0');
		u /= 10;
	} while (u != 0);

	dput++;
	return fputs(dput, fp);
};

static int _put_o(unsigned int o, FILE *fp)
{
	char buffer[20];
	char *dput = &buffer[19];
	*dput-- = 0;

	do
	{
		*dput-- = ((o % 8)+'0');
		o /= 8;
	} while (o != 0);

	dput++;
	return fputs(dput, fp);
};

static int _put_x(unsigned int x, FILE *fp)
{
	char buffer[20];
	char *dput = &buffer[19];
	*dput-- = 0;

	do
	{
		int digit = x % 16;
		if (digit < 10)
		{
			*dput-- = (digit+'0');
		}
		else
		{
			*dput-- = ((digit-10)+'a');
		};
		x /= 16;
	} while (x != 0);

	dput++;
	return fputs(dput, fp);
};

static int _put_p(uint64_t p, FILE *fp)
{
	char hexd[] = "0123456789ABCDEF";
	fputs("0x", fp);
	int count = 16;
	while (count--)
	{
		fputc((int) hexd[(p >> 60) & 0xF], fp);
		p <<= 4;
	};
	return 1;
};

static int _put_X(unsigned int x, FILE *fp)
{
	char buffer[20];
	char *dput = &buffer[19];
	*dput-- = 0;

	while (x != 0)
	{
		int digit = x % 16;
		if (digit < 10)
		{
			*dput-- = (digit+'0');
		}
		else
		{
			*dput-- = ((digit-10)+'A');
		};
		x /= 16;
	};

	dput++;
	return fputs(dput, fp);
};

/* PUBLIC SECTION */
int fputc(int ch, FILE *fp)
{
	char c = (char) ch;
	return fwrite(&c, 1, 1, fp);
};

int putchar(int ch)
{
	return fputc(ch, stdout);
};

int vfprintf(FILE *fp, const char *fmt, va_list ap)
{
	int ret = 0;
	while (*fmt != 0)
	{
		char c = *fmt++;
		if (c != '%')
		{
			fputc((int) c, fp);
			ret++;
		}
		else
		{
			c = *fmt++;
			if (c == 's')
			{
				const char *s = va_arg(ap, const char*);
				ret += fputs(s, fp);
			}
			else if (c == 'd')
			{
				int d = va_arg(ap, int);
				ret += _put_d(d, fp);
			}
			else if (c == 'i')
			{
				int i = va_arg(ap, int);
				ret += _put_d(i, fp);
			}
			else if (c == 'u')
			{
				unsigned int u = va_arg(ap, unsigned int);
				ret += _put_u(u, fp);
			}
			else if (c == 'o')
			{
				unsigned int o = va_arg(ap, unsigned int);
				ret += _put_o(o, fp);
			}
			else if (c == 'x')
			{
				unsigned int x = va_arg(ap, unsigned int);
				ret += _put_x(x, fp);
			}
			else if (c == 'X')
			{
				unsigned int X = va_arg(ap, unsigned int);
				ret += _put_X(X, fp);
			}
			else if (c == 'p')
			{
				void *p = va_arg(ap, void*);
				ret += _put_p((uint64_t)p, fp);
			}
			else if (c == '%')
			{
				fputc((int) '%', fp);
				ret++;
			}
			else if (c == 'c')
			{
				fputc(va_arg(ap, int), fp);
				ret++;
			}
			else if (c == 0)
			{
				// that string shouldn't end with a %.
				break;
			};
		};
	};

	return ret;
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

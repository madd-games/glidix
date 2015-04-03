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

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>

#ifndef ULLONG_MAX
#	define	ULLONG_MAX	ULONG_MAX
#endif

#ifndef LLONG_MAX
#	define	LLONG_MAX	LONG_MAX
#endif

#ifndef	LLONG_MIN
#	define	LLONG_MIN	LONG_MIN
#endif

#ifndef ULLONG_MIN
#	define	ULLONG_MIN	ULONG_MIN
#endif

long long strtoll(const char *str, const char **str_end, int base)
{
	if (((base < 2) || (base > 36)) && (base != 0))
	{
		if (str_end != NULL) *str_end = str;
		errno = EINVAL;
		return 0;
	};

	while (isspace(*str)) str++;

	int negative = 0;
	if (*str == '+')
	{
		str++;
	}
	else if (*str == '-')
	{
		negative = 1;
		str++;
	};

	long long out = 0;
	if (base == 0)
	{
		if (*str == '0')
		{
			str++;
			base = 8;

			if (*str == 'x')
			{
				str++;
				base = 16;
			};
		}
		else
		{
			base = 10;
		};
	};

	long long llbase = (long long) base;
	while (*str != 0)
	{
		char c = *str++;
		if (((c < '0') || (c > '9')) && ((c < 'a') || (c > 'z')) && ((c < 'A') || (c > 'Z')))
		{
			goto finished;
		};

		long long digit = 0;
		if ((c >= '0') && (c <= '9'))
		{
			digit = c - '0';
		}
		else if ((c >= 'a') && (c <= 'z'))
		{
			digit = 10 + c - 'a';
		}
		else
		{
			digit = 10 + c - 'A';
		};

		if (digit >= llbase)
		{
			goto finished;
		};

		if (out > ((LLONG_MAX-digit)/llbase))
		{
			errno = ERANGE;
			if (str_end != NULL) *str_end = str;
			if (negative) return LLONG_MIN;
			else return LLONG_MAX;
		};

		out = out * llbase + digit;
	};

	finished:
	if (str_end != NULL) *str_end = str;
	if (negative) return -out;
	else return out;
};

long strtol(const char *str, const char **str_end, int base)
{
	long long out = strtoll(str, str_end, base);
	if (out > LONG_MAX)
	{
		errno = ERANGE;
		return LONG_MAX;
	};

	if (out < LONG_MIN)
	{
		errno = ERANGE;
		return LONG_MIN;
	};

	return (long) out;
};

unsigned long long strtoull(const char *str, const char **str_end, int base)
{
	if (((base < 2) || (base > 36)) && (base != 0))
	{
		if (str_end != NULL) *str_end = str;
		errno = EINVAL;
		return 0;
	};

	while (isspace(*str)) str++;

	unsigned long long out = 0;
	if (base == 0)
	{
		if (*str == '0')
		{
			str++;
			base = 8;

			if (*str == 'x')
			{
				str++;
				base = 16;
			};
		}
		else
		{
			base = 10;
		};
	};

	unsigned long long llbase = (unsigned long long) base;
	while (*str != 0)
	{
		char c = *str++;
		if (((c < '0') || (c > '9')) && ((c < 'a') || (c > 'z')) && ((c < 'A') || (c > 'Z')))
		{
			goto finished;
		};

		unsigned long long digit = 0;
		if ((c >= '0') && (c <= '9'))
		{
			digit = c - '0';
		}
		else if ((c >= 'a') && (c <= 'z'))
		{
			digit = 10 + c - 'a';
		}
		else
		{
			digit = 10 + c - 'A';
		};

		if (digit >= llbase)
		{
			goto finished;
		};

		if (out > ((ULLONG_MAX-digit)/llbase))
		{
			errno = ERANGE;
			if (str_end != NULL) *str_end = str;
			return ULLONG_MAX;
		};

		out = out * llbase + digit;
	};

	finished:
	if (str_end != NULL) *str_end = str;
	return out;
};

unsigned long strtoul(const char *str, const char **str_end, int base)
{
	unsigned long out = strtoull(str, str_end, base);
	if (out > ULONG_MAX)
	{
		errno = ERANGE;
		return ULONG_MAX;
	};

	return out;
};

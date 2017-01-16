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

#include "dynld.h"

size_t strlen(const char *s)
{
	size_t count = 0;
	for (; *s!=0; s++) count++;
	return count;
};

void strcpy(char *dst, const char *src)
{
	while (*src != 0) *dst++ = *src++;
	*dst = 0;
};

int memcmp(const void *a_, const void *b_, size_t sz)
{
	const char *a = (const char*) a_;
	const char *b = (const char*) b_;
	
	while (sz--)
	{
		if ((*a++) != (*b++)) return 1;
	};
	
	return 0;
};

char* strchr(const char *s, char c)
{
	while (*s != 0)
	{
		if (*s == c) return (char*) s;
		s++;
	};
	
	return NULL;
};

void memcpy(void *dst_, const void *src_, size_t sz)
{
	char *dst = (char*) dst_;
	const char *src = (const char*) src_;
	while (sz--) *dst++ = *src++;
};

int strcmp(const char *a, const char *b)
{
	while ((*a) == (*b))
	{
		if (*a == 0) return 0;
		a++;
		b++;
	};
	
	return 1;
};

void memset(void *put_, int val, size_t sz)
{
	char *put = (char*) put_;
	while (sz--) *put++ = (char) val;
};

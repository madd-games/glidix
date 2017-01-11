/*
	Glidix bootloader (gxboot)

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

#include "gxboot.h"

void memset(void *buffer, unsigned char b, unsigned int size)
{
	unsigned char *put = (unsigned char*) buffer;
	while (size--) *put++ = b;
};

void memcpy(void *dest_, const void *src_, unsigned int size)
{
	char *dest = (char*) dest_;
	char *src = (char*) src_;
	while (size--) *dest++ = *src++;
};

int memcmp(const void *a_, const void *b_, unsigned int size)
{
	const char *a = (const char*) a_;
	const char *b = (const char*) b_;
	while (size--) if (*a++ != *b++) return 1;
	return 0;
};

int strcmp(const char *a, const char *b)
{
	while ((*a != 0) && (*b != 0))
	{
		if ((*a++) != (*b++)) return 1;
	};
	
	return *a - *b;
};

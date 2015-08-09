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

#include <arpa/inet.h>
#include <stdio.h>
#include <sys/glidix.h>
#include <errno.h>
#include <ctype.h>

const char *inet_ntop4(const uint8_t *src, char *dst, socklen_t size)
{
	if (size < INET_ADDRSTRLEN)
	{
		_glidix_seterrno(ENOSPC);
		return NULL;
	};
	
	sprintf(dst, "%d.%d.%d.%d", (int)src[0], (int)src[1], (int)src[2], (int)src[3]);
	return dst;
};

const char *inet_ntop(int af, const void *src, char *dst, socklen_t size)
{
	switch (af)
	{
	case AF_INET:
		return inet_ntop4((const uint8_t*)src, dst, size);
		break;
	default:
		_glidix_seterrno(EAFNOSUPPORT);
		return NULL;
	};
};

int inet_pton4(const char *src, uint8_t *dst)
{
	int count = 0;
	int nextNum = 0;
	
	while (1)
	{
		if (isdigit(*src))
		{
			nextNum = nextNum * 10 + ((*src)-'0');
			src++;
		}
		else
		{
			if (nextNum > 255)
			{
				_glidix_seterrno(EAFNOSUPPORT);
				return 0;
			};
			
			*dst++ = nextNum;
			count++;
			
			if (count == 4)
			{
				if (*src != 0)
				{
					_glidix_seterrno(EAFNOSUPPORT);
					return 0;
				};
				
				return 1;
			}
			else
			{
				if (*src != '.')
				{
					_glidix_seterrno(EAFNOSUPPORT);
					return 0;
				};
				
				src++;
				nextNum = 0;
			};
		};
	};
};

int inet_pton(int af, const char *src, void *dst)
{
	switch (af)
	{
	case AF_INET:
		return inet_pton4(src, (uint8_t*)dst);
		break;
	default:
		_glidix_seterrno(EAFNOSUPPORT);
		return 0;
	};
};

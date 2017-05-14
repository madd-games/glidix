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

#include <arpa/inet.h>
#include <stdio.h>
#include <sys/glidix.h>
#include <errno.h>
#include <ctype.h>
#include <string.h>

/**
 * Returns true if 'addr' is an IPv4-mapped-onto-IPv6 address (::ffff/96).
 */
static int is_4map6_addr(const void *addr)
{
	uint8_t subnet[12] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xFF, 0xFF};
	if (memcmp(subnet, addr, 12) == 0)
	{
		return 1;
	};
	
	return 0;
};

static const char *inet_ntop4(const uint8_t *src, char *dst, socklen_t size)
{
	if (size < INET_ADDRSTRLEN)
	{
		errno = ENOSPC;
		return NULL;
	};
	
	sprintf(dst, "%d.%d.%d.%d", (int)src[0], (int)src[1], (int)src[2], (int)src[3]);
	return dst;
};

static const char *inet_ntop6(const uint16_t *src, char *dst, socklen_t size)
{
	if (size < INET6_ADDRSTRLEN)
	{
		errno = ENOSPC;
		return NULL;
	};
	
	// first handle IPv4-mapped-onto-IPv6 addresses
	if (is_4map6_addr(src))
	{
		uint8_t *ip4 = (uint8_t*) &src[6];
		sprintf(dst, "::ffff:%d.%d.%d.%d", (int)ip4[0], (int)ip4[1], (int)ip4[2], (int)ip4[3]);
		return dst;
	};
	
	// index of the first group in the longest trail of zeroes, and its size in groups.
	// -1 means there are no zeroes in the address.
	int idxStartTrail = -1;
	int sizeTrail = 0;
	int i;
	
	for (i=0; i<8; i++)
	{
		int thisTrailSize = 0;
		int j;
		for (j=i; j<8; j++)
		{
			if (src[j] == 0)
			{
				thisTrailSize++;
			}
			else
			{
				break;
			};
		};
		
		if (thisTrailSize > sizeTrail)
		{
			sizeTrail = thisTrailSize;
			idxStartTrail = i;
		};
	};
	
	// turn dst into an empty string so we can strcat() to it
	*dst = 0;
	
	// if the address starts with a trail of zeroes, we must prefix it with the double-colon
	if (idxStartTrail == 0)
	{
		strcat(dst, "::");
	};
	
	for (i=0; i<8; i++)
	{
		if ((i < idxStartTrail) || (i >= (idxStartTrail+sizeTrail)))
		{
			char buf[5];
			sprintf(buf, "%x", __builtin_bswap16(src[i]));
			strcat(dst, buf);
			
			if ((i != (idxStartTrail-1)) && (i != 7))
			{
				strcat(dst, ":");
			};
		};
		
		if (i == (idxStartTrail-1))
		{
			strcat(dst, "::");
		};
	};
	
	return dst;
};

const char *inet_ntop(int af, const void *src, char *dst, socklen_t size)
{
	switch (af)
	{
	case AF_INET:
		return inet_ntop4((const uint8_t*)src, dst, size);
		break;
	case AF_INET6:
		return inet_ntop6((const uint16_t*)src, dst, size);
		break;
	default:
		errno = EAFNOSUPPORT;
		return NULL;
	};
};

static int inet_pton4(const char *src, uint8_t *dst)
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
				errno = EAFNOSUPPORT;
				return 0;
			};
			
			*dst++ = nextNum;
			count++;
			
			if (count == 4)
			{
				if (*src != 0)
				{
					errno = EAFNOSUPPORT;
					return 0;
				};
				
				return 1;
			}
			else
			{
				if (*src != '.')
				{
					errno = EAFNOSUPPORT;
					return 0;
				};
				
				src++;
				nextNum = 0;
			};
		};
	};
};

static uint16_t inet6_nextgroup(const char *src, const char **endptr)
{
	uint16_t result = 0;
	int count = 0;
	
	while (1)
	{
		char c = *src;
		if (c == 0)
		{
			if (count == 0)
			{
				*endptr = NULL;
				return 0;
			}
			else
			{
				*endptr = src;
				return __builtin_bswap16(result);
			};
		};
		
		if (((c >= '0') && (c <= '9')) || ((c >= 'a') && (c <= 'f')) || ((c >= 'A') && (c <= 'F')))
		{
			if (count == 4)
			{
				*endptr = NULL;
				return 0;
			};
			
			uint16_t digit;
			if ((c >= '0') && (c <= '9'))
			{
				digit = (uint16_t)(c-'0');
			}
			else if ((c >= 'a') && (c <= 'f'))
			{
				digit = (uint16_t)(c-'a'+10);
			}
			else
			{
				digit = (uint16_t)(c-'A'+10);
			};
			
			count++;
			result = (result << 4) | digit;
			src++;
		}
		else
		{
			if (count == 0)
			{
				*endptr = NULL;
				return 0;
			}
			else
			{
				*endptr = src;
				return __builtin_bswap16(result);
			};
		};
	};
};

static int inet_pton6(const char *src, uint16_t *dst)
{
	// specifically accept an IPv4 address as an IPv6 address and map it to the
	// ::ffff:0:0/96 prefix
	uint8_t v4addr[4];
	if (inet_pton4(src, v4addr))
	{
		memset(dst, 0, 10);
		memset(&dst[5], 0xFF, 2);		// fifth group: ffff
		memcpy(&dst[6], v4addr, 4);
		return 1;
	};
	
	// first handle the trivial case of a zero address (::)
	if (strcmp(src, "::") == 0)
	{
		memset(dst, 0, 16);
		return 1;
	};
	
	// now the case of an IPv4-mapped-onto-IPv6 address
	if (memcmp(src, "::ffff:", 7) == 0)
	{
		memset(dst, 0, 16);
		dst[5] = 0xFFFF;
		int ok = inet_pton4(&src[7], (uint8_t*) &dst[6]);
		if (ok) return 1;
	};
	
	uint16_t startGroups[8];
	uint16_t endGroups[8];
	int numStartGroups = 0;
	int numEndGroups = 0;
	int whichGroup = 0;
	
	if (memcmp(src, "::", 2) == 0)
	{
		whichGroup = 1;
		src = &src[2];
	};
	
	while (1)
	{
		const char *endptr;
		uint16_t group = inet6_nextgroup(src, &endptr);
		
		if (endptr == NULL)
		{
			errno = EAFNOSUPPORT;
			return 0;
		};

		if (whichGroup == 0)
		{
			if (numStartGroups == 8)
			{
				errno = EAFNOSUPPORT;
				return 0;
			};
			
			startGroups[numStartGroups++] = group;
		}
		else
		{
			if (numEndGroups == 8)
			{
				errno = EAFNOSUPPORT;
				return 0;
			};
			
			endGroups[numEndGroups++] = group;
		};
		
		src = endptr;
		
		if (*src == 0)
		{
			break;
		};
		
		if (memcmp(src, "::", 2) == 0)
		{
			if (whichGroup == 0)
			{
				whichGroup = 1;
			}
			else
			{
				errno = EAFNOSUPPORT;
				return 0;
			};
			
			src = &src[2];
			if ((*src) == 0) break;
			continue;
		};
		
		if (*src != ':')
		{
			errno = EAFNOSUPPORT;
			return 0;
		};
		
		src++;
	};
	
	if ((numStartGroups+numEndGroups) > 8)
	{
		errno = EAFNOSUPPORT;
		return 0;
	};
	
	int numZeroGroups = 8-(numStartGroups+numEndGroups);
	memcpy(dst, startGroups, 2*numStartGroups);
	dst += numStartGroups;
	memset(dst, 0, 2*numZeroGroups);
	dst += numZeroGroups;
	memcpy(dst, endGroups, 2*numEndGroups);
	return 1;
};

int inet_pton(int af, const char *src, void *dst)
{
	switch (af)
	{
	case AF_INET:
		return inet_pton4(src, (uint8_t*)dst);
		break;
	case AF_INET6:
		return inet_pton6(src, (uint16_t*)dst);
		break;
	default:
		errno = EAFNOSUPPORT;
		return 0;
	};
};

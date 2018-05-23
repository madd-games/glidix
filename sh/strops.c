/*
	Glidix Shell

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

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/debug.h>

#include "strops.h"

char *str_concat(const char *a, const char *b)
{
	size_t newSize = strlen(a) + strlen(b) + 1;
	char *result = (char*) malloc(newSize);
	strcpy(result, a);
	strcat(result, b);
	return result;
};

char *str_find(char *haystack, const char *needleList, const char *quoteChars)
{
	char currentQuote = 0;
	int bracketLevel = 0;
	
	for (; *haystack!=0; haystack++)
	{
		char c = *haystack;
		
		if (c == '\\')
		{
			haystack++;
			continue;
		};
		
		if (strchr(quoteChars, currentQuote) == NULL && bracketLevel == 0 && strchr(needleList, c) != NULL)
		{
			return haystack;
		};
		
		if (currentQuote == 0)
		{
			if (c == '(' || c == '{')
			{
				bracketLevel++;
			}
			else if (c == ')' || c == '}')
			{
				if (bracketLevel != 0) bracketLevel--;
			}
			else if (strchr("\"'", c) != NULL)
			{
				currentQuote = c;
			}
			else if (bracketLevel != 0)
			{
				// NOP
			}
			else if (strchr(needleList, c) != NULL)
			{
				return haystack;
			};
		}
		else if (currentQuote == c)
		{
			currentQuote = 0;
		};
	};
	
	return NULL;
};

char *str_concatn(const char *a, const char *b, const char *end)
{
	size_t newSize = strlen(a) + (end - b) + 1;
	char *result = (char*) malloc(newSize);
	strcpy(result, a);
	
	char *put = &result[strlen(a)];
	while (b != end)
	{
		*put++ = *b++;
	};
	
	*put = 0;
	return result;
};

char *str_token(char **strp, const char *delimiterList, const char *quoteChars)
{
	char *token = *strp;
	if (token == NULL) return NULL;
	while ((strchr(delimiterList, *token) != NULL) && (*token != 0)) token++;
	if (*token == 0)
	{
		*strp = NULL;
		return NULL;
	};
	
	char *delimPos = str_find(token, delimiterList, quoteChars);
	if (delimPos == NULL)
	{
		*strp = NULL;
	}
	else
	{
		*delimPos = 0;
		*strp = delimPos + 1;
	};
	
	return token;
};

void str_canon(char *str)
{
	static const char *quoteChars = "\"'";
	
	int bracketLevel = 0;
	char quote = 0;
	char temp[strlen(str)+1];
	strcpy(temp, str);
	
	char *scan = temp;
	while (*scan != 0)
	{
		if (*scan == '\\')
		{
			scan++;
			*str++ = *scan++;
			continue;
		};
		
		if (quote != 0)
		{
			if (*scan == quote)
			{
				if (bracketLevel != 0) *str++ = *scan;
				quote = 0;
				scan++;
			}
			else
			{
				*str++ = *scan++;
			};
		}
		else
		{
			if (*scan == '(' || *scan == '{')
			{
				bracketLevel++;
				*str++ = *scan++;
			}
			else if (*scan == ')' || *scan == '}')
			{
				if (bracketLevel != 0) bracketLevel--;
				*str++ = *scan++;
			}
			else if (strchr(quoteChars, *scan) != NULL)
			{
				quote = *scan++;
				if (bracketLevel != 0) *str++ = quote; 
			}
			else
			{
				*str++ = *scan++;
			};
		};
	};
	
	*str = 0;
};

/*
	Glidix GUI

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
#include <stdio.h>
#include <string.h>

#include "lex.h"

static Matcher* addMatcher(Regex *regex)
{
	size_t index = regex->numMatchers++;
	regex->matcherList = (Matcher*) realloc(regex->matcherList, regex->numMatchers * sizeof(Matcher));
	return &regex->matcherList[index];
};

Regex* lexCompileRegex(const char *spec)
{
	Regex* regex = (Regex*) malloc(sizeof(Regex));
	regex->matcherList = NULL;
	regex->numMatchers = 0;
	regex->down = NULL;
	regex->next = NULL;
	
	while (*spec != 0)
	{
		if (*spec == '\\')
		{
			// escape, so exact match next element.
			spec++;
			char c = *spec++;
			if (c == 0)
			{
				return NULL;
			};
			
			if (c == 'n') c = '\n';
			
			Matcher *matcher = addMatcher(regex);
			matcher->exact.mode = LEX_MATCH_EXACT;
			matcher->exact.c = c;
		}
		else if (*spec == '*')
		{
			if (regex->numMatchers == 0) return NULL;
			if ((regex->matcherList[regex->numMatchers-1].mode & ~0xFF) != 0) return NULL;
			regex->matcherList[regex->numMatchers-1].mode |= LEX_MATCH_ASTERISK;
			spec++;
		}
		else if (*spec == '+')
		{
			if (regex->numMatchers == 0) return NULL;
			if ((regex->matcherList[regex->numMatchers-1].mode & ~0xFF) != 0) return NULL;
			regex->matcherList[regex->numMatchers-1].mode |= LEX_MATCH_PLUS;
			spec++;
		}
		else if (*spec == '?')
		{
			if (regex->numMatchers == 0) return NULL;
			regex->matcherList[regex->numMatchers-1].mode |= LEX_MATCH_NOGREED;
			spec++;
		}
		else if (*spec == '[')
		{
			spec++;
			
			Matcher *matcher = addMatcher(regex);
			matcher->set.mode = LEX_MATCH_SET;
			memset(matcher->set.bitmap, 0, 32);
			
			while (*spec != ']')
			{
				if (*spec == 0) return NULL;
				if (*spec == '\\')
				{
					spec++;
					unsigned char c = (unsigned char) *spec++;
					
					if (c == 0) return NULL;
					matcher->set.bitmap[c / 8] |= (1 << (c % 8));
					continue;
				};
				
				char start = *spec++;
				char end = start;
				
				if (*spec == '-')
				{
					spec++;
					end = *spec++;
					
					if (end == 0) return NULL;
				};
				
				if ((unsigned char)end < (unsigned char)start)
				{
					return NULL;
				};
				
				unsigned char c;
				for (c=(unsigned char)start; c<=(unsigned char)end; c++)
				{
					matcher->set.bitmap[c / 8] |= (1 << (c % 8));
				};
			};
			
			spec++;	// skip over ']'
		}
		else if (*spec == '.')
		{
			Matcher *matcher = addMatcher(regex);
			matcher->set.mode = LEX_MATCH_SET;
			memset(matcher->set.bitmap, 0xFF, 32);
			matcher->set.bitmap[0] = 0xFE;		// do not match \0 !
			spec++;
		}
		else if (*spec == '$')
		{
			Matcher *matcher = addMatcher(regex);
			matcher->set.mode = LEX_MATCH_SET;
			memset(matcher->set.bitmap, 0, 32);
			matcher->set.bitmap[(unsigned char)' ' / 8] |= (1 << ((unsigned char)' ' % 8));
			matcher->set.bitmap[(unsigned char)'\t' / 8] |= (1 << ((unsigned char)'\t' % 8));
			matcher->set.bitmap[(unsigned char)'\n' / 8] |= (1 << ((unsigned char)'\n' % 8));
			spec++;
		}
		else if (*spec == '(')
		{
			Matcher *matcher = addMatcher(regex);
			matcher->bracket.mode = LEX_MATCH_BRACKET;
			
			Regex* new = (Regex*) malloc(sizeof(Regex));
			new->matcherList = NULL;
			new->numMatchers = 0;
			new->down = regex;
			new->next = NULL;
			
			regex = new;
			matcher->bracket.first = regex;
			spec++;
		}
		else if (*spec == '|')
		{
			if (regex->down == NULL)
			{
				// bottom expression cannot have chains, they must be in brackets.
				return NULL;
			};
			
			Regex* new = (Regex*) malloc(sizeof(Regex));
			new->matcherList = NULL;
			new->numMatchers = 0;
			new->down = regex->down;
			new->next = NULL;
			
			regex->next = new;
			regex = new;
			spec++;
		}
		else if (*spec == ')')
		{
			if (regex->down == NULL)
			{
				return NULL;
			};
			
			regex = regex->down;
			spec++;
		}
		else if (*spec == '%')
		{
			// end of string
			Matcher *matcher = addMatcher(regex);
			matcher->mode = LEX_MATCH_END;
			spec++;
		}
		else
		{
			// unknown, so exact match
			Matcher *matcher = addMatcher(regex);
			matcher->exact.mode = LEX_MATCH_EXACT;
			matcher->exact.c = *spec++;
		};
	};
	
	if (regex->down != NULL) return NULL;
	return regex;
};

void lexDeleteRegex(Regex *regex)
{
	free(regex->matcherList);
	free(regex);
};

static ssize_t lexMatchSingle(Matcher *matcher, const char *str)
{
	unsigned char c;
	Regex *sub;
	ssize_t submatch;
	switch (matcher->mode & 0xFF)
	{
	case LEX_MATCH_EXACT:
		if (*str == matcher->exact.c) return 1;
		else return -1;
	case LEX_MATCH_SET:
		c = (unsigned char) *str;
		if (matcher->set.bitmap[c / 8] & (1 << (c % 8))) return 1;
		else return -1;
	case LEX_MATCH_BRACKET:
		for (sub=matcher->bracket.first; sub!=NULL; sub=sub->next)
		{
			submatch = lexMatch(sub, str);
			if (submatch != -1) return submatch;
		};
		return -1;
	case LEX_MATCH_END:
		if (*str == 0) return 0;
		else return -1;
	default:
		fprintf(stderr, "liblex internal error: unknown matcher type\n");
		abort();
		return -1;
	};
};

ssize_t lexMatch(Regex *regex, const char *str)
{
	ssize_t result = 0;
	
	size_t i = 0;
	int caughtOne = 0;
	while (i != regex->numMatchers)
	{
	 	Matcher *matcher = &regex->matcherList[i];	 	
	 	if (matcher->mode & LEX_MATCH_NOGREED)
	 	{
	 		if (i != regex->numMatchers-1)
	 		{
	 			if (lexMatchSingle(&regex->matcherList[i+1], str) != -1)
	 			{
	 				i++;
	 				continue;
	 			};
	 		};
	 	};
	 	
	 	ssize_t matchSize = lexMatchSingle(matcher, str);
	 	if (matchSize == -1)
	 	{
	 		if (matcher->mode & LEX_MATCH_ASTERISK)
	 		{
	 			i++;
	 			continue;
	 		};
	 		
	 		if ((matcher->mode & LEX_MATCH_PLUS) && (caughtOne))
	 		{
	 			caughtOne = 0;
	 			i++;
	 			continue;
	 		};
	 		
	 		if (matcher->mode & LEX_MATCH_NOGREED)
	 		{
	 			// '?' at the end
	 			i++;
	 			continue;
	 		};
	 		
	 		return -1;
	 	};
	 	
	 	result += matchSize;
	 	str += matchSize;
	 	
	 	if (matcher->mode & LEX_MATCH_PLUS)
	 	{
	 		caughtOne = 1;
	 	}
	 	else if ((matcher->mode & LEX_MATCH_ASTERISK) == 0)
	 	{
	 		i++;
	 	};
	};
	
	return result;
};

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

#include <string.h>
#include <inttypes.h>
#include <stdlib.h>
#include <stdio.h>

#include "dict.h"

Dict dictEnviron;
Dict dictShellVars;

void dictInit(Dict *dict)
{
	dict->count = 0;
	dict->list = (char**) malloc(sizeof(char*));
	dict->list[0] = NULL;
};

void dictInitFrom(Dict *dict, char *list[])
{
	for (dict->count = 0; list[dict->count] != NULL; dict->count++);
	dict->list = (char**) malloc(sizeof(char*) * (dict->count+1));
	
	size_t i;
	for (i=0; i<dict->count; i++)
	{
		dict->list[i] = strdup(list[i]);
	};
	
	dict->list[dict->count] = NULL;
};

const char *dictGet(Dict *dict, const char *key)
{
	size_t keySize = strlen(key);
	
	size_t i;
	for (i=0; i<dict->count; i++)
	{
		if (strlen(dict->list[i]) <= keySize) continue;
		
		if (dict->list[i][keySize] == '=')
		{
			if (memcmp(dict->list[i], key, keySize) == 0)
			{
				return &dict->list[i][keySize+1];
			};
		};
	};
	
	return NULL;
};

void dictPut(Dict *dict, const char *spec)
{
	char *equalPos = strchr(spec, '=');
	if (equalPos == NULL)
	{
		fprintf(stderr, "SHELL INTERNAL ERROR: 'spec' without '=' passed to dictPut()!\n");
		abort();
	};
	
	size_t prefixSize = (equalPos - spec) + 1;
	
	size_t i;
	for (i=0; i<dict->count; i++)
	{
		if (memcmp(dict->list[i], spec, prefixSize) == 0)
		{
			free(dict->list[i]);
			dict->list[i] = strdup(spec);
			return;
		};
	};
	
	size_t insertPos = dict->count++;
	dict->list = (char**) realloc(dict->list, sizeof(char*)*(dict->count+1));
	dict->list[insertPos] = strdup(spec);
	dict->list[insertPos+1] = NULL;
};

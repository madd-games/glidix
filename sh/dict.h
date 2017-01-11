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

#ifndef DICT_H_
#define DICT_H_

#include <string.h>
#include <inttypes.h>

/**
 * Describes a dictionary. This is used for storing environment variables, shell
 * variables, etc, in different lists.
 */
typedef struct
{
	/**
	 * Number of entries (excluding the final NULL).
	 */
	size_t count;
	
	/**
	 * The list. Terminatied by a NULL entry. Each other entry is a string in the form
	 * name=value.
	 */
	char **list;
} Dict;

/**
 * Initialize a dictionary to contain no values.
 */
void dictInit(Dict *dict);

/**
 * Initialize a dictionary to contain the values from the given list.
 */
void dictInitFrom(Dict *dict, char *list[]);

/**
 * Get the value of a key in a given dictionary. Returns NULL if there is no value.
 */
const char *dictGet(Dict *dict, const char *key);

/**
 * Put a value in the dictionary. The 'spec' must have the format "key=value".
 */
void dictPut(Dict *dict, const char *spec);

/**
 * Special dictionaries.
 */
extern Dict dictEnviron;
extern Dict dictShellVars;

#endif

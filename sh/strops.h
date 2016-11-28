/*
	Glidix Shell

	Copyright (c) 2014-2016, Madd Games.
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

#ifndef STROPS_H_
#define STROPS_H_

/**
 * Concatenate 2 strings, and return the result as a new string on the heap.
 * free() must be called on the result when it's no longer needed.
 */
char *str_concat(const char *a, const char *b);

/**
 * Find the first occurence of one of the characters in 'needleList', in the
 * string 'haystack', but only if they're not quotes by one of the 'quoteChars'.
 * Returns a pointer to the character in question, or NULL if it's not there.
 */
char *str_find(char *haystack, const char *needleList, const char *quoteChars);

/**
 * Concatenate string "a" and "b", but only follow "b" up to "end".
 */
char *str_concatn(const char *a, const char *b, const char *end);

/**
 * Get the next token of a string delimited by the specified unquoted delimeter.
 */
char *str_token(char **strp, const char *delimiterList, const char *quoteChars);

/**
 * Canonicalize the given argument string; strip off unquoted quote characters.
 * Note this edits the string in-place.
 */
void str_canon(char *in);

#endif

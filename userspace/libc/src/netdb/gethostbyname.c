/*
	Glidix Runtime

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

#include <netdb.h>
#include <string.h>
#include <stdlib.h>

struct hostent *gethostbyname(const char *name)
{
	static char *localhost_aliases[] = {
		"glidix-unknown",
		"localhost",
		"127.0.0.1",
		NULL
	};

	static struct hostent localhost = {
		"glidix-unknown",			/* h_name */
		localhost_aliases,			/* h_aliases */
		0,					/* h_addrtype */
		0,					/* h_length */
		NULL					/* h_addr_list */
	};

	if (strcmp(name, "localhost") == 0)
	{
		return &localhost;
	}
	else if (strcmp(name, "glidix-unknown") == 0)
	{
		return &localhost;
	}
	else if (strcmp(name, "127.0.0.1") == 0)
	{
		return &localhost;
	}
	else
	{
		return NULL;
	};
};

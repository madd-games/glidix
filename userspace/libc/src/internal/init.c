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

#include <sys/glidix.h>
#include <stddef.h>
#include <_heap.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

char **environ;

int __errno_initial;
void __init_sig();
void __glidixrt_init(int (*_main)(int,char*[],char*[]))
{
	_glidix_seterrnoptr(&__errno_initial);
	_heap_init();
	__init_sig();

	// parse the execPars
	size_t parsz = _glidix_getparsz();
	char *buffer = (char*) malloc(parsz);
	_glidix_getpars(buffer, parsz);

	int argc = 0;
	char **argv = NULL;

	char *scan = buffer;
	char *prevStart = buffer;
	while (1)
	{
		char c = *scan++;
		if (c == 0)
		{
			if ((scan-prevStart) == 1) break;
			argv = realloc(argv, sizeof(char*)*(argc+1));
			argv[argc++] = prevStart;
			prevStart = scan;
		};
	};

	argv = realloc(argv, sizeof(char*)*(argc+1));
	argv[argc] = NULL;

	environ = NULL;
	int envc = 0;
	prevStart++;
	scan = prevStart;
	while (1)
	{
		char c = *scan++;
		if (c == 0)
		{
			if ((scan-prevStart) == 1) break;
			environ = realloc(environ, sizeof(char*)*(envc+1));
			environ[envc++] = strdup(prevStart);
			prevStart = scan;
		};
	};
	environ = realloc(environ, sizeof(char*)*(envc+1));
	environ[envc] = NULL;

	exit(_main(argc, argv, environ));
};

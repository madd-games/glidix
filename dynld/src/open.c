/*
	Glidix dynamic linker

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

#include <unistd.h>
#include <fcntl.h>

#include "dynld.h"

int dynld_open(const char *soname, ...)
{
	if (strchr(soname, '/') != NULL)
	{
		strcpy(dynld_errmsg, "library not found");		// in case open() returns -1
		return open(soname, O_RDONLY);
	}
	else
	{
		char libpath[PATH_MAX];
		
		va_list ap;
		va_start(ap, soname);
		
		while (1)
		{
			const char *pathspec = va_arg(ap, char*);
			if (pathspec == NULL)
			{
				strcpy(dynld_errmsg, "library not found");
				return -1;
			};
			
			while (*pathspec != 0)
			{
				size_t size;
				char *colonPos = strchr(pathspec, ':');
				
				if (colonPos == NULL)
				{
					size = strlen(pathspec);
				}
				else
				{
					size = colonPos - pathspec;
				};
				
				if ((size+strlen(soname)+1) < PATH_MAX)
				{
					memcpy(libpath, pathspec, size);
					libpath[size] = '/';
					strcpy(&libpath[size+1], soname);
					
					int fd = open(libpath, O_RDONLY);
					if (fd != -1) return fd;
				};
				
				pathspec += size;
				if (*pathspec == ':') pathspec++;
			};
		};
	};
};

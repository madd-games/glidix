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

#include <sys/mman.h>
#include <sys/glidix.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>

/* dlfcn.c */
uint64_t __alloc_pages(size_t len);

void* mmap(void *addr, size_t len, int prot, int flags, int fd, off_t offset)
{
	uint64_t actualAddr = (uint64_t) addr;
	actualAddr &= ~0xFFF;
	if (actualAddr == 0) actualAddr = __alloc_pages(len);

	if (flags & MAP_ANONYMOUS)
	{
		if (mprotect((void*)actualAddr, len, prot | PROT_ALLOC) != 0)
		{
			return (void*) -1;
		};
	}
	else
	{
		uint64_t outAddr = _glidix_mmap(actualAddr, len, prot, flags, fd, offset);
		if (outAddr == 0)
		{
			if (errno == ENODEV)
			{
				if ((flags & PROT_WRITE) == 0)
				{
					// the specified file does not support mmap() but we can emulate
					// read-only mmap()s by means of map-and-read
					// stuff like 'gcc' really needs that apparently
					if (mmap((void*)actualAddr, len, prot | PROT_WRITE, flags | MAP_ANONYMOUS, 0, offset) != (void*)-1)
					{
						off_t pos = lseek(fd, 0, SEEK_CUR);
						if (pos == (off_t)-1)
						{
							errno = ENODEV;
							return (void*)-1;
						};
						
						lseek(fd, offset, SEEK_SET);
						read(fd, (void*)actualAddr, len);
						lseek(fd, pos, SEEK_SET);
						return (void*) actualAddr;
					};
				};
			};
			
			return (void*) -1;
		};
	};

	return (void*) actualAddr;
};

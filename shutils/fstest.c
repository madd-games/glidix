/*
	Glidix Shell Utilities

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

#include <sys/types.h>
#include <sys/time.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <fcntl.h>

char buffer[0x200000];

void doTest(int fd, size_t sz)
{
	clock_t start = clock();
	off_t off;
	for (off=0; off<0x200000; off+=sz)
	{
		pwrite(fd, buffer, sz, off);
	};
	
	clock_t diff = clock() - start;
	
	printf("This took %ldms\n", (1000 * diff / CLOCKS_PER_SEC));
};

int main(int argc, char *argv[])
{
	int fd = open(".testfile", O_RDWR | O_CREAT | O_TRUNC, 0600);
	if (fd == -1)
	{
		perror("open .testfile");
		return 1;
	};
	
	ftruncate(fd, 0x200000);
	
	printf("Testing with 1 byte blocks...\n");
	doTest(fd, 1);
	
	printf("Testing with 512 byte blocks...\n");
	doTest(fd, 512);
	
	printf("testing with 512 byte blocks again...\n");
	doTest(fd, 512);
	
	printf("Testing with 4KB blocks...\n");
	doTest(fd, 0x1000);
	
	printf("testing with 2MB blocks...\n");
	doTest(fd, 0x200000);
	
	printf("Tests complete.\n");
	close(fd);
	return 0;
};

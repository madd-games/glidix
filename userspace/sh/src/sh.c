/*
	Glidix Shell

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

#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <locale.h>
#include "command.h"
#include "sh.h"

int getline(int fd, char *buf, size_t max)
{
	size_t sofar = 0;
	int status = 0;
	char *bufstart = buf;

	while (1)
	{
		char c;
		ssize_t sz = read(fd, &c, 1);
		if (sz == -1)
		{
			*bufstart = 0;
			return -2;
		};

		if (sz == 0)
		{
			c = '\n';
		};

		if (c == '\n')
		{
			*buf = 0;
			return 0;
		};

		if (sofar == max)
		{
			status = -1;
			continue;
		};

		*buf++ = c;
		sofar++;
	};

	return status;
};

int main(int argc, char *argv[])
{
	while (1)
	{
		char cwd[256];
		getcwd(cwd, 256);

		char prompt;
		if (getuid() == 0)
		{
			prompt = '#';
		}
		else
		{
			prompt = '$';
		};

		printf("%s%c ", cwd, prompt);

		char line[256];
		int status = getline(0, line, 256);
		if (status == -1)
		{
			fprintf(stderr, "%s: line too long\n", argv[0]);
		}
		else if (status == -2)
		{
			if (errno == EINTR)
			{
				printf("\n");
			};
		};

		if (line[0] != 0)
		{
			execCommand(line);
		};
	};
	return 0;
};

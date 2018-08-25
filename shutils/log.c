/*
	Glidix Shell Utilities

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

#include <sys/stat.h>
#include <sys/wait.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>

#define	DEFAULT_MAX_SIZE			(20 * 1024)
#define	DEFAULT_MAX_BACKLOGS			7
#define	DEFAULT_BUFFER_SIZE			1024
#define	DEFAULT_MODE				0600

char *progName = NULL;
char *filename = NULL;

size_t maxSize = DEFAULT_MAX_SIZE;
int maxBacklogs = DEFAULT_MAX_BACKLOGS;
size_t bufferSize = DEFAULT_BUFFER_SIZE;
mode_t logMode = DEFAULT_MODE;

void rotateRecur(int level)
{
	char path[strlen(filename) + 16];
	sprintf(path, "%s.%d.gz", filename, level);
	
	if (level == maxBacklogs)
	{
		unlink(path);
	}
	else
	{
		rotateRecur(level+1);
		char newpath[strlen(filename) + 16];
		sprintf(newpath, "%s.%d.gz", filename, level+1);
		
		rename(path, newpath);
	};
};

void rotateLogs()
{
	rotateRecur(1);
};

int main(int argc, char *argv[])
{
	progName = argv[0];
	
	int i;
	for (i=1; i<argc; i++)
	{
		if (argv[i][0] == '-')
		{
			fprintf(stderr, "%s: unrecognised command-line switch: `%s'\n", argv[0], argv[i]);
			return 1;
		}
		else
		{
			if (filename == NULL)
			{
				filename = argv[i];
			}
			else
			{
				fprintf(stderr, "%s: multiple files specified\n", argv[0]);
				return 1;
			};
		};
	};
	
	if (filename == NULL)
	{
		fprintf(stderr, "%s: no file name specified\n", argv[0]);
		return 1;
	};
	
	int logfd = open(filename, O_RDWR | O_CREAT | O_APPEND, logMode);
	if (logfd == -1)
	{
		fprintf(stderr, "%s: cannot open %s: %s\n", argv[0], filename, strerror(errno));
		return 1;
	};
	
	char buffer[bufferSize];
	while (1)
	{
		ssize_t sz = read(0, buffer, bufferSize);
		if (sz == 0) break;
		if (sz == -1)
		{
			fprintf(stderr, "%s: cannot read: %s\n", argv[0], strerror(errno));
			return 1;
		};
		
		struct stat st;
		if (fstat(logfd, &st) != 0)
		{
			fprintf(stderr, "%s: cannot fstat: %s\n", argv[0], strerror(errno));
			return 1;
		};
		
		if (st.st_size > maxSize)
		{
			rotateLogs();
			char compath[strlen(filename)+16];
			sprintf(compath, "%s.1.gz", filename);
			
			int comfd = open(compath, O_WRONLY | O_CREAT | O_TRUNC, logMode);
			if (comfd == -1)
			{
				fprintf(stderr, "%s: cannot open %s: %s\n", argv[0], compath, strerror(errno));
				return 1;
			};
			
			pid_t pid = fork();
			if (pid == 0)
			{
				close(0);
				dup(logfd);
				lseek(0, 0, SEEK_SET);
				close(1);
				dup(comfd);
				execl("/usr/bin/deflate", "deflate", NULL);
				perror("deflate");
				_exit(1);
			}
			else
			{
				close(comfd);
				waitpid(pid, NULL, 0);
			};
			
			ftruncate(logfd, 0);
		};
		
		write(logfd, buffer, sz);
	};
	
	close(logfd);
	sync();
	return 0;
};

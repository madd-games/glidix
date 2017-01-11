/*
	Glidix Runtime

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

#include <sys/types.h>
#include <sys/wait.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>

enum
{
	__MODE_READ,
	__MODE_WRITE
};

/* internal/file.c */
void __fd_flush(FILE *fp);

FILE *popen(const char *cmd, const char *mode)
{
	int m;
	if (strcmp(mode, "r") == 0)
	{
		m = __MODE_READ;
	}
	else if (strcmp(mode, "w") == 0)
	{
		m = __MODE_WRITE;
	}
	else
	{
		errno = EINVAL;
		return NULL;
	};
	
	int pipefd[2];
	if (pipe(pipefd) != 0)
	{
		return NULL;
	};
	
	pid_t pid = fork();
	if (pid == -1)
	{
		return NULL;
	};
	
	if (pid == 0)
	{
		if (m == __MODE_READ)
		{
			// they're reading from our stdout/stderr
			close(1);
			close(2);
			dup2(pipefd[1], 1);
			dup2(pipefd[2], 2);
		}
		else
		{
			// they're writing to out stdin
			close(0);
			dup2(pipefd[0], 0);
		};
		
		execl("/bin/sh", "sh", "-c", cmd, NULL);
		_exit(127);
	}
	else
	{
		FILE *fp = (FILE*) malloc(sizeof(FILE));
		fp->_buf = &fp->_nanobuf;
		fp->_rdbuf = fp->_buf;
		fp->_wrbuf = fp->_buf;
		fp->_bufsiz = 1;
		fp->_bufsiz_org = 1;
		fp->_trigger = 0;
		fp->_flush = __fd_flush;
		if (m == __MODE_READ)
		{
			fp->_fd = pipefd[0];
			fp->_flags = __FILE_READ;
		}
		else
		{
			fp->_fd = pipefd[1];
			fp->_flags = __FILE_WRITE;
		};
		fp->_ungot = -1;
		fp->_pid = pid;
		
		return fp;
	};
};

int pclose(FILE *fp)
{
	int status;
	do
	{
		if (waitpid(fp->_pid, &status, 0) > 0)
		{
			close(fp->_fd);
			free(fp);
			if (WIFEXITED(status))
			{
				return WEXITSTATUS(status);
			};
			
			return 127;
		};
	} while (errno == EINTR);
	
	return -1;
};

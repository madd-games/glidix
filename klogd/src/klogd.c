/*
	Glidix Kernel Log Daemon

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

#include <sys/wait.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

void daemon()
{
	int fd = open("/run/klogd.pid", O_WRONLY | O_CREAT | O_TRUNC | O_EXCL, 0600);
	if (fd == -1) return;
	char buf[64];
	sprintf(buf, "%d", getpid());
	write(fd, buf, strlen(buf));
	close(fd);
	
	fd = open("/dev/klog", O_RDONLY);
	if (fd == -1) return;
	
	int pipefd[2];
	if (pipe(pipefd) != 0)
	{
		return;
	};
	
	pid_t pid = fork();
	if (pid == 0)
	{
		close(fd);
		close(pipefd[1]);
		close(0);
		dup(pipefd[0]);
		close(pipefd[0]);
		
		execl("/usr/bin/log", "log", "/var/log/kernel.log", NULL);
		_exit(1);
	}
	else
	{
		close(pipefd[0]);
		
		char buffer[4096];
		while (1)
		{
			ssize_t sz = read(fd, buffer, 4096);
			if (sz < 1) break;
			write(pipefd[1], buffer, sz);
		};
	};
};

int main()
{
	pid_t pid = fork();
	if (pid == -1)
	{
		perror("fork");
		return 1;
	}
	else if (pid == 0)
	{
		setsid();
		pid = fork();
		
		if (pid == -1)
		{
			return 1;
		}
		else if (pid == 0)
		{
			daemon();
			_exit(0);
		}
		else
		{
			_exit(1);
		};
	}
	else
	{
		waitpid(pid, NULL, 0);
	};
	
	return 0;
};

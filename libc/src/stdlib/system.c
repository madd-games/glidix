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

#include <sys/wait.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>

int system(const char *cmd)
{
	const char *shell = getenv("SHELL");
	if (shell == NULL)
	{
		shell = "/bin/sh";
	};
	
	int stat;
	pid_t pid;
	struct sigaction sa, savintr, savequit;
	if (cmd == NULL) return 1;
	sa.sa_handler = SIG_IGN;
	sa.sa_flags = 0;
	sigaction(SIGINT, &sa, &savintr);
	sigaction(SIGQUIT, &sa, &savequit);
	
	if ((pid = fork()) == 0)
	{
		sigaction(SIGINT, &savintr, NULL);
		sigaction(SIGQUIT, &savequit, NULL);
		execl(shell, "sh", "-c", cmd, NULL);
		_exit(127);
	};
	
	if (pid == -1)
	{
		stat = -1; /* errno comes from fork() */
	}
	else
	{
		while (waitpid(pid, &stat, 0) == -1)
		{
			if (errno != EINTR)
			{
				stat = -1;
				break;
			};
		};
	};
	
	sigaction(SIGINT, &savintr, NULL);
	sigaction(SIGQUIT, &savequit, NULL);
	return stat;
};

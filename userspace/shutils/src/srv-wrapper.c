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

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

pid_t childPid = 0;

void on_signal(int sig, siginfo_t *si, void *ignore)
{
	switch (sig)
	{
	case SIGINT:
	case SIGTERM:
	case SIGHUP:
		kill(childPid, sig);
		break;
	};
};

int main(int argc, char *argv[])
{
	if (argc < 2)
	{
		return 1;
	};

	if (geteuid() != 0)
	{
		fprintf(stderr, "%s: only root can do this\n", argv[0]);
		return 1;
	};
	
	struct sigaction sa;
	sa.sa_sigaction = on_signal;
	sa.sa_flags = SA_SIGINFO;
	if (sigaction(SIGINT, &sa, NULL) != 0)
	{
		perror("sigaction SIGINT");
		return 1;
	};
	if (sigaction(SIGTERM, &sa, NULL) != 0)
	{
		perror("sigaction SIGTERM");
		return 1;
	};
	if (sigaction(SIGINT, &sa, NULL) != 0)
	{
		perror("sigaction SIGHUP");
		return 1;
	};
	
	while (1)
	{
		pid_t pid = fork();
		if (pid == 0)
		{
			if (execv(argv[1], &argv[1]) != 0)
			{
				perror("exec");
				return 1;
			};
		}
		else
		{
			childPid = pid;
			
			while (1)
			{
				pid_t status = waitpid(pid, NULL, 0);
				if (status != -1)
				{
					// not EINTR
					fprintf(stderr, "WARNING: service respawning\n");
					break;
				};
			};
		};
	};
	
	return 0;
};

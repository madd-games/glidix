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

#include <string.h>
#include <signal.h>
#include <errno.h>

const char *sys_signame[__SIG_COUNT] = {
	"0",
	"SIGHUP",
	"SIGINT",
	"SIGQUIT",
	"SIGILL",
	"SIGTRAP",
	"SIGABRT",
	"SIGEMT",
	"SIGFPE",
	"SIGKILL",
	"SIGBUS",
	"SIGSEGV",
	"SIGSYS",
	"SIGPIPE",
	"SIGALRM",
	"SIGTERM",
	"SIGUSR1",
	"SIGUSR2",
	"SIGCHLD",
	"SIGPWR",
	"SIGWINCH",
	"SIGURG",
	"SIGPOLL",
	"SIGSTOP",
	"SIGTSTP",
	"SIGCONT",
	"SIGTTIN",
	"SIGTTOU",
	"SIGVTALRM",
	"SIGPROF",
	"SIGXCPU",
	"SIGXFSZ",
	"SIGWAITING",
	"SIGLWP",
	"SIGAIO",
	"SIGTHKILL",
	"SIGTHWAKE",
	"SIGTRACE",
	"SIGTHSUSP"
};

const char *sys_siglist[__SIG_COUNT] = {
	"Null",
	"Hangup",
	"Interrupted",
	"Quit",
	"Illegal instruction",
	"Trapped",
	"Aborted",
	"Emulator trap",
	"Arithmetic error",
	"Killed",
	"Bus error",
	"Invalid memory access",
	"Bad system call",
	"Broken pipe",
	"Alarm",
	"Terminated",
	"User-defined 1",
	"User-defined 1",
	"Child status change",
	"Power failure",
	"Window size change",
	"Urgent I/O",
	"Pollable event",
	"Stopped",
	"Terminal stop",
	"Continued",
	"Terminal input from background",
	"Terminal output from background",
	"Virtual timer expired",
	"Polling timer expired",
	"CPU time limit exceeded",
	"File size limit exceeded",
	"Waiting",
	"Threading signal",
	"Asynchronous I/O",
	"Thread killed",
	"Thread wake-up",
	"Debugged child event",
	"Thread suspended"
};

char *strsignal(int signum)
{
	if ((signum < 0) || (signum >= __SIG_COUNT))
	{
		errno = EINVAL;
		return (char*) "Invalid signal";
	};
	
	return (char*) sys_siglist[signum];
};

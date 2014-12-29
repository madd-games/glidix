/*
	Glidix kernel

	Copyright (c) 2014, Madd Games.
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

#ifndef __glidix_signal_h
#define __glidix_signal_h

#include <glidix/common.h>

#define	SIGHUP		1
#define	SIGINT		2
#define	SIGQUIT		3
#define	SIGILL		4
#define	SIGTRAP		5
#define	SIGABRT		6
#define	SIGEMT		7
#define	SIGFPE		8
#define	SIGKILL		9
#define	SIGBUS		10
#define	SIGSEGV		11
#define	SIGSYS		12
#define	SIGPIPE		13
#define	SIGALRM		14
#define	SIGTERM		15
#define	SIGUSR1		16
#define	SIGUSR2		17
#define	SIGCHLD		18
#define	SIGPWR		19
#define	SIGWINCH	20
#define	SIGURG		21
#define	SIGPOLL		22
#define	SIGSTOP		23
#define	SIGTSTP		24
#define	SIGCONT		25
#define	SIGTTIN		26
#define	SIGTTOU		27
#define	SIGVTALRM	28
#define	SIGPROF		29
#define	SIGXCPU		30
#define	SIGXFSZ		31
#define	SIGWAITING	32
#define	SIGLWP		33
#define	SIGAIO		34
#define	SIG_NUM		35

/**
 * si_code for SIGSEGV
 */
#define	SEGV_MAPERR	1
#define	SEGV_ACCERR	2

/**
 * si_code for SIGCHLD
 */
#define	CLD_EXITED	0
#define	CLD_KILLED	1
#define	CLD_DUMPED	2		/* never returned by glidix */
#define	CLD_TRAPPED	3
#define	CLD_STOPPED	4
#define	CLD_CONTINUED	5

union sigval
{
	int		sival_int;
	void*		sival_ptr;
};

typedef struct
{
	int		si_signo;
	int		si_code;
	int		si_errno;
	pid_t		si_pid;
	uid_t		si_uid;
	void*		si_addr;
	int		si_status;
	long		si_band;
	union sigval	si_value;
} siginfo_t;

typedef struct _sigq
{
	siginfo_t	si;
	struct _sigq	*next;
} SignalQueue;

struct _Thread;
void dispatchSignal(struct _Thread *thread);
void sendSignal(struct _Thread *thread, siginfo_t *siginfo);
void sigret(Regs *regs, void *ret);

#endif

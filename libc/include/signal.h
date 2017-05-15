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

#ifndef _SIGNAL_H
#define _SIGNAL_H

#include <sys/types.h>
#include <stddef.h>
#include <time.h>

/**
 * Signals, as used by Glidix.
 */
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
#define	SIGTHKILL	35			/* kill a single thread */
#define	SIGTHWAKE	36			/* wake a thread without dispatching a signal */
#define	SIGTRACE	37			/* debugger notification */
#define	__SIG_COUNT	38

#define	SIG_DFL		((void (*)(int)) 1)
#define	SIG_ERR		((void (*)(int)) 2)
#define	SIG_HOLD	((void (*)(int)) 3)
#define	SIG_IGN		((void (*)(int)) 4)

#define	SA_NOCLDSTOP	(1 << 0)
#define	SA_NOCLDWAIT	(1 << 1)
#define	SA_NODEFER	(1 << 2)		/* ignored by Glidix currently */
#define	SA_ONSTACK	(1 << 3)		/* ignored by Glidix currently */
#define	SA_RESETHAND	(1 << 4)
#define	SA_RESTART	(1 << 5)		/* ignored by Glidix currently */
#define	SA_SIGINFO	(1 << 6)

#define	SIG_BLOCK	0
#define	SIG_UNBLOCK	1
#define	SIG_SETMASK	2

#define	SEGV_MAPERR	1
#define	SEGV_ACCERR	2

#define	CLD_EXITED	0
#define	CLD_KILLED	1
#define	CLD_DUMPED	2		/* never returned by glidix */
#define	CLD_TRAPPED	3
#define	CLD_STOPPED	4
#define	CLD_CONTINUED	5

#define	FPE_INTDIV	0
#define	FPE_INTOVF	1
#define	FPE_FLTDIV	2
#define	FPE_FLTOVF	3
#define	FPE_FLTUND	4
#define	FPE_FLTRES	5
#define	FPE_FLTINV	6
#define	FPE_FLTSUB	7

#define	BUS_ADRALN	0
#define	BUS_ADRERR	1
#define	BUS_OBJERR	2

#define	ILL_ILLOPC	0
#define	ILL_ILLOPN	1
#define	ILL_ILLADR	2
#define	ILL_ILLTRP	3
#define	ILL_PRVOPC	4
#define	ILL_PRVREG	5
#define	ILL_COPROC	6
#define	ILL_BADSTK	7

/**
 * 'int' is signal-atomic on x86_64 according to Intel spec.
 */
typedef int sig_atomic_t;

union sigval
{
	int		sival_int;
	void*		sival_ptr;
};

typedef struct __siginfo
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

typedef	uint64_t sigset_t;

struct sigaction
{
	void (*sa_handler)(int);
	sigset_t sa_mask;
	int sa_flags;
	void (*sa_sigaction)(int, siginfo_t*, void*);
};

#ifdef __cplusplus
extern "C" {
#endif

/* implemented by the runtime */
int sigaction(int signum, const struct sigaction *act, struct sigaction *oldact);
void (*signal(int sig, void (*func)(int)))(int);
int sigemptyset(sigset_t *set);
int sigfillset(sigset_t *set);
int sigaddset(sigset_t *set, int signo);
int sigdelset(sigset_t *set, int signo);
int sigismember(const sigset_t *set, int signum);
int sigwait(const sigset_t *set, int *sig);
int sigwaitinfo(const sigset_t *set, siginfo_t *info);
int sigtimedwait(const sigset_t *set, siginfo_t *info, const struct timespec *timeout);
int sigsuspend(const sigset_t *mask);

/* implemented by libglidix directly */
int raise(int sig);
int kill(pid_t pid, int sig);
int sigprocmask(int how, const sigset_t *set, sigset_t *oldset);
#define	pthread_sigmask sigprocmask

extern const char *sys_signame[];
extern const char *sys_siglist[];

#ifdef __cplusplus
};	/* extern "C" */
#endif

#endif

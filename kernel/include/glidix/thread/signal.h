/*
	Glidix kernel

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

#ifndef __glidix_signal_h
#define __glidix_signal_h

#include <glidix/util/common.h>
#include <glidix/thread/spinlock.h>

#ifndef _SIGNAL_H
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
#define	SIGTHKILL	35		/* kill a single thread */
#define	SIGTHWAKE	36		/* wake a thread without dispatching a signal */
#define	SIGTRACE	37		/* debugger notification */
#define	SIGTHSUSP	38		/* suspend thread */
#endif
#define	SIG_NUM		39
#ifndef _SIGNAL_H

/**
 * Generic si_codes.
 */
#define	SI_USER		0
#define	SI_QUEUE	1
#define	SI_TIMER	2
#define	SI_ASYNCIO	3
#define	SI_MESGQ	4

/**
 * si_code for SIGSEGV
 */
#define	SEGV_MAPERR	0x1001
#define	SEGV_ACCERR	0x1002

/**
 * si_code for SIGCHLD
 */
#define	CLD_EXITED	0x2001
#define	CLD_KILLED	0x2002
#define	CLD_DUMPED	0x2003		/* never returned by glidix */
#define	CLD_TRAPPED	0x2004
#define	CLD_STOPPED	0x2005
#define	CLD_CONTINUED	0x2006

/**
 * si_code for SIGFPE
 */
#define	FPE_INTDIV	0x3001
#define	FPE_INTOVF	0x3002
#define	FPE_FLTDIV	0x3003
#define	FPE_FLTOVF	0x3004
#define	FPE_FLTUND	0x3005
#define	FPE_FLTRES	0x3006
#define	FPE_FLTINV	0x3007
#define	FPE_FLTSUB	0x3008

/**
 * si_code for SIGBUS
 */
#define	BUS_ADRALN	0x4001
#define	BUS_ADRERR	0x4002
#define	BUS_OBJERR	0x4003

/**
 * si_code for SIGILL
 */
#define	ILL_ILLOPC	0x5001
#define	ILL_ILLOPN	0x5002
#define	ILL_ILLADR	0x5003
#define	ILL_ILLTRP	0x5004
#define	ILL_PRVOPC	0x5005
#define	ILL_PRVREG	0x5006
#define	ILL_COPROC	0x5007
#define	ILL_BADSTK	0x5008

/**
 * si_code for SIGTRAP
 */
#define	TRAP_BRKPT	0x6001
#define	TRAP_TRACE	0x6002

/**
 * si_code for SIGPOLL
 */
#define	POLL_IN		0x7001
#define	POLL_OUT	0x7002
#define	POLL_MSG	0x7003
#define	POLL_ERR	0x7004
#define	POLL_PRI	0x7005
#define	POLL_HUP	0x7006

/**
 * Max number of signals that can be sent at once to a process.
 */
#define	SIGQ_SIZE	16

#define	SIG_DFL		((void (*)(int)) 1)
#define	SIG_ERR		((void (*)(int)) 2)
#define	SIG_HOLD	((void (*)(int)) 3)
#define	SIG_IGN		((void (*)(int)) 4)
#define	SIG_CORE	((void (*)(int)) 5)

#define	SA_NOCLDSTOP	(1 << 0)
#define	SA_NOCLDWAIT	(1 << 1)
#define	SA_NODEFER	(1 << 2)		/* ignored by Glidix currently */
#define	SA_ONSTACK	(1 << 3)		/* ignored by Glidix currently */
#define	SA_RESETHAND	(1 << 4)
#define	SA_RESTART	(1 << 5)		/* ignored by Glidix currently */
#define	SA_SIGINFO	(1 << 6)

/**
 * 'how' values for sigprocmask().
 */
#define	SIG_BLOCK	0
#define	SIG_UNBLOCK	1
#define	SIG_SETMASK	2

/**
 * Flags for sendSignalEx().
 */
#define	SS_NONBLOCKED	(1 << 0)		/* only send if not blocked */

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
#endif			/* _SIGNAL_H */

/**
 * Must match 'struct sigaction' from libc.
 */
typedef struct
{
	void (*sa_handler)(int);
	uint64_t sa_mask;
	int sa_flags;
	void (*sa_sigaction)(int, siginfo_t*, void*);
} SigAction;

/**
 * Must match 'ucontext_t' (plus mcontext_t) from libc.
 * The size must be a multiple of 16 to properly align the signal stack.
 */
typedef struct
{
	void*		link;			/* always NULL */
	uint64_t	sigmask;
	uint64_t	sp;
	size_t		stackSize;
	int		stackFlags;

	/* mcontext_t part */
	uint64_t	rflags;
	uint64_t	rip;
	uint64_t	rdi, rsi, rbp, rbx, rdx, rcx, rax;
	uint64_t	r8, r9, r10, r11, r12, r13, r14, r15;
	uint64_t	rsp;
} SignalContext;

/**
 * Represents a list of signal dispositions of a process. Shared by multiple
 * threads. The list is shared by threads when they also share a memory object.
 */
typedef struct
{
	int		refcount;
	SigAction	actions[SIG_NUM];
} SigDisp;

extern uint64_t sizeSignalStackFrame;
struct _Thread;
void dispatchSignal();
int sendSignal(struct _Thread *thread, siginfo_t *siginfo);	// returns 0 on success, -1 if ignored.
int sendSignalEx(struct _Thread *thread, siginfo_t *siginfo, int flags);
int isUnblockableSig(int sig);
void sigret(void *ret);

SigDisp* sigdispCreate();
void sigdispUpref(SigDisp *sd);
void sigdispDownref(SigDisp *sd);
SigDisp *sigdispExec(SigDisp *old);

#endif

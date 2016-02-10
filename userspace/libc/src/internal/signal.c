/*
	Glidix Runtime

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
#include <string.h>
#include <sys/glidix.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>

static struct sigaction sighandlers[__SIG_COUNT];

static void defSigHandler(siginfo_t *si)
{
	switch (si->si_signo)
	{
	case SIGABRT:
		_Exit(-SIGABRT);
	case SIGALRM:
		exit(-SIGALRM);
	case SIGBUS:
		_Exit(-SIGBUS);
	case SIGCHLD:
		break;
	case SIGCONT:
		break;
	case SIGFPE:
		_Exit(-SIGFPE);
	case SIGHUP:
		_Exit(-SIGHUP);
	case SIGILL:
		_Exit(-SIGILL);
	case SIGINT:
		exit(-SIGINT);
	/* SIGKILL cannot be caught or ignored */
	case SIGPIPE:
		exit(-SIGPIPE);
	case SIGQUIT:
		_Exit(-SIGQUIT);
	case SIGSEGV:
		_Exit(-SIGSEGV);
	/* SIGSTOP cannot be caught or ignored */
	case SIGTERM:
		exit(-SIGTERM);
	case SIGTSTP:
		raise(SIGSTOP);
		break;
	case SIGTTIN:
		raise(SIGSTOP);
		break;
	case SIGTTOU:
		raise(SIGSTOP);
		break;
	case SIGUSR1:
		exit(-SIGUSR1);
	case SIGUSR2:
		exit(-SIGUSR2);
	case SIGPOLL:
		exit(-SIGPOLL);
	case SIGPROF:
		exit(-SIGPROF);
	case SIGSYS:
		_Exit(-SIGSYS);
	case SIGTRAP:
		exit(-SIGTRAP);
	case SIGURG:
		break;
	case SIGVTALRM:
		exit(-SIGVTALRM);
	case SIGXCPU:
		_Exit(-SIGXCPU);
	case SIGXFSZ:
		_Exit(-SIGXFSZ);
	};
};

void __rootSigHandler(void *ret, siginfo_t *si)
{
	struct sigaction *sa = &sighandlers[si->si_signo];
	if (sa->sa_flags & SA_SIGINFO)
	{
		sa->sa_sigaction(si->si_signo, si, NULL);
	}
	else
	{
		if (sa->sa_handler == SIG_DFL)
		{
			defSigHandler(si);
		}
		else if (sa->sa_handler == SIG_IGN)
		{
			// NOP
		}
		else if (sa->sa_handler == SIG_HOLD)
		{
			fprintf(stderr, "libc: a SIG_HOLD was found in a sigaction (unsupported)\n");
			abort();
		}
		else
		{
			sa->sa_handler(si->si_signo);
		};
	};

	_glidix_sigret(ret);
};

void __init_sig()
{
	int i;
	for (i=0; i<__SIG_COUNT; i++)
	{
		sighandlers[i].sa_handler = SIG_DFL;
		sighandlers[i].sa_flags = 0;
	};
	_glidix_sighandler(__rootSigHandler);
};

int sigaction(int signum, const struct sigaction *act, struct sigaction *oldact)
{
	if ((signum < 1) || (signum >= __SIG_COUNT))
	{
		_glidix_seterrno(EINVAL);
		return -1;
	};

	if ((signum == SIGSTOP) || (signum == SIGKILL))
	{
		_glidix_seterrno(EINVAL);
		return -1;
	};

	if (oldact != NULL) memcpy(oldact, &sighandlers[signum], sizeof(struct sigaction));
	if (act != NULL) memcpy(&sighandlers[signum], act, sizeof(struct sigaction));
	return 0;
};

void (*signal(int sig, void (*func)(int)))(int)
{
	struct sigaction old;
	struct sigaction sa;
	sa.sa_handler = func;
	sa.sa_flags = 0;
	int s = sigaction(sig, &sa, &old);
	if (s == -1) return SIG_ERR;
	return old.sa_handler;
};

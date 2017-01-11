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

#include <signal.h>
#include <string.h>
#include <sys/glidix.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <time.h>

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

int sigemptyset(sigset_t *set)
{
	*set = 0UL;
	return 0;
};

int sigfillset(sigset_t *set)
{
	*set = 0xFFFFFFFFFFFFFFFFUL;
	return 0;
};

int sigaddset(sigset_t *set, int signo)
{
	if ((signo < 1) || (signo > __SIG_COUNT))
	{
		errno = EINVAL;
		return -1;
	};
	
	(*set) |= (1UL << signo);
	return 0;
};

int sigdelset(sigset_t *set, int signo)
{
	if ((signo < 1) || (signo > __SIG_COUNT))
	{
		errno = EINVAL;
		return -1;
	};
	
	(*set) &= ~(1UL << signo);
	return 0;
};

int sigismember(const sigset_t *set, int signum)
{
	return !!((*set) & (1UL << signum));
};

int sigwait(const sigset_t *set, int *sig)
{
	siginfo_t si;
	int status = _glidix_sigwait(*set, &si, 0xFFFFFFFFFFFFFFFF);
	*sig = si.si_signo;
	return status;
};

int sigwaitinfo(const sigset_t *set, siginfo_t *info)
{
	siginfo_t temp;
	int status = _glidix_sigwait(*set, &temp, 0xFFFFFFFFFFFFFFFF);
	if (status != 0)
	{
		errno = status;
		return -1;
	};
	
	if (info != NULL) memcpy(info, &temp, sizeof(siginfo_t));
	return temp.si_signo;
};

int sigtimedwait(const sigset_t *set, siginfo_t *info, const struct timespec *timeout)
{
	siginfo_t temp;
	int status = _glidix_sigwait(*set, &temp, (uint64_t)timeout->tv_sec * 1000000000UL + (uint64_t)timeout->tv_nsec);
	if (status != 0)
	{
		errno = status;
		return -1;
	};
	
	if (info != NULL) memcpy(info, &temp, sizeof(siginfo_t));
	return temp.si_signo;
};

int sigsuspend(const sigset_t *set)
{
	return _glidix_sigsuspend(*set);
};

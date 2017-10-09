/*
	Glidix dynamic linker

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

#define _GLIDIX_SOURCE
#include <sys/debug.h>
#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <signal.h>
#include <ucontext.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

void on_sigsegv(int signo, siginfo_t *si, void *ctx)
{
	ucontext_t *ucontext = (ucontext_t*) ctx;
	mcontext_t *mcontext = &ucontext->uc_mcontext;

	struct stack_frame topFrame;
	topFrame.sf_next = (struct stack_frame*) mcontext->rbp;
	topFrame.sf_ip = (void*) mcontext->rip;
	
	printf("Stack trace:\n");
	struct stack_frame *frame;
	for (frame=&topFrame; frame!=NULL; frame=frame->sf_next)
	{
		char *ripinfo;
		char *symbol;
		Dl_AddrInfo info;
		if (dladdrinfo(frame->sf_ip, &info, sizeof(Dl_AddrInfo)) == 0)
		{
			ripinfo = strdup("???");
			symbol = strdup("???");
		}
		else
		{
			ripinfo = __dbg_addr2line(info.dl_path, info.dl_offset);
			symbol = __dbg_getsym(info.dl_path, info.dl_offset);
		};

		printf("<%p> %s() at %s\n", frame->sf_ip, symbol, ripinfo);
		free(ripinfo);
		free(symbol);
	};
	_exit(1);
};

void bar()
{
	*((int*)0) = 0;
};

void foo()
{
	bar();
};

void test()
{
	foo();
};

int main()
{
	struct sigaction sa;
	memset(&sa, 0, sizeof(struct sigaction));
	sa.sa_sigaction = on_sigsegv;
	sa.sa_flags = SA_SIGINFO;
	if (sigaction(SIGSEGV, &sa, NULL) != 0)
	{
		fprintf(stderr, "unwind-test: failed to set action for SIGSEGV: %s\n", strerror(errno));
		return 1;
	};

	test();
	return 0;
};

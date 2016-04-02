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

#include <stdio.h>
#include <signal.h>
#include <errno.h>
#include <string.h>

const char *progName;

void usage()
{
	fprintf(stderr, "USAGE:\t%s [signal] <pid>\n", progName);
	fprintf(stderr, "\tSends a signal to the given pid.\n");
	fprintf(stderr, "\tAvaliable signals: SIGCONT, SIGINT, SIGHUP, SIGKILL,\n");
	fprintf(stderr, "\tSIGQUIT, SIGTERM, SIGSTOP, SIGTSTP, SIGUSR, SIGUSR2.\n");
	fprintf(stderr, "\tSIGTERM is the default signal - if none is specified,\n");
	fprintf(stderr, "\tSIGTERM will be sent.\n");
}
int get_signal_name( const char *signame )
{
	if (strcmp(signame, "SIGCONT") == 0) return SIGCONT;
	if (strcmp(signame, "SIGINT") == 0) return SIGINT;
	if (strcmp(signame, "SIGHUP") == 0) return SIGHUP;
	if (strcmp(signame, "SIGKILL") == 0) return SIGKILL;
	if (strcmp(signame, "SIGQUIT") == 0) return SIGQUIT;
	if (strcmp(signame, "SIGTERM") == 0) return SIGTERM;
	if (strcmp(signame, "SIGSTOP") == 0) return SIGSTOP;
	if (strcmp(signame, "SIGTSTP") == 0) return SIGTSTP;
	if (strcmp(signame, "SIGUSR1") == 0) return SIGUSR1;
	if (strcmp(signame, "SIGUSR2") == 0) return SIGUSR2;
	return -1;
}
int main( int argc, char *argv[] )
{
	progName = argv[0];
	if(argc==2)
	{
		int pid;
		if (sscanf(argv[1], "%d", &pid) != 1)
		{
			usage();
			return 1;
		}
		if (kill(pid, SIGTERM) != 0)
		{
			fprintf(stderr, "%s: error: %s\n", argv[0], strerror(errno));
			return 1;
		}
		else
		{
			return 0;
		}
	}
	if(argc==3)
	{
		int pid;
		int sig;
		sig = get_signal_name( argv[1] );
		if (sig == -1)
		{
			fprintf(stderr, "%s: invalid signal name\n", argv[0]);
			return 1;
		}
		if (sscanf(argv[2], "%d", &pid) != 1)
		{
			usage();
			return 1;
		}
		if (kill(pid, sig) != 0)
		{
			fprintf(stderr, "%s: error: %s\n", argv[0], strerror(errno));
			return 1;
		}
		else
		{
			return 0;
		}
	}	
	else
	{
		usage();
		return 1;
	}
}

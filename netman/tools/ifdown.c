/*
	Glidix Network Manager

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

#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <dlfcn.h>
#include <gxnetman.h>

void ifdown(const char *progName, const char *ifname, const char *lockfile)
{
	FILE *fp = fopen(lockfile, "r+");
	if (fp == NULL) return;
	
	if (lockf(fileno(fp), F_TLOCK, 0) != 0)
	{
		// we acquired the lock, so the daemon is running
		pid_t pid;
		if (fscanf(fp, "%d\n", &pid) != 1)
		{
			fprintf(stderr, "%s: cannot bring down %s: failed to parse %s\n", progName, ifname, lockfile);
		}
		else
		{
			if (kill(pid, SIGINT) != 0)
			{
				fprintf(stderr, "%s: cannot bring down %s: cannot interrupt %d: %s\n",
						progName, ifname, pid, strerror(errno));
			};
		};
	};
	fclose(fp);
};

int main(int argc, char *argv[])
{
	if (argc != 2)
	{
		fprintf(stderr, "USAGE:\t%s <interface-name>\n", argv[0]);
		fprintf(stderr, "\tBrings down an interface.\n");
		return 1;
	};
	
	if (geteuid() != 0)
	{
		fprintf(stderr, "%s: you must be root\n", argv[0]);
		return 1;
	};
	
	// try to lock the interfaces file
	int iffd = open("/etc/if.conf", O_RDWR);
	if (iffd == -1)
	{
		fprintf(stderr, "%s: failed to open /etc/if.conf: %s\n", argv[0], strerror(errno));
		return 1;
	};
	
	if (lockf(iffd, F_TLOCK, 0) != 0)
	{
		fprintf(stderr, "%s: failed to acquire if.conf lock: %s\n", argv[0], strerror(errno));
		return 1;
	};
	
	// try to bring down the interfaces
	char lockfile4[256];
	char lockfile6[256];
	sprintf(lockfile4, "/run/netman/%s/ipv4", argv[1]);
	sprintf(lockfile6, "/run/netman/%s/ipv6", argv[1]);
	
	ifdown(argv[0], argv[1], lockfile4);
	ifdown(argv[0], argv[1], lockfile6);
	
	close(iffd);
	return 0;
};

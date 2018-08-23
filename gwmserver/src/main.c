/*
	Glidix GUI

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

#include <sys/socket.h>
#include <sys/debug.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/un.h>
#include <libddi.h>
#include <libgwm.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <ucontext.h>
#include <dlfcn.h>

#include "screen.h"
#include "server.h"
#include "window.h"
#include "ptr.h"
#include "input.h"
#include "kblayout.h"

int main(int argc, char *argv[])
{
	if (argc < 2)
	{
		fprintf(stderr, "[gwmserver] no launcher specified\n");
		return 1;
	};
	
	char dispdev[1024];
	uint64_t requestRes;
	char linebuf[1024];
	
	if (geteuid() != 0)
	{
		fprintf(stderr, "[gwmserver] you need to be root to start the window manager\n");
		return 1;
	};

	int sockfd = socket(AF_UNIX, SOCK_SEQPACKET, 0);
	if (sockfd == -1)
	{
		fprintf(stderr, "[gwmserver] cannot create socket: %s\n", strerror(errno));
		return 1;
	};
	
	struct sockaddr_un srvaddr;
	srvaddr.sun_family = AF_UNIX;
	strcpy(srvaddr.sun_path, "/run/gwmserver");
	
	if (bind(sockfd, (struct sockaddr*) &srvaddr, sizeof(struct sockaddr_un)) != 0)
	{
		fprintf(stderr, "[gwmserver] cannot bind to /run/gwmserver: %s\n", strerror(errno));
		return 1;
	};
	
	if (listen(sockfd, 5) != 0)
	{
		fprintf(stderr, "[gwmserver] cannot listen: %s\n", strerror(errno));
		return 1;
	};
	
	// make sure the clipboard and shared surface directories actually exist
	mkdir("/run/clipboard", 0777);
	mkdir("/run/shsurf", 0777);
	mkdir("/run/gwmserver-wd", 0777);
	mkdir("/var", 0755);
	mkdir("/var/log", 0755);
	
	chdir("/run/gwmserver-wd");
	
	FILE *fp = fopen("/etc/gwm.conf", "r");
	if (fp == NULL)
	{
		fprintf(stderr, "could not open /etc/gwm.conf: %s\n", strerror(errno));
		return 1;
	};
	
	requestRes = DDI_RES_AUTO;
	dispdev[0] = 0;
	
	char *line;
	int lineno = 0;
	char logdest[PATH_MAX];
	strcpy(logdest, "/var/log/gwmserver.log");
	while ((line = fgets(linebuf, 1024, fp)) != NULL)
	{
		lineno++;
		
		char *endline = strchr(line, '\n');
		if (endline != NULL)
		{
			*endline = 0;
		};
		
		if (strlen(line) >= 1023)
		{
			fprintf(stderr, "/etc/gwm.conf:%d: buffer overflow\n", lineno);
			return 1;
		};
		
		if ((line[0] == 0) || (line[0] == '#'))
		{
			continue;
		}
		else
		{
			char *cmd = strtok(line, " \t");
			if (cmd == NULL)
			{
				continue;
			};
			
			if (strcmp(cmd, "display") == 0)
			{
				char *name = strtok(NULL, " \t");
				if (name == NULL)
				{
					fprintf(stderr, "/etc/gwm.conf:%d: 'display' needs a parameter\n", lineno);
					return 1;
				};
				
				strcpy(dispdev, name);
			}
			else if (strcmp(cmd, "resolution") == 0)
			{
				char *res = strtok(NULL, " \t");
				if (res == NULL)
				{
					fprintf(stderr, "/etc/gwm.conf:%d: 'resolution' needs a parameter\n", lineno);
					return 1;
				};
				
				uint64_t reqWidth, reqHeight;
				if (strcmp(res, "auto") == 0)
				{
					requestRes = DDI_RES_AUTO;
				}
				else if (strcmp(res, "safe") == 0)
				{
					requestRes = DDI_RES_SAFE;
				}
				else if (sscanf(res, "%lux%lu", &reqWidth, &reqHeight) == 2)
				{
					requestRes = DDI_RES_SPECIFIC(reqWidth, reqHeight);
				}
				else
				{
					fprintf(stderr, "/etc/gwm.conf:%d: invalid resolution: %s\n", lineno, res);
					return 1;
				};
			}
			else if (strcmp(cmd, "log") == 0)
			{
				char *path = strtok(NULL, " \t");
				if (path == NULL)
				{
					fprintf(stderr, "/etc/gwm.conf:%d: 'log' needs a parameter\n", lineno);
					return 1;
				};
				
				if (strlen(path) >= PATH_MAX)
				{
					fprintf(stderr, "/etc/gwm.conf:%d: log path too long\n", lineno);
					return 1;
				};
				
				strcpy(logdest, path);
			}
			else
			{
				fprintf(stderr, "/etc/gwm.conf:%d: invalid directive: %s\n", lineno, cmd);
				return 1;
			};
		};
	};
	fclose(fp);
	
	int logfd = open(logdest, O_WRONLY | O_CREAT | O_APPEND, 0644);
	if (logfd == -1)
	{
		fprintf(stderr, "[gwmserver] could not open log file %s: %s\n", logdest, strerror(errno));
		return 1;
	};
	
	dup2(logfd, 1);
	dup2(logfd, 2);
	close(logfd);
	
	if (dispdev[0] == 0)
	{
		fprintf(stderr, "/etc/gwm.conf: no display device specified!\n");
		return 1;
	};

	if (ddiInit(dispdev, O_RDWR) != 0)
	{
		fprintf(stderr, "ddiInit: %s: %s\n", dispdev, strerror(errno));
		return 1;
	};
	
	if (symlink(dispdev, "/run/gwmdisp") != 0)
	{
		fprintf(stderr, "symlink /run/gwmdisp: %s\n", strerror(errno));
		return 1;
	};

	frontBuffer = ddiSetVideoMode(requestRes);
	screen = ddiCreateSurface(&frontBuffer->format, frontBuffer->width, frontBuffer->height, NULL, 0);
	
	// GWM information
	int fd = open("/run/gwminfo", O_RDWR | O_CREAT | O_TRUNC, 0644);
	if (fd == -1)
	{
		fprintf(stderr, "Failed to open /run/gwminfo! %s\n", strerror(errno));
		return 1;
	};

	DDIColor backgroundColor = {0, 0, 0x77, 0xFF};
	desktopBackground = ddiCreateSurface(&screen->format, screen->width, screen->height, NULL, DDI_SHARED);
	ddiFillRect(desktopBackground, 0, 0, screen->width, screen->height, &backgroundColor);
	ddiFillRect(screen, 0, 0, screen->width, screen->height, &backgroundColor);
	ddiFillRect(frontBuffer, 0, 0, screen->width, screen->height, &backgroundColor);
	
	ftruncate(fd, sizeof(GWMInfo));
	gwminfo = (GWMInfo*) mmap(NULL, sizeof(GWMInfo), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	gwminfo->backgroundID = desktopBackground->id;

	printf("[gwmserver] initialize theme info\n");
	gwmGlobalThemeInit(&frontBuffer->format);
	
	imgWincap = ddiOpenSurface(gwminfo->imgWinCap);
	imgWinbut = ddiOpenSurface(gwminfo->imgWinButtons);
	
	if (fork() == 0)
	{
		execv(argv[1], &argv[1]);
		perror("exec launcher");
		_exit(1);
	};
	
	// now that we won't be starting any processes, we can set core dumps
	signal(SIGSEGV, SIG_CORE);
	signal(SIGILL, SIG_CORE);
	signal(SIGBUS, SIG_CORE);
	signal(SIGABRT, SIG_CORE);
	signal(SIGALRM, SIG_CORE);
	
	kblSet("/usr/share/kblayout/en_US/int", stderr);
	wndInit();
	ptrInit();
	inputInit();
	runServer(sockfd);
	return 0;
};

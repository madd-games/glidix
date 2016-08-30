/*
	Glidix GUI

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
#include <libgwm.h>
#include <time.h>
#include <sys/time.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>

/**
 * The system bar at the bottom of the screen.
 */

DDISurface *imgSysBar;

void on_signal(int sig, siginfo_t *si, void *ignore)
{
	if (sig == SIGCHLD)
	{
		waitpid(si->si_pid, NULL, 0);
	};
};

int sysbarEventHandler(GWMEvent *ev, GWMWindow *win)
{
	switch (ev->type)
	{
	case GWM_EVENT_DOWN:
		if (ev->scancode == GWM_SC_MOUSE_LEFT)
		{
			if (ev->x < 40)
			{
				if (fork() == 0)
				{
					execl("/usr/bin/terminal", "terminal", NULL);
					perror("exec terminal");
					exit(1);
				};
			};
		};
		return 0;
	default:
		return gwmDefaultHandler(ev, win);
	};
};

int main()
{
	struct sigaction sa;
	sa.sa_sigaction = on_signal;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_SIGINFO;
	if (sigaction(SIGCHLD, &sa, NULL) != 0)
	{
		perror("sigaction SIGCHLD");
		return 1;
	};

	if (gwmInit() != 0)
	{
		fprintf(stderr, "Failed to initialize GWM!\n");
		return 1;
	};

	int width, height;
	gwmScreenSize(&width, &height);
	
	GWMWindow *win = gwmCreateWindow(NULL, "sysbar", 0, height-40, width, 40,
						GWM_WINDOW_MKFOCUSED | GWM_WINDOW_NODECORATE);
	
	if (win == NULL)
	{
		fprintf(stderr, "Failed to create window!\n");
		return 1;
	};
	
	DDISurface *canvas = gwmGetWindowCanvas(win);
	
	const char *error;
	imgSysBar = ddiLoadAndConvertPNG(&canvas->format, "/usr/share/images/sysbar.png", &error);
	if (imgSysBar == NULL)
	{
		fprintf(stderr, "Failed to load sysbar pattern: %s\n", error);
		return 1;
	};
	
	int i;
	for (i=0; i<width; i++)
	{
		ddiBlit(imgSysBar, 0, 0, canvas, i, 0, 1, 40);
	};
	
	DDISurface *menuButton = ddiLoadAndConvertPNG(&canvas->format, "/usr/share/images/sysmenubutton.png", &error);
	if (menuButton == NULL)
	{
		fprintf(stderr, "Failed to load menu button image: %s\n", error);
		return 1;
	};
	
	ddiBlit(menuButton, 0, 0, canvas, 2, 2, 32, 32);

	gwmPostDirty(win);
	
	gwmSetEventHandler(win, sysbarEventHandler);
	gwmMainLoop();
	gwmQuit();
	return 0;
};

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
DDISurface *menuButton;
DDISurface *imgIconbar;
DDISurface *imgBackground;
GWMWindow *win;
int currentMouseX=0, currentMouseY=0;
int lastSelectRow=-1;
int lastSelectColumn=-1;
int pressed = 0;
GWMGlobWinRef windows[128];
int numWindows = 0;

void on_signal(int sig, siginfo_t *si, void *ignore)
{
	if (sig == SIGCHLD)
	{
		waitpid(si->si_pid, NULL, 0);
	};
};

void sysbarRedraw()
{
	DDISurface *canvas = gwmGetWindowCanvas(win);
	ddiBlit(imgBackground, 0, 0, canvas, 0, 0, canvas->width, canvas->height);
	
	GWMGlobWinRef focwin;
	GWMGlobWinRef winlist[128];
	int count = gwmGetDesktopWindows(&focwin, winlist);
	int displayIndex = 0;
	
	int selectedColumn = (currentMouseX-40)/150;
	int selectedRow = currentMouseY/20;
	
	int i;
	for (i=0; i<count; i++)
	{
		GWMWindowParams params;
		if (gwmGetGlobWinParams(&winlist[i], &params) == 0)
		{
			if ((params.flags & GWM_WINDOW_NOTASKBAR) == 0)
			{
				int spriteIndex = 0;

				int row = displayIndex & 1;
				int column = displayIndex >> 1;
				memcpy(&windows[displayIndex], &winlist[i], sizeof(GWMGlobWinRef));
				displayIndex++;
				
				if ((row == selectedRow) && (column == selectedColumn))
				{
					spriteIndex = 1;
					
					if (pressed)
					{
						spriteIndex = 2;
					};
				};
				
				if ((winlist[i].pid == focwin.pid) && (winlist[i].fd == focwin.fd)
					&& (winlist[i].id == focwin.id))
				{
					spriteIndex += 3;
				};
		
				ddiBlit(imgIconbar, 0, 20*spriteIndex, canvas, 40+150*column, 20*row, 150, 20);

				DDIPen *pen = ddiCreatePen(&canvas->format, gwmGetDefaultFont(), 45+150*column, 20*row+2,
								143, 18, 0, 0, NULL);
				if (pen != NULL)
				{
					char name[258];
					if (params.flags & GWM_WINDOW_HIDDEN)
					{
						sprintf(name, "[%s]", params.iconName);
					}
					else
					{
						strcpy(name, params.iconName);
					};
					
					ddiSetPenWrap(pen, 0);
					ddiWritePen(pen, name);
					ddiExecutePen(pen, canvas);
					ddiDeletePen(pen);
				};
			};
		};
	};
	
	numWindows = displayIndex;
	ddiBlit(menuButton, 0, 0, canvas, 2, 2, 32, 32);
	gwmPostDirty(win);
};

void triggerWindow(int index)
{
	if ((index < 0) || (index >= numWindows))
	{
		return;
	};
	
	gwmToggleWindow(&windows[index]);
};

int sysbarEventHandler(GWMEvent *ev, GWMWindow *win)
{
	int newSelectRow, newSelectColumn;
	int winIndex;
	
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

		if (ev->scancode == GWM_SC_MOUSE_LEFT)
		{
			pressed = 1;
			sysbarRedraw();
		};

		return 0;
	case GWM_EVENT_DESKTOP_UPDATE:
		sysbarRedraw();
		return 0;
	case GWM_EVENT_ENTER:
	case GWM_EVENT_MOTION:
		currentMouseX = ev->x;
		currentMouseY = ev->y;
		newSelectColumn = (ev->x-40)/150;
		newSelectRow = ev->y/20;
		if ((newSelectColumn != lastSelectColumn) || (newSelectRow != lastSelectRow))
		{
			lastSelectColumn = newSelectColumn;
			lastSelectRow = newSelectRow;
			sysbarRedraw();
		};
		return 0;
	case GWM_EVENT_LEAVE:
		currentMouseX = 0;
		currentMouseY = 0;
		lastSelectColumn = -1;
		lastSelectRow = -1;
		sysbarRedraw();
		return 0;
	case GWM_EVENT_UP:
		if (ev->scancode == GWM_SC_MOUSE_LEFT)
		{
			pressed = 0;
			winIndex = 2 * lastSelectColumn + lastSelectRow;
			triggerWindow(winIndex);
			sysbarRedraw();
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
	
	win = gwmCreateWindow(NULL, "sysbar", 0, height-40, width, 40,
				GWM_WINDOW_MKFOCUSED | GWM_WINDOW_NODECORATE | GWM_WINDOW_NOTASKBAR);
	
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
	
	menuButton = ddiLoadAndConvertPNG(&canvas->format, "/usr/share/images/sysmenubutton.png", &error);
	if (menuButton == NULL)
	{
		fprintf(stderr, "Failed to load menu button image: %s\n", error);
		return 1;
	};
	
	imgIconbar = ddiLoadAndConvertPNG(&canvas->format, "/usr/share/images/iconbar.png", &error);
	if (imgIconbar == NULL)
	{
		fprintf(stderr, "Failed to load icon bar image: %s\n", error);
		return 1;
	};

	imgBackground = ddiCreateSurface(&canvas->format, width, 40, NULL, 0);
	int i;
	for (i=0; i<canvas->width; i++)
	{
		ddiBlit(imgSysBar, 0, 0, imgBackground, i, 0, 1, 40);
	};

	sysbarRedraw();
	
	gwmSetListenWindow(win); 
	gwmSetEventHandler(win, sysbarEventHandler);
	gwmMainLoop();
	gwmQuit();
	return 0;
};

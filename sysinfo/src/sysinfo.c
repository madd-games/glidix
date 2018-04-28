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

#if 0
#include <stdio.h>
#include <libgwm.h>
#include <time.h>
#include <sys/time.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <dirent.h>
#include <pthread.h>
#include <poll.h>

void initPCITab(GWMWindow *notebook);

int main()
{
	if (gwmInit() != 0)
	{
		fprintf(stderr, "gwmInit() failed!\n");
		return 1;
	};
	
	GWMWindow *win = gwmCreateWindow(NULL, "System Information", GWM_POS_UNSPEC, GWM_POS_UNSPEC,
						500, 300, GWM_WINDOW_NOTASKBAR | GWM_WINDOW_HIDDEN);
	
	DDISurface *canvas = gwmGetWindowCanvas(win);
	DDISurface *icon = ddiLoadAndConvertPNG(&canvas->format, "/usr/share/images/sysinfo.png", NULL);
	if (icon != NULL)
	{
		gwmSetWindowIcon(win, icon);
		ddiDeleteSurface(icon);
	};
	
	GWMWindow *notebook = gwmCreateNotebook(win, 0, 0, 500, 300, 0);
	initPCITab(notebook);
	
	gwmNotebookSetTab(notebook, 0);
	gwmSetWindowFlags(win, GWM_WINDOW_MKFOCUSED);
	gwmMainLoop();
	gwmQuit();
	return 0;
};
#endif

int main()
{
	return 0;
};

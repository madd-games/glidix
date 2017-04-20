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
#include <errno.h>

#define	DEFAULT_WIDTH			500
#define	DEFAULT_HEIGHT			500

GWMWindow *topWindow;
GWMWindow *textArea;

int eventHandler(GWMEvent *ev, GWMWindow *win)
{
	switch (ev->type)
	{
	case GWM_EVENT_CLOSE:
		return -1;
	default:
		return gwmDefaultHandler(ev, win);
	};
};

int main(int argc, char *argv[])
{
	if (gwmInit() != 0)
	{
		fprintf(stderr, "Failed to initialize GWM!\n");
		return 1;
	};
	
	topWindow = gwmCreateWindow(NULL, "Untitled - Minipad", GWM_POS_UNSPEC, GWM_POS_UNSPEC,
						DEFAULT_WIDTH, DEFAULT_HEIGHT, GWM_WINDOW_NOTASKBAR | GWM_WINDOW_HIDDEN);
	DDISurface *canvas = gwmGetWindowCanvas(topWindow);
	DDISurface *icon = ddiLoadAndConvertPNG(&canvas->format, "/usr/share/images/minipad.png", NULL);
	if (icon != NULL)
	{
		gwmSetWindowIcon(topWindow, icon);
		ddiDeleteSurface(icon);
	};
	
	DDIFont *font = ddiLoadFont("DejaVu Sans Mono", 14, 0, NULL);
	textArea = gwmCreateTextArea(topWindow, 0, 20, DEFAULT_WIDTH, DEFAULT_HEIGHT-20, 0);
	if (font == NULL)
	{
		fprintf(stderr, "Failed to load font!\n");
	}
	else
	{
		gwmSetTextAreaStyle(textArea, font, NULL, NULL, NULL);
	};
	
	gwmSetWindowFlags(topWindow, 0);
	gwmSetWindowFlags(textArea, GWM_WINDOW_MKFOCUSED);
	gwmSetEventHandler(topWindow, eventHandler);
	gwmMainLoop();
	gwmQuit();
	return 0;
};

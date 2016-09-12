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

#include <sys/glidix.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libddi.h>
#include <pthread.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include "gui.h"

GWMWindow *win;

void redraw()
{
	DDISurface *canvas = gwmGetWindowCanvas(win);

	DDIPen *pen = ddiCreatePen(&canvas->format, gwmGetDefaultFont(), 0, 0, canvas->width, canvas->height, 0, 0, NULL);
	ddiWritePen(pen, "Tick me");
	
	int txtWidth, txtHeight;
	ddiGetPenSize(pen, &txtWidth, &txtHeight);
	ddiSetPenPosition(pen, 126, 17-(txtHeight/2));
	ddiExecutePen(pen, canvas);
	ddiDeletePen(pen);

	gwmPostDirty(win);
};

int myEventHandler(GWMEvent *ev, GWMWindow *win)
{
	switch (ev->type)
	{
	case GWM_EVENT_CLOSE:
		return -1;
	case GWM_EVENT_RESIZE_REQUEST:
		gwmMoveWindow(win, ev->x, ev->y);
		gwmResizeWindow(win, ev->width, ev->height);
		redraw();
		return 0;
	default:
		return gwmDefaultHandler(ev, win);
	};
};

int sbarCallback(void *context)
{
	GWMWindow *sbar = (GWMWindow*) context;
	printf("You moved a scrollbar to: %d\n", gwmGetScrollbarOffset(sbar));
	return 0;
};

int main()
{
	if (gwmInit() != 0)
	{
		fprintf(stderr, "Failed to initialize GWM!\n");
		return 1;
	};

	win = gwmCreateWindow(NULL, "Widgets Test", 10, 10, 500, 500, GWM_WINDOW_MKFOCUSED | GWM_WINDOW_RESIZEABLE);
	if (win == NULL)
	{
		fprintf(stderr, "Failed to create window!\n");
		return 1;
	};

	gwmCreateButton(win, "Sample button", 2, 2, 100, 0);
	gwmCreateButton(win, "Disabled button", 2, 34, 100, GWM_BUTTON_DISABLED);
	gwmCreateTextField(win, "Type here :)", 2, 66, 100, 0);
	gwmCreateTextField(win, "Disabled text field", 2, 88, 100, GWM_TXT_DISABLED);
	gwmCreateTextField(win, "Masked", 2, 110, 100, GWM_TXT_MASKED);
	gwmCreateTextField(win, "Disabled, masked", 2, 132, 100, GWM_TXT_MASKED | GWM_TXT_DISABLED);
	
	gwmCreateCheckbox(win, 104, 7, GWM_CB_TRI, 0);
	
	int i;
	for (i=0; i<3; i++)
	{
		gwmCreateCheckbox(win, 104+32*i, 39, i, GWM_CB_DISABLED);
	};
	
	GWMWindow *sbar;
	sbar = gwmCreateScrollbar(win, 104, 61, 100, 0, 66, 500, GWM_SCROLLBAR_HORIZ);
	gwmSetScrollbarCallback(sbar, sbarCallback, sbar);
	sbar = gwmCreateScrollbar(win, 104, 71, 100, 50, 66, 500, GWM_SCROLLBAR_HORIZ | GWM_SCROLLBAR_DISABLED);
	gwmSetScrollbarCallback(sbar, sbarCallback, sbar);
	
	sbar = gwmCreateScrollbar(win, 206, 7, 100, 20, 100, 500, 0);
	gwmSetScrollbarCallback(sbar, sbarCallback, sbar);
	sbar = gwmCreateScrollbar(win, 216, 7, 100, 10, 100, 500, GWM_SCROLLBAR_DISABLED);
	gwmSetScrollbarCallback(sbar, sbarCallback, sbar);
	
	redraw();
	gwmSetEventHandler(win, myEventHandler);
	gwmMainLoop();
	gwmQuit();
	return 0;
};

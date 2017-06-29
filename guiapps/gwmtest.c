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

#include <sys/glidix.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <libgwm.h>

int handler(GWMEvent *ev, GWMWindow *win)
{
	switch (ev->type)
	{
	case GWM_EVENT_FOCUS_IN:
		printf("FOCUS_IN\n");
		return 0;
	case GWM_EVENT_FOCUS_OUT:
		printf("FOCUS_OUT\n");
		return 0;
	case GWM_EVENT_ENTER:
		printf("ENTER: (%d, %d)\n", ev->x, ev->y);
		return 0;
	case GWM_EVENT_MOTION:
		printf("MOTION: (%d, %d)\n", ev->x, ev->y);
		return 0;
	case GWM_EVENT_LEAVE:
		printf("LEAVE\n");
		return 0;
	case GWM_EVENT_DOWN:
		printf("DOWN (scancode=%d, keycode=0x%03X, keychar=%c, keymod=%d)\n", ev->scancode, ev->keycode, (char) ev->keychar, ev->keymod);
		return 0;
	case GWM_EVENT_UP:
		printf("UP (scancode=%d, keychar=%c, keycode=0x%03X, keymod=%d)\n", ev->scancode, ev->keycode, (char) ev->keychar, ev->keymod);
		return 0;
	default:
		return gwmDefaultHandler(ev, win);
	};
};

int main()
{
	if (gwmInit() != 0)
	{
		fprintf(stderr, "Failed to initialize GWM!\n");
		return 1;
	};
	
	printf("create window.\n");
	GWMWindow *win = gwmCreateWindow(NULL, "Le test", GWM_POS_UNSPEC, GWM_POS_UNSPEC, 250, 250, GWM_WINDOW_MKFOCUSED);
	if (win == NULL)
	{
		fprintf(stderr, "Failed to create window!\n");
		return 1;
	};
	
	printf("set cursor.\n");
	gwmSetWindowCursor(win, GWM_CURSOR_HAND);
	
	printf("draw stuff.\n");
	DDISurface *canvas = gwmGetWindowCanvas(win);
	DDIColor color = {0xFF, 0x00, 0x00, 0xFF};
	ddiFillRect(canvas, 10, 10, 100, 100, &color);
	
	printf("set event handler.\n");
	gwmSetEventHandler(win, handler);
	printf("post dirty.\n");
	gwmPostDirty(win);
	printf("main loop:\n");
	gwmMainLoop();
	
	gwmQuit();
	return 0;
};

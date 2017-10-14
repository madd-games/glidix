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

/**
 * This program must have setuid root. It allows a normal GUI user to shut down the system
 * or reboot.
 *
 * Possible TODO: permission control for who can shut down?
 */
 
#include <stdio.h>
#include <unistd.h>
#include <libgwm.h>
#include <libddi.h>
#include <string.h>
#include <errno.h>

#define	BOX_WIDTH			320
#define	BOX_HEIGHT			200

#define	REBOOT_X1			74
#define	REBOOT_Y1			68
#define	REBOOT_X2			138
#define	REBOOT_Y2			132

#define	SHUTDOWN_X1			180
#define	SHUTDOWN_Y1			68
#define	SHUTDOWN_X2			244
#define	SHUTDOWN_Y2			132

int boxX, boxY;

void doShutdown(const char *how)
{
	execl("/usr/bin/halt", how, NULL);
	
	char message[512];
	sprintf(message, "Failed to execute 'halt': %s", strerror(errno));
	
	gwmMessageBox(NULL, "Shutdown failed", message, GWM_MBUT_OK | GWM_MBICON_ERROR);
};

int shutdownHandler(GWMEvent *ev, GWMWindow *win, void *context)
{
	switch (ev->type)
	{
	case GWM_EVENT_CLOSE:
		return GWM_EVSTATUS_BREAK;
	case GWM_EVENT_DOWN:
		if (ev->keycode == GWM_KC_MOUSE_LEFT)
		{
			if (ev->x >= (boxX+REBOOT_X1) && ev->x < (boxX+REBOOT_X2)
				&& ev->y >= (boxY+REBOOT_Y1) && ev->y < (boxY+REBOOT_Y2))
			{
				doShutdown("reboot");
				return 0;
			};
			
			if (ev->x >= (boxX+SHUTDOWN_X1) && ev->x < (boxX+SHUTDOWN_X2)
				&& ev->y >= (boxY+SHUTDOWN_Y1) && ev->y < (boxY+SHUTDOWN_Y2))
			{
				doShutdown("poweroff");
				return 0;
			};
			
			// cancel if clicked outside the box
			if (ev->x < boxX || ev->x >= (boxX+BOX_WIDTH) || ev->y < boxY || ev->y >= (boxY+BOX_HEIGHT))
			{
				return -1;
			};
		};
		return GWM_EVSTATUS_OK;
	default:
		return GWM_EVSTATUS_CONT;
	};
	
};

int main(int argc, char *argv[])
{
	// make sure we are root
	if (geteuid() != 0)
	{
		fprintf(stderr, "%s: must be run as root (should have setuid root!)\n", argv[0]);
		return 1;
	};
	
	// set real user to root
	setuid(0);
	setgid(0);
	
	if (gwmInit() != 0)
	{
		fprintf(stderr, "%s: failed to initialize GWM\n", argv[0]);
		return 1;
	};
	
	int width, height;
	gwmScreenSize(&width, &height);
	
	GWMWindow *win = gwmCreateWindow(NULL, argv[0], 0, 0, width, height,
		GWM_WINDOW_HIDDEN | GWM_WINDOW_NODECORATE | GWM_WINDOW_NOTASKBAR);
	if (win == NULL)
	{
		fprintf(stderr, "%s: window creation failed\n", argv[0]);
		gwmQuit();
		return 1;
	};
	
	DDISurface *canvas = gwmGetWindowCanvas(win);
	DDISurface *box = (DDISurface*) gwmGetThemeProp("gwm.tools.shutdown", GWM_TYPE_SURFACE, NULL);
	if (box == NULL)
	{
		fprintf(stderr, "%s: failed to get surface gwm.tools.shutdown!\n", argv[0]);
		gwmQuit();
		return 1;
	};
	
	DDIColor background = {0x00, 0x00, 0x00, 0x7F};
	ddiFillRect(canvas, 0, 0, width, height, &background);

	boxX = (width - BOX_WIDTH) / 2;
	boxY = (height - BOX_HEIGHT) / 2;
	
	ddiBlit(box, 0, 0, canvas, boxX, boxY, BOX_WIDTH, BOX_HEIGHT);	
	
	gwmPushEventHandler(win, shutdownHandler, NULL);
	gwmPostDirty(win);
	gwmSetWindowFlags(win, GWM_WINDOW_NODECORATE | GWM_WINDOW_NOTASKBAR | GWM_WINDOW_MKFOCUSED);
	
	gwmMainLoop();
	gwmQuit();
	return 0;
};

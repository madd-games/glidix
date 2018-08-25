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

#include <libgwm.h>
#include <libddi.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

static void redrawProgressBar(GWMWindow *progbar)
{
	DDISurface *canvas = gwmGetWindowCanvas(progbar);
	float value = gwmGetScaleValue(progbar);
	
	if (canvas->width < 2 || canvas->height < 2) return;
	
	static DDIColor black = {0, 0, 0, 0xFF};
	static DDIColor white = {0xFF, 0xFF, 0xFF, 0xFF};
	
	GWMInfo *info = gwmGetInfo();
	
	ddiFillRect(canvas, 0, 0, canvas->width, 1, &black);
	ddiFillRect(canvas, 0, 0, 1, canvas->height, &black);
	ddiFillRect(canvas, canvas->width-1, 0, 1, canvas->height, &white);
	ddiFillRect(canvas, 1, canvas->height-1, canvas->width-1, 1, &white);
	ddiFillRect(canvas, 1, 1, canvas->width-2, canvas->height-2, &info->colProgressBackground);
	
	int progWidth = (canvas->width-2) * value;
	
	int x;
	for (x=1; x<(1+progWidth); x++)
	{
		float fac = (float) x / (float) canvas->width;
		DDIColor color;
		ddiSampleLinearGradient(&color, fac, &info->colProgressLeft, &info->colProgressRight);
		ddiFillRect(canvas, x, 1, 1, canvas->height-2, &color);
	};
	
	gwmPostDirty(progbar);
};

static int progbarHandler(GWMEvent *ev, GWMWindow *progbar, void *context)
{
	switch (ev->type)
	{
	case GWM_EVENT_RETHEME:
		redrawProgressBar(progbar);
		return GWM_EVSTATUS_OK;
	case GWM_EVENT_VALUE_CHANGED:
		redrawProgressBar(progbar);
		return GWM_EVSTATUS_CONT;
	default:
		return GWM_EVSTATUS_CONT;
	};
};

static void minsizeProgressBar(GWMWindow *progbar, int *width, int *height)
{
	*width = 0;
	*height = 20;
};

static void prefsizeProgressBar(GWMWindow *progbar, int *width, int *height)
{
	*width = 100;
	*height = 20;
};

static void positionProgressBar(GWMWindow *progbar, int x, int y, int width, int height)
{
	gwmMoveWindow(progbar, x, y);
	gwmResizeWindow(progbar, width, height);
	redrawProgressBar(progbar);
};

GWMWindow* gwmNewProgressBar(GWMWindow *parent)
{
	GWMWindow *progbar = gwmNewScale(parent);
	
	progbar->getMinSize = minsizeProgressBar;
	progbar->getPrefSize = prefsizeProgressBar;
	progbar->position = positionProgressBar;
	
	gwmPushEventHandler(progbar, progbarHandler, NULL);
	return progbar;
};

void gwmDestroyProgressBar(GWMWindow *progbar)
{
	gwmDestroyScale(progbar);
};

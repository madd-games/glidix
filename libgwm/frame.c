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
#include <assert.h>

typedef struct
{
	char*				caption;
	GWMWindow*			panel;
} FrameData;

static DDIColor *colFrame;

static void redrawFrame(GWMWindow *frame);

static int frameHandler(GWMEvent *ev, GWMWindow *frame, void *context)
{
	switch (ev->type)
	{
	case GWM_EVENT_RETHEME:
		redrawFrame(frame);
		return GWM_EVSTATUS_OK;
	default:
		return GWM_EVSTATUS_CONT;
	};
};

static void redrawFrame(GWMWindow *frame)
{
	FrameData *data = (FrameData*) gwmGetData(frame, frameHandler);
	DDISurface *canvas = gwmGetWindowCanvas(frame);
	
	if (colFrame == NULL)
	{
		colFrame = (DDIColor*) gwmGetThemeProp("gwm.toolkit.frame", GWM_TYPE_COLOR, NULL);
		assert(colFrame != NULL);
	};
	
	ddiFillRect(canvas, 0, 0, canvas->width, canvas->height, GWM_COLOR_BACKGROUND);
	ddiFillRect(canvas, 2, 10, canvas->width-4, 1, colFrame);
	ddiFillRect(canvas, 2, 10, 1, canvas->height-13, colFrame);
	ddiFillRect(canvas, canvas->width-3, 10, 1, canvas->height-13, colFrame);
	ddiFillRect(canvas, 2, canvas->height-3, canvas->width-4, 1, colFrame);
	
	DDIPen *pen = ddiCreatePen(&canvas->format, gwmGetDefaultFont(), 15, 2, canvas->width-19, 20, 0, 0, NULL);
	if (pen != NULL)
	{
		ddiSetPenAlignment(pen, DDI_ALIGN_CENTER);
		ddiSetPenBackground(pen, GWM_COLOR_BACKGROUND);
		ddiSetPenColor(pen, colFrame);
		ddiSetPenWrap(pen, 0);
		ddiWritePen(pen, data->caption);
		ddiExecutePen(pen, canvas);
		ddiDeletePen(pen);
	};
	
	gwmPostDirty(frame);
};

static void frameMinSize(GWMWindow *frame, int *width, int *height)
{
	FrameData *data = (FrameData*) gwmGetData(frame, frameHandler);
	
	if (data->panel->layout == NULL)
	{
		*width = 8;
		*height = 24;
	}
	else
	{
		data->panel->layout->getMinSize(data->panel->layout, width, height);
		*width += 8;
		*height += 24;
	};
};

static void framePrefSize(GWMWindow *frame, int *width, int *height)
{
	FrameData *data = (FrameData*) gwmGetData(frame, frameHandler);
	
	if (data->panel->layout == NULL)
	{
		*width = 8;
		*height = 24;
	}
	else
	{
		data->panel->layout->getPrefSize(data->panel->layout, width, height);
		*width += 8;
		*height += 24;
	};
};

static void framePosition(GWMWindow *frame, int x, int y, int width, int height)
{
	gwmMoveWindow(frame, x, y);
	gwmResizeWindow(frame, width, height);
	redrawFrame(frame);
	
	FrameData *data = (FrameData*) gwmGetData(frame, frameHandler);
	gwmMoveWindow(data->panel, 4, 20);
	gwmLayout(data->panel, width-8, height-24);
};

GWMWindow *gwmCreateFrame(GWMWindow *parent, const char *caption, int x, int y, int width, int height)
{
	GWMWindow *frame = gwmCreateWindow(parent, caption, x, y, width, height, 0);
	if (frame == NULL) return NULL;
	
	FrameData *data = (FrameData*) malloc(sizeof(FrameData));
	gwmPushEventHandler(frame, frameHandler, data);

	data->caption = strdup(caption);
	data->panel = gwmCreateWindow(frame, "FramePanel", 4, 20, width-8, height-24, 0);
	assert(data->panel != NULL);
	
	frame->getMinSize = frameMinSize;
	frame->getPrefSize = framePrefSize;
	frame->position = framePosition;
	
	redrawFrame(frame);
	return frame;
};

GWMWindow* gwmNewFrame(GWMWindow *parent)
{
	return gwmCreateFrame(parent, "", 0, 0, 0, 0);
};

void gwmSetFrameCaption(GWMWindow *frame, const char *caption)
{
	FrameData *data = (FrameData*) gwmGetData(frame, frameHandler);
	free(data->caption);
	data->caption = strdup(caption);
	
	redrawFrame(frame);
};

void gwmDestroyFrame(GWMWindow *frame)
{
	FrameData *data = (FrameData*) gwmGetData(frame, frameHandler);
	gwmDestroyWindow(data->panel);
	free(data->caption);
	free(data);
	gwmDestroyWindow(frame);
};

GWMWindow* gwmGetFramePanel(GWMWindow *frame)
{
	FrameData *data = (FrameData*) gwmGetData(frame, frameHandler);
	return data->panel;
};

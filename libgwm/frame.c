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

typedef struct
{
	char*				caption;
	GWMWindow*			panel;
} FrameData;

static void redrawFrame(GWMWindow *frame)
{
	FrameData *data = (FrameData*) frame->data;
	DDISurface *canvas = gwmGetWindowCanvas(frame);
	
	static DDIColor frameColor = {0x55, 0x55, 0xFF, 0xFF};
	static DDIColor backColor = {0xDD, 0xDD, 0xDD, 0xFF};
	
	ddiFillRect(canvas, 0, 0, canvas->width, canvas->height, &backColor);
	ddiFillRect(canvas, 0, 10, canvas->width, 1, &frameColor);
	ddiFillRect(canvas, 0, 10, 1, canvas->height-10, &frameColor);
	ddiFillRect(canvas, canvas->width-1, 10, 1, canvas->height-10, &frameColor);
	ddiFillRect(canvas, 0, canvas->height-1, canvas->width, 1, &frameColor);
	
	DDIPen *pen = ddiCreatePen(&canvas->format, gwmGetDefaultFont(), 15, 2, canvas->width-19, 20, 0, 0, NULL);
	if (pen != NULL)
	{
		ddiSetPenBackground(pen, &backColor);
		ddiSetPenColor(pen, &frameColor);
		ddiSetPenWrap(pen, 0);
		ddiWritePen(pen, data->caption);
		ddiExecutePen(pen, canvas);
		ddiDeletePen(pen);
	};
	
	gwmPostDirty(frame);
};

GWMWindow *gwmCreateFrame(GWMWindow *parent, const char *caption, int x, int y, int width, int height)
{
	GWMWindow *frame = gwmCreateWindow(parent, caption, x, y, width, height, 0);
	if (frame == NULL) return NULL;
	
	FrameData *data = (FrameData*) malloc(sizeof(FrameData));
	frame->data = data;
	
	data->caption = strdup(caption);
	data->panel = gwmCreateWindow(frame, "FramePanel", x+2, y+20, width-4, height-22, 0);
	
	redrawFrame(frame);
	return frame;
};

void gwmDestroyFrame(GWMWindow *frame)
{
	FrameData *data = (FrameData*) frame->data;
	gwmDestroyWindow(data->panel);
	free(data->caption);
	free(data);
	gwmDestroyWindow(frame);
};

GWMWindow* gwmGetFramePanel(GWMWindow *frame)
{
	FrameData *data = (FrameData*) frame->data;
	return data->panel;
};

/*
	Glidix GUI

	Copyright (c) 2014-2017, Madd Games.
	All rights reserved.
	
	Redistribution and use in source and binary forms, with or without
	modification, are permitted provided that the following conditions are met:
	
	* Redistributions of source code must retain the above copyright notice, this
	  list of conditions and the following disclaimer.
	
	* Redistributions in binary form must reproduce the above copyright notice,
	  this list of conditions and the following disclaimer in the documentationx
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

#include <assert.h>
#include <libgwm.h>
#include <libddi.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

typedef struct
{
	int				flags;
	GWMWindow*			panels[2];
	float				proportion;
	int				clicked;
	float				minProp;
	float				maxProp;
} SplitterData;

static int splitHandler(GWMEvent *ev, GWMSplitter *split, void *context)
{
	SplitterData *data = (SplitterData*) context;
	DDISurface *canvas = gwmGetWindowCanvas(split);
	
	switch (ev->type)
	{
	case GWM_EVENT_DOWN:
		if (ev->keycode == GWM_KC_MOUSE_LEFT)
		{
			data->clicked = 1;
		};
		return GWM_EVSTATUS_CONT;
	case GWM_EVENT_UP:
		if (ev->keycode == GWM_KC_MOUSE_LEFT)
		{
			data->clicked = 0;
		};
		return GWM_EVSTATUS_CONT;
	case GWM_EVENT_MOTION:
		if (data->clicked)
		{
			if (data->flags & GWM_SPLITTER_HORIZ)
			{
				gwmSetSplitterProportion(split, (float)(ev->x-4) / (float)(canvas->width-8));
			}
			else
			{
				gwmSetSplitterProportion(split, (float)(ev->y-4) / (float)(canvas->height-8));
			};
		};
		return GWM_EVSTATUS_CONT;
	default:
		return GWM_EVSTATUS_CONT;
	};
};

static void updateSplitter(GWMSplitter *split)
{
	SplitterData *data = (SplitterData*) gwmGetData(split, splitHandler);
	DDISurface *canvas = gwmGetWindowCanvas(split);
	
	static DDIColor transparent = {0, 0, 0, 0};
	ddiFillRect(canvas, 0, 0, canvas->width, canvas->height, &transparent);
	
	float prop = data->proportion;
	if (prop < data->minProp) prop = data->minProp;
	if (prop > data->maxProp) prop = data->maxProp;
	data->proportion = prop;
	
	if (data->flags & GWM_SPLITTER_HORIZ)
	{
		int len0 = (canvas->width - 8) * prop;
		int len1 = (canvas->width - 8) - len0;
		
		gwmMoveWindow(data->panels[0], 0, 0);
		gwmLayout(data->panels[0], len0, canvas->height);
		
		gwmMoveWindow(data->panels[1], len0+8, 0);
		gwmLayout(data->panels[1], len1, canvas->height);
	}
	else
	{
		int len0 = (canvas->height - 8) * prop;
		int len1 = (canvas->height - 8) - len0;
		
		gwmMoveWindow(data->panels[0], 0, 0);
		gwmLayout(data->panels[0], canvas->width, len0);
		
		gwmMoveWindow(data->panels[1], 0, len0+8);
		gwmLayout(data->panels[1], canvas->width, len1);
	};
	
	gwmPostDirty(split);
};

static void splitterMinSize(GWMSplitter *split, int *width, int *height)
{
	SplitterData *data = (SplitterData*) gwmGetData(split, splitHandler);
	
	int w0, h0;
	int w1, h1;
	
	if (data->panels[0]->layout != NULL)
	{
		data->panels[0]->layout->getMinSize(data->panels[0]->layout, &w0, &h0);
	}
	else
	{
		w0 = h0 = 100;
	};

	if (data->panels[1]->layout != NULL)
	{
		data->panels[1]->layout->getMinSize(data->panels[0]->layout, &w1, &h1);
	}
	else
	{
		w1 = h1 = 100;
	};
	
	// the minimum size of the splitter is the sum of its panels minimum length on the main
	// axis, and the maximum of the two minimums on the other axis
	if (data->flags & GWM_SPLITTER_HORIZ)
	{
		*width = w0 + w1 + 8;
		*height = (h0 > h1) ? h0:h1;
	}
	else
	{
		*width = (w0 > w1) ? w0:w1;
		*height = h0 + h1 + 8;
	};
};

static void splitterPrefSize(GWMSplitter *split, int *width, int *height)
{
	SplitterData *data = (SplitterData*) gwmGetData(split, splitHandler);
	
	int w0, h0;
	int w1, h1;
	
	if (data->panels[0]->layout != NULL)
	{
		data->panels[0]->layout->getMinSize(data->panels[0]->layout, &w0, &h0);
	}
	else
	{
		w0 = h0 = 100;
	};

	if (data->panels[1]->layout != NULL)
	{
		data->panels[1]->layout->getMinSize(data->panels[0]->layout, &w1, &h1);
	}
	else
	{
		w1 = h1 = 100;
	};
	
	// the preffered size is double the maximum of the preffered sizes on the main axis, and
	// the maximum on the other.
	if (data->flags & GWM_SPLITTER_HORIZ)
	{
		*width = 2*((w0 > w1) ? w0:w1) + 8;
		*height = (h0 > h1) ? h0:h1;
	}
	else
	{
		*width = (w0 > w1) ? w0:w1;
		*height = 2*((h0 > h1) ? h0:h1) + 8;
	};
};

static void splitterPosition(GWMSplitter *split, int x, int y, int width, int height)
{
	gwmMoveWindow(split, x, y);
	gwmResizeWindow(split, width, height);
	
	// figure out min/max proportions
	SplitterData *data = (SplitterData*) gwmGetData(split, splitHandler);
	
	int w0, h0;
	int w1, h1;
	
	if (data->panels[0]->layout != NULL)
	{
		data->panels[0]->layout->getMinSize(data->panels[0]->layout, &w0, &h0);
	}
	else
	{
		w0 = h0 = 100;
	};

	if (data->panels[1]->layout != NULL)
	{
		data->panels[1]->layout->getMinSize(data->panels[0]->layout, &w1, &h1);
	}
	else
	{
		w1 = h1 = 100;
	};
	
	if (data->flags & GWM_SPLITTER_HORIZ)
	{
		data->minProp = (float) w0 / (float) (width-8);
		data->maxProp = 1.0 - (float) w1 / (float) (width-8);
	}
	else
	{
		data->minProp = (float) h0 / (float) (height-8);
		data->maxProp = 1.0 - (float) h1 / (float) (height-8);
	};
	
	updateSplitter(split);
};

GWMSplitter* gwmNewSplitter(GWMWindow *parent)
{
	GWMSplitter *split = gwmCreateWindow(parent, "GWMSplitter", 0, 0, 0, 0, 0);
	if (split == NULL) return NULL;
	
	SplitterData *data = (SplitterData*) malloc(sizeof(SplitterData));
	data->flags = 0;
	data->panels[0] = gwmCreateWindow(split, "GWMSplitterPanel", 0, 0, 0, 0, 0);
	data->panels[1] = gwmCreateWindow(split, "GWMSplitterPanel", 0, 0, 0, 0, 0);
	data->proportion = 0.5;
	data->clicked = 0;
	data->minProp = 0.0;
	data->maxProp = 1.0;
	
	split->getMinSize = splitterMinSize;
	split->getPrefSize = splitterPrefSize;
	split->position = splitterPosition;
	
	gwmSetWindowCursor(split, GWM_CURSOR_SPLIT_VERT);
	gwmPushEventHandler(split, splitHandler, data);
	return split;
};

void gwmDestroySplitter(GWMSplitter *split)
{
	SplitterData *data = (SplitterData*) gwmGetData(split, splitHandler);
	gwmDestroyWindow(data->panels[0]);
	gwmDestroyWindow(data->panels[1]);
	gwmDestroyWindow(split);
};

void gwmSetSplitterFlags(GWMSplitter *split, int flags)
{
	SplitterData *data = (SplitterData*) gwmGetData(split, splitHandler);
	data->flags = flags;
	
	if (data->flags & GWM_SPLITTER_HORIZ)
	{
		gwmSetWindowCursor(split, GWM_CURSOR_SPLIT_HORIZ);
	}
	else
	{
		gwmSetWindowCursor(split, GWM_CURSOR_SPLIT_VERT);
	};
	
	updateSplitter(split);
};

GWMWindow* gwmGetSplitterPanel(GWMSplitter *split, int index)
{
	SplitterData *data = (SplitterData*) gwmGetData(split, splitHandler);
	assert(index == 0 || index == 1);
	return data->panels[index];
};

void gwmSetSplitterProportion(GWMSplitter *split, float prop)
{
	SplitterData *data = (SplitterData*) gwmGetData(split, splitHandler);
	if (prop < 0.0) prop = 0.0;
	if (prop > 1.0) prop = 1.0;
	data->proportion = prop;
	updateSplitter(split);
};

float gwmGetSplitterProportion(GWMSplitter *split)
{
	SplitterData *data = (SplitterData*) gwmGetData(split, splitHandler);
	return data->proportion;
};

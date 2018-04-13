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
	float				pos;
	float				len;
	int				flags;
	int				clickPos;
	float				refPos;
} ScrollbarData;

static int sbarHandler(GWMEvent *ev, GWMWindow *sbar, void *context);
static DDIColor *colScrollbarBg;
static DDIColor *colScrollbarFg;
static DDIColor *colScrollbarDisabled;

static void redrawScrollbar(GWMWindow *sbar)
{
	ScrollbarData *data = (ScrollbarData*) gwmGetData(sbar, sbarHandler);
	
	if (colScrollbarBg == NULL)
	{
		colScrollbarBg = (DDIColor*) gwmGetThemeProp("gwm.toolkit.scrollbar.bg", GWM_TYPE_COLOR, NULL);
		colScrollbarFg = (DDIColor*) gwmGetThemeProp("gwm.toolkit.scrollbar.fg", GWM_TYPE_COLOR, NULL);
		colScrollbarDisabled = (DDIColor*) gwmGetThemeProp("gwm.toolkit.scrollbar.disabled", GWM_TYPE_COLOR, NULL);
		
		assert(colScrollbarBg != NULL);
		assert(colScrollbarFg != NULL);
		assert(colScrollbarDisabled != NULL);
	};
	
	DDISurface *canvas = gwmGetWindowCanvas(sbar);
	if (data->flags & GWM_SCROLLBAR_DISABLED)
	{
		ddiFillRect(canvas, 0, 0, canvas->width, canvas->height, colScrollbarDisabled);
	}
	else
	{
		ddiFillRect(canvas, 0, 0, canvas->width, canvas->height, colScrollbarBg);

		if (data->flags & GWM_SCROLLBAR_HORIZ)
		{
			int len = canvas->width * data->len;
			int pos = (canvas->width - len) * data->pos;
			ddiFillRect(canvas, pos, 0, len, canvas->height, colScrollbarFg);
		}
		else
		{
			int len = canvas->height * data->len;
			int pos = (canvas->height - len) * data->pos;
			ddiFillRect(canvas, 0, pos, canvas->width, len, colScrollbarFg);
		};
	};
	
	gwmPostDirty(sbar);
};

static int sbarHandler(GWMEvent *ev, GWMWindow *sbar, void *context)
{
	ScrollbarData *data = (ScrollbarData*) gwmGetData(sbar, sbarHandler);
	DDISurface *canvas = gwmGetWindowCanvas(sbar);
	
	switch (ev->type)
	{
	case GWM_EVENT_RETHEME:
		redrawScrollbar(sbar);
		return GWM_EVSTATUS_OK;
	case GWM_EVENT_DOWN:
		if (ev->keycode == GWM_KC_MOUSE_LEFT)
		{
			if (data->flags & GWM_SCROLLBAR_HORIZ)
			{
				data->clickPos = ev->x;
			}
			else
			{
				data->clickPos = ev->y;
			};
			data->refPos = data->pos;
		};
		return GWM_EVSTATUS_CONT;
	case GWM_EVENT_UP:
		if (ev->keycode == GWM_KC_MOUSE_LEFT)
		{
			data->clickPos = -1;
		};
		return GWM_EVSTATUS_CONT;
	case GWM_EVENT_MOTION:
		if (data->flags & GWM_SCROLLBAR_DISABLED)
		{
			return GWM_EVSTATUS_OK;
		};
		
		if (data->clickPos != -1)
		{
			if (data->flags & GWM_SCROLLBAR_HORIZ)
			{
				gwmSetScrollbarPosition(sbar, data->refPos + (float) (ev->x - data->clickPos) /
					(float) (canvas->width - canvas->width * data->len));
			}
			else
			{
				gwmSetScrollbarPosition(sbar, data->refPos + (float) (ev->y - data->clickPos) /
					(float) (canvas->height - canvas->height * data->len));
			};
		};
		return GWM_EVSTATUS_CONT;
	default:
		return GWM_EVSTATUS_CONT;
	};
};

static void sizeScrollbar(GWMWindow *sbar, int *width, int *height)
{
	ScrollbarData *data = (ScrollbarData*) gwmGetData(sbar, sbarHandler);
	if (data->flags & GWM_SCROLLBAR_HORIZ)
	{
		*width = 100;
		*height = 10;
	}
	else
	{
		*width = 10;
		*height = 100;
	};
};

static void positionScrollbar(GWMWindow *sbar, int x, int y, int width, int height)
{
	gwmMoveWindow(sbar, x, y);
	gwmResizeWindow(sbar, width, height);
	redrawScrollbar(sbar);
};

GWMWindow* gwmNewScrollbar(GWMWindow *parent)
{
	GWMWindow *sbar = gwmCreateWindow(parent, "GWMScrollbar", 0, 0, 0, 0, 0);
	if (sbar == NULL) return NULL;
	
	ScrollbarData *data = (ScrollbarData*) malloc(sizeof(ScrollbarData));
	data->pos = 0.0;
	data->len = 1.0;
	data->flags = 0;
	data->clickPos = -1;
	
	sbar->getMinSize = sbar->getPrefSize = sizeScrollbar;
	sbar->position = positionScrollbar;
	
	gwmPushEventHandler(sbar, sbarHandler, data);
	redrawScrollbar(sbar);
	return sbar;
};

void gwmDestroyScrollbar(GWMWindow *sbar)
{
	ScrollbarData *data = (ScrollbarData*) gwmGetData(sbar, sbarHandler);
	free(data);
	gwmDestroyWindow(sbar);
};

void gwmSetScrollbarFlags(GWMWindow *sbar, int flags)
{
	ScrollbarData *data = (ScrollbarData*) gwmGetData(sbar, sbarHandler);
	data->flags = flags;
	redrawScrollbar(sbar);
};

void gwmSetScrollbarPosition(GWMWindow *sbar, float pos)
{
	ScrollbarData *data = (ScrollbarData*) gwmGetData(sbar, sbarHandler);
	if (pos < 0.0) pos = 0.0;
	if (pos > 1.0) pos = 1.0;
	data->pos = pos;
	redrawScrollbar(sbar);
};

void gwmSetScrollbarLength(GWMWindow *sbar, float len)
{
	ScrollbarData *data = (ScrollbarData*) gwmGetData(sbar, sbarHandler);
	if (len < 0.0) len = 0.0;
	if (len > 1.0) len = 1.0;
	data->len = len;
	redrawScrollbar(sbar);
};

float gwmGetScrollbarPosition(GWMWindow *sbar)
{
	ScrollbarData *data = (ScrollbarData*) gwmGetData(sbar, sbarHandler);
	return data->pos;
};

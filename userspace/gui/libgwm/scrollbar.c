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

#include <libgwm.h>
#include <libddi.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define	SCROLLBAR_WIDTH					8
#define	SCROLLBAR_MIN_LEN				8
#define	SCROLLBAR_UPDATEABLE_FLAGS			(GWM_SCROLLBAR_DISABLED)

typedef struct
{
	int						viewOffset;
	int						viewSize;
	int						viewTotal;
	int						len;
	int						flags;
	int						mouseX, mouseY;
	int						pressed;
	int						anchorX, anchorY;
	GWMScrollbarCallback				callback;
	void*						callbackParam;
} GWMScrollbarData;

static void gwmRedrawScrollbar(GWMWindow *sbar)
{
	GWMScrollbarData *data = (GWMScrollbarData*) sbar->data;
	DDISurface *canvas = gwmGetWindowCanvas(sbar);
	
	if (data->flags & GWM_SCROLLBAR_DISABLED)
	{
		static DDIColor disabledColor = {0x20, 0x20, 0x20, 0xFF};
		ddiFillRect(canvas, 0, 0, canvas->width, canvas->height, &disabledColor);
	}
	else
	{
		static DDIColor backgroundColor = {0xFF, 0xFF, 0xFF, 0xFF};
		static DDIColor sliderColorNormal = {0x7F, 0x7F, 0x7F, 0xFF};
		static DDIColor sliderColorHover = {0xBF, 0xBF, 0xBF, 0xFF};
		static DDIColor sliderColorPressed = {0x40, 0x40, 0x40, 0xFF};
		ddiFillRect(canvas, 0, 0, canvas->width, canvas->height, &backgroundColor);
	
		int sliderLen = data->viewSize * data->len / data->viewTotal + 1;
		int sliderOffset = data->viewOffset * data->len / data->viewTotal;
	
		DDIColor *sliderColor = &sliderColorNormal;
		if (data->flags & GWM_SCROLLBAR_HORIZ)
		{
			if (data->pressed)
			{
				sliderColor = &sliderColorPressed;
			}
			else if ((data->mouseX >= sliderOffset) && (data->mouseX < sliderOffset+sliderLen))
			{
				sliderColor = &sliderColorHover;
			};
		
			ddiFillRect(canvas, sliderOffset, 0, sliderLen, SCROLLBAR_WIDTH, sliderColor);
		}
		else
		{
			if (data->pressed)
			{
				sliderColor = &sliderColorPressed;
			}
			else if ((data->mouseY >= sliderOffset) && (data->mouseY < sliderOffset+sliderLen))
			{
				sliderColor = &sliderColorHover;
			};

			ddiFillRect(canvas, 0, sliderOffset, SCROLLBAR_WIDTH, sliderLen, sliderColor);
		};
	};
	
	gwmPostDirty(sbar);
};

int gwmScrollbarHandler(GWMEvent *ev, GWMWindow *sbar)
{
	GWMScrollbarData *data = (GWMScrollbarData*) sbar->data;
	int delta;
	int sliderLen = data->viewSize * data->len / data->viewTotal + 1;
	int sliderOffset = data->viewOffset * data->len / data->viewTotal;
	int newOffset;
	int status = 0;
	
	switch (ev->type)
	{
	case GWM_EVENT_LEAVE:
		if ((data->flags & GWM_SCROLLBAR_DISABLED) == 0)
		{
			data->mouseX = data->mouseY = -1;
			gwmRedrawScrollbar(sbar);
		};
		return 0;
	case GWM_EVENT_ENTER:
	case GWM_EVENT_MOTION:
		if ((data->flags & GWM_SCROLLBAR_DISABLED) == 0)
		{
			data->mouseX = ev->x;
			data->mouseY = ev->y;
			if (data->pressed)
			{
				if (data->flags & GWM_SCROLLBAR_HORIZ)
				{
					delta = ev->x - data->anchorX;
				}
				else
				{
					delta = ev->y - data->anchorY;
				};
			
				data->anchorX = ev->x;
				data->anchorY = ev->y;
			
				sliderOffset += delta;
				newOffset = sliderOffset * data->viewTotal / data->len;
				if (newOffset < 0)
					data->viewOffset = 0;
				else if (newOffset > (data->viewTotal-data->viewSize))
					data->viewOffset = data->viewTotal-data->viewSize;
				else
					data->viewOffset = newOffset;

				if (data->callback != NULL)
				{
					status = data->callback(data->callbackParam);
				};
			};
			gwmRedrawScrollbar(sbar);
			return status;
		};
		return 0;
	case GWM_EVENT_DOWN:
		if ((data->flags & GWM_SCROLLBAR_DISABLED) == 0)
		{
			if (ev->scancode == GWM_SC_MOUSE_LEFT)
			{
				if (data->flags & GWM_SCROLLBAR_HORIZ)
				{
					if ((data->mouseX >= sliderOffset) && (data->mouseX < sliderOffset+sliderLen))
					{
						data->anchorX = ev->x;
						data->anchorY = ev->y;
						data->pressed = 1;
					};
				}
				else
				{
					if ((data->mouseY >= sliderOffset) && (data->mouseY < sliderOffset+sliderLen))
					{
						data->anchorX = ev->x;
						data->anchorY = ev->y;
						data->pressed = 1;
					};
				};
			
				gwmRedrawScrollbar(sbar);
			};
		};
		return 0;
	case GWM_EVENT_UP:
		if ((data->flags & GWM_SCROLLBAR_DISABLED) == 0)
		{
			if (ev->scancode == GWM_SC_MOUSE_LEFT)
			{
				data->pressed = 0;
				gwmRedrawScrollbar(sbar);
			};
		};
		return 0;
	default:
		return gwmDefaultHandler(ev, sbar);
	};
};

GWMWindow *gwmCreateScrollbar(GWMWindow *parent, int x, int y, int len, int viewOffset, int viewSize, int viewTotal, int flags)
{
	if (len < SCROLLBAR_MIN_LEN)
	{
		len = SCROLLBAR_MIN_LEN;
	};
	
	GWMWindow *sbar;
	if (flags & GWM_SCROLLBAR_HORIZ)
	{
		sbar = gwmCreateWindow(parent, "GWMScrollbar", x, y, len, SCROLLBAR_WIDTH, 0);
	}
	else
	{
		sbar = gwmCreateWindow(parent, "GWMScrollbar", x, y, SCROLLBAR_WIDTH, len, 0);
	};
	
	if (sbar == NULL) return NULL;
	
	GWMScrollbarData *data = (GWMScrollbarData*) malloc(sizeof(GWMScrollbarData));
	sbar->data = data;
	
	data->viewOffset = viewOffset;
	data->viewSize = viewSize;
	data->viewTotal = viewTotal;
	data->len = len;
	data->flags = flags;
	data->mouseX = -1;
	data->mouseY = -1;
	data->pressed = 0;
	data->callback = NULL;

	gwmSetEventHandler(sbar, gwmScrollbarHandler);
	gwmRedrawScrollbar(sbar);	
	return sbar;
};

void gwmDestroyScrollbar(GWMWindow *sbar)
{
	free(sbar->data);
	gwmDestroyWindow(sbar);
};

void gwmSetScrollbarCallback(GWMWindow *sbar, GWMScrollbarCallback callback, void *param)
{
	GWMScrollbarData *data = (GWMScrollbarData*) sbar->data;
	data->callback = callback;
	data->callbackParam = param;
};

int gwmGetScrollbarOffset(GWMWindow *sbar)
{
	GWMScrollbarData *data = (GWMScrollbarData*) sbar->data;
	return data->viewOffset;
};

void gwmSetScrollbarParams(GWMWindow *sbar, int viewOffset, int viewSize, int viewTotal, int flags)
{
	GWMScrollbarData *data = (GWMScrollbarData*) sbar->data;
	data->viewOffset = viewOffset;
	data->viewSize = viewSize;
	data->viewTotal = viewTotal;
	data->flags = (data->flags & ~SCROLLBAR_UPDATEABLE_FLAGS) | (flags & SCROLLBAR_UPDATEABLE_FLAGS);
	gwmRedrawScrollbar(sbar);
};

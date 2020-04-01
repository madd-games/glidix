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

#include <assert.h>
#include <libgwm.h>
#include <libddi.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define	BUTTON_TEMPL_WIDTH	17
#define	BUTTON_HEIGHT		30

enum
{
	BUTTON_STATE_NORMAL,
	BUTTON_STATE_HOVERING,
	BUTTON_STATE_CLICKED
};

typedef struct
{
	char			*text;
	int			flags;
	int			state;
	GWMButtonCallback	callback;
	void			*callbackParam;
	int			symbol;
	int			minWidth;
	int			prefWidth;
	int			focused;
} GWMButtonData;

static void gwmRedrawButton(GWMWindow *button);

int gwmButtonHandler(GWMEvent *ev, GWMWindow *button, void *context)
{
	int retval = GWM_EVSTATUS_CONT;
	GWMButtonData *data = (GWMButtonData*) context;
	DDISurface *canvas = gwmGetWindowCanvas(button);
	
	switch (ev->type)
	{
	case GWM_EVENT_FOCUS_IN:
		data->focused = 1;
		gwmRedrawButton(button);
		return GWM_EVSTATUS_OK;
	case GWM_EVENT_FOCUS_OUT:
		data->focused = 0;
		data->state = BUTTON_STATE_NORMAL;
		gwmRedrawButton(button);
		return GWM_EVSTATUS_OK;
	case GWM_EVENT_ENTER:
		data->state = BUTTON_STATE_HOVERING;
		gwmRedrawButton(button);
		return GWM_EVSTATUS_OK;
	case GWM_EVENT_LEAVE:
		data->state = BUTTON_STATE_NORMAL;
		gwmRedrawButton(button);
		return GWM_EVSTATUS_OK;
	case GWM_EVENT_MOTION:
		if (ev->x < 0 || ev->x >= canvas->width || ev->y < 0 || ev->y >= canvas->height)
		{
			data->state = BUTTON_STATE_NORMAL;
			gwmRedrawButton(button);
		};
		return GWM_EVSTATUS_OK;
	case GWM_EVENT_DOWN:
		if (ev->keycode == GWM_KC_MOUSE_LEFT || ev->keycode == GWM_KC_RETURN)
		{
			data->state = BUTTON_STATE_CLICKED;
			gwmRedrawButton(button);
		};
		return GWM_EVSTATUS_CONT;
	case GWM_EVENT_UP:
		if (ev->keycode == GWM_KC_MOUSE_LEFT || ev->keycode == GWM_KC_RETURN)
		{
			if (data->state == BUTTON_STATE_CLICKED)
			{
				if (data->callback != NULL)
				{
					if ((data->flags & GWM_BUTTON_DISABLED) == 0)
					{
						retval = data->callback(data->callbackParam);
					};
				};
				data->state = BUTTON_STATE_HOVERING;
			}
			else
			{
				data->state = BUTTON_STATE_NORMAL;
			};
			gwmRedrawButton(button);
		};
		return retval;
	case GWM_EVENT_RETHEME:
		gwmRedrawButton(button);
		return GWM_EVSTATUS_OK;
	default:
		return GWM_EVSTATUS_CONT;
	};
};

static void gwmRedrawButton(GWMWindow *button)
{
	GWMButtonData *data = (GWMButtonData*) gwmGetData(button, gwmButtonHandler);
	DDISurface *canvas = gwmGetWindowCanvas(button);

	static DDIColor transparent = {0, 0, 0, 0};
	ddiFillRect(canvas, 0, 0, canvas->width, canvas->height, &transparent);
	
	int whichImg = data->state;
	if (data->flags & GWM_BUTTON_DISABLED)
	{
		whichImg = 3;
	};
	
	DDISurface *imgButton = gwmGetThemeSurface("gwm.toolkit.button");

	DDISurface *temp = ddiCreateSurface(&canvas->format, BUTTON_TEMPL_WIDTH, BUTTON_HEIGHT, NULL, 0);
	assert(temp != NULL);
	ddiOverlay(imgButton, BUTTON_TEMPL_WIDTH*data->focused, BUTTON_HEIGHT*whichImg, temp, 0, 0, BUTTON_TEMPL_WIDTH, BUTTON_HEIGHT);
	DDISurface *scaled = ddiScale(temp, canvas->width, canvas->height, DDI_SCALE_BORDERED_GRADIENT);
	assert(scaled != NULL);
	ddiBlit(scaled, 0, 0, canvas, 0, 0, canvas->width, canvas->height);
	ddiDeleteSurface(scaled);
	ddiDeleteSurface(temp);
	
	DDIPen *pen = gwmGetPen(button, 0, 0, canvas->width, canvas->height-11);
	ddiSetPenAlignment(pen, DDI_ALIGN_CENTER);
	ddiSetPenWrap(pen, 0);
	ddiWritePen(pen, data->text);
	
	int txtWidth, txtHeight;
	ddiGetPenSize(pen, &txtWidth, &txtHeight);
	ddiSetPenPosition(pen, 0, (canvas->height-txtHeight)/2);
	ddiExecutePen(pen, canvas);
	ddiDeletePen(pen);
	
	data->minWidth = txtWidth + 16;
	gwmPostDirty(button);
};

static int defaultButtonCallback(void *context)
{
	GWMWindow *button = (GWMWindow*) context;
	GWMButtonData *data = (GWMButtonData*) gwmGetData(button, gwmButtonHandler);

	GWMCommandEvent event;
	memset(&event, 0, sizeof(GWMCommandEvent));
	event.header.type = GWM_EVENT_COMMAND;
	event.symbol = data->symbol;
	
	return gwmPostEvent((GWMEvent*) &event, button);
};

static void gwmSizeButton(GWMWindow *button, int *width, int *height)
{
	GWMButtonData *data = (GWMButtonData*) gwmGetData(button, gwmButtonHandler);
	if (data->prefWidth != 0) *width = data->prefWidth;
	else *width = data->minWidth;
	*height = BUTTON_HEIGHT;
};

static void gwmPositionButton(GWMWindow *button, int x, int y, int width, int height)
{
	gwmMoveWindow(button, x, y);
	gwmResizeWindow(button, width, height);
	gwmRedrawButton(button);
};

GWMWindow* gwmCreateButton(GWMWindow *parent, const char *text, int x, int y, int width, int flags)
{
	GWMWindow *button = gwmCreateWindow(parent, "GWMButton", x, y, width, BUTTON_HEIGHT, 0);
	if (button == NULL) return NULL;
	
	GWMButtonData *data = (GWMButtonData*) malloc(sizeof(GWMButtonData));
	data->text = strdup(text);
	data->flags = flags;
	data->state = BUTTON_STATE_NORMAL;
	data->callback = defaultButtonCallback;
	data->callbackParam = button;
	data->symbol = 0;
	data->minWidth = 0;
	data->prefWidth = 0;
	data->focused = 0;
	
	button->getMinSize = button->getPrefSize = gwmSizeButton;
	button->position = gwmPositionButton;
	
	gwmAcceptTabs(button);
	gwmPushEventHandler(button, gwmButtonHandler, data);
	gwmRedrawButton(button);
	return button;
};

void gwmSetButtonSymbol(GWMWindow *button, int symbol)
{
	GWMButtonData *data = (GWMButtonData*) gwmGetData(button, gwmButtonHandler);
	data->symbol = symbol;
};

void gwmDestroyButton(GWMWindow *button)
{
	GWMButtonData *data = (GWMButtonData*) gwmGetData(button, gwmButtonHandler);
	free(data->text);
	free(data);
	gwmDestroyWindow(button);
};

void gwmSetButtonCallback(GWMWindow *button, GWMButtonCallback callback, void *param)
{
	GWMButtonData *data = (GWMButtonData*) gwmGetData(button, gwmButtonHandler);
	data->callback = callback;
	data->callbackParam = param;
};

GWMWindow* gwmCreateStockButton(GWMWindow *parent, int symbol)
{
	return gwmCreateButtonWithLabel(parent, symbol, NULL);
};

GWMWindow *gwmNewButton(GWMWindow *parent)
{
	return gwmCreateButton(parent, "", 0, 0, 0, 0);
};

void gwmSetButtonLabel(GWMWindow *button, const char *label)
{
	GWMButtonData *data = (GWMButtonData*) gwmGetData(button, gwmButtonHandler);
	free(data->text);
	data->text = strdup(label);
	gwmRedrawButton(button);
};

void gwmSetButtonFlags(GWMWindow *button, int flags)
{
	GWMButtonData *data = (GWMButtonData*) gwmGetData(button, gwmButtonHandler);
	data->flags = flags;
	gwmRedrawButton(button);
};

GWMWindow* gwmCreateButtonWithLabel(GWMWindow *parent, int symbol, const char *label)
{
	if (label == NULL)
	{
		label = gwmGetStockLabel(symbol);
	};
	
	GWMWindow *btn = gwmCreateButton(parent, label, 0, 0, 0, 0);
	gwmSetButtonSymbol(btn, symbol);
	return btn;
};

void gwmSetButtonPrefWidth(GWMWindow *button, int width)
{
	GWMButtonData *data = (GWMButtonData*) gwmGetData(button, gwmButtonHandler);
	data->prefWidth = width;
};

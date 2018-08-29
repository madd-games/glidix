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

enum
{
	TOOL_BUTTON_STATE_NORMAL,
	TOOL_BUTTON_STATE_HOVERING,
	TOOL_BUTTON_STATE_CLICKED
};

static DDISurface *imgToolButton = NULL;

typedef struct
{
	DDISurface*		icon;
	int			state;
	int			symbol;
} ToolButtonData;

static void redrawToolButton(GWMWindow *button);

int toolbtnHandler(GWMEvent *ev, GWMWindow *toolbtn, void *context)
{
	ToolButtonData *data = (ToolButtonData*) context;
	
	switch (ev->type)
	{
	case GWM_EVENT_ENTER:
		data->state = TOOL_BUTTON_STATE_HOVERING;
		redrawToolButton(toolbtn);
		return GWM_EVSTATUS_OK;
	case GWM_EVENT_LEAVE:
		data->state = TOOL_BUTTON_STATE_NORMAL;
		redrawToolButton(toolbtn);
		return GWM_EVSTATUS_OK;
	case GWM_EVENT_DOWN:
		if (ev->keycode == GWM_KC_MOUSE_LEFT)
		{
			data->state = TOOL_BUTTON_STATE_CLICKED;
			redrawToolButton(toolbtn);
		};
		return GWM_EVSTATUS_OK;
	case GWM_EVENT_UP:
		if (ev->keycode == GWM_KC_MOUSE_LEFT)
		{
			if (data->state == TOOL_BUTTON_STATE_CLICKED)
			{
				data->state = TOOL_BUTTON_STATE_HOVERING;
				redrawToolButton(toolbtn);
				
				GWMCommandEvent event;
				memset(&event, 0, sizeof(GWMCommandEvent));
				event.header.type = GWM_EVENT_COMMAND;
				event.symbol = data->symbol;
	
				return gwmPostEvent((GWMEvent*) &event, toolbtn);
			}
			else
			{
				data->state = TOOL_BUTTON_STATE_NORMAL;
			};
			redrawToolButton(toolbtn);
		};
		return GWM_EVSTATUS_OK;
	case GWM_EVENT_RETHEME:
		redrawToolButton(toolbtn);
		return GWM_EVSTATUS_OK;
	default:
		return GWM_EVSTATUS_CONT;
	};
};

static void redrawToolButton(GWMWindow *toolbtn)
{
	ToolButtonData *data = (ToolButtonData*) gwmGetData(toolbtn, toolbtnHandler);
	DDISurface *canvas = gwmGetWindowCanvas(toolbtn);

	static DDIColor transparent = {0, 0, 0, 0};
	ddiFillRect(canvas, 0, 0, canvas->width, canvas->height, &transparent);
	
	int whichImg = data->state;
	
	if (imgToolButton == NULL)
	{
		imgToolButton = (DDISurface*) gwmGetThemeProp("gwm.toolkit.toolbtn", GWM_TYPE_SURFACE, NULL);
		if (imgToolButton == NULL)
		{
			fprintf(stderr, "Failed to load tool button image\n");
			abort();
		};
	};
	
	ddiBlit(imgToolButton, 0, 30*whichImg, canvas, 0, 0, 30, 30);
	
	DDISurface *icon = data->icon;
	if (icon == NULL)
	{
		icon = gwmGetStockIcon(data->symbol);
	};
	
	if (icon != NULL)
	{
		ddiBlit(icon, 0, 0, canvas, 3, 3, 24, 24);
	};
	
	gwmPostDirty(toolbtn);
};

static void sizeToolButton(GWMWindow *toolbtn, int *width, int *height)
{
	*width = 30;
	*height = 30;
};

static void positionToolButton(GWMWindow *toolbtn, int x, int y, int width, int height)
{
	gwmMoveWindow(toolbtn, x, y);
	gwmResizeWindow(toolbtn, width, height);
	redrawToolButton(toolbtn);
};

GWMWindow* gwmNewToolButton(GWMWindow *parent)
{
	GWMWindow *toolbtn = gwmCreateWindow(parent, "GWMToolButton", 0, 0, 0, 0, 0);
	if (toolbtn == NULL) return NULL;
	
	ToolButtonData *data = (ToolButtonData*) malloc(sizeof(ToolButtonData));
	data->icon = NULL;
	data->state = TOOL_BUTTON_STATE_NORMAL;
	data->symbol = 0;
	
	toolbtn->getMinSize = toolbtn->getPrefSize = sizeToolButton;
	toolbtn->position = positionToolButton;
	
	gwmPushEventHandler(toolbtn, toolbtnHandler, data);
	redrawToolButton(toolbtn);
	
	return toolbtn;
};

void gwmDestroyToolButton(GWMWindow *toolbtn)
{
	ToolButtonData *data = (ToolButtonData*) gwmGetData(toolbtn, toolbtnHandler);
	free(data);
	gwmDestroyWindow(toolbtn);
};

void gwmSetToolButtonSymbol(GWMWindow *toolbtn, int symbol)
{
	ToolButtonData *data = (ToolButtonData*) gwmGetData(toolbtn, toolbtnHandler);
	data->symbol = symbol;
};

void gwmSetToolButtonIcon(GWMWindow *toolbtn, DDISurface *icon)
{
	ToolButtonData *data = (ToolButtonData*) gwmGetData(toolbtn, toolbtnHandler);
	data->icon = icon;
};

GWMToolButton* gwmAddToolButtonBySymbol(GWMWindow *parent, GWMLayout *toolbar, int symbol)
{
	GWMToolButton *tool = gwmNewToolButton(parent);
	gwmBoxLayoutAddWindow(toolbar, tool, 0, 0, 0);
	gwmSetToolButtonSymbol(tool, symbol);
	return tool;
};

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
	CB_MSTATE_NORMAL,
	CB_MSTATE_HOVERING,
	CB_MSTATE_CLICKED
};

static DDISurface *imgCheckbox = NULL;

typedef struct
{
	int					state;
	int					mstate;
	int					flags;
	char*					text;
	int					minWidth;
	int					symbol;
} GWMCheckboxData;

int gwmCheckboxHandler(GWMEvent *ev, GWMWindow *checkbox, void *context);

static void gwmRedrawCheckbox(GWMWindow *checkbox)
{
	GWMCheckboxData *data = (GWMCheckboxData*) gwmGetData(checkbox, gwmCheckboxHandler);
	DDISurface *canvas = gwmGetWindowCanvas(checkbox);
	
	static DDIColor transparent = {0, 0, 0, 0};
	ddiFillRect(canvas, 0, 0, canvas->width, canvas->height, &transparent);
	
	int whichY = data->mstate;
	if (data->flags & GWM_CB_DISABLED)
	{
		whichY = 3;
	};
	
	if (imgCheckbox == NULL)
	{
		imgCheckbox = (DDISurface*) gwmGetThemeProp("gwm.toolkit.checkbox", GWM_TYPE_SURFACE, NULL);
		if (imgCheckbox == NULL)
		{
			fprintf(stderr, "Failed to load checkbox image\n");
			abort();
		};
	};
	
	ddiBlit(imgCheckbox, 20*data->state, 20*whichY, canvas, 0, 0, 20, 20);

	DDIPen *pen = ddiCreatePen(&canvas->format, gwmGetDefaultFont(), 0, 0, canvas->width, canvas->height, 0, 0, NULL);
	ddiSetPenWrap(pen, 0);
	ddiWritePen(pen, data->text);
	
	int txtWidth, txtHeight;
	ddiGetPenSize(pen, &txtWidth, &txtHeight);
	ddiSetPenPosition(pen, 22, 10-(txtHeight/2));
	ddiExecutePen(pen, canvas);
	ddiDeletePen(pen);
	
	data->minWidth = txtWidth + 22;

	gwmPostDirty(checkbox);
};

int gwmCheckboxHandler(GWMEvent *ev, GWMWindow *checkbox, void *context)
{
	GWMCheckboxData *data = (GWMCheckboxData*) gwmGetData(checkbox, gwmCheckboxHandler);
	
	switch (ev->type)
	{
	case GWM_EVENT_ENTER:
		data->mstate = CB_MSTATE_HOVERING;
		gwmRedrawCheckbox(checkbox);
		return GWM_EVSTATUS_OK;
	case GWM_EVENT_LEAVE:
		data->mstate = CB_MSTATE_NORMAL;
		gwmRedrawCheckbox(checkbox);
		return GWM_EVSTATUS_OK;
	case GWM_EVENT_DOWN:
		if (ev->keycode == GWM_KC_MOUSE_LEFT)
		{
			data->mstate = CB_MSTATE_CLICKED;
			gwmRedrawCheckbox(checkbox);
		};
		return GWM_EVSTATUS_OK;
	case GWM_EVENT_UP:
		if (ev->keycode == GWM_KC_MOUSE_LEFT)
		{
			if (data->mstate == CB_MSTATE_CLICKED)
			{
				if ((data->flags & GWM_CB_DISABLED) == 0)
				{
					GWMCommandEvent cmdev;
					memset(&cmdev, 0, sizeof(GWMCommandEvent));
					cmdev.header.type = GWM_EVENT_COMMAND;
					cmdev.symbol = data->symbol;
					
					if (gwmPostEvent((GWMEvent*) &cmdev, checkbox) == GWM_EVSTATUS_DEFAULT)
						data->state = !data->state;
				};
				data->mstate = CB_MSTATE_HOVERING;
			}
			else
			{
				data->mstate = CB_MSTATE_NORMAL;
			};
			gwmRedrawCheckbox(checkbox);
		};
		return GWM_EVSTATUS_OK;
	default:
		return GWM_EVSTATUS_CONT;
	};
};

static void gwmSizeCheckbox(GWMWindow *checkbox, int *width, int *height)
{
	GWMCheckboxData *data = (GWMCheckboxData*) gwmGetData(checkbox, gwmCheckboxHandler);
	*width = data->minWidth;
	*height = 20;
};

static void gwmPositionCheckbox(GWMWindow *checkbox, int x, int y, int width, int height)
{
	y += (height - 20) / 2;
	gwmMoveWindow(checkbox, x, y);
	gwmResizeWindow(checkbox, width, 20);
	gwmRedrawCheckbox(checkbox);
};

GWMWindow *gwmCreateCheckbox(GWMWindow *parent, int x, int y, int state, int flags)
{
	GWMWindow *checkbox = gwmCreateWindow(parent, "GWMCheckbox", x, y, 0, 0, 0);
	if (checkbox == NULL) return NULL;
	
	GWMCheckboxData *data = (GWMCheckboxData*) malloc(sizeof(GWMCheckboxData));
	data->flags = flags;
	data->state = state;
	data->mstate = CB_MSTATE_NORMAL;
	data->text = strdup("");
	data->minWidth = 20;
	data->symbol = 0;
	
	checkbox->getMinSize = checkbox->getPrefSize = gwmSizeCheckbox;
	checkbox->position = gwmPositionCheckbox;

	gwmPushEventHandler(checkbox, gwmCheckboxHandler, data);
	gwmRedrawCheckbox(checkbox);
	return checkbox;
};

GWMWindow *gwmNewCheckbox(GWMWindow *parent)
{
	return gwmCreateCheckbox(parent, 0, 0, 0, 0);
};

void gwmSetCheckboxLabel(GWMWindow *checkbox, const char *text)
{
	GWMCheckboxData *data = (GWMCheckboxData*) gwmGetData(checkbox, gwmCheckboxHandler);
	free(data->text);
	data->text = strdup(text);
	gwmRedrawCheckbox(checkbox);
};

void gwmDestroyCheckbox(GWMWindow *checkbox)
{
	GWMCheckboxData *data = (GWMCheckboxData*) gwmGetData(checkbox, gwmCheckboxHandler);
	free(data);
	gwmDestroyWindow(checkbox);
};

void gwmSetCheckboxFlags(GWMWindow *checkbox, int flags)
{
	GWMCheckboxData *data = (GWMCheckboxData*) gwmGetData(checkbox, gwmCheckboxHandler);
	data->flags = flags;
	gwmRedrawCheckbox(checkbox);
};

int gwmGetCheckboxState(GWMWindow *checkbox)
{
	GWMCheckboxData *data = (GWMCheckboxData*) gwmGetData(checkbox, gwmCheckboxHandler);
	return data->state;
};

void gwmSetCheckboxState(GWMWindow *checkbox, int state)
{
	GWMCheckboxData *data = (GWMCheckboxData*) gwmGetData(checkbox, gwmCheckboxHandler);
	data->state = state;
};

void gwmSetCheckboxSymbol(GWMWindow *checkbox, int symbol)
{
	GWMCheckboxData *data = (GWMCheckboxData*) gwmGetData(checkbox, gwmCheckboxHandler);
	data->symbol = symbol;
};

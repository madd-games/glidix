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

#define	CB_WIDTH				20
#define	CB_HEIGHT				20

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
} GWMCheckboxData;

static void gwmRedrawCheckbox(GWMWindow *checkbox)
{
	GWMCheckboxData *data = (GWMCheckboxData*) checkbox->data;
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
		const char *error;
		imgCheckbox = ddiLoadAndConvertPNG(&canvas->format, "/usr/share/images/checkbox.png", &error);
		if (imgCheckbox == NULL)
		{
			fprintf(stderr, "Failed to load checkbox image (/usr/share/images/checkbox.png): %s\n", error);
			abort();
		};
	};
	
	ddiBlit(imgCheckbox, CB_WIDTH*data->state, CB_HEIGHT*whichY, canvas, 0, 0, CB_WIDTH, CB_HEIGHT);
	gwmPostDirty(checkbox);
};

int gwmCheckboxHandler(GWMEvent *ev, GWMWindow *checkbox)
{
	GWMCheckboxData *data = (GWMCheckboxData*) checkbox->data;
	
	switch (ev->type)
	{
	case GWM_EVENT_ENTER:
		data->mstate = CB_MSTATE_HOVERING;
		gwmRedrawCheckbox(checkbox);
		return 0;
	case GWM_EVENT_LEAVE:
		data->mstate = CB_MSTATE_NORMAL;
		gwmRedrawCheckbox(checkbox);
		return 0;
	case GWM_EVENT_DOWN:
		if (ev->scancode == GWM_SC_MOUSE_LEFT)
		{
			data->mstate = CB_MSTATE_CLICKED;
			gwmRedrawCheckbox(checkbox);
		};
		return 0;
	case GWM_EVENT_UP:
		if (ev->scancode == GWM_SC_MOUSE_LEFT)
		{
			if (data->mstate == CB_MSTATE_CLICKED)
			{
				if ((data->flags & GWM_CB_DISABLED) == 0)
				{
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
		return 0;
	default:
		return gwmDefaultHandler(ev, checkbox);
	};
};

GWMWindow *gwmCreateCheckbox(GWMWindow *parent, int x, int y, int state, int flags)
{
	GWMWindow *checkbox = gwmCreateWindow(parent, "GWMCheckbox", x, y, CB_WIDTH, CB_HEIGHT, 0);
	if (checkbox == NULL) return NULL;
	
	GWMCheckboxData *data = (GWMCheckboxData*) malloc(sizeof(GWMCheckboxData));
	checkbox->data = data;
	
	data->flags = flags;
	data->state = state;
	data->mstate = CB_MSTATE_NORMAL;
	
	gwmRedrawCheckbox(checkbox);
	gwmSetEventHandler(checkbox, gwmCheckboxHandler);
	return checkbox;
};

void gwmDestroyCheckbox(GWMWindow *checkbox)
{
	GWMCheckboxData *data = (GWMCheckboxData*) checkbox->data;
	free(data);
	gwmDestroyWindow(checkbox);
};

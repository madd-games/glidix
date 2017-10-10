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
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define	OPTMENU_MIN_WIDTH		50
#define	OPTMENU_HEIGHT			20
#define	OPTMENU_BUTTON_WIDTH		16

enum
{
	OPTMENU_NORMAL,
	OPTMENU_HOVER,
	OPTMENU_DOWN
};

typedef struct OptmenuOption_
{
	struct OptmenuOption_*	prev;
	char*			label;
	uint64_t		id;
	GWMWindow*		optmenu;
} OptmenuOption;

typedef struct
{
	GWMMenu*		options;
	int			width;
	int			state;
	int			flags;
	char*			currentText;
	uint64_t		currentID;
	OptmenuOption*		optStack;
} OptmenuData;

static DDISurface *imgOptmenu;

static void redrawOptmenu(GWMWindow *optmenu)
{
	OptmenuData *data = (OptmenuData*) optmenu->data;
	DDISurface *canvas = gwmGetWindowCanvas(optmenu);
	
	if (imgOptmenu == NULL)
	{
		const char *error;
		imgOptmenu = ddiLoadAndConvertPNG(&canvas->format, "/usr/share/images/optmenu.png", &error);
		if (imgOptmenu == NULL)
		{
			fprintf(stderr, "Failed to load option menu image (/usr/share/images/optmenu.png): %s\n", error);
			abort();
		};
	};
	
	DDIColor transparent = {0, 0, 0, 0};
	ddiFillRect(canvas, 0, 0, data->width, OPTMENU_HEIGHT, &transparent);
	
	int state = data->state;
	if (data->flags & GWM_OPTMENU_DISABLED)
	{
		state = 3;
	};
	
	ddiBlit(imgOptmenu, 0, OPTMENU_HEIGHT * state, canvas, 0, 0, 3, OPTMENU_HEIGHT);
	int i;
	for (i=0; i<data->width-3-OPTMENU_BUTTON_WIDTH; i++)
	{
		ddiBlit(imgOptmenu, 3, OPTMENU_HEIGHT * state, canvas, 3+i, 0, 1, OPTMENU_HEIGHT);
	};
	ddiBlit(imgOptmenu, imgOptmenu->width - OPTMENU_BUTTON_WIDTH, OPTMENU_HEIGHT * state,
			canvas, data->width - OPTMENU_BUTTON_WIDTH, 0,
			OPTMENU_BUTTON_WIDTH, OPTMENU_HEIGHT);
	
	DDIPen *pen = ddiCreatePen(&canvas->format, gwmGetDefaultFont(), 3, canvas->height-6, canvas->width-3, canvas->height-3, 0, 0, NULL);
	if (pen != NULL)
	{
		ddiWritePen(pen, data->currentText);
		ddiExecutePen2(pen, canvas, DDI_POSITION_BASELINE);
		ddiDeletePen(pen);
	};
	gwmPostDirty(optmenu);
};

static int optmenuHandler(GWMEvent *ev, GWMWindow *optmenu)
{
	OptmenuData *data = (OptmenuData*) optmenu->data;
	
	switch (ev->type)
	{
	case GWM_EVENT_ENTER:
	case GWM_EVENT_MOTION:
		if (data->state != OPTMENU_DOWN)
		{
			data->state = OPTMENU_HOVER;
		};
		redrawOptmenu(optmenu);
		return 0;
	case GWM_EVENT_LEAVE:
		if (data->state != OPTMENU_DOWN) data->state = OPTMENU_NORMAL;
		redrawOptmenu(optmenu);
		return 0;
	case GWM_EVENT_UP:
		if (ev->keycode == GWM_KC_MOUSE_LEFT)
		{
			if ((data->flags & GWM_OPTMENU_DISABLED) == 0)
			{
				data->state = OPTMENU_DOWN;
				redrawOptmenu(optmenu);
				gwmOpenMenu(data->options, optmenu, 0, OPTMENU_HEIGHT-1);
			};
		};
		return 0;
	default:
		return gwmDefaultHandler(ev, optmenu);
	};
};

static void onOptmenuClose(void *context)
{
	GWMWindow *optmenu = (GWMWindow*) context;
	OptmenuData *data = (OptmenuData*) optmenu->data;
	data->state = OPTMENU_NORMAL;
	redrawOptmenu(optmenu);
};

GWMWindow* gwmCreateOptionMenu(GWMWindow *parent, uint64_t id, const char *text, int x, int y, int width, int flags)
{
	if (width < OPTMENU_MIN_WIDTH) width = OPTMENU_MIN_WIDTH;
	
	GWMWindow *optmenu = gwmCreateWindow(parent, "GWMOptionMenu", x, y, width, OPTMENU_HEIGHT, 0);
	if (optmenu == NULL) return NULL;
	
	OptmenuData *data = (OptmenuData*) malloc(sizeof(OptmenuData));
	if (data == NULL)
	{
		gwmDestroyWindow(optmenu);
		return NULL;
	};
	
	data->options = gwmCreateMenu();
	gwmMenuSetCloseCallback(data->options, onOptmenuClose, optmenu);
	data->width = width;
	data->state = OPTMENU_NORMAL;
	data->flags = flags;
	data->currentText = strdup(text);
	data->currentID = id;
	data->optStack = NULL;
	
	optmenu->data = data;
	redrawOptmenu(optmenu);
	gwmSetEventHandler(optmenu, optmenuHandler);
	return optmenu;
};

static void deleteOptions(GWMWindow *optmenu)
{
	OptmenuData *data = (OptmenuData*) optmenu->data;
	gwmDestroyMenu(data->options);
	
	while (data->optStack != NULL)
	{
		OptmenuOption *opt = data->optStack;
		data->optStack = opt->prev;
		free(opt->label);
		free(opt);
	};
};

void gwmDestroyOptionMenu(GWMWindow *optmenu)
{
	OptmenuData *data = (OptmenuData*) optmenu->data;
	deleteOptions(optmenu);
	free(data->currentText);
	free(data);
	gwmDestroyWindow(optmenu);
};

void gwmClearOptionMenu(GWMWindow *optmenu)
{
	OptmenuData *data = (OptmenuData*) optmenu->data;
	deleteOptions(optmenu);
	data->options = gwmCreateMenu();
	gwmMenuSetCloseCallback(data->options, onOptmenuClose, optmenu);
};

void gwmSetOptionMenu(GWMWindow *optmenu, uint64_t id, const char *text)
{
	OptmenuData *data = (OptmenuData*) optmenu->data;
	free(data->currentText);
	data->currentText = strdup(text);
	data->currentID = id;
	redrawOptmenu(optmenu);
};

static int onOptmenuOption(void *param)
{
	OptmenuOption *opt = (OptmenuOption*) param;
	
	gwmSetOptionMenu(opt->optmenu, opt->id, opt->label);
	gwmSetWindowFlags(opt->optmenu, GWM_WINDOW_MKFOCUSED);
	return 0;
};

void gwmAddOptionMenu(GWMWindow *optmenu, uint64_t id, const char *text)
{
	OptmenuData *data = (OptmenuData*) optmenu->data;
	
	OptmenuOption *opt = (OptmenuOption*) malloc(sizeof(OptmenuOption));
	opt->prev = data->optStack;
	opt->label = strdup(text);
	opt->id = id;
	opt->optmenu = optmenu;
	data->optStack = opt;
	
	gwmMenuAddEntry(data->options, text, onOptmenuOption, opt);
};

uint64_t gwmReadOptionMenu(GWMWindow *optmenu)
{
	OptmenuData *data = (OptmenuData*) optmenu->data;
	return data->currentID;
};

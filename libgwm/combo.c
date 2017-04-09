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

#define	COMBO_MIN_WIDTH		50
#define	COMBO_HEIGHT		20
#define	COMBO_BUTTON_WIDTH	20

enum
{
	COMBO_NORMAL,
	COMBO_HOVER,
	COMBO_DOWN
};

typedef struct ComboOption_
{
	struct ComboOption_*	prev;
	char*			label;
	GWMWindow*		combo;
} ComboOption;

typedef struct
{
	GWMWindow*		field;
	GWMMenu*		options;
	int			width;
	int			state;
	int			flags;
	ComboOption*		optStack;
} ComboData;

static DDISurface *imgCombo = NULL;

static void redrawCombo(GWMWindow *combo)
{
	ComboData *data = (ComboData*) combo->data;
	DDISurface *canvas = gwmGetWindowCanvas(combo);
	
	if (imgCombo == NULL)
	{
		const char *error;
		imgCombo = ddiLoadAndConvertPNG(&canvas->format, "/usr/share/images/combo.png", &error);
		if (imgCombo == NULL)
		{
			fprintf(stderr, "Failed to load combobox image (/usr/share/images/combo.png): %s\n", error);
			abort();
		};
	};
	
	DDIColor transparent = {0, 0, 0, 0};
	ddiFillRect(canvas, 0, 0, data->width, COMBO_HEIGHT, &transparent);
	
	int state = data->state;
	if (data->flags & GWM_COMBO_DISABLED)
	{
		state = 3;
	};
	
	ddiBlit(imgCombo, 0, COMBO_HEIGHT * state, canvas, data->width - COMBO_BUTTON_WIDTH, 0, COMBO_BUTTON_WIDTH, COMBO_HEIGHT);
	gwmPostDirty(combo);
};

static int comboHandler(GWMEvent *ev, GWMWindow *combo)
{
	ComboData *data = (ComboData*) combo->data;
	
	switch (ev->type)
	{
	case GWM_EVENT_ENTER:
	case GWM_EVENT_MOTION:
		if (ev->x >= (data->width-COMBO_BUTTON_WIDTH))
		{
			if (data->state != COMBO_DOWN)
			{
				data->state = COMBO_HOVER;
			};
		}
		else
		{
			if (data->state != COMBO_DOWN)
			{
				data->state = COMBO_NORMAL;
			};
		};
		redrawCombo(combo);
		return 0;
	case GWM_EVENT_LEAVE:
		if (data->state != COMBO_DOWN) data->state = COMBO_NORMAL;
		redrawCombo(combo);
		return 0;
	case GWM_EVENT_UP:
		if (ev->scancode == GWM_SC_MOUSE_LEFT)
		{
			if ((data->flags & GWM_COMBO_DISABLED) == 0)
			{
				data->state = COMBO_DOWN;
				redrawCombo(combo);
				gwmOpenMenu(data->options, combo, 0, COMBO_HEIGHT-1);
			};
		};
		return 0;
	default:
		return gwmDefaultHandler(ev, combo);
	};
};

static void onComboClose(void *context)
{
	GWMWindow *combo = (GWMWindow*) context;
	ComboData *data = (ComboData*) combo->data;
	data->state = COMBO_NORMAL;
	redrawCombo(combo);
};

GWMWindow* gwmCreateCombo(GWMWindow *parent, const char *text, int x, int y, int width, int flags)
{
	if (width < COMBO_MIN_WIDTH) width = COMBO_MIN_WIDTH;
	
	GWMWindow *combo = gwmCreateWindow(parent, "GWMCombo", x, y, width, COMBO_HEIGHT, 0);
	if (combo == NULL) return NULL;
	
	ComboData *data = (ComboData*) malloc(sizeof(ComboData));
	if (data == NULL)
	{
		gwmDestroyWindow(combo);
		return NULL;
	};
	
	data->field = gwmCreateTextField(combo, text, 0, 0, width-COMBO_BUTTON_WIDTH, flags);
	if (data->field == NULL)
	{
		gwmDestroyWindow(combo);
		free(data);
		return NULL;
	};
	
	data->options = gwmCreateMenu();
	gwmMenuSetCloseCallback(data->options, onComboClose, combo);
	data->width = width;
	data->state = COMBO_NORMAL;
	data->flags = flags;
	data->optStack = NULL;
	
	combo->data = data;
	redrawCombo(combo);
	gwmSetEventHandler(combo, comboHandler);
	return combo;
};

static void deleteOptions(GWMWindow *combo)
{
	ComboData *data = (ComboData*) combo->data;
	gwmDestroyMenu(data->options);
	
	while (data->optStack != NULL)
	{
		ComboOption *opt = data->optStack;
		data->optStack = opt->prev;
		free(opt->label);
		free(opt);
	};
};

void gwmDestroyCombo(GWMWindow *combo)
{
	ComboData *data = (ComboData*) combo->data;
	gwmDestroyTextField(data->field);
	deleteOptions(combo);
	free(data);
	gwmDestroyWindow(combo);
};

GWMWindow* gwmGetComboTextField(GWMWindow *combo)
{
	ComboData *data = (ComboData*) combo->data;
	return data->field;
};

void gwmClearComboOptions(GWMWindow *combo)
{
	ComboData *data = (ComboData*) combo->data;
	deleteOptions(combo);
	data->options = gwmCreateMenu();
	gwmMenuSetCloseCallback(data->options, onComboClose, combo);
};

static int onComboOption(void *param)
{
	ComboOption *opt = (ComboOption*) param;
	ComboData *data = (ComboData*) opt->combo->data;
	
	gwmWriteTextField(data->field, opt->label);
	gwmTextFieldSelectAll(data->field);
	gwmSetWindowFlags(data->field, GWM_WINDOW_MKFOCUSED);
	return 0;
};

void gwmAddComboOption(GWMWindow *combo, const char *label)
{
	ComboData *data = (ComboData*) combo->data;
	
	ComboOption *opt = (ComboOption*) malloc(sizeof(ComboOption));
	opt->prev = data->optStack;
	opt->label = strdup(label);
	opt->combo = combo;
	data->optStack = opt;
	
	gwmMenuAddEntry(data->options, label, onComboOption, opt);
};

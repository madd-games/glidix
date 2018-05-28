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

enum
{
	COMBO_NORMAL,
	COMBO_HOVER,
	COMBO_PRESSED
};

typedef struct ComboOption_
{
	struct ComboOption_*			next;
	char					text[];
} ComboOption;

typedef struct
{
	/**
	 * The text field inside the combo box.
	 */
	GWMTextField*				field;
	
	/**
	 * The layout for the combo box.
	 */
	GWMLayout*				layout;
	
	/**
	 * The menu.
	 */
	GWMMenu*				menu;
	
	/**
	 * Linked list of combo box options. This is needed so that the strings can be
	 * freed.
	 */
	ComboOption*				opts;
	
	/**
	 * Symbol for the menu commands.
	 */
	int					menusym;
	
	/**
	 * State.
	 */
	int					state;
} ComboData;

static DDISurface *imgCombo;

static int comboHandler(GWMEvent *ev, GWMCombo *combo, void *context);

static void redrawCombo(GWMCombo *combo)
{
	ComboData *data = (ComboData*) gwmGetData(combo, comboHandler);
	DDISurface *canvas = gwmGetWindowCanvas(combo);
	
	if (imgCombo == NULL)
	{
		imgCombo = (DDISurface*) gwmGetThemeProp("gwm.toolkit.combo", GWM_TYPE_SURFACE, NULL);
		if (imgCombo == NULL)
		{
			fprintf(stderr, "Failed to load combo image\n");
			abort();
		};
	};
	
	static DDIColor transparent = {0x00, 0x00, 0x00, 0x00};
	ddiFillRect(canvas, 0, 0, canvas->width, canvas->height, &transparent);
	
	ddiBlit(imgCombo, 0, 20*data->state, canvas, canvas->width-20, 0, 20, 20);
	gwmPostDirty(combo);
};

static int comboHandler(GWMEvent *ev, GWMCombo *combo, void *context)
{
	ComboData *data = (ComboData*) context;
	GWMCommandEvent *cmdev = (GWMCommandEvent*) ev;
	
	switch (ev->type)
	{
	case GWM_EVENT_RESIZED:
		redrawCombo(combo);
		return GWM_EVSTATUS_OK;
	case GWM_EVENT_ENTER:
		if (data->state != COMBO_PRESSED) data->state = COMBO_HOVER;
		redrawCombo(combo);
		return GWM_EVSTATUS_OK;
	case GWM_EVENT_LEAVE:
		if (data->state != COMBO_PRESSED) data->state = COMBO_NORMAL;
		redrawCombo(combo);
		return GWM_EVSTATUS_OK;
	case GWM_EVENT_DOWN:
		if (ev->keycode == GWM_KC_MOUSE_LEFT)
		{
			data->state = COMBO_PRESSED;
			redrawCombo(combo);
			gwmOpenMenu(data->menu, combo, 0, 20);
		};
		return GWM_EVSTATUS_CONT;
	case GWM_EVENT_MENU_CLOSE:
		data->state = COMBO_NORMAL;
		redrawCombo(combo);
		return GWM_EVSTATUS_OK;
	case GWM_EVENT_COMMAND:
		gwmWriteTextField(data->field, (char*) cmdev->data);
		gwmSetWindowFlags(data->field, GWM_WINDOW_MKFOCUSED);
		return GWM_EVSTATUS_OK;
	default:
		return GWM_EVSTATUS_CONT;
	};
};

GWMCombo* gwmNewCombo(GWMWindow *parent)
{
	GWMCombo *combo = gwmCreateWindow(parent, "GWMCombo", 0, 0, 0, 0, 0);
	if (combo == NULL) return NULL;
	
	ComboData *data = (ComboData*) malloc(sizeof(ComboData));
	data->layout = gwmCreateBoxLayout(GWM_BOX_HORIZONTAL);
	gwmSetWindowLayout(combo, data->layout);
	
	data->field = gwmNewTextField(combo);
	gwmBoxLayoutAddWindow(data->layout, data->field, 1, 20, GWM_BOX_RIGHT | GWM_BOX_FILL);
	
	data->menu = gwmCreateMenu();
	data->opts = NULL;
	data->menusym = gwmGenerateSymbol();
	data->state = COMBO_NORMAL;
	
	gwmPushEventHandler(combo, comboHandler, data);
	return combo;
};

static void deleteOptions(GWMCombo *combo, ComboData *data)
{
	gwmDestroyMenu(data->menu);
	
	while (data->opts != NULL)
	{
		ComboOption *opt = data->opts;
		data->opts = opt->next;
		free(opt);
	};
};

void gwmDestroyCombo(GWMCombo *combo)
{
	ComboData *data = (ComboData*) gwmGetData(combo, comboHandler);
	gwmSetWindowLayout(combo, NULL);
	gwmDestroyBoxLayout(data->layout);
	gwmDestroyTextField(data->field);
	gwmDestroyWindow(combo);
	deleteOptions(combo, data);
	free(data);
};

void gwmAddComboOption(GWMCombo *combo, const char *option)
{
	ComboData *data = (ComboData*) gwmGetData(combo, comboHandler);
	ComboOption *opt = (ComboOption*) malloc(sizeof(ComboOption) + strlen(option) + 1);
	opt->next = data->opts;
	data->opts = opt;
	strcpy(opt->text, option);
	
	gwmMenuAddCommand(data->menu, data->menusym, option, opt->text);
};

void gwmClearComboOptions(GWMCombo *combo)
{
	ComboData *data = (ComboData*) gwmGetData(combo, comboHandler);
	deleteOptions(combo, data);
	data->menu = gwmCreateMenu();
};

void gwmWriteCombo(GWMCombo *combo, const char *text)
{
	ComboData *data = (ComboData*) gwmGetData(combo, comboHandler);
	gwmWriteTextField(data->field, text);
};

const char* gwmReadCombo(GWMCombo *combo)
{
	ComboData *data = (ComboData*) gwmGetData(combo, comboHandler);
	return gwmReadTextField(data->field);
};

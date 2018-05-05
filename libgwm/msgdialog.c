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

#define	MSG_MAX_BUTTONS				4

static DDISurface *imgMessageIcons;

typedef struct
{
	/**
	 * The text to be displayed.
	 */
	char *text;
	
	/**
	 * Symbols and labels of buttons to display; and the number of buttons.
	 */
	int symbols[MSG_MAX_BUTTONS];
	char *labels[MSG_MAX_BUTTONS];
	int numButtons;
	
	/**
	 * Icon surface and viewport. In most cases this will be from the "mbicons" image.
	 */
	DDISurface *surf;
	int x, y;
	int width, height;
	
	/**
	 * The parent, if any.
	 */
	GWMWindow *parent;
	
	/**
	 * The answer.
	 */
	int answer;
} MessageData;

static int msgHandler(GWMEvent *ev, GWMWindow *win, void *context)
{
	MessageData *data = (MessageData*) context;
	
	switch (ev->type)
	{
	case GWM_EVENT_COMMAND:
		data->answer = ((GWMCommandEvent*)ev)->symbol;
		return GWM_EVSTATUS_BREAK;
	default:
		return GWM_EVSTATUS_CONT;
	};
};

GWMWindow* gwmNewMessageDialog(GWMWindow *parent)
{
	GWMWindow *win = gwmCreateModal("GWMMessageDialog", GWM_POS_UNSPEC, GWM_POS_UNSPEC, 0, 0);
	
	MessageData *data = (MessageData*) malloc(sizeof(MessageData));
	data->text = strdup("");
	data->numButtons = 0;
	data->surf = NULL;
	data->x = data->y = 0;
	data->width = data->height = 0;
	data->parent = parent;
	
	if (imgMessageIcons == NULL)
	{
		int error;
		imgMessageIcons = (DDISurface*) gwmGetThemeProp("gwm.toolkit.mbicons", GWM_TYPE_SURFACE, &error);
		if (imgMessageIcons == NULL)
		{
			fprintf(stderr, "libgwm: could not get theme property `gwm.toolkit.mbicons': error %d\n", error);
			abort();
		};
	};
	
	gwmPushEventHandler(win, msgHandler, data);
	return win;
};

void gwmSetMessageText(GWMWindow *msg, const char *text)
{
	MessageData *data = (MessageData*) gwmGetData(msg, msgHandler);
	free(data->text);
	data->text = strdup(text);
};

void gwmSetMessageIconCustom(GWMWindow *msg, DDISurface *surf, int x, int y, int width, int height)
{
	MessageData *data = (MessageData*) gwmGetData(msg, msgHandler);
	data->surf = surf;
	data->x = x;
	data->y = y;
	data->width = width;
	data->height = height;
};

void gwmSetMessageIconStd(GWMWindow *msg, int inum)
{
	MessageData *data = (MessageData*) gwmGetData(msg, msgHandler);
	if (inum == 0)
	{
		data->surf = NULL;
		data->x = 0;
		data->y = 0;
		data->width = 0;
		data->height = 0;
	}
	else
	{
		data->surf = imgMessageIcons;
		data->x = 32 * (inum-1);
		data->y = 0;
		data->width = 32;
		data->height = 32;
	};
};

int gwmMessageAddStockButton(GWMWindow *msg, int symbol)
{
	MessageData *data = (MessageData*) gwmGetData(msg, msgHandler);
	if (data->numButtons == MSG_MAX_BUTTONS)
	{
		return GWM_ERR_NORC;
	};
	
	int index = data->numButtons++;
	data->symbols[index] = symbol;
	data->labels[index] = strdup(gwmGetStockLabel(symbol));
	return 0;
};

int gwmMessageAddButton(GWMWindow *msg, int symbol, const char *label)
{
	MessageData *data = (MessageData*) gwmGetData(msg, msgHandler);
	if (data->numButtons == MSG_MAX_BUTTONS)
	{
		return GWM_ERR_NORC;
	};
	
	int index = data->numButtons++;
	data->symbols[index] = symbol;
	data->labels[index] = strdup(label);
	return 0;
};

int gwmRunMessageDialog(GWMWindow *msg)
{
	MessageData *data = (MessageData*) gwmGetData(msg, msgHandler);
	
	GWMLayout *rootLayout = gwmCreateBoxLayout(GWM_BOX_VERTICAL);
	gwmSetWindowLayout(msg, rootLayout);
	
	GWMLayout *topBox = gwmCreateBoxLayout(GWM_BOX_HORIZONTAL);
	gwmBoxLayoutAddLayout(rootLayout, topBox, 0, 0, 0);
	
	GWMLayout *bottomBox = gwmCreateBoxLayout(GWM_BOX_HORIZONTAL);
	gwmBoxLayoutAddLayout(rootLayout, bottomBox, 0, 0, 0);
	
	GWMWindow *image;
	GWMWindow *label;
	
	gwmBoxLayoutAddWindow(topBox, image = gwmCreateImage(msg, data->surf, data->x, data->y, data->width, data->height),
		0, 5, GWM_BOX_ALL);
	gwmBoxLayoutAddWindow(topBox, label = gwmCreateLabel(msg, data->text, 400), 0, 5, GWM_BOX_ALL);
	
	// add all the buttons to the bottom box
	int i;
	GWMWindow *buttons[MSG_MAX_BUTTONS];
	for (i=0; i<data->numButtons; i++)
	{
		gwmBoxLayoutAddWindow(bottomBox, buttons[i] = gwmCreateButtonWithLabel(msg, data->symbols[i], data->labels[i]),
			1, 2, GWM_BOX_ALL);
		gwmSetButtonPrefWidth(buttons[i], 60);
	};
	
	gwmFit(msg);
	gwmRunModal(msg, GWM_WINDOW_NOTASKBAR | GWM_WINDOW_NOSYSMENU | GWM_WINDOW_MKFOCUSED);
	gwmSetWindowFlags(msg, GWM_WINDOW_HIDDEN | GWM_WINDOW_NOTASKBAR);
	
	int answer = data->answer;
	
	for (i=0; i<data->numButtons; i++)
	{
		gwmDestroyButton(buttons[i]);
	};
	
	gwmDestroyImage(image);
	gwmDestroyLabel(label);
	gwmDestroyBoxLayout(topBox);
	gwmDestroyBoxLayout(bottomBox);
	gwmDestroyBoxLayout(rootLayout);
	gwmDestroyWindow(msg);
	free(data);
	
	return answer;
};

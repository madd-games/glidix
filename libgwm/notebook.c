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

typedef struct
{
	DDISurface*			label;
	GWMWindow*			win;
} Tab;

typedef struct
{
	int				numTabs;
	Tab*				tabs;
	int				currentTab;
	int				selectedTab;
	int				width, height;
} GWMNotebookData;

static DDISurface *imgNotebook = NULL;

static void gwmRedrawNotebook(GWMWindow *notebook)
{
	static DDIColor colorTransparent = {0, 0, 0, 0};
	static DDIColor colorBlack = {0, 0, 0, 0xFF};
	
	GWMNotebookData *data = (GWMNotebookData*) notebook->data;
	
	DDISurface *canvas = gwmGetWindowCanvas(notebook);
	ddiFillRect(canvas, 0, 0, data->width, imgNotebook->height/3, &colorTransparent);
	ddiFillRect(canvas, 0, imgNotebook->height/3-1, data->width, data->height-imgNotebook->height/3+1, &colorBlack);
	
	int borderWidth = imgNotebook->width/2;
	int tabHeight = imgNotebook->height/3;
	
	int i;
	int xoff = 0;
	for (i=0; i<data->numTabs; i++)
	{
		int yi = 0;
		
		if (data->selectedTab == i)
		{
			yi = 1;
		};
		
		if (data->currentTab == i)
		{
			yi = 2;
		};
		
		ddiBlit(imgNotebook, 0, yi*tabHeight, canvas, xoff, 0, borderWidth, tabHeight);
		xoff += borderWidth;
		
		int drawAt = xoff;
		
		int j;
		for (j=0; j<data->tabs[i].label->width; j++)
		{
			ddiBlit(imgNotebook, borderWidth, yi*tabHeight, canvas, xoff, 0, 1, tabHeight);
			xoff++;
		};
		
		ddiBlit(imgNotebook, borderWidth+1, yi*tabHeight, canvas, xoff, 0, borderWidth, tabHeight);
		xoff += borderWidth;
		
		ddiBlit(data->tabs[i].label, 0, 0, canvas, drawAt, 2, data->tabs[i].label->width, data->tabs[i].label->height);
		
		// we put the ends of tabs on top of each other
		xoff--;
	};
	
	gwmPostDirty(notebook);
};

int notebookHandler(GWMEvent *ev, GWMWindow *notebook, void *context)
{
	GWMNotebookData *data = (GWMNotebookData*) notebook->data;
	int newSelectedTab;
	int switchTab = 0;
	
	switch (ev->type)
	{
	case GWM_EVENT_LEAVE:
		newSelectedTab = -1;
		break;
	case GWM_EVENT_DOWN:
		if (ev->keycode != GWM_KC_MOUSE_LEFT)
		{
			return GWM_EVSTATUS_OK;
		};
		switchTab = 1;
		// no break
	case GWM_EVENT_ENTER:
	case GWM_EVENT_MOTION:
		for (newSelectedTab=0; newSelectedTab<data->numTabs; newSelectedTab++)
		{
			if ((ev->x >= 0) && (ev->x < (data->tabs[newSelectedTab].label->width+imgNotebook->width-2)))
			{
				break;
			};
			
			ev->x -= (data->tabs[newSelectedTab].label->width+imgNotebook->width-2);
		};
		
		if (newSelectedTab == data->numTabs)
		{
			newSelectedTab = -1;
		};
		
		break;
	default:
		return GWM_EVSTATUS_CONT;
	};
	
	if (newSelectedTab != data->selectedTab)
	{
		data->selectedTab = newSelectedTab;
		gwmRedrawNotebook(notebook);
	};
	
	if (switchTab)
	{
		if (newSelectedTab != -1)
		{
			gwmNotebookSetTab(notebook, newSelectedTab);
		};
	};

	return GWM_EVSTATUS_OK;
};

GWMWindow *gwmCreateNotebook(GWMWindow *parent, int x, int y, int width, int height, int flags)
{
	GWMWindow *notebook = gwmCreateWindow(parent, "GWMNotebook", x, y, width, height, 0);
	GWMNotebookData *data = (GWMNotebookData*) malloc(sizeof(GWMNotebookData));
	
	DDISurface *canvas = gwmGetWindowCanvas(notebook);
	if (imgNotebook == NULL)
	{
		imgNotebook = ddiLoadAndConvertPNG(&canvas->format, "/usr/share/images/notebook.png", NULL);
		if (imgNotebook == NULL)
		{
			fprintf(stderr, "Failed to load /usr/share/images/notebook.png");
			abort();
		};
	};
	
	data->numTabs = 0;
	data->tabs = NULL;
	data->currentTab = -1;
	data->selectedTab = -1;
	data->width = width;
	data->height = height;
	
	notebook->data = data;
	gwmRedrawNotebook(notebook);
	gwmPushEventHandler(notebook, notebookHandler, NULL);
	return notebook;
};

void gwmNotebookGetDisplaySize(GWMWindow *notebook, int *width, int *height)
{
	GWMNotebookData *data = (GWMNotebookData*) notebook->data;
	
	if (width != NULL) *width = data->width - 2;
	if (height != NULL) *height = data->height - 1 - imgNotebook->height/3;
};

GWMWindow *gwmNotebookAdd(GWMWindow *notebook, const char *label)
{
	static DDIColor colorTransparent = {0, 0, 0, 0};
	GWMNotebookData *data = (GWMNotebookData*) notebook->data;
	
	int width, height;
	gwmNotebookGetDisplaySize(notebook, &width, &height);
	
	int index = data->numTabs++;
	data->tabs = realloc(data->tabs, sizeof(Tab)*data->numTabs);
	Tab *tab = &data->tabs[index];
	
	tab->win = gwmCreateWindow(notebook, "GWMNotebookTab", 1, imgNotebook->height/3, width, height, GWM_WINDOW_HIDDEN);
	
	DDISurface *canvas = gwmGetWindowCanvas(tab->win);
	DDIPen *pen = ddiCreatePen(&canvas->format, gwmGetDefaultFont(), 0, 0, 100, imgNotebook->height/3, 0, 0, NULL);
	ddiSetPenWrap(pen, 0);
	ddiWritePen(pen, label);
	ddiGetPenSize(pen, &width, &height);
	
	tab->label = ddiCreateSurface(&canvas->format, width, height, NULL, 0);
	ddiFillRect(tab->label, 0, 0, width, height, &colorTransparent);
	ddiExecutePen(pen, tab->label);
	ddiDeletePen(pen);
	
	gwmRedrawNotebook(notebook);
	return tab->win;
};

void gwmNotebookSetTab(GWMWindow *notebook, int index)
{
	GWMNotebookData *data = (GWMNotebookData*) notebook->data;
	
	if (data->currentTab != -1)
	{
		gwmSetWindowFlags(data->tabs[data->currentTab].win, GWM_WINDOW_HIDDEN);
	};
	
	if ((index < 0) || (index >= data->numTabs))
	{
		data->currentTab = -1;
	}
	else
	{
		data->currentTab = index;
		gwmSetWindowFlags(data->tabs[index].win, 0);
	};
	
	gwmRedrawNotebook(notebook);
};

void gwmDestroyNotebook(GWMWindow *notebook)
{
	GWMNotebookData *data = (GWMNotebookData*) notebook->data;
	
	int i;
	for (i=0; i<data->numTabs; i++)
	{
		ddiDeleteSurface(data->tabs[i].label);
		gwmDestroyWindow(data->tabs[i].win);
	};
	
	free(data->tabs);
	free(data);
	gwmDestroyWindow(notebook);
};

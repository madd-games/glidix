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
	int			flags;
	int*			widths;
	int			hoveredTab;
} NotebookData;

static DDISurface *imgNotebook = NULL;

static int notebookHandler(GWMEvent *ev, GWMWindow *tablist, void *context);

static void redrawNotebook(GWMWindow *notebook)
{
	NotebookData *data = (NotebookData*) gwmGetData(notebook, notebookHandler);
	DDISurface *canvas = gwmGetWindowCanvas(notebook);
	if (canvas->width == 0) return;

	if (imgNotebook == NULL)
	{
		imgNotebook = (DDISurface*) gwmGetThemeProp("gwm.toolkit.notebook", GWM_TYPE_SURFACE, NULL);
		if (imgNotebook == NULL)
		{
			fprintf(stderr, "Failed to load notebook image\n");
			abort();
		};
	};
	
	static DDIColor black = {0, 0, 0, 0xFF};
	ddiFillRect(canvas, 0, 0, canvas->width, canvas->height, GWM_COLOR_BACKGROUND);
	ddiFillRect(canvas, 0, 20, 1, canvas->height-20, &black);
	ddiFillRect(canvas, 0, canvas->height-1, canvas->width, 1, &black);
	ddiFillRect(canvas, canvas->width-1, 20, 1, canvas->height-20, &black);
	ddiFillRect(canvas, 0, 19, canvas->width, 1, &black);
	
	// draw tabs
	GWMWindow* activeTab = gwmGetActiveTab(notebook);
	int i;
	int drawX = 0;
	free(data->widths);
	data->widths = NULL;
	for (i=0;;i++)
	{
		GWMWindow *tab = gwmGetTab(notebook, i);
		if (tab == NULL) break;
		
		int state = 0;
		if (tab == activeTab)
		{
			gwmSetWindowFlags(tab, 0);
			state = 2;
		}
		else
		{
			gwmSetWindowFlags(tab, GWM_WINDOW_HIDDEN);
			
			if (data->hoveredTab == i)
			{
				state = 1;
			};
		};
		
		DDIPen *pen = ddiCreatePen(&canvas->format, gwmGetDefaultFont(), drawX+20, 2, canvas->width, canvas->height, 0, 0, NULL);
		ddiSetPenWrap(pen, 0);
		ddiWritePen(pen, gwmGetWindowCaption(tab));
	
		int txtWidth, txtHeight;
		ddiGetPenSize(pen, &txtWidth, &txtHeight);
		
		int tabWidth = txtWidth + 22;
		int middleWidth = tabWidth - 16;
		int iconX = drawX + 2;
		
		ddiBlit(imgNotebook, 0, state*20, canvas, drawX, 0, 8, 20);
		drawX += 8;
		while (middleWidth--)
		{
			ddiBlit(imgNotebook, 8, state*20, canvas, drawX++, 0, 1, 20);
		};
		ddiBlit(imgNotebook, 9, state*20, canvas, drawX, 0, 8, 20);
		drawX += 8;
		
		ddiExecutePen(pen, canvas);
		ddiDeletePen(pen);
		
		DDISurface *icon = gwmGetWindowIcon(tab);
		if (icon != NULL)
		{
			ddiBlit(icon, 0, 0, canvas, iconX, 2, 16, 16);
		};
		
		data->widths = realloc(data->widths, sizeof(int) * (i+1));
		data->widths[i] = tabWidth;
	};
	
	data->widths = realloc(data->widths, sizeof(int) * (i+1));
	data->widths[i] = -1;
	
	gwmPostDirty(notebook);
};

static int notebookHandler(GWMEvent *ev, GWMWindow *notebook, void *context)
{
	NotebookData *data = (NotebookData*) context;
	int newTab;
	
	switch (ev->type)
	{
	case GWM_EVENT_ENTER:
	case GWM_EVENT_MOTION:
		if (ev->y < 0 || ev->y > 20 || ev->x < 0 || data->widths == NULL)
		{
			newTab = -1;
		}
		else
		{
			int i;
			for (i=0;;i++)
			{
				if (data->widths[i] == -1)
				{
					newTab = -1;
					break;
				};
				
				if (data->widths[i] > ev->x)
				{
					newTab = i;
					break;
				};
				
				ev->x -= data->widths[i];
			};
		};
		if (data->hoveredTab != newTab)
		{
			data->hoveredTab = newTab;
			redrawNotebook(notebook);
		};
		return GWM_EVSTATUS_CONT;
	case GWM_EVENT_LEAVE:
		data->hoveredTab = -1;
		redrawNotebook(notebook);
		return GWM_EVSTATUS_CONT;
	case GWM_EVENT_DOWN:
		if (ev->keycode == GWM_KC_MOUSE_LEFT)
		{
			GWMWindow *tab = gwmGetTab(notebook, data->hoveredTab);
			if (tab != NULL) gwmActivateTab(tab);
		};
		return GWM_EVSTATUS_CONT;
	case GWM_EVENT_TAB_LIST_UPDATED:
		redrawNotebook(notebook);
		return GWM_EVSTATUS_CONT;
	default:
		return GWM_EVSTATUS_CONT;
	};
};

static void sizeNotebook(GWMWindow *notebook, int *width, int *height)
{
	int maxWidth=0, maxHeight=0;
	
	int i;
	for (i=0;;i++)
	{
		GWMWindow *tab = gwmGetTab(notebook, i);
		if (tab == NULL) break;
		
		int w, h;
		if (tab->layout != NULL)
		{
			tab->layout->getMinSize(tab->layout, &w, &h);
		}
		else
		{
			w = 150;
			h = 150;
		};
			
		if (w > maxWidth) maxWidth = w;
		if (h > maxHeight) maxHeight = h;
	};
	
	*width = maxWidth + 2;
	*height = maxHeight + 21;
};

static void positionNotebook(GWMWindow *notebook, int x, int y, int width, int height)
{
	gwmMoveWindow(notebook, x, y);
	gwmResizeWindow(notebook, width, height);
	
	if (width < 2) width = 2;
	if (height < 21) height = 21;
	
	int i;
	for (i=0;;i++)
	{
		GWMWindow *tab = gwmGetTab(notebook, i);
		if (tab == NULL) break;
		
		gwmMoveWindow(tab, 1, 20);
		gwmLayout(tab, width-2, height-21);
	};
	
	redrawNotebook(notebook);
};

GWMWindow* gwmNewNotebook(GWMWindow *parent)
{
	GWMWindow *notebook = gwmNewTabList(parent);
	if (notebook == NULL) return NULL;
	
	NotebookData *data = (NotebookData*) malloc(sizeof(NotebookData));
	data->flags = 0;
	data->widths = NULL;
	data->hoveredTab = -1;
	
	notebook->getMinSize = notebook->getPrefSize = sizeNotebook;
	notebook->position = positionNotebook;

	gwmPushEventHandler(notebook, notebookHandler, data);
	return notebook;
};

void gwmDestroyNotebook(GWMWindow *notebook)
{
	gwmDestroyTabList(notebook);
};

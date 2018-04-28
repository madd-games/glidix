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

typedef struct
{
	GWMWindow**		tabs;
	int			numTabs;
	int			currentTab;
} TabListData;

static int tablistHandler(GWMEvent *ev, GWMWindow *tablist, void *context)
{
	return GWM_EVSTATUS_CONT;
};

GWMWindow* gwmNewTabList(GWMWindow *parent)
{
	GWMWindow *tablist = gwmCreateWindow(parent, "GWMTabList", 0, 0, 0, 0, 0);
	if (tablist == NULL) return NULL;
	
	TabListData *data = (TabListData*) malloc(sizeof(TabListData));
	data->tabs = NULL;
	data->numTabs = 0;
	data->currentTab = -1;
	
	gwmPushEventHandler(tablist, tablistHandler, data);
	return tablist;
};

void gwmDestroyTabList(GWMWindow *tablist)
{
	TabListData *data = (TabListData*) gwmGetData(tablist, tablistHandler);
	int i;
	for (i=0; i<data->numTabs; i++)
	{
		gwmDestroyWindow(data->tabs[i]);
	};
	
	free(data->tabs);
	free(data);
};

GWMWindow* gwmNewTab(GWMWindow *tablist)
{
	TabListData *data = (TabListData*) gwmGetData(tablist, tablistHandler);
	GWMWindow *tab = gwmCreateWindow(tablist, "GWMTab", 0, 0, 0, 0, GWM_WINDOW_HIDDEN);
	if (tab == NULL) return NULL;
	
	int index = data->numTabs++;
	data->tabs = (GWMWindow**) realloc(data->tabs, sizeof(GWMWindow*) * data->numTabs);
	data->tabs[index] = tab;

	if (data->numTabs == 1)
	{
		data->currentTab = 0;
	};
	
	GWMTabEvent ev;
	memset(&ev, 0, sizeof(GWMTabEvent));
	ev.header.type = GWM_EVENT_TAB_LIST_UPDATED;
	
	gwmPostEvent((GWMEvent*) &ev, tablist);
	return tab;
};

GWMWindow* gwmGetTab(GWMWindow *tablist, int index)
{
	TabListData *data = (TabListData*) gwmGetData(tablist, tablistHandler);
	if ((index < 0) || (index >= data->numTabs)) return NULL;
	return data->tabs[index];
};

void gwmDestroyTab(GWMWindow *tab)
{
	TabListData *data = (TabListData*) gwmGetData(tab->parent, tablistHandler);
	
	int i;
	for (i=0; i<data->numTabs; i++)
	{
		if (data->tabs[i] == tab) break;
	};
	
	if (i != data->numTabs)
	{
		GWMWindow **newList = (GWMWindow**) malloc(sizeof(GWMWindow*) * (data->numTabs-1));
		memcpy(newList, data->tabs, sizeof(GWMWindow*) * i);
		memcpy(&newList[i], &data->tabs[i+1], sizeof(GWMWindow*) * (data->numTabs-i-1));
		data->numTabs--;
		free(data->tabs);
		data->tabs = newList;
	};
	
	GWMWindow *tablist = tab->parent;
	gwmDestroyWindow(tab);
	
	if (data->numTabs >= data->currentTab)
	{
		data->currentTab = data->numTabs-1;
	};
	
	GWMTabEvent ev;
	memset(&ev, 0, sizeof(GWMTabEvent));
	ev.header.type = GWM_EVENT_TAB_LIST_UPDATED;
	
	gwmPostEvent((GWMEvent*) &ev, tablist);
};

void gwmActivateTab(GWMWindow *tab)
{
	TabListData *data = (TabListData*) gwmGetData(tab->parent, tablistHandler);
	
	int i;
	for (i=0; i<data->numTabs; i++)
	{
		if (data->tabs[i] == tab) break;
	};
	
	if (i == data->numTabs)
	{
		return;
	};
	
	// currentTab cannot be -1 when there are tabs, so we can safely ignore this case
	
	GWMTabEvent ev;
	memset(&ev, 0, sizeof(GWMTabEvent));
	ev.header.type = GWM_EVENT_TAB_FADING;
	ev.tab = data->tabs[data->currentTab];
	
	if (gwmPostEvent((GWMEvent*) &ev, tab->parent) == GWM_EVSTATUS_DEFAULT)
	{
		// change the current tab number and then emit the "faded" event for the old tab
		int oldTab = data->currentTab;
		data->currentTab = i;
		
		memset(&ev, 0, sizeof(GWMTabEvent));
		ev.header.type = GWM_EVENT_TAB_FADED;
		ev.tab = data->tabs[oldTab];
		
		gwmPostEvent((GWMEvent*) &ev, tab->parent);
		
		// inform the subclass that the tab list has been updated (so that it can redraw/hide/show etc)
		memset(&ev, 0, sizeof(GWMTabEvent));
		ev.header.type = GWM_EVENT_TAB_LIST_UPDATED;
		
		gwmPostEvent((GWMEvent*) &ev, tab->parent);
	};
};

GWMWindow* gwmGetActiveTab(GWMWindow *tablist)
{
	TabListData *data = (TabListData*) gwmGetData(tablist, tablistHandler);
	if (data->numTabs == 0) return NULL;
	return data->tabs[data->currentTab];
};

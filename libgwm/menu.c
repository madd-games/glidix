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

#include <sys/glidix.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <signal.h>
#include <libgwm.h>
#include <errno.h>

#define	MENU_WIDTH			150
#define	MENU_ENTRY_HEIGHT		20

GWMMenu *gwmCreateMenu()
{
	GWMMenu *menu = (GWMMenu*) malloc(sizeof(GWMMenu));
	menu->numEntries = 0;
	menu->entries = NULL;
	menu->win = NULL;
	menu->closeCallback = NULL;
	return menu;
};

void gwmMenuAddEntry(GWMMenu *menu, const char *label, GWMMenuCallback callback, void *param)
{
	menu->entries = (GWMMenuEntry*) realloc(menu->entries, sizeof(GWMMenuEntry)*(menu->numEntries+1));
	int index = menu->numEntries++;
	
	memset(&menu->entries[index], 0, sizeof(GWMMenuEntry));
	menu->entries[index].label = strdup(label);
	menu->entries[index].callback = callback;
	menu->entries[index].param = param;
};

static void closeSubMenu(void *context)
{
	GWMMenu *menu = (GWMMenu*) context;
	
	if (menu->selectedSub != -1)
	{
		if (menu->entries[menu->selectedSub].submenu->win == NULL)
		{
			gwmCloseMenu(menu);
		};
	};
};

GWMMenu *gwmMenuAddSub(GWMMenu *menu, const char *label)
{
	GWMMenu *submenu = gwmCreateMenu();
	
	menu->entries = (GWMMenuEntry*) realloc(menu->entries, sizeof(GWMMenuEntry)*(menu->numEntries+1));
	int index = menu->numEntries++;
	
	memset(&menu->entries[index], 0, sizeof(GWMMenuEntry));
	menu->entries[index].label = strdup(label);
	menu->entries[index].submenu = submenu;
	
	gwmMenuSetCloseCallback(submenu, closeSubMenu, menu);
	return submenu;
};

void gwmMenuAddSeparator(GWMMenu *menu)
{
	if (menu->numEntries > 0)
	{
		menu->entries[menu->numEntries-1].flags |= GWM_ME_SEPARATOR;
	};
};

void gwmMenuSetIcon(GWMMenu *menu, DDISurface *icon)
{
	if (menu->numEntries > 0)
	{
		menu->entries[menu->numEntries-1].icon = icon;
	};
};

static void redrawMenu(GWMMenu *menu, int selectPos)
{
	DDISurface *canvas = gwmGetWindowCanvas(menu->win);
	ddiOverlay(menu->background, 0, 0, canvas, 0, 0, MENU_WIDTH, MENU_ENTRY_HEIGHT*menu->numEntries);
	
	if (selectPos != -1)
	{
		ddiFillRect(canvas, 1, MENU_ENTRY_HEIGHT*selectPos+1, MENU_WIDTH-2, MENU_ENTRY_HEIGHT-2, GWM_COLOR_SELECTION);
	};
	
	ddiBlit(menu->overlay, 0, 0, canvas, 0, 0, MENU_WIDTH, MENU_ENTRY_HEIGHT*menu->numEntries);
	gwmPostDirty(menu->win);
};

static int menuEventHandler(GWMEvent *ev, GWMWindow *win)
{
	GWMMenu *menu = (GWMMenu*) win->data;
	int i;
	int selected;
	
	switch (ev->type)
	{
	case GWM_EVENT_ENTER:
	case GWM_EVENT_MOTION:
		i = ev->y/MENU_ENTRY_HEIGHT;
		redrawMenu(menu, i);
		if (i < menu->numEntries)
		{
			if (i != menu->selectedSub)
			{
				if (menu->selectedSub != -1)
				{
					selected = menu->selectedSub;
					menu->selectedSub = -1;
					
					if (menu->entries[selected].submenu != NULL)
					{
						gwmCloseMenu(menu->entries[selected].submenu);
					};
					
					gwmSetWindowFlags(win,
						GWM_WINDOW_MKFOCUSED | GWM_WINDOW_NOTASKBAR | GWM_WINDOW_NODECORATE);
				};
				
				if (menu->entries[i].submenu != NULL)
				{
					menu->selectedSub = i;
					gwmOpenMenu(menu->entries[i].submenu, win, MENU_WIDTH-1, MENU_ENTRY_HEIGHT*i);
				};
			};
		}
		else
		{
			menu->selectedSub = -1;
		};
		return 0;
	case GWM_EVENT_LEAVE:
		redrawMenu(menu, menu->selectedSub);
		return 0;
	case GWM_EVENT_FOCUS_OUT:
		menu->focused = 0;
		if (menu->selectedSub == -1) gwmCloseMenu(menu);
		return 0;
	case GWM_EVENT_FOCUS_IN:
		menu->focused = 1;
		return 0;
	case GWM_EVENT_UP:
		if (ev->scancode == GWM_SC_MOUSE_LEFT)
		{
			gwmCloseMenu(menu);
			i = ev->y/MENU_ENTRY_HEIGHT;
			if (i < menu->numEntries)
			{
				if (menu->entries[i].callback != NULL)
				{
					return menu->entries[i].callback(menu->entries[i].param);
				};
			};
		};
		return 0;
	default:
		return gwmDefaultHandler(ev, win);
	};
};

void gwmOpenMenu(GWMMenu *menu, GWMWindow *win, int relX, int relY)
{
	static DDISurface *imgSubmenu = NULL;	
	if (imgSubmenu == NULL)
	{
		DDISurface *winCanvas = gwmGetWindowCanvas(win);
		imgSubmenu = ddiLoadAndConvertPNG(&winCanvas->format, "/usr/share/images/submenu.png", NULL);
		if (imgSubmenu == NULL)
		{
			fprintf(stderr, "Failed to load /usr/share/images/submenu.png");
			abort();
		};
	};
	
	static DDIColor menuBackground = {0xFF, 0xFF, 0xFF, 0xFF};
	static DDIColor menuBorders = {0x00, 0x00, 0x00, 0xFF};
	static DDIColor menuTransparent = {0x00, 0x00, 0x00, 0x00};
	
	int screenWidth, screenHeight;
	gwmScreenSize(&screenWidth, &screenHeight);
	
	int x, y;
	gwmRelToAbs(win, relX, relY, &x, &y);
	
	int menuHeight = MENU_ENTRY_HEIGHT * menu->numEntries;
	
	if ((y + menuHeight) > screenHeight)
	{
		y -= menuHeight;
	};
	
	if ((x + MENU_WIDTH) > screenWidth)
	{
		x -= MENU_WIDTH;
	};
	
	if (menu->win != NULL)
	{
		gwmCloseMenu(menu);
	};
	
	if (menu->numEntries == 0) menuHeight = 10;
	
	menu->win = gwmCreateWindow(NULL, "GWMMenu", x, y, MENU_WIDTH, menuHeight, GWM_WINDOW_HIDDEN | GWM_WINDOW_NOTASKBAR);
	if (menu->win == NULL)
	{
		return;
	};

	DDISurface *canvas = gwmGetWindowCanvas(menu->win);
	menu->background = ddiCreateSurface(&canvas->format, MENU_WIDTH, menuHeight, NULL, 0);
	menu->overlay = ddiCreateSurface(&canvas->format, MENU_WIDTH, menuHeight, NULL, 0);
	
	ddiFillRect(menu->background, 0, 0, MENU_WIDTH, menuHeight, &menuBorders);
	ddiFillRect(menu->background, 1, 1, MENU_WIDTH-2, menuHeight-2, &menuBackground);
	
	ddiFillRect(menu->overlay, 0, 0, MENU_WIDTH, menuHeight, &menuTransparent);
	
	size_t i;
	for (i=0; i<menu->numEntries; i++)
	{
		if (menu->entries[i].flags & GWM_ME_SEPARATOR)
		{
			ddiFillRect(menu->background, 0, MENU_ENTRY_HEIGHT*(i+1)-1, MENU_WIDTH, 2, &menuBorders);
		};
		
		if (menu->entries[i].submenu != NULL)
		{
			ddiBlit(imgSubmenu, 0, 0, menu->overlay, MENU_WIDTH-10, MENU_ENTRY_HEIGHT*i+(MENU_ENTRY_HEIGHT/2)-4,
					8, 8);
		};
		
		if (menu->entries[i].icon != NULL)
		{
			ddiBlit(menu->entries[i].icon, 0, 0, menu->overlay, 2, MENU_ENTRY_HEIGHT*i+2, 16, 16);
		};
		
		DDIPen *pen = ddiCreatePen(&menu->overlay->format, gwmGetDefaultFont(), 20,
						MENU_ENTRY_HEIGHT*i+MENU_ENTRY_HEIGHT-6,
						MENU_WIDTH-4, MENU_ENTRY_HEIGHT-4, 0, 0, NULL);
		if (pen != NULL)
		{
			ddiSetPenWrap(pen, 0);
			ddiWritePen(pen, menu->entries[i].label);
			ddiExecutePen2(pen, menu->overlay, DDI_POSITION_BASELINE);
			ddiDeletePen(pen);
		};
	};
	
	ddiOverlay(menu->background, 0, 0, canvas, 0, 0, MENU_WIDTH, menuHeight);
	ddiBlit(menu->overlay, 0, 0, canvas, 0, 0, MENU_WIDTH, menuHeight);
	
	menu->win->data = menu;
	menu->selectedSub = -1;
	menu->focused = 1;
	gwmPostDirty(menu->win);
	gwmSetWindowFlags(menu->win, GWM_WINDOW_MKFOCUSED | GWM_WINDOW_NOTASKBAR | GWM_WINDOW_NODECORATE);
	gwmSetEventHandler(menu->win, menuEventHandler);
};

void gwmCloseMenu(GWMMenu *menu)
{
	if (menu->win != NULL)
	{
		gwmDestroyWindow(menu->win);
		ddiDeleteSurface(menu->background);
		ddiDeleteSurface(menu->overlay);
		menu->win = NULL;
		
		if (menu->closeCallback != NULL)
		{
			menu->closeCallback(menu->closeParam);
		};
	};
};

void gwmDestroyMenu(GWMMenu *menu)
{
	gwmCloseMenu(menu);
	
	size_t i;
	for (i=0; i<menu->numEntries; i++)
	{
		if (menu->entries[i].submenu != NULL) gwmDestroyMenu(menu->entries[i].submenu);
		free(menu->entries[i].label);
	};
	
	free(menu->entries);
	free(menu);
};

void gwmMenuSetCloseCallback(GWMMenu *menu, GWMMenuCloseCallback callback, void *param)
{
	menu->closeCallback = callback;
	menu->closeParam = param;
};

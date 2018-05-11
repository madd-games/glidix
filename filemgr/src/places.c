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

#include <assert.h>

#include "places.h"
#include "filemgr.h"
#include "dirview.h"

extern DirView *dirView;

static int plHandler(GWMEvent *ev, Places *pl, void *context);

void plRedraw(Places *pl)
{
	PlacesData *data = (PlacesData*) gwmGetData(pl, plHandler);
	DDISurface *canvas = gwmGetWindowCanvas(pl);
	
	static DDIColor bg = {0xBB, 0xBB, 0xBB, 0xFF};
	ddiFillRect(canvas, 0, 0, canvas->width, canvas->height, &bg);
	
	const char *location = dvGetLocation(dirView);
	
	int drawY = 0;
	Category *cat;
	for (cat=data->cats; cat!=NULL; cat=cat->next)
	{
		DDIPen *pen = ddiCreatePen(&canvas->format, fntCategory, 2, drawY+3, canvas->width-4, 16, 0, 0, NULL);
		assert(pen != NULL);
		ddiWritePen(pen, cat->label);
		ddiExecutePen(pen, canvas);
		ddiDeletePen(pen);
		drawY += 20;
		
		Bookmark *bm;
		for (bm=cat->bookmarks; bm!=NULL; bm=bm->next)
		{
			bm->baseY = drawY;
			
			if (strcmp(location, bm->path) == 0)
			{
				ddiFillRect(canvas, 0, drawY, canvas->width, 20, GWM_COLOR_SELECTION);
			};
			
			ddiBlit(bm->icon, 0, 0, canvas, 14, drawY+2, 16, 16);
			
			pen = ddiCreatePen(&canvas->format, gwmGetDefaultFont(), 32, drawY+4, canvas->width-34, 16, 0, 0, NULL);
			assert(pen != NULL);
			ddiWritePen(pen, bm->label);
			ddiExecutePen(pen, canvas);
			ddiDeletePen(pen);
			
			drawY += 20;
		};
	};
	
	gwmPostDirty(pl);
};

static int plHandler(GWMEvent *ev, Places *pl, void *context)
{
	PlacesData *data = (PlacesData*) context;
	
	switch (ev->type)
	{
	case GWM_EVENT_DOWN:
		if (ev->keycode == GWM_KC_MOUSE_LEFT)
		{
			Category *cat;
			Bookmark *bm;
			
			for (cat=data->cats; cat!=NULL; cat=cat->next)
			{
				for (bm=cat->bookmarks; bm!=NULL; bm=bm->next)
				{
					if ((bm->baseY <= ev->y) && ((bm->baseY+20) > ev->y))
					{
						dvGoTo(dirView, bm->path);
						return GWM_EVSTATUS_OK;
					};
				};
			};
		};
		return GWM_EVSTATUS_CONT;
	default:
		return GWM_EVSTATUS_CONT;
	};
};

static void plMinSize(Places *pl, int *width, int *height)
{
	*width = 20;
	*height = 20;
};

static void plPrefSize(Places *pl, int *width, int *height)
{
	*width = 150;
	*height = 480;
};

static void plPosition(Places *pl, int x, int y, int width, int height)
{
	gwmMoveWindow(pl, x, y);
	gwmResizeWindow(pl, width, height);
	plRedraw(pl);
};

Places* plNew(GWMWindow *parent)
{
	Places *pl = gwmCreateWindow(parent, "Places", 0, 0, 0, 0, 0);
	if (pl == NULL) return NULL;

	pl->getMinSize = plMinSize;
	pl->getPrefSize = plPrefSize;
	pl->position = plPosition;
	
	PlacesData *data = (PlacesData*) malloc(sizeof(PlacesData));
	data->cats = NULL;
	
	gwmPushEventHandler(pl, plHandler, data);
	return pl;
};

Category* plNewCat(Places *pl, const char *label)
{
	PlacesData *data = (PlacesData*) gwmGetData(pl, plHandler);
	
	Category *cat = (Category*) malloc(sizeof(Category));
	cat->prev = NULL;
	cat->next = data->cats;
	if (data->cats != NULL) data->cats->prev = cat;
	data->cats = cat;
	
	cat->label = strdup(label);
	cat->bookmarks = NULL;
	
	return cat;
};

void plAddBookmark(Category *cat, DDISurface *icon, const char *label, const char *path)
{
	Bookmark *bm = (Bookmark*) malloc(sizeof(Bookmark));
	bm->prev = NULL;
	bm->next = cat->bookmarks;
	if (cat->bookmarks != NULL) cat->bookmarks->prev = bm;
	cat->bookmarks = bm;
	
	bm->label = strdup(label);
	bm->path = strdup(path);
	bm->icon = icon;
};

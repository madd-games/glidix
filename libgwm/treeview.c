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

#include <sys/stat.h>
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
#include <dirent.h>

#define	ROW_HEIGHT					20

static DDISurface *imgTreePtr = NULL;

typedef struct GWMTreeEntry_
{
	DDISurface**					labels;
	int						state;		// only if has children; 0 = closed, 1 = open
	int						flags;
	DDISurface*					icon;
	struct GWMTreeEntry_*				next;
	struct GWMTreeEntry_*				down;
	char						path[];
} GWMTreeEntry;

typedef struct
{
	GWMTreeEnum*					tree;
	void*						root;
	int						flags;
	int*						colWidths;
	DDISurface**					colLabels;
	int						scroll;
	GWMTreeEntry*					head;
	GWMWindow*					sbar;
	int						viewTotal;
	int						hoverX;
	int						hoverY;
	int						clicked;
	int						scrollbarDirty;
	void*						selectedPath;
	int						colResizing;
	int						resizeAnchor;
	int						oldColSize;
	GWMTreeViewActivateCallback			actCallback;
	void*						actParam;
	GWMTreeViewSelectCallback			selCallback;
	void*						selParam;
	char						pathBuffer[];
} GWMTreeViewData;

static GWMTreeEntry *readTree(GWMWindow *treeview, const void *path)
{
	GWMTreeViewData *data = (GWMTreeViewData*) treeview->data;
	DDISurface *canvas = gwmGetWindowCanvas(treeview);

	void *node = data->tree->teOpenNode(path);
	if (node == NULL) return NULL;
	
	GWMTreeEntry *head = NULL;
	GWMTreeEntry **put = &head;
	
	char chpath[data->tree->tePathSize];
	void* values[data->tree->teNumCols];
	GWMNodeInfo info;
	info.niPath = chpath;
	info.niFlags = 0;
	info.niValues = values;
	info.niIcon = NULL;
	
	while (data->tree->teGetNext(node, &info, sizeof(GWMNodeInfo)) == 0)
	{
		GWMTreeEntry *entry = (GWMTreeEntry*) malloc(sizeof(GWMTreeEntry) + data->tree->tePathSize);
		entry->labels = (DDISurface**) malloc(sizeof(void*) * data->tree->teNumCols);
		memcpy(entry->path, chpath, data->tree->tePathSize);
		entry->state = 0;
		entry->flags = info.niFlags;
		entry->next = NULL;
		entry->down = NULL;
		entry->icon = info.niIcon;
		
		int i;
		for (i=0; i<data->tree->teNumCols; i++)
		{
			static DDIColor black = {0x00, 0x00, 0x00, 0xFF};
			entry->labels[i] = ddiRenderText(&canvas->format, gwmGetDefaultFont(), &black, (char*)values[i], NULL);
			free(values[i]);
		};
		
		*put = entry;
		put = &entry->next;

		info.niPath = chpath;
		info.niFlags = 0;
		info.niValues = values;
		info.niIcon = NULL;
	};
	
	data->tree->teCloseNode(node);
	return head;
};

static void adjustScrollbar(GWMWindow *treeview)
{
#if 0
	GWMTreeViewData *data = (GWMTreeViewData*) treeview->data;
	if (data->sbar != NULL)
	{
		DDISurface *canvas = gwmGetWindowCanvas(treeview);
		int viewSize = canvas->height;
		
		if ((data->flags & GWM_TV_NOHEADS) == 0)
		{
			viewSize -= ROW_HEIGHT;
		};
		
		gwmSetScrollbarParams(data->sbar, data->scroll, viewSize, data->viewTotal, 0);
	};
#endif
};

static void releaseTree(GWMTreeViewData *data, GWMTreeEntry *head)
{
	while (head != NULL)
	{
		int i;
		for (i=0; i<data->tree->teNumCols; i++)
		{
			ddiDeleteSurface(head->labels[i]);
		};
		
		free(head->labels);
		GWMTreeEntry *down = head;
		head = down->next;
		releaseTree(data, down->down);
		free(down);
	};
};

static int renderTree(GWMWindow *treeview, GWMTreeEntry *entry, int indent, int y)
{
	GWMTreeViewData *data = (GWMTreeViewData*) treeview->data;
	DDISurface *canvas = gwmGetWindowCanvas(treeview);
	
	int ymargin = 0;
	if ((data->flags & GWM_TV_NOHEADS) == 0)
	{
		ymargin = ROW_HEIGHT;
	};
	
	for (; entry!=NULL; entry=entry->next)
	{
		if (((data->hoverX < indent) && (data->hoverX >= 0)) || (data->hoverX > (indent+12)))
		{
			if (data->clicked)
			{
				if ((data->hoverY/ROW_HEIGHT) == (y/ROW_HEIGHT))
				{
					memcpy(data->pathBuffer, entry->path, data->tree->tePathSize);
					data->selectedPath = data->pathBuffer;
					
					if (data->selCallback != NULL)
					{
						data->selCallback(data->selParam);
					};
				};
			};
		};
		
		if (data->selectedPath != NULL)
		{
			if (memcmp(entry->path, data->selectedPath, data->tree->tePathSize) == 0)
			{
				ddiFillRect(canvas, 1, y-data->scroll+ymargin,
					canvas->width-2, ROW_HEIGHT, GWM_COLOR_SELECTION);
			};
		};

		if (entry->flags & GWM_NODE_HASCHLD)
		{
			int xi = 0;
			if ((data->hoverX >= indent) && (data->hoverX < 12+indent))
			{
				if ((data->hoverY/ROW_HEIGHT) == (y/ROW_HEIGHT))
				{
					if (data->clicked)
					{
						if (entry->state == 0)
						{
							// open it
							entry->down = readTree(treeview, entry->path);
							entry->state = 1;
						}
						else
						{
							// close it
							releaseTree(data, entry->down);
							entry->down = NULL;
							entry->state = 0;
						};
						
						data->scrollbarDirty = 1;
					};
					
					xi = 1;
				};
			};
			
			ddiBlit(imgTreePtr, 8*xi, 8*entry->state, canvas, indent+2, y+6-data->scroll+ymargin, 8, 8);
		};
		
		int i;
		int x = 30 + indent;
		for (i=0; i<data->tree->teNumCols; i++)
		{
			int margin = 0;
			if (i == 0)
			{
				if (entry->icon != NULL)
				{
					ddiBlit(entry->icon, 0, 0, canvas, indent+12, y+ymargin-data->scroll+(ROW_HEIGHT/2-8), 16, 16);
				};
				margin = 30 + indent;
			};
			
			ddiBlit(entry->labels[i], 0, 0,
					canvas, x, y+ymargin-data->scroll+(ROW_HEIGHT/2)-(entry->labels[i]->height/2),
					data->colWidths[i]-margin, ROW_HEIGHT);
			x += data->colWidths[i] - margin;
		};
		
		y += ROW_HEIGHT;
		y = renderTree(treeview, entry->down, indent+12, y);
	};
	
	return y;
};

void gwmRedrawTreeView(GWMWindow *treeview)
{
	static DDIColor headerColor = {0xAA, 0xAA, 0xAA, 0xFF};
	static DDIColor separatorColor = {0xEE, 0xEE, 0xEE, 0xFF};
	static DDIColor black = {0, 0, 0, 0xFF};
	static DDIColor white = {0xFF, 0xFF, 0xFF, 0xFF};
	
	GWMTreeViewData *data = (GWMTreeViewData*) treeview->data;
	DDISurface *canvas = gwmGetWindowCanvas(treeview);
	
	ddiFillRect(canvas, 0, 0, canvas->width, canvas->height, &black);
	ddiFillRect(canvas, 1, 1, canvas->width-2, canvas->height-2, &white);
	
	if (data->clicked)
	{
		data->selectedPath = NULL;
	};
	
	data->scrollbarDirty = 0;
	data->viewTotal = renderTree(treeview, data->head, 0, 0);
	if (data->scrollbarDirty)
	{
		adjustScrollbar(treeview);
	};
	
	if ((data->flags & GWM_TV_NOHEADS) == 0)
	{
		ddiFillRect(canvas, 1, 1, canvas->width-2, ROW_HEIGHT-1, &headerColor);
		ddiFillRect(canvas, 0, ROW_HEIGHT-1, canvas->width, 1, &black);
		
		int i;
		int xoff = 0;
		for (i=0; i<data->tree->teNumCols; i++)
		{
			ddiFillRect(canvas, xoff + data->colWidths[i], 0, 1, ROW_HEIGHT-1, &separatorColor);
			ddiBlit(data->colLabels[i], 0, 0, canvas, xoff + 2, (ROW_HEIGHT/2)-(data->colLabels[i]->height/2),
					data->colWidths[i]-2, data->colLabels[i]->height);
			xoff += data->colWidths[i];
		};
	};
	
	data->clicked = 0;
	gwmPostDirty(treeview);
};

static int getColToResize(GWMTreeViewData *data, int clickX)
{
	int i;
	int offset = 0;
	
	for (i=0; i<data->tree->teNumCols; i++)
	{
		int border = offset + data->colWidths[i];
		offset += data->colWidths[i];
		
		int delta = border - clickX;
		if ((delta >= -4) && (delta <= 4))
		{
			return i;
		};
	};
	
	return -1;
};

static int treeviewHandler(GWMEvent *ev, GWMWindow *treeview, void *context)
{
	GWMTreeViewData *data = (GWMTreeViewData*) treeview->data;
	int ymargin = 0;
	if ((data->flags & GWM_TV_NOHEADS) == 0)
	{
		ymargin = ROW_HEIGHT;
	};

	switch (ev->type)
	{
	case GWM_EVENT_DOWN:
		if (ev->keycode == GWM_KC_MOUSE_LEFT)
		{
			data->colResizing = getColToResize(data, ev->x);
			data->resizeAnchor = ev->x;
			if (data->colResizing != -1) data->oldColSize = data->colWidths[data->colResizing];
		};
		return GWM_EVSTATUS_OK;
	case GWM_EVENT_UP:
		if (ev->keycode == GWM_KC_MOUSE_LEFT)
		{
			data->colResizing = -1;
			data->clicked = 1;
		};
		// no break!
	case GWM_EVENT_ENTER:
	case GWM_EVENT_MOTION:
		if (data->colResizing != -1)
		{
			data->colWidths[data->colResizing] = data->oldColSize + (ev->x - data->resizeAnchor);
			if (data->colWidths[data->colResizing] < 16) data->colWidths[data->colResizing] = 16;
		}
		else if (ev->y > ymargin)
		{
			data->hoverX = ev->x;
			data->hoverY = ev->y + data->scroll - ymargin;
		};
		gwmRedrawTreeView(treeview);
		return GWM_EVSTATUS_OK;
	case GWM_EVENT_LEAVE:
		data->hoverX = data->hoverY = -1;
		gwmRedrawTreeView(treeview);
		return GWM_EVSTATUS_OK;
	case GWM_EVENT_DOUBLECLICK:
		if (data->actCallback != NULL)
		{
			return data->actCallback(data->actParam);
		};
		return GWM_EVSTATUS_OK;
	default:
		return GWM_EVSTATUS_CONT;
	};
};

#if 0
static int onScroll(void *context)
{
	GWMWindow *treeview = (GWMWindow*) context;
	GWMTreeViewData *data = (GWMTreeViewData*) treeview->data;
	data->scroll = gwmGetScrollbarOffset(data->sbar);
	gwmRedrawTreeView(treeview);
	return 0;
};
#endif

GWMWindow *gwmCreateTreeView(GWMWindow *parent, int x, int y, int width, int height, GWMTreeEnum *tree, const void *root, int flags)
{
	GWMWindow *treeview = gwmCreateWindow(parent, "GWMTreeView", x, y, width, height, 0);
	if (treeview == NULL) return NULL;
	if (height < ROW_HEIGHT) height = ROW_HEIGHT;
	
	DDISurface *canvas = gwmGetWindowCanvas(treeview);
	
	if (imgTreePtr == NULL)
	{
		imgTreePtr = ddiLoadAndConvertPNG(&canvas->format, "/usr/share/images/treeptr.png", NULL);
		if (imgTreePtr == NULL)
		{
			fprintf(stderr, "Failed to load /usr/share/images/treeptr.png\n");
			abort();
		};
	};
	
	GWMTreeViewData *data = (GWMTreeViewData*) malloc(sizeof(GWMTreeViewData)+tree->tePathSize);
	treeview->data = data;
	data->tree = tree;
	data->root = malloc(tree->tePathSize);
	memcpy(data->root, root, tree->tePathSize);
	data->flags = flags;
	data->colWidths = (int*) malloc(sizeof(int) * tree->teNumCols);
	
	int colWidth = width / tree->teNumCols;
	if (colWidth < 16) colWidth = 16;
	
	int i;
	for (i=0; i<tree->teNumCols; i++)
	{
		data->colWidths[i] = colWidth;
	};
	
	data->colLabels = (DDISurface**) malloc(sizeof(void*) * tree->teNumCols);
	for (i=0; i<tree->teNumCols; i++)
	{
		char *caption = tree->teGetColCaption(i);
		static DDIColor black = {0x00, 0x00, 0x00, 0xFF};
		data->colLabels[i] = ddiRenderText(&canvas->format, gwmGetDefaultFont(), &black, caption, NULL);
		free(caption);
	};
	
	data->scroll = 0;
	data->head = readTree(treeview, root);
	data->sbar = NULL;
	data->hoverX = data->hoverY = -1;
	data->clicked = 0;
	data->selectedPath = NULL;
	data->colResizing = -1;
	data->actCallback = NULL;
	data->selCallback = NULL;
	
	//int scrollbarY = 1;
	//int scrollbarLen = height-2;
	
	if ((flags & GWM_TV_NOHEADS) == 0)
	{
		//scrollbarY = ROW_HEIGHT;
		//scrollbarLen = height - scrollbarY - 1;
	};

	gwmPushEventHandler(treeview, treeviewHandler, NULL);
	gwmRedrawTreeView(treeview);
	
	//GWMWindow *sbar = gwmCreateScrollbar(treeview, width-8, scrollbarY, scrollbarLen, 0, 0, 10, GWM_SCROLLBAR_VERT);
	//data->sbar = sbar;
	//gwmSetScrollbarCallback(sbar, onScroll, treeview);
	adjustScrollbar(treeview);

	return treeview;
};

void gwmDestroyTreeView(GWMWindow *treeview)
{
	GWMTreeViewData *data = (GWMTreeViewData*) treeview->data;
	//gwmDestroyScrollbar(data->sbar);
	releaseTree(data, data->head);
	
	int i;
	for (i=0; i<data->tree->teNumCols; i++)
	{
		ddiDeleteSurface(data->colLabels[i]);
	};
	
	free(data->colLabels);
	free(data->colWidths);
	free(data->root);
	
	gwmDestroyWindow(treeview);
};

void gwmTreeViewSetActivateCallback(GWMWindow *treeview, GWMTreeViewActivateCallback cb, void *param)
{
	GWMTreeViewData *data = (GWMTreeViewData*) treeview->data;
	data->actCallback = cb;
	data->actParam = param;
};

void gwmTreeViewSetSelectCallback(GWMWindow *treeview, GWMTreeViewSelectCallback cb, void *param)
{
	GWMTreeViewData *data = (GWMTreeViewData*) treeview->data;
	data->selCallback = cb;
	data->selParam = param;
};

int gwmTreeViewGetSelection(GWMWindow *treeview, void *buffer)
{
	GWMTreeViewData *data = (GWMTreeViewData*) treeview->data;
	if (data->selectedPath == NULL) return -1;
	memcpy(buffer, data->selectedPath, data->tree->tePathSize);
	return 0;
};

void gwmTreeViewUpdate(GWMWindow *treeview, const void *root)
{
	GWMTreeViewData *data = (GWMTreeViewData*) treeview->data;
	releaseTree(data, data->head);
	data->selectedPath = NULL;
	memcpy(data->root, root, data->tree->tePathSize);
	data->head = readTree(treeview, root);
	gwmRedrawTreeView(treeview);
	adjustScrollbar(treeview);
};

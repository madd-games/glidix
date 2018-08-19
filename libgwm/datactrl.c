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

#define	DATA_ROW_HEIGHT				20
#define	DATA_CHILD_INDENT			20

static DDISurface* imgTreePtr;

/**
 * Represents a column of data in a data control. Structure is opaquely typedefed to GWMDataColumn by libgwm.h.
 */
struct GWMDataColumn_
{
	/**
	 * Links.
	 */
	struct GWMDataColumn_*			prev;
	struct GWMDataColumn_*			next;
	
	/**
	 * Column label displayed in the header.
	 */
	char*					label;
	
	/**
	 * The key for GWMDataNode records.
	 */
	int					key;
	
	/**
	 * Data type (GWM_DATA_*).
	 */
	int					type;
	
	/**
	 * Width of the column.
	 */
	int					width;
};

/**
 * Data cell.
 */
typedef struct DataCell_
{
	/**
	 * Links.
	 */
	struct DataCell_*			next;
	
	/**
	 * Cell key.
	 */
	int					key;
	
	/**
	 * Value (depends on key type).
	 */
	union
	{
		char*				strval;
	};
	
	/**
	 * Drawing of the cell or NULL if not drawn yet.
	 */
	DDISurface*				surf;
	
	/**
	 * Does free() need to be called on the value?
	 */
	int					needFree;
} DataCell;

/**
 * Data node. Structure is opaquely typedefed to GWMDataNode by libgwm.h.
 */
struct GWMDataNode_
{
	/**
	 * Links.
	 */
	GWMDataNode*				prev;
	GWMDataNode*				next;
	GWMDataNode*				children;
	GWMDataNode*				parent;
	
	/**
	 * Linked list of cells.
	 */
	DataCell*				first;
	DataCell*				last;
	
	/**
	 * Arbitrary "description" assigned by user.
	 */
	void*					desc;
	
	/**
	 * Node flags (GWM_DATA_NODE_*).
	 */
	int					flags;
	
	/**
	 * Is this node expanded (if it has children)?
	 */
	int					expanded;
	
	/**
	 * Where the node was last drawn.
	 */
	int					lastDrawY;
	
	/**
	 * Is the node selected?
	 */
	int					selected;
	
	/**
	 * Node icon or NULL.
	 */
	DDISurface*				icon;
};

typedef struct
{
	/**
	 * List of columns.
	 */
	GWMDataColumn*				cols;
	
	/**
	 * Root node.
	 */
	GWMDataNode*				root;
	
	/**
	 * The scroll bar.
	 */
	GWMScrollbar*				sbar;
	
	/**
	 * Flags.
	 */
	int					flags;
	
	/**
	 * Scroll position.
	 */
	int					scrollX;
	int					scrollY;
	
	/**
	 * Current mouse position (-1, -1 means outside the widget).
	 */
	int					mouseX;
	int					mouseY;
	
	/**
	 * Previous mouse position (before current).
	 */
	int					prevMouseX;
	int					prevMouseY;
	
	/**
	 * Symbol.
	 */
	int					symbol;
	
	/**
	 * Total height.
	 */
	int					totalHeight;
	
	/**
	 * If set to 1, the control is "completly dirty" and all cells reuqire a re-draw.
	 */
	int					dirty;
} DataCtrlData;

static DataCell* getCellByKey(GWMDataNode *node, int key)
{
	DataCell *cell;
	for (cell=node->first; cell!=NULL; cell=cell->next)
	{
		if (cell->key == key) return cell;
	};
	
	cell = (DataCell*) malloc(sizeof(DataCell));
	memset(cell, 0, sizeof(DataCell));
	cell->key = key;
	
	if (node->last == NULL)
	{
		node->first = node->last = cell;
	}
	else
	{
		node->last->next = cell;
		node->last = cell;
	};
	
	return cell;
};

void gwmSetDataString(GWMDataCtrl *ctrl, GWMDataNode *node, int key, const char *str)
{
	DataCell *cell = getCellByKey(node, key);
	cell->needFree = 1;
	free(cell->strval);
	cell->strval = strdup(str);
	
	gwmPostUpdate(ctrl);
};

const char* gwmGetDataString(GWMDataCtrl *ctrl, GWMDataNode *node, int key)
{
	DataCell *cell = getCellByKey(node, key);
	
	if (cell->strval == NULL)
	{
		return "";
	}
	else
	{
		return cell->strval;
	};
};

void gwmSetDataNodeIcon(GWMDataCtrl *ctrl, GWMDataNode *node, DDISurface *icon)
{
	node->icon = icon;
	gwmPostUpdate(ctrl);
};

static int drawNode(DDISurface *canvas, DataCtrlData *data, GWMDataNode *parent, int firstIndent, int plotY)
{
	static DDIColor transparent = {0, 0, 0, 0};
	int plotX = 1;
	
	GWMDataNode *node;
	for (node=parent->children; node!=NULL; node=node->next)
	{
		int drawY = plotY - data->scrollY;
		plotY += DATA_ROW_HEIGHT;
		
		node->lastDrawY = drawY;
		
		if (drawY > -DATA_ROW_HEIGHT && drawY < canvas->height)
		{
			// actually visible; let's draw
			if (node->selected)
			{
				ddiFillRect(canvas, 1, drawY, canvas->width-2, DATA_ROW_HEIGHT, GWM_COLOR_SELECTION);
			};
			
			int drawX = plotX - data->scrollX;
			
			GWMDataColumn *col;
			for (col=data->cols; col!=NULL; col=col->next)
			{
				DataCell *cell = getCellByKey(node, col->key);
				
				int update = 0;
				if (data->mouseY > drawY && data->mouseY < (drawY+DATA_ROW_HEIGHT)) update = 1;
				if (data->prevMouseY > drawY && data->prevMouseY < (drawY+DATA_ROW_HEIGHT)) update = 1;
				if (data->dirty) update = 1;
				
				if (cell->surf == NULL || cell->surf->width != col->width || update)
				{
					if (cell->surf != NULL) ddiDeleteSurface(cell->surf);
					
					cell->surf = ddiCreateSurface(&canvas->format, col->width, DATA_ROW_HEIGHT, NULL, 0);
					ddiFillRect(cell->surf, 0, 0, col->width, DATA_ROW_HEIGHT, &transparent);
					
					int indent = 2;
					if (col->prev == NULL)
					{
						indent = 12 + firstIndent;
						
						if (node->children != NULL || (node->flags & GWM_DATA_NODE_FORCE_PTR))
						{
							int whichImg = 0;
							if (node->expanded)
							{
								whichImg = 1;
							};
						
							int whichX = 0;
							if (data->mouseY > drawY && data->mouseY < (drawY+DATA_ROW_HEIGHT)
								&& data->mouseX > firstIndent && data->mouseX < firstIndent+12)
							{
								whichX = 1;
							};
							
							ddiOverlay(imgTreePtr, 8*whichX, 8*whichImg, cell->surf, firstIndent+2, 6, 8, 8);
						};
						
						if (node->icon)
						{
							ddiBlit(node->icon, 0, 0, cell->surf, indent, (DATA_ROW_HEIGHT-16)/2, 16, 16);
							indent += 18;
						};
					};
					
					if (col->type == GWM_DATA_STRING)
					{
						if (cell->strval != NULL)
						{
							DDIPen *pen = ddiCreatePen(&canvas->format, gwmGetDefaultFont(), indent, 0,
											col->width, DATA_ROW_HEIGHT,
											0, 0, NULL);
							ddiSetPenWrap(pen, 0);
							ddiWritePen(pen, cell->strval);
							
							int txtWidth, txtHeight;
							ddiGetPenSize(pen, &txtWidth, &txtHeight);
							ddiSetPenPosition(pen, indent, (DATA_ROW_HEIGHT-txtHeight)/2);
							ddiExecutePen(pen, cell->surf);
							ddiDeletePen(pen);
						};
					};
				};
				
				ddiBlit(cell->surf, 0, 0, canvas, drawX, drawY, col->width, DATA_ROW_HEIGHT);
				drawX += col->width;
			};
		};
		
		if (node->expanded)
		{
			plotY = drawNode(canvas, data, node, firstIndent+DATA_CHILD_INDENT, plotY);
		};
	};
	
	return plotY;
};

static int ctrlHandler(GWMEvent *ev, GWMSpinner *spin, void *context);
static void ctrlRedraw(GWMDataCtrl *ctrl)
{
	if (imgTreePtr == NULL)
	{
		imgTreePtr = (DDISurface*) gwmGetThemeProp("gwm.toolkit.treeptr", GWM_TYPE_SURFACE, NULL);
		if (imgTreePtr == NULL)
		{
			fprintf(stderr, "Failed to load tree pointer image\n");
			abort();
		};
	};
	
	DataCtrlData *data = (DataCtrlData*) gwmGetData(ctrl, ctrlHandler);

	static DDIColor border = {0x00, 0x00, 0x00, 0xFF};
	
	DDISurface *canvas = gwmGetWindowCanvas(ctrl);
	if (canvas->height == 0) return;
	ddiFillRect(canvas, 0, 0, canvas->width, canvas->height, &border);
	ddiFillRect(canvas, 1, 1, canvas->width-2, canvas->height-2, GWM_COLOR_EDITOR);
	
	int plotY = 1;
	if (data->flags & GWM_DATA_SHOW_HEADERS) plotY = DATA_ROW_HEIGHT;
	
	int maxY = drawNode(canvas, data, data->root, 0, plotY);
	
	if (data->flags & GWM_DATA_SHOW_HEADERS)
	{
		ddiFillRect(canvas, 0, 0, canvas->width, DATA_ROW_HEIGHT, &border);
		ddiFillRect(canvas, 1, 1, canvas->width-2, DATA_ROW_HEIGHT-2, GWM_COLOR_BACKGROUND);
		
		int drawX = 1;
		
		GWMDataColumn *col;
		for (col=data->cols; col!=NULL; col=col->next)
		{
			ddiFillRect(canvas, drawX, 0, col->width, DATA_ROW_HEIGHT, &border);
			ddiFillRect(canvas, drawX, 1, col->width-1, DATA_ROW_HEIGHT-2, GWM_COLOR_BACKGROUND);
			
			DDIPen *pen = ddiCreatePen(&canvas->format, gwmGetDefaultFont(), drawX+2, 1,
							col->width, DATA_ROW_HEIGHT,
							0, 0, NULL);
			ddiSetPenWrap(pen, 0);
			ddiWritePen(pen, col->label);
			
			int txtWidth, txtHeight;
			ddiGetPenSize(pen, &txtWidth, &txtHeight);
			ddiSetPenPosition(pen, drawX+2, 1+(DATA_ROW_HEIGHT-txtHeight)/2);
			ddiExecutePen(pen, canvas);
			ddiDeletePen(pen);
			
			drawX += col->width;
		};
	};
	
	float len;
	if (data->flags & GWM_DATA_SHOW_HEADERS)
	{
		len = (float)(canvas->height - DATA_ROW_HEIGHT - 2) / (float)(maxY - DATA_ROW_HEIGHT);
		data->totalHeight = maxY - DATA_ROW_HEIGHT;
	}
	else
	{
		len = (float)(canvas->height - 2) / (float) (maxY);
		data->totalHeight = maxY;
	};
	
	if (len > 1.0)
	{
		gwmHide(data->sbar);
		
		if (data->scrollY != 0)
		{
			data->scrollY = 0;
			gwmPostUpdate(ctrl);
		};
	}
	else
	{
		gwmShow(data->sbar);
		gwmSetScrollbarLength(data->sbar, len);
	};
	
	data->dirty = 0;
	gwmPostDirty(ctrl);
};

static void unselectRecur(GWMDataNode *parent)
{
	GWMDataNode *node;
	for (node=parent->children; node!=NULL; node=node->next)
	{
		node->selected = 0;
		unselectRecur(node);
	};
};

static void ctrlDoSelect(GWMDataCtrl *ctrl, DataCtrlData *data, int x, int y, GWMDataNode *parent, int multiSelect, int indent)
{
	GWMDataNode *node;
	for (node=parent->children; node!=NULL; node=node->next)
	{
		if (node->children != NULL || (node->flags & GWM_DATA_NODE_FORCE_PTR))
		{
			if (x > indent && x < (indent+12) && y >= node->lastDrawY && y < (node->lastDrawY+DATA_ROW_HEIGHT))
			{
				GWMDataEvent ev;
				memset(&ev, 0, sizeof(GWMDataEvent));
				
				ev.node = node;
				ev.symbol = data->symbol;
				
				if (node->expanded)
				{
					ev.header.type = GWM_EVENT_DATA_COLLAPSING;
				}
				else
				{
					ev.header.type = GWM_EVENT_DATA_EXPANDING;
				};
				
				if (gwmPostEvent((GWMEvent*) &ev, ctrl) == GWM_EVSTATUS_DEFAULT)
				{
					node->expanded = !node->expanded;
					unselectRecur(node);
					break;
				};
			};
		};
		
		if (y >= node->lastDrawY && y < (node->lastDrawY+DATA_ROW_HEIGHT))
		{
			node->selected = 1;
		}
		else if (!multiSelect)
		{
			node->selected = 0;
		};
		
		if (node->expanded)
		{
			ctrlDoSelect(ctrl, data, x, y, node, multiSelect, indent+DATA_CHILD_INDENT);
		};
	};
};

static int ctrlDoubleClick(GWMDataCtrl *ctrl, DataCtrlData *data, int y, GWMDataNode *parent)
{
	GWMDataNode *node;
	for (node=parent->children; node!=NULL; node=node->next)
	{
		if (y >= node->lastDrawY && y < (node->lastDrawY+DATA_ROW_HEIGHT))
		{
			GWMDataEvent ev;
			memset(&ev, 0, sizeof(GWMDataEvent));
			ev.header.type = GWM_EVENT_DATA_ACTIVATED;
			ev.node = node;
			ev.symbol = data->symbol;
			if (gwmPostEvent((GWMEvent*) &ev, ctrl) == GWM_EVSTATUS_BREAK) return GWM_EVSTATUS_BREAK;
			
			return GWM_EVSTATUS_OK;
		};
		
		if (node->expanded)
		{
			if (ctrlDoubleClick(ctrl, data, y, node) == GWM_EVSTATUS_BREAK) return GWM_EVSTATUS_BREAK;
		};
	};
	
	return GWM_EVSTATUS_OK;
};

static void ctrlSelectOnlyRecur(DataCtrlData *data, GWMDataNode *parent, GWMDataNode *sel)
{
	GWMDataNode *node;
	for (node=parent->children; node!=NULL; node=node->next)
	{
		node->selected = (node == sel);
		ctrlSelectOnlyRecur(data, node, sel);
	};
};

static void ctrlSelectOnly(GWMDataCtrl *ctrl, DataCtrlData *data, GWMDataNode *sel)
{
	ctrlSelectOnlyRecur(data, data->root, sel);
	
	int cap = 1;
	if (data->flags & GWM_DATA_SHOW_HEADERS)
	{
		cap = DATA_ROW_HEIGHT;
	};
	
	DDISurface *canvas = gwmGetWindowCanvas(ctrl);
	int update = 0;
	int newPos;
	if (sel->lastDrawY > (canvas->height-DATA_ROW_HEIGHT))
	{
		newPos = data->scrollY + (sel->lastDrawY - (canvas->height-DATA_ROW_HEIGHT));
		update = 1;
	}
	else if (sel->lastDrawY < cap)
	{
		newPos = data->scrollY + (sel->lastDrawY-cap);
		update = 1;
	};
	
	if (update)
	{
		float pos = (float) newPos / (float) data->totalHeight;
		gwmSetScrollbarPosition(data->sbar, pos);
	};
	
	GWMDataEvent ev;
	memset(&ev, 0, sizeof(GWMDataEvent));
	ev.header.type = GWM_EVENT_DATA_SELECT_CHANGED;
	ev.symbol = data->symbol;
	gwmPostEvent((GWMEvent*) &ev, ctrl);
};

static int ctrlHandler(GWMEvent *ev, GWMDataCtrl *ctrl, void *context)
{
	DataCtrlData *data = (DataCtrlData*) context;

	switch (ev->type)
	{
	case GWM_EVENT_RETHEME:
		data->dirty = 1;
		/* no break */
	case GWM_EVENT_UPDATE:
		ctrlRedraw(ctrl);
		return GWM_EVSTATUS_OK;
	case GWM_EVENT_DOWN:
		if (ev->keycode == GWM_KC_MOUSE_LEFT)
		{
			if (ev->y < DATA_ROW_HEIGHT && (data->flags & GWM_DATA_SHOW_HEADERS))
			{
				return GWM_EVSTATUS_CONT;
			};
			
			int multiSelect = 0;
			
			ctrlDoSelect(ctrl, data, ev->x, ev->y, data->root, multiSelect, 0);
			GWMDataEvent sev;
			memset(&sev, 0, sizeof(GWMDataEvent));
			sev.header.type = GWM_EVENT_DATA_SELECT_CHANGED;
			sev.symbol = data->symbol;
			gwmPostEvent((GWMEvent*) &sev, ctrl);
			gwmPostUpdate(ctrl);
		}
		else if (ev->keycode == GWM_KC_UP)
		{
			GWMDataNode *sel = gwmGetDataSelection(ctrl, 0);
			if (sel != NULL)
			{
				if (sel->prev != NULL)
				{
					GWMDataNode *cand = sel->prev;
					while (cand->expanded && cand->children != NULL)
					{
						GWMDataNode *last = cand->children;
						while (last->next != NULL) last = last->next;
						cand = last;
					};
					ctrlSelectOnly(ctrl, data, cand);
					gwmPostUpdate(ctrl);
				}
				else if (sel->parent != NULL && sel->parent != data->root)
				{
					ctrlSelectOnly(ctrl, data, sel->parent);
					gwmPostUpdate(ctrl);
				};
			};
			return GWM_EVSTATUS_OK;
		}
		else if (ev->keycode == GWM_KC_DOWN)
		{
			GWMDataNode *sel = gwmGetDataSelection(ctrl, 0);
			if (sel != NULL)
			{
				if (sel->expanded)
				{
					if (sel->children != NULL)
					{
						ctrlSelectOnly(ctrl, data, sel->children);
						gwmPostUpdate(ctrl);
						return GWM_EVSTATUS_OK;
					};
				};
				
				while (sel->next == NULL)
				{
					if (sel->parent == NULL || sel->parent == data->root)
					{
						return GWM_EVSTATUS_OK;
					};
					
					sel = sel->parent;
				};
				
				ctrlSelectOnly(ctrl, data, sel->next);
				gwmPostUpdate(ctrl);
			};
			return GWM_EVSTATUS_OK;
		}
		else if (ev->keycode == GWM_KC_RETURN)
		{
			GWMDataNode *sel = gwmGetDataSelection(ctrl, 0);
			if (sel != NULL)
			{
				GWMDataEvent ev;
				memset(&ev, 0, sizeof(GWMDataEvent));
				ev.header.type = GWM_EVENT_DATA_ACTIVATED;
				ev.node = sel;
				ev.symbol = data->symbol;
				if (gwmPostEvent((GWMEvent*) &ev, ctrl) == GWM_EVSTATUS_BREAK) return GWM_EVSTATUS_BREAK;
			};
			return GWM_EVSTATUS_CONT;
		}
		else if (ev->keycode == GWM_KC_SPACE)
		{
			GWMDataNode *sel = gwmGetDataSelection(ctrl, 0);
			if (sel != NULL)
			{
				if (sel->children != NULL || (sel->flags & GWM_DATA_NODE_FORCE_PTR))
				{
					GWMDataEvent ev;
					memset(&ev, 0, sizeof(GWMDataEvent));
					if (sel->expanded)
					{
						ev.header.type = GWM_EVENT_DATA_COLLAPSING;
					}
					else
					{
						ev.header.type = GWM_EVENT_DATA_EXPANDING;
					};
					ev.node = sel;
					ev.symbol = data->symbol;
					if (gwmPostEvent((GWMEvent*) &ev, ctrl) == GWM_EVSTATUS_DEFAULT)
					{
						sel->expanded = !sel->expanded;
						data->dirty = 1;
						gwmPostUpdate(ctrl);
					};
				};
			};
			return GWM_EVSTATUS_CONT;
		};
		return GWM_EVSTATUS_CONT;
	case GWM_EVENT_ENTER:
	case GWM_EVENT_MOTION:
		data->prevMouseX = data->mouseX;
		data->prevMouseY = data->mouseY;
		data->mouseX = ev->x;
		data->mouseY = ev->y;
		gwmPostUpdate(ctrl);
		return GWM_EVSTATUS_OK;
	case GWM_EVENT_LEAVE:
		data->prevMouseX = data->mouseX;
		data->prevMouseY = data->mouseY;
		data->mouseX = -1;
		data->mouseY = -1;
		gwmPostUpdate(ctrl);
		return GWM_EVSTATUS_OK;
	case GWM_EVENT_VALUE_CHANGED:
		data->scrollY = gwmGetScrollbarPosition(data->sbar) * data->totalHeight;
		gwmPostUpdate(ctrl);
		return GWM_EVSTATUS_OK;
	case GWM_EVENT_DOUBLECLICK:
		return ctrlDoubleClick(ctrl, data, ev->y, data->root);
	default:
		return GWM_EVSTATUS_CONT;
	};
};

static void ctrlMinSize(GWMDataCtrl *ctrl, int *width, int *height)
{
	*width = 50;
	*height = 50;
};

static void ctrlPrefSize(GWMDataCtrl *ctrl, int *width, int *height)
{
	*width = 300;
	*height = 100;
};

static void ctrlPosition(GWMDataCtrl *ctrl, int x, int y, int width, int height)
{
	DataCtrlData *data = (DataCtrlData*) gwmGetData(ctrl, ctrlHandler);
	
	gwmMoveWindow(ctrl, x, y);
	gwmResizeWindow(ctrl, width, height);
	data->sbar->position(data->sbar, width-10, 0, 10, height);
	gwmPostUpdate(ctrl);
};

GWMDataCtrl* gwmNewDataCtrl(GWMWindow *parent)
{
	GWMDataCtrl *ctrl = gwmCreateWindow(parent, "GWMDataCtrl", 0, 0, 0, 0, 0);
	if (ctrl == NULL) return NULL;
	
	DataCtrlData *data = (DataCtrlData*) malloc(sizeof(DataCtrlData));
	data->cols = NULL;
	
	GWMDataNode *root = (GWMDataNode*) malloc(sizeof(GWMDataNode));
	memset(root, 0, sizeof(GWMDataNode));
	data->root = root;
	
	data->sbar = gwmNewScrollbar(ctrl);
	data->flags = 0;

	data->scrollX = data->scrollY = 0;
	data->mouseX = data->mouseY = -1;
	data->prevMouseX = data->prevMouseY = -1;
	
	data->symbol = 0;
	data->totalHeight = 0;
	
	data->dirty = 0;
	
	ctrl->getMinSize = ctrlMinSize;
	ctrl->getPrefSize = ctrlPrefSize;
	ctrl->position = ctrlPosition;
	
	gwmPushEventHandler(ctrl, ctrlHandler, data);
	gwmPostUpdate(ctrl);
	return ctrl;
};

void gwmSetDataFlags(GWMDataCtrl *ctrl, int flags)
{
	DataCtrlData *data = (DataCtrlData*) gwmGetData(ctrl, ctrlHandler);
	if (data == NULL) return;
	
	data->flags = flags;
	gwmPostUpdate(ctrl);
};

void gwmSetDataSymbol(GWMDataCtrl *ctrl, int symbol)
{
	DataCtrlData *data = (DataCtrlData*) gwmGetData(ctrl, ctrlHandler);
	if (data == NULL) return;
	
	data->symbol = symbol;
};

GWMDataColumn* gwmAddDataColumn(GWMDataCtrl *ctrl, const char *label, int type, int key)
{
	DataCtrlData *data = (DataCtrlData*) gwmGetData(ctrl, ctrlHandler);
	if (data == NULL) return NULL;

	GWMDataColumn *col = (GWMDataColumn*) malloc(sizeof(GWMDataColumn));
	memset(col, 0, sizeof(GWMDataColumn));
	
	col->label = strdup(label);
	col->type = type;
	col->key = key;
	col->width = 100;
	
	if (data->cols == NULL)
	{
		data->cols = col;
	}
	else
	{
		GWMDataColumn *last = data->cols;
		while (last->next != NULL) last = last->next;
		last->next = col;
		col->prev = last;
	};
	
	return col;
};

void gwmSetDataColumnWidth(GWMDataCtrl *ctrl, GWMDataColumn* col, int width)
{
	col->width = width;
	gwmPostUpdate(ctrl);
};

GWMDataNode* gwmAddDataNode(GWMDataCtrl *ctrl, int whence, GWMDataNode *ref)
{
	DataCtrlData *data = (DataCtrlData*) gwmGetData(ctrl, ctrlHandler);
	if (data == NULL) return NULL;
	
	if (ref == NULL)
	{
		switch (whence)
		{
		case GWM_DATA_ADD_TOP_CHILD:
		case GWM_DATA_ADD_BOTTOM_CHILD:
			break;
		default:
			gwmThrow(GWM_ERR_INVAL);
			return NULL;
		};
		
		ref = data->root;
	};
	
	GWMDataNode *node = (GWMDataNode*) malloc(sizeof(GWMDataNode));
	memset(node, 0, sizeof(GWMDataNode));
	
	switch (whence)
	{
	case GWM_DATA_ADD_BEFORE:
		if (ref->prev == NULL)
		{
			if (ref->parent == NULL)
			{
				gwmThrow(GWM_ERR_INVAL);
				return NULL;
			};
			
			ref->parent->children = node;
		}
		else
		{
			ref->prev->next = node;
		};
		
		node->prev = ref->prev;
		ref->prev = node;
		node->next = ref;
		node->parent = ref->parent;
		break;
	case GWM_DATA_ADD_AFTER:
		if (ref->next != NULL) ref->next->prev = node;
		node->next = ref->next;
		ref->next = node;
		node->prev = ref;
		node->parent = ref->parent;
		break;
	case GWM_DATA_ADD_TOP_CHILD:
		node->parent = ref;
		node->next = ref->children;
		ref->children = node;
		break;
	case GWM_DATA_ADD_BOTTOM_CHILD:
		if (ref->children == NULL)
		{
			node->parent = ref;
			ref->children = node;
		}
		else
		{
			GWMDataNode *last = ref->children;
			while (last->next != NULL) last = last->next;
			last->next = node;
			node->prev = last;
			node->parent = ref;
		};
		break;
	default:
		gwmThrow(GWM_ERR_INVAL);
		return NULL;
	};
	
	gwmPostUpdate(ctrl);
	return node;
};

GWMDataNode* gwmGetDataNode(GWMDataCtrl *ctrl, GWMDataNode *parent, int index)
{
	DataCtrlData *data = (DataCtrlData*) gwmGetData(ctrl, ctrlHandler);
	if (data == NULL) return NULL;
	
	if (parent == NULL) parent = data->root;
	
	GWMDataNode *node;
	for (node=parent->children; node!=NULL; node=node->next)
	{
		if ((index--) == 0) return node;
	};
	
	return NULL;
};

void gwmSetDataNodeFlags(GWMDataCtrl *ctrl, GWMDataNode *node, int flags)
{
	node->flags = flags;
	gwmPostUpdate(ctrl);
};

void gwmSetDataNodeDesc(GWMDataCtrl *ctrl, GWMDataNode *node, void *desc)
{
	node->desc = desc;
};

void* gwmGetDataNodeDesc(GWMDataCtrl *ctrl, GWMDataNode *node)
{
	return node->desc;
};

static void deleteNodeRecur(GWMDataCtrl *ctrl, GWMDataNode *node)
{
	DataCtrlData *data = (DataCtrlData*) gwmGetData(ctrl, ctrlHandler);
	if (data == NULL) return;

	// emit deletion event (to perhaps free the description, etc)
	GWMDataEvent ev;
	memset(&ev, 0, sizeof(GWMDataEvent));
	ev.header.type = GWM_EVENT_DATA_DELETING;
	ev.node = node;
	ev.symbol = data->symbol;
	gwmPostEvent((GWMEvent*) &ev, ctrl);
	
	// delete the node from the tree
	if (node->prev == NULL)
	{
		if (node->parent != NULL)
		{
			node->parent->children = node->next;
		};
	}
	else
	{
		node->prev->next = node->next;
	};
	
	if (node->next != NULL) node->next->prev = node->prev;
	
	GWMDataNode *child;
	for (child=node->children; child!=NULL; child=child->next)
	{
		child->parent = NULL;
	};
	
	// delete cells
	while (node->first != NULL)
	{
		DataCell *cell = node->first;
		node->first = cell->next;
		
		if (cell->needFree)
		{
			free(cell->strval);
		};
		
		if (cell->surf != NULL)
		{
			ddiDeleteSurface(cell->surf);
		};
		
		free(cell);
	};
	
	// recursively delete all children
	while (node->children != NULL)
	{
		child = node->children;
		node->children = child->next;
		
		deleteNodeRecur(ctrl, child);
	};
	
	// finally free it
	free(node);
};

void gwmDeleteDataNode(GWMDataCtrl *ctrl, GWMDataNode *node)
{
	deleteNodeRecur(ctrl, node);
	gwmPostUpdate(ctrl);
};

void gwmDestroyDataCtrl(GWMDataCtrl *ctrl)
{
	DataCtrlData *data = (DataCtrlData*) gwmGetData(ctrl, ctrlHandler);
	if (data == NULL) return;
	
	deleteNodeRecur(ctrl, data->root);
	
	while (data->cols != NULL)
	{
		GWMDataColumn *col = data->cols;
		data->cols = col->next;
		free(col);
	};
	
	gwmDestroyScrollbar(data->sbar);
	free(data);
	gwmDestroyWindow(ctrl);
};

static GWMDataNode* getSelectionRecur(GWMDataNode *parent, int *counter)
{
	GWMDataNode *node;
	for (node=parent->children; node!=NULL; node=node->next)
	{
		if (node->selected)
		{
			if ((*counter) == 0)
			{
				return node;
			}
			else
			{
				(*counter)--;
			};
		};
		
		GWMDataNode *candidate = getSelectionRecur(node, counter);
		if (candidate != NULL) return candidate;
	};
	
	return NULL;
};

GWMDataNode* gwmGetDataSelection(GWMDataCtrl *ctrl, int index)
{
	DataCtrlData *data = (DataCtrlData*) gwmGetData(ctrl, ctrlHandler);
	if (data == NULL) return NULL;
	
	return getSelectionRecur(data->root, &index);
};

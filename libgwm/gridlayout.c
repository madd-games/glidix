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

/**
 * GRID LAYOUT
 * Lays children out in a grid, with all cells being the same size.
 */

typedef struct
{
	GWMLayout*				layout;
	int					rowspan;
	int					colspan;
} GridChild;

typedef struct GridRow_
{
	/**
	 * Bitmap of which columns are taken (may be set even if something is NULL, due to
	 * rowspan/colspan).
	 */
	uint8_t*				bitmap;
	
	/**
	 * Array of entries, each representing a column. Some may be NULL.
	 */
	GridChild**				ents;
	
	/**
	 * Pointer to next row.
	 */
	struct GridRow_*			next;
} GridRow;

typedef struct
{
	/**
	 * Number of columns (immutable).
	 */
	int					cols;
	
	/**
	 * Number of rows, starts at 0 and grows.
	 */
	int					rows;
	
	/**
	 * Pointer to first row.
	 */
	GridRow*				head;
} GridLayoutData;

static void gridMinSize(GWMLayout *grid, int *width, int *height)
{
	GridLayoutData *data = (GridLayoutData*) grid->data;
	int mw=0, mh=0;
	
	GridRow *row;
	for (row=data->head; row!=NULL; row=row->next)
	{
		int col;
		for (col=0; col<data->cols; col++)
		{
			int w, h;
			if (row->ents[col] != NULL)
			{
				GridChild *child = row->ents[col];
				child->layout->getMinSize(child->layout, &w, &h);
				w /= child->colspan;
				h /= child->rowspan;
				
				if (w > mw) mw = w;
				if (h > mh) mh = h;
			};
		};
	};
	
	*width = mw;
	*height = mh;
};

static void gridPrefSize(GWMLayout *grid, int *width, int *height)
{
	GridLayoutData *data = (GridLayoutData*) grid->data;
	int mw=0, mh=0;
	
	GridRow *row;
	for (row=data->head; row!=NULL; row=row->next)
	{
		int col;
		for (col=0; col<data->cols; col++)
		{
			int w, h;
			if (row->ents[col] != NULL)
			{
				GridChild *child = row->ents[col];
				child->layout->getPrefSize(child->layout, &w, &h);
				w /= child->colspan;
				h /= child->rowspan;
				
				if (w > mw) mw = w;
				if (h > mh) mh = h;
			};
		};
	};
	
	*width = mw;
	*height = mh;
};

static void gridRun(GWMLayout *grid, int x, int y, int width, int height)
{
	GridLayoutData *data = (GridLayoutData*) grid->data;
	if (data->rows == 0) return;
	
	int cellWidth = width / data->cols;
	int cellHeight = height / data->rows;
	
	GridRow *row;
	int rowno = 0;
	for (row=data->head; row!=NULL; row=row->next)
	{
		int col;
		for (col=0; col<data->cols; col++)
		{
			if (row->ents[col] != NULL)
			{
				GridChild *child = row->ents[col];
				child->layout->run(child->layout, x+cellWidth*col, y+cellHeight*rowno, cellWidth*child->colspan, cellHeight*child->rowspan);
			};
		};
		
		rowno++;
	};
};

GWMLayout* gwmCreateGridLayout(int cols)
{
	GWMLayout *grid = gwmCreateAbstractLayout();
	GridLayoutData *data = (GridLayoutData*) malloc(sizeof(GridLayoutData));
	data->cols = cols;
	data->rows = 0;
	data->head = NULL;
	
	grid->data = data;
	grid->getMinSize = gridMinSize;
	grid->getPrefSize = gridPrefSize;
	grid->run = gridRun;
	
	return grid;
};

static int isTaken(GridLayoutData *data, int col, int row)
{
	if (data->rows <= row) return 0;
	if (data->cols <= col) return 1;	// don't allow anything to be placed outside column limit
	
	GridRow *r = data->head;
	while (row--) r = r->next;
	
	return r->bitmap[col/8] & (1 << (col%8));
};

static int isRectTaken(GridLayoutData *data, int col, int row, int colspan, int rowspan)
{
	int x, y;
	for (x=col; x<(col+colspan); x++)
	{
		for (y=row; y<(row+rowspan); y++)
		{
			if (isTaken(data, x, y)) return 1;
		};
	};
	
	return 0;
};

static void markTaken(GridLayoutData *data, int col, int row)
{
	if (data->head == NULL)
	{
		GridRow *new = (GridRow*) malloc(sizeof(GridRow));
		new->bitmap = (uint8_t*) malloc(data->cols/8+1);
		memset(new->bitmap, 0, data->cols/8+1);
		new->ents = (GridChild**) malloc(sizeof(void*) * data->cols);
		memset(new->ents, 0, sizeof(void*) * data->cols);
		new->next = NULL;
		
		data->head = new;
		data->rows++;
	};
	
	GridRow *r = data->head;
	while (row--)
	{
		if (r->next == NULL)
		{
			GridRow *new = (GridRow*) malloc(sizeof(GridRow));
			new->bitmap = (uint8_t*) malloc(data->cols/8+1);
			memset(new->bitmap, 0, data->cols/8+1);
			new->ents = (GridChild**) malloc(sizeof(void*) * data->cols);
			memset(new->ents, 0, sizeof(void*) * data->cols);
			new->next = NULL;
			
			r->next = new;
			data->rows++;
		};
		
		r = r->next;
	};
	
	r->bitmap[col/8] |= (1<< (col%8));
};

static void markRectTaken(GridLayoutData *data, int col, int row, int colspan, int rowspan)
{
	int x, y;
	for (x=col; x<(col+colspan); x++)
	{
		for (y=row; y<(row+rowspan); y++)
		{
			markTaken(data, x, y);
		};
	};
};

void gwmGridLayoutAddLayout(GWMLayout *grid, GWMLayout *sublayout, int colspan, int rowspan)
{
	GridLayoutData *data = (GridLayoutData*) grid->data;
	if (colspan > data->cols)
	{
		// TODO: we need to make that error API that exists in my head
		return;
	};
	
	if (colspan < 1 || rowspan < 1)
	{
		// TODO
		return;
	};
	
	int x, y;
	for (y=0;;y++)
	{
		for (x=0; x<=(data->cols-colspan); x++)
		{
			if (!isRectTaken(data, x, y, colspan, rowspan))
			{
				// we can add it here
				markRectTaken(data, x, y, colspan, rowspan);
				
				GridRow *r = data->head;
				while (y--) r = r->next;
				
				GridChild *child = (GridChild*) malloc(sizeof(GridChild));
				child->layout = sublayout;
				child->colspan = colspan;
				child->rowspan = rowspan;
				
				r->ents[x] = child;
				return;
			};
		};
	};
};

void gwmGridLayoutAddWindow(GWMLayout *grid, GWMWindow *child, int colspan, int rowspan)
{
	gwmGridLayoutAddLayout(grid, gwmCreateLeafLayout(child), colspan, rowspan);
};

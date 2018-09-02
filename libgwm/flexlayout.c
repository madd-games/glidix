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
 * FLEX LAYOUT
 * Lays children out in a flexible grid. Each row and column may be a different size.
 */

typedef struct
{
	GWMLayout*				layout;
	int					xScaling;
	int					yScaling;
	int					xProp;
	int					yProp;
} FlexChild;

typedef struct FlexRow_
{
	/**
	 * Bitmap of which columns are taken.
	 */
	uint8_t*				bitmap;
	
	/**
	 * Array of entries, each representing a column. Some may be NULL.
	 */
	FlexChild**				ents;
	
	/**
	 * Pointer to next row.
	 */
	struct FlexRow_*			next;
} FlexRow;

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
	FlexRow*				head;
} FlexLayoutData;

typedef struct
{
	int minLen;
	int prefLen;
	int prop;
} FlexGridInfo;

void gwmDestroyFlexLayout(GWMLayout *flex)
{
	FlexLayoutData *data = (FlexLayoutData*) flex->data;
	while (data->head != NULL)
	{
		FlexRow *row = data->head;
		data->head = row->next;
		
		int i;
		for (i=0; i<data->cols; i++)
		{
			free(row->ents[i]);
		};
		
		free(row->bitmap);
		free(row);
	};
	
	free(data);
	gwmDestroyAbstractLayout(flex);
};

static void getRowInfo(FlexLayoutData *data, FlexGridInfo *info)
{
	// get information about each row
	FlexRow *row;
	int counter = 0;
	for (row=data->head; row!=NULL; row=row->next)
	{
		FlexGridInfo *rowInfo = &info[counter++];
		rowInfo->minLen = 0;
		rowInfo->prefLen = 0;
		rowInfo->prop = 0;
		
		int i;
		for (i=0; i<data->cols; i++)
		{
			FlexChild *child = row->ents[i];
			if (child != NULL)
			{
				int minWidth, minHeight;
				child->layout->getMinSize(child->layout, &minWidth, &minHeight);
				
				int prefWidth, prefHeight;
				child->layout->getPrefSize(child->layout, &prefWidth, &prefHeight);
				
				if (minHeight > rowInfo->minLen) rowInfo->minLen = minHeight;
				if (prefHeight > rowInfo->prefLen) rowInfo->prefLen = prefHeight;
				if (child->yProp > rowInfo->prop) rowInfo->prop = child->yProp;
			};
		};
	};
};

static void getColInfo(FlexLayoutData *data, FlexGridInfo *info)
{
	// get information about each column
	int i;
	for (i=0; i<data->cols; i++)
	{
		FlexGridInfo *colInfo = &info[i];
		colInfo->minLen = 0;
		colInfo->prefLen = 0;
		colInfo->prop = 0;
		
		FlexRow *row;
		for (row=data->head; row!=NULL; row=row->next)
		{
			FlexChild *child = row->ents[i];
			if (child != NULL)
			{
				int minWidth, minHeight;
				child->layout->getMinSize(child->layout, &minWidth, &minHeight);
				
				int prefWidth, prefHeight;
				child->layout->getPrefSize(child->layout, &prefWidth, &prefHeight);
				
				if (minWidth > colInfo->minLen) colInfo->minLen = minWidth;
				if (prefWidth > colInfo->prefLen) colInfo->prefLen = prefWidth;
				if (child->xProp > colInfo->prop) colInfo->prop = child->xProp;
			};
		};
	};
};

static void flexMinSize(GWMLayout *flex, int *width, int *height)
{
	FlexLayoutData *data = (FlexLayoutData*) flex->data;
	
	FlexGridInfo cols[data->cols];
	FlexGridInfo rows[data->rows];
	
	getColInfo(data, cols);
	getRowInfo(data, rows);

	int i;
		
	// first handle the columns
	int units=0, longestUnit=0, totalLen=0;
	for (i=0; i<data->cols; i++)
	{
		FlexGridInfo *info = &cols[i];
		if (info->prop == 0)
		{
			totalLen += info->minLen;
		}
		else
		{
			units += info->prop;
			
			int unitLen = info->minLen / info->prop;
			if (unitLen > longestUnit) longestUnit = unitLen;
		};
	};
	
	*width = totalLen + longestUnit * units;

	// now handle rows
	units=0; longestUnit=0; totalLen=0;
	for (i=0; i<data->rows; i++)
	{
		FlexGridInfo *info = &rows[i];
		if (info->prop == 0)
		{
			totalLen += info->minLen;
		}
		else
		{
			units += info->prop;
			
			int unitLen = info->minLen / info->prop;
			if (unitLen > longestUnit) longestUnit = unitLen;
		};
	};
	
	*height = totalLen + longestUnit * units;
};

static void flexPrefSize(GWMLayout *flex, int *width, int *height)
{
	// for preffered size, we still assign minimum length to children with zero proportion,
	// but we ensure at least preffered length for proportioned children
	FlexLayoutData *data = (FlexLayoutData*) flex->data;
	
	FlexGridInfo cols[data->cols];
	FlexGridInfo rows[data->rows];
	
	getColInfo(data, cols);
	getRowInfo(data, rows);

	int i;
		
	// first handle the columns
	int units=0, longestUnit=0, totalLen=0;
	for (i=0; i<data->cols; i++)
	{
		FlexGridInfo *info = &cols[i];
		if (info->prop == 0)
		{
			totalLen += info->minLen;
		}
		else
		{
			units += info->prop;
			
			// for preffered size, do not divide by proportion, because we actually
			// want the preffered size to be longer for higher proportions
			int unitLen = info->prefLen;
			if (unitLen > longestUnit) longestUnit = unitLen;
		};
	};
	
	*width = totalLen + longestUnit * units;

	// now handle rows
	units=0; longestUnit=0; totalLen=0;
	for (i=0; i<data->rows; i++)
	{
		FlexGridInfo *info = &rows[i];
		if (info->prop == 0)
		{
			totalLen += info->minLen;
		}
		else
		{
			units += info->prop;

			// for preffered size, do not divide by proportion, because we actually
			// want the preffered size to be longer for higher proportions
			int unitLen = info->prefLen;
			if (unitLen > longestUnit) longestUnit = unitLen;
		};
	};
	
	*height = totalLen + longestUnit * units;
};

static void flexRun(GWMLayout *flex, int x, int y, int width, int height)
{
	FlexLayoutData *data = (FlexLayoutData*) flex->data;
	if (data->rows == 0) return;
	
	FlexGridInfo cols[data->cols];
	FlexGridInfo rows[data->rows];
	
	getColInfo(data, cols);
	getRowInfo(data, rows);
	
	int i;
	
	// calculate fixed space and units
	int fixedWidth=0, fixedHeight=0;
	int xUnits=0, yUnits=0;
	for (i=0; i<data->cols; i++)
	{
		if (cols[i].prop == 0) fixedWidth += cols[i].minLen;
		else xUnits += cols[i].prop;
	};
	for (i=0; i<data->rows; i++)
	{
		if (rows[i].prop == 0) fixedHeight += rows[i].minLen;
		else yUnits += rows[i].prop;
	};
	
	// calculate unit lengths
	int xUnitLen=0, yUnitLen=0;
	if (xUnits != 0) xUnitLen = (width-fixedWidth)/xUnits;
	if (yUnits != 0) yUnitLen = (height-fixedHeight)/yUnits;
	
	// lay out children
	int counter=0;
	int yOffset = 0;
	
	FlexRow *row;
	for (row=data->head; row!=NULL; row=row->next)
	{
		int rowno = counter++;
		FlexGridInfo *rowInfo = &rows[rowno];
		
		// figure out how much height this row gets
		int rowHeight;
		if (rowInfo->prop == 0) rowHeight = rowInfo->minLen;
		else rowHeight = yUnitLen * rowInfo->prop;
		
		// go through all columns
		int xOffset = 0;
		int colno;
		for (colno=0; colno<data->cols; colno++)
		{
			FlexGridInfo *colInfo = &cols[colno];
			
			// figure out how much width this column gets
			int colWidth;
			if (colInfo->prop == 0) colWidth = colInfo->minLen;
			else colWidth = xUnitLen * colInfo->prop;
			
			// assign this space to the child
			FlexChild *child = row->ents[colno];
			if (child != NULL)
			{
				int childX = x + xOffset;
				int childY = y + yOffset;
				int childWidth = colWidth;
				int childHeight = rowHeight;
				
				int minWidth, minHeight;
				child->layout->getMinSize(child->layout, &minWidth, &minHeight);
				
				if (child->xScaling == GWM_FLEX_CENTER)
				{
					childWidth = minWidth;
					childX += (colWidth-childWidth)/2;
				};
				
				if (child->yScaling == GWM_FLEX_CENTER)
				{
					childHeight = minHeight;
					childY += (rowHeight-childHeight)/2;
				};
				
				child->layout->run(child->layout, childX, childY, childWidth, childHeight);
			};
			
			// advance the X position
			xOffset += colWidth;
		};
		
		// advance the Y position
		yOffset += rowHeight;
	};
};

GWMLayout* gwmCreateFlexLayout(int cols)
{
	GWMLayout *flex = gwmCreateAbstractLayout();
	FlexLayoutData *data = (FlexLayoutData*) malloc(sizeof(FlexLayoutData));
	data->cols = cols;
	data->rows = 0;
	data->head = NULL;
	
	flex->data = data;
	flex->getMinSize = flexMinSize;
	flex->getPrefSize = flexPrefSize;
	flex->run = flexRun;
	
	return flex;
};

static int isTaken(FlexLayoutData *data, int col, int row)
{
	if (data->rows <= row) return 0;
	if (data->cols <= col) return 1;	// don't allow anything to be placed outside column limit
	
	FlexRow *r = data->head;
	while (row--) r = r->next;
	
	return r->bitmap[col/8] & (1 << (col%8));
};

static void markTaken(FlexLayoutData *data, int col, int row)
{
	if (data->head == NULL)
	{
		FlexRow *new = (FlexRow*) malloc(sizeof(FlexRow));
		new->bitmap = (uint8_t*) malloc(data->cols/8+1);
		memset(new->bitmap, 0, data->cols/8+1);
		new->ents = (FlexChild**) malloc(sizeof(void*) * data->cols);
		memset(new->ents, 0, sizeof(void*) * data->cols);
		new->next = NULL;
		
		data->head = new;
		data->rows++;
	};
	
	FlexRow *r = data->head;
	while (row--)
	{
		if (r->next == NULL)
		{
			FlexRow *new = (FlexRow*) malloc(sizeof(FlexRow));
			new->bitmap = (uint8_t*) malloc(data->cols/8+1);
			memset(new->bitmap, 0, data->cols/8+1);
			new->ents = (FlexChild**) malloc(sizeof(void*) * data->cols);
			memset(new->ents, 0, sizeof(void*) * data->cols);
			new->next = NULL;
			
			r->next = new;
			data->rows++;
		};
		
		r = r->next;
	};
	
	r->bitmap[col/8] |= (1<< (col%8));
};

void gwmFlexLayoutAddLayout(GWMLayout *flex, GWMLayout *sublayout, int xProp, int yProp, int xScaling, int yScaling)
{
	FlexLayoutData *data = (FlexLayoutData*) flex->data;
	
	int x, y;
	for (y=0;;y++)
	{
		for (x=0; x<=data->cols; x++)
		{
			if (!isTaken(data, x, y))
			{
				// we can add it here
				markTaken(data, x, y);
				
				FlexRow *r = data->head;
				while (y--) r = r->next;
				
				FlexChild *child = (FlexChild*) malloc(sizeof(FlexChild));
				child->layout = sublayout;
				child->xScaling = xScaling;
				child->yScaling = yScaling;
				child->xProp = xProp;
				child->yProp = yProp;
				
				r->ents[x] = child;
				return;
			};
		};
	};
};

void gwmFlexLayoutAddWindow(GWMLayout *flex, GWMWindow *child, int xProp, int yProp, int xScaling, int yScaling)
{
	gwmFlexLayoutAddLayout(flex, gwmCreateLeafLayout(child), xProp, yProp, xScaling, yScaling);
};

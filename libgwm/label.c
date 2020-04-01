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
	/**
	 * The text of the label.
	 */
	char *text;
	
	/**
	 * Maximum width. 0 means unlimited. If the width is limited, the text wraps around.
	 */
	int width;
	
	/**
	 * Resulting preferred width and height after drawing the text.
	 */
	int prefWidth, prefHeight;
	
	/**
	 * Font.
	 */
	DDIFont* font;
	
	/**
	 * Alignment.
	 */
	int align;
	
	/**
	 * Border.
	 */
	int borderStyle;
	int borderWidth;
} LabelData;

static void gwmRedrawLabel(GWMWindow *label);

static int labelHandler(GWMEvent *ev, GWMWindow *win, void *context)
{
	switch (ev->type)
	{
	case GWM_EVENT_RETHEME:
		gwmRedrawLabel(win);
		return GWM_EVSTATUS_OK;
	default:
		return GWM_EVSTATUS_CONT;
	};
};

static void gwmRedrawLabel(GWMWindow *label)
{
	static DDIColor trans = {0, 0, 0, 0};
	
	LabelData *data = (LabelData*) gwmGetData(label, labelHandler);
	DDISurface *canvas = gwmGetWindowCanvas(label);
	
	int width = data->width;
	if (width == 0) width = canvas->width;
	if (canvas->width != 0 && width > canvas->width) width = canvas->width;
	
	DDIPen *pen = gwmGetPen(label, 1+data->borderWidth, 1+data->borderWidth, width-2*data->borderWidth-2, 0);
	assert(pen != NULL);
	ddiSetPenFont(pen, data->font);
	if (data->width == 0) ddiSetPenWrap(pen, 0);
	ddiSetPenAlignment(pen, data->align);
	ddiWritePen(pen, data->text);
	ddiGetPenSize(pen, &data->prefWidth, &data->prefHeight);
	ddiFillRect(canvas, 0, 0, canvas->width, canvas->height, &trans);
	if (canvas->width != 0) ddiExecutePen(pen, canvas);
	ddiDeletePen(pen);
	
	if (data->borderStyle != GWM_BORDER_NONE)
	{
		DDIColor *top;
		DDIColor *bottom;
		
		if (data->borderStyle == GWM_BORDER_RAISED)
		{
			top = GWM_COLOR_BORDER_LIGHT;
			bottom = GWM_COLOR_BORDER_DARK;
		}
		else
		{
			top = GWM_COLOR_BORDER_DARK;
			bottom = GWM_COLOR_BORDER_LIGHT;
		};
		
		int i;
		for (i=0; i<data->borderWidth; i++)
		{
			ddiFillRect(canvas, i, i, canvas->width-2*i, 1, top);
			ddiFillRect(canvas, i, i, 1, canvas->height-2*i, top);
			
			ddiFillRect(canvas, i, canvas->height-1-i, canvas->width-i, 1, bottom);
			ddiFillRect(canvas, canvas->width-1-i, i, 1, canvas->height-i, bottom);
		};
	};
	
	data->prefWidth += 2 + 2*data->borderWidth;
	data->prefHeight += 2 + 2*data->borderWidth;
	
	gwmPostDirty(label);
};

static void gwmSizeLabel(GWMWindow *label, int *width, int *height)
{
	LabelData *data = (LabelData*) gwmGetData(label, labelHandler);
	*width = data->prefWidth;
	*height = data->prefHeight;
};

static void gwmPositionLabel(GWMWindow *label, int x, int y, int width, int height)
{
	gwmMoveWindow(label, x, y);
	gwmResizeWindow(label, width, height);
	gwmRedrawLabel(label);
};

GWMWindow* gwmNewLabel(GWMWindow *parent)
{
	GWMWindow *label = gwmCreateWindow(parent, "GWMLabel", 0, 0, 0, 0, 0);
	
	LabelData *data = (LabelData*) malloc(sizeof(LabelData));
	data->text = strdup("");
	data->width = 0;
	data->prefWidth = 0;
	data->prefHeight = 0;
	data->font = gwmGetDefaultFont();
	data->align = DDI_ALIGN_LEFT;
	data->borderStyle = GWM_BORDER_NONE;
	data->borderWidth = 0;
	
	label->getMinSize = label->getPrefSize = gwmSizeLabel;
	label->position = gwmPositionLabel;
	
	gwmPushEventHandler(label, labelHandler, data);
	return label;
};

void gwmSetLabelText(GWMWindow *label, const char *text)
{
	LabelData *data = (LabelData*) gwmGetData(label, labelHandler);
	free(data->text);
	data->text = strdup(text);
	gwmRedrawLabel(label);
};

void gwmSetLabelWidth(GWMWindow *label, int width)
{
	LabelData *data = (LabelData*) gwmGetData(label, labelHandler);
	data->width = width;
	gwmRedrawLabel(label);
};

GWMWindow* gwmCreateLabel(GWMWindow *parent, const char *text, int width)
{
	GWMWindow *label = gwmNewLabel(parent);
	gwmSetLabelText(label, text);
	gwmSetLabelWidth(label, width);
	return label;
};

void gwmDestroyLabel(GWMWindow *label)
{
	LabelData *data = (LabelData*) gwmGetData(label, labelHandler);
	gwmDestroyWindow(label);
	free(data->text);
	free(data);
};

void gwmSetLabelFont(GWMLabel *label, DDIFont *font)
{
	LabelData *data = (LabelData*) gwmGetData(label, labelHandler);
	data->font = font;
	gwmRedrawLabel(label);
};

void gwmSetLabelAlignment(GWMLabel *label, int align)
{
	LabelData *data = (LabelData*) gwmGetData(label, labelHandler);
	data->align = align;
	gwmRedrawLabel(label);
};

void gwmSetLabelBorder(GWMLabel *label, int style, int width)
{
	LabelData *data = (LabelData*) gwmGetData(label, labelHandler);
	data->borderStyle = style;
	data->borderWidth = width;
	gwmRedrawLabel(label);
};

GWMLabel* gwmNewStatusBarLabel(GWMWindow *statbar)
{
	GWMLabel *label = gwmNewLabel(statbar);
	gwmBoxLayoutAddWindow(statbar->layout, label, 0, 2, GWM_BOX_LEFT);
	gwmSetLabelBorder(label, GWM_BORDER_SUNKEN, 1);
	return label;
};

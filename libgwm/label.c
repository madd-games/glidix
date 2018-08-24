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
} LabelData;

static int labelHandler(GWMEvent *ev, GWMWindow *win, void *context)
{
	return GWM_EVSTATUS_CONT;
};

static void gwmRedrawLabel(GWMWindow *label)
{
	static DDIColor trans = {0, 0, 0, 0};
	
	LabelData *data = (LabelData*) gwmGetData(label, labelHandler);
	DDISurface *canvas = gwmGetWindowCanvas(label);
	
	int width = data->width;
	if (width == 0) width = canvas->width;
	if (canvas->width != 0 && width > canvas->width) width = canvas->width;
	
	DDIPen *pen = ddiCreatePen(&canvas->format, data->font, 0, 0, width, 0, 0, 0, NULL);
	assert(pen != NULL);
	if (data->width == 0) ddiSetPenWrap(pen, 0);
	ddiSetPenAlignment(pen, data->align);
	ddiWritePen(pen, data->text);
	ddiGetPenSize(pen, &data->prefWidth, &data->prefHeight);
	ddiFillRect(canvas, 0, 0, canvas->width, canvas->height, &trans);
	if (canvas->width != 0) ddiExecutePen(pen, canvas);
	ddiDeletePen(pen);
	
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

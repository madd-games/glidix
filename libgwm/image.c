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
	 * The surface from which to get the image.
	 */
	DDISurface* surf;
	
	/**
	 * The viewport. If width or height is zero, then as much of the surface as possible
	 * is shown on the corresponding axis.
	 */
	int x, y;
	int width, height;
} ImageData;

static int imageHandler(GWMEvent *ev, GWMWindow *win, void *context)
{
	return GWM_EVSTATUS_CONT;
};

static void gwmRedrawImage(GWMWindow *image)
{
	ImageData *data = (ImageData*) gwmGetData(image, imageHandler);
	DDISurface *canvas = gwmGetWindowCanvas(image);
	
	static DDIColor trans = {0, 0, 0, 0};
	ddiFillRect(canvas, 0, 0, canvas->width, canvas->height, &trans);
	
	if (data->surf != NULL)
	{
		int x = data->x;
		int y = data->y;
		int width = data->width;
		int height = data->height;
		
		if (width == 0) width = canvas->width - x;
		if (height == 0) height = canvas->height - y;
		
		ddiBlit(data->surf, x, y, canvas, 0, 0, width, height);
	};
	
	gwmPostDirty(image);
};

static void gwmSizeImage(GWMWindow *image, int *widthOut, int *heightOut)
{
	ImageData *data = (ImageData*) gwmGetData(image, imageHandler);
	
	int width = data->width;
	int height = data->height;
	
	if (data->surf != NULL)
	{
		if (width == 0) width = data->surf->width - data->x;
		if (height == 0) height = data->surf->height - data->y;
	};
	
	*widthOut = width;
	*heightOut = height;
};

static void gwmPositionImage(GWMWindow *image, int x, int y, int width, int height)
{
	gwmMoveWindow(image, x, y);
	gwmResizeWindow(image, width, height);
	gwmRedrawImage(image);
};

GWMWindow* gwmNewImage(GWMWindow *parent)
{
	GWMWindow *image = gwmCreateWindow(parent, "GWMImage", 0, 0, 0, 0, 0);
	
	ImageData *data = (ImageData*) malloc(sizeof(ImageData));
	data->surf = NULL;
	data->x = 0;
	data->y = 0;
	data->width = 0;
	data->height = 0;
	
	image->getMinSize = image->getPrefSize = gwmSizeImage;
	image->position = gwmPositionImage;
	
	gwmPushEventHandler(image, imageHandler, data);
	return image;
};

void gwmSetImageSurface(GWMWindow *image, DDISurface *surf)
{
	ImageData *data = (ImageData*) gwmGetData(image, imageHandler);
	data->surf = surf;
	gwmRedrawImage(image);
};

void gwmSetImageViewport(GWMWindow *image, int x, int y, int width, int height)
{
	ImageData *data = (ImageData*) gwmGetData(image, imageHandler);
	data->x = x;
	data->y = y;
	data->width = width;
	data->height = height;
	gwmRedrawImage(image);
};

GWMWindow* gwmCreateImage(GWMWindow *parent, DDISurface *surf, int x, int y, int width, int height)
{
	GWMWindow *image = gwmNewImage(parent);
	gwmSetImageSurface(image, surf);
	gwmSetImageViewport(image, x, y, width, height);
	return image;
};

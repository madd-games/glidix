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
	int				flags;
	int				clicked;
} SliderData;

static DDISurface* imgSlider;
static DDIColor *colSliderActive;
static DDIColor *colSliderInactive;
static DDIColor *colSliderDisabled;

static int sliderHandler(GWMEvent *ev, GWMWindow *slider, void *context);

static void redrawSlider(GWMWindow *slider)
{
	SliderData *data = (SliderData*) gwmGetData(slider, sliderHandler);

	if (imgSlider == NULL)
	{
		imgSlider = (DDISurface*) gwmGetThemeProp("gwm.toolkit.slider", GWM_TYPE_SURFACE, NULL);
		if (imgSlider == NULL)
		{
			fprintf(stderr, "libgwm: failed to load slider image\n");
			abort();
		};
		
		colSliderActive = (DDIColor*) gwmGetThemeProp("gwm.toolkit.slider.active", GWM_TYPE_COLOR, NULL);
		colSliderInactive = (DDIColor*) gwmGetThemeProp("gwm.toolkit.slider.inactive", GWM_TYPE_COLOR, NULL);
		colSliderDisabled = (DDIColor*) gwmGetThemeProp("gwm.toolkit.slider.disabled", GWM_TYPE_COLOR, NULL);
		
		assert(colSliderActive != NULL);
		assert(colSliderInactive != NULL);
		assert(colSliderDisabled != NULL);
	};
	
	static DDIColor transparent = {0, 0, 0, 0};
	DDISurface *canvas = gwmGetWindowCanvas(slider);
	ddiFillRect(canvas, 0, 0, canvas->width, canvas->height, &transparent);
	
	if (data->flags & GWM_SLIDER_DISABLED)
	{
		if (data->flags & GWM_SLIDER_HORIZ)
		{
			ddiFillRect(canvas, 10, 5, canvas->width-20, 10, colSliderDisabled);
		}
		else
		{
			ddiFillRect(canvas, 5, 10, 10, canvas->height-20, colSliderDisabled);
		};
	}
	else
	{
		float value = gwmGetScaleValue(slider);
		if (data->flags & GWM_SLIDER_HORIZ)
		{
			int len = (canvas->width-20) * value;
			ddiFillRect(canvas, 10, 5, canvas->width-20, 10, colSliderInactive);
			ddiFillRect(canvas, 10, 5, len, 10, colSliderActive);
			ddiBlit(imgSlider, 0, 0, canvas, len, 0, 20, 20);
		}
		else
		{
			int len = (canvas->height-20) * value;
			ddiFillRect(canvas, 5, 10, 10, canvas->height-20, colSliderInactive);
			ddiFillRect(canvas, 5, 10, 10, len, colSliderActive);
			ddiBlit(imgSlider, 0, 0, canvas, 0, len, 20, 20);
		};
	};
	
	gwmPostDirty(slider);
};

static int sliderHandler(GWMEvent *ev, GWMWindow *slider, void *context)
{
	SliderData *data = (SliderData*) context;
	DDISurface *canvas = gwmGetWindowCanvas(slider);
	
	switch (ev->type)
	{
	case GWM_EVENT_VALUE_CHANGED:
	case GWM_EVENT_RETHEME:
		redrawSlider(slider);
		return GWM_EVSTATUS_CONT;
	case GWM_EVENT_UP:
		if (ev->keycode == GWM_KC_MOUSE_LEFT)
		{
			data->clicked = 0;
		};
		return GWM_EVSTATUS_CONT;
	case GWM_EVENT_DOWN:
		if (ev->keycode == GWM_KC_MOUSE_LEFT)
		{
			data->clicked = 1;
		};
		// no break
	case GWM_EVENT_MOTION:
		if (data->flags & GWM_SLIDER_DISABLED)
		{
			return GWM_EVSTATUS_OK;
		};
		
		if (data->clicked)
		{
			if (data->flags & GWM_SLIDER_HORIZ)
			{
				float newValue = (float) (ev->x - 5) / (float) (canvas->width - 10);
				gwmSetSliderValue(slider, newValue);
			}
			else
			{
				float newValue = (float) (ev->y - 5) / (float) (canvas->height - 10);
				gwmSetSliderValue(slider, newValue);
			};
		};
		return GWM_EVSTATUS_CONT;
	default:
		return GWM_EVSTATUS_CONT;
	};
};

static void sizeSlider(GWMWindow *slider, int *width, int *height)
{
	SliderData *data = (SliderData*) gwmGetData(slider, sliderHandler);
	if (data->flags & GWM_SLIDER_HORIZ)
	{
		*width = 100;
		*height = 20;
	}
	else
	{
		*width = 20;
		*height = 100;
	};
};

static void positionSlider(GWMWindow *slider, int x, int y, int width, int height)
{
	SliderData *data = (SliderData*) gwmGetData(slider, sliderHandler);
	if (data->flags & GWM_SLIDER_HORIZ)
	{
		y += (20 - height) / 2;
		height = 20;
	}
	else
	{
		x += (20 - width) / 2;
		width = 20;
	};
	
	gwmMoveWindow(slider, x, y);
	gwmResizeWindow(slider, width, height);
	redrawSlider(slider);
};

GWMWindow* gwmNewSlider(GWMWindow *parent)
{
	GWMWindow *slider = gwmNewScale(parent);
	if (slider == NULL) return NULL;
	
	SliderData *data = (SliderData*) malloc(sizeof(SliderData));
	data->flags = 0;
	data->clicked = 0;
	
	slider->getMinSize = slider->getPrefSize = sizeSlider;
	slider->position = positionSlider;
	
	gwmPushEventHandler(slider, sliderHandler, data);
	redrawSlider(slider);
	return slider;
};

void gwmDestroySlider(GWMWindow *slider)
{
	SliderData *data = (SliderData*) gwmGetData(slider, sliderHandler);
	free(data);
	gwmDestroyScale(slider);
};

void gwmSetSliderFlags(GWMWindow *slider, int flags)
{
	SliderData *data = (SliderData*) gwmGetData(slider, sliderHandler);
	data->flags = flags;
	redrawSlider(slider);
};

void gwmSetSliderValue(GWMWindow *slider, float value)
{
	gwmSetScaleValue(slider, value);
};

float gwmGetSliderValue(GWMWindow *slider)
{
	return gwmGetScaleValue(slider);
};

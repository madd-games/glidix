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

#define	SLIDER_MIN_LEN				30
#define	SLIDER_BREADTH				20

typedef struct
{
	int					len;
	int					flags;
	int					value;
	int					max;
	int					pressed;
} GWMSliderData;

static DDISurface *imgSlider = NULL;

static void gwmRedrawSlider(GWMWindow *slider)
{
	static DDIColor backColor = {0x77, 0x77, 0x77, 0xFF};
	static DDIColor foreColor = {0, 0xAA, 0, 0xFF};
	static DDIColor disabledColor = {0x44, 0x44, 0x44, 0xFF};
	
	GWMSliderData *data = (GWMSliderData*) slider->data;
	
	DDISurface *canvas = gwmGetWindowCanvas(slider);
	static DDIColor transparent = {0, 0, 0, 0};
	ddiFillRect(canvas, 0, 0, canvas->width, canvas->height, &transparent);

	if (imgSlider == NULL)
	{
		const char *error;
		imgSlider = ddiLoadAndConvertPNG(&canvas->format, "/usr/share/images/slider.png", &error);
		if (imgSlider == NULL)
		{
			fprintf(stderr, "Failed to load slider image (/usr/share/images/slider.png): %s\n", error);
			abort();
		};
	};

	int sliderX = data->value * (data->len-SLIDER_BREADTH) / data->max;
	if (data->flags & GWM_SLIDER_HORIZ)
	{
		if (data->flags & GWM_SLIDER_DISABLED)
		{
			ddiFillRect(canvas, SLIDER_BREADTH/2, SLIDER_BREADTH/3, data->len-SLIDER_BREADTH, SLIDER_BREADTH/3+1,
				&disabledColor);
		}
		else
		{
			ddiFillRect(canvas, SLIDER_BREADTH/2, SLIDER_BREADTH/3, data->len-SLIDER_BREADTH, SLIDER_BREADTH/3+1,
				&backColor);
			ddiFillRect(canvas, SLIDER_BREADTH/2, SLIDER_BREADTH/3, sliderX, SLIDER_BREADTH/3+1, &foreColor);
			
			ddiBlit(imgSlider, 0, 0, canvas, sliderX, 0, SLIDER_BREADTH, SLIDER_BREADTH);
		};
	}
	else
	{
		if (data->flags & GWM_SLIDER_DISABLED)
		{
			ddiFillRect(canvas, SLIDER_BREADTH/3, SLIDER_BREADTH/2, SLIDER_BREADTH/3+1, data->len-SLIDER_BREADTH,
				&disabledColor);
		}
		else
		{
			ddiFillRect(canvas, SLIDER_BREADTH/3, SLIDER_BREADTH/2, SLIDER_BREADTH/3+1, data->len-SLIDER_BREADTH,
				&backColor);
			ddiFillRect(canvas, SLIDER_BREADTH/3, SLIDER_BREADTH/2, SLIDER_BREADTH/3+1, sliderX, &foreColor);
			
			ddiBlit(imgSlider, 0, 0, canvas, 0, sliderX, SLIDER_BREADTH, SLIDER_BREADTH);
		};
	};
	
	gwmPostDirty(slider);
};

static int gwmSliderHandler(GWMEvent *ev, GWMWindow *slider)
{
	GWMSliderData *data = (GWMSliderData*) slider->data;
	
	int pressPos = ev->y;
	if (data->flags & GWM_SLIDER_HORIZ) pressPos = ev->x;
	
	if (data->flags & GWM_SLIDER_DISABLED)
	{
		return gwmDefaultHandler(ev, slider);
	};
	
	switch (ev->type)
	{
	case GWM_EVENT_DOWN:
		if (ev->keycode == GWM_KC_MOUSE_LEFT)
		{
			data->pressed = 1;
			data->value = (pressPos-SLIDER_BREADTH/2) * data->max / (data->len-SLIDER_BREADTH);
			if (data->value < 0) data->value = 0;
			if (data->value > data->max) data->value = data->max;
			gwmRedrawSlider(slider);
		};
		return 0;
	case GWM_EVENT_UP:
		if (ev->keycode == GWM_KC_MOUSE_LEFT)
		{
			data->pressed = 0;
		};
		return 0;
	case GWM_EVENT_MOTION:
		if (data->pressed)
		{
			data->value = (pressPos-SLIDER_BREADTH/2) * data->max / (data->len-SLIDER_BREADTH);
			if (data->value < 0) data->value = 0;
			if (data->value > data->max) data->value = data->max;
			gwmRedrawSlider(slider);
		};
		return 0;
	default:
		return gwmDefaultHandler(ev, slider);
	};
};

GWMWindow* gwmCreateSlider(GWMWindow *parent, int x, int y, int len, int value, int max, int flags)
{
	int width, height;
	if (len < SLIDER_MIN_LEN) len = SLIDER_MIN_LEN;
	
	if (flags & GWM_SLIDER_HORIZ)
	{
		width = len;
		height = SLIDER_BREADTH;
	}
	else
	{
		width = SLIDER_BREADTH;
		height = len;
	};
	
	GWMWindow *slider = gwmCreateWindow(parent, "GWMSlider", x, y, width, height, 0);
	if (slider == NULL) return NULL;
	
	GWMSliderData *data = (GWMSliderData*) malloc(sizeof(GWMSliderData));
	data->len = len;
	data->flags = flags;
	data->value = value;
	data->max = max;
	data->pressed = 0;
	
	slider->data = data;
	
	gwmRedrawSlider(slider);
	gwmSetEventHandler(slider, gwmSliderHandler);
	return slider;
};

void gwmDestroySlider(GWMWindow *slider)
{
	free(slider->data);
	gwmDestroyWindow(slider);
};

void gwmSetSliderParams(GWMWindow *slider, int value, int max, int flags)
{
	GWMSliderData *data = (GWMSliderData*) slider->data;
	data->value = value;
	data->max = max;
	data->flags = (data->flags & GWM_SLIDER_HORIZ) | (flags & ~GWM_SLIDER_HORIZ);
	data->pressed = 0;
	gwmRedrawSlider(slider);
};

int gwmGetSliderValue(GWMWindow *slider)
{
	GWMSliderData *data = (GWMSliderData*) slider->data;
	return data->value;
};

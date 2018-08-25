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
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

enum
{
	SPIN_NORMAL,
	SPIN_HOVER,
	SPIN_PRESSED,
	SPIN_DISABLED
};

typedef struct
{
	/**
	 * The text field inside the spinner.
	 */
	GWMTextField*				field;
	
	/**
	 * The layout.
	 */
	GWMLayout*				layout;
	
	/**
	 * State of the dec/inc buttons.
	 */
	int					decState;
	int					incState;
} SpinnerData;

typedef struct
{
	int					value;
	int					minval;
	int					maxval;
	int					step;
	char*					format;
} IntSpinnerData;

static DDISurface* imgSpin;

static int spinHandler(GWMEvent *ev, GWMSpinner *spin, void *context);

static void redrawSpinner(GWMSpinner *spin)
{
	SpinnerData *data = (SpinnerData*) gwmGetData(spin, spinHandler);
	DDISurface *canvas = gwmGetWindowCanvas(spin);
	
	if (imgSpin == NULL)
	{
		imgSpin = (DDISurface*) gwmGetThemeProp("gwm.toolkit.spin", GWM_TYPE_SURFACE, NULL);
		if (imgSpin == NULL)
		{
			fprintf(stderr, "Failed to load spinner image\n");
			abort();
		};
	};
	
	static DDIColor transparent = {0x00, 0x00, 0x00, 0x00};
	ddiFillRect(canvas, 0, 0, canvas->width, canvas->height, &transparent);
	
	ddiBlit(imgSpin, 0, 20*data->decState, canvas, 0, 0, 20, 20);
	ddiBlit(imgSpin, 20, 20*data->incState, canvas, canvas->width-20, 0, 20, 20);
	gwmPostDirty(spin);
};

static int spinHandler(GWMEvent *ev, GWMSpinner *spin, void *context)
{
	SpinnerData *data = (SpinnerData*) context;
	DDISurface *canvas = gwmGetWindowCanvas(spin);
	
	switch (ev->type)
	{
	case GWM_EVENT_RETHEME:
	case GWM_EVENT_RESIZED:
		redrawSpinner(spin);
		return GWM_EVSTATUS_OK;
	case GWM_EVENT_ENTER:
	case GWM_EVENT_MOTION:
		if (ev->x < 20)
		{
			if (data->decState != SPIN_DISABLED) data->decState = SPIN_HOVER;
			if (data->incState != SPIN_DISABLED) data->incState = SPIN_NORMAL;
		}
		else if (ev->x > canvas->width-20)
		{
			if (data->decState != SPIN_DISABLED) data->decState = SPIN_NORMAL;
			if (data->incState != SPIN_DISABLED) data->incState = SPIN_HOVER;
		}
		else
		{
			if (data->decState != SPIN_DISABLED) data->decState = SPIN_NORMAL;
			if (data->incState != SPIN_DISABLED) data->incState = SPIN_NORMAL;
		};
		redrawSpinner(spin);
		return GWM_EVSTATUS_OK;
	case GWM_EVENT_LEAVE:
		if (data->decState != SPIN_DISABLED) data->decState = SPIN_NORMAL;
		if (data->incState != SPIN_DISABLED) data->incState = SPIN_NORMAL;
		redrawSpinner(spin);
		return GWM_EVSTATUS_OK;
	case GWM_EVENT_DOWN:
		if (ev->keycode == GWM_KC_MOUSE_LEFT)
		{
			if (ev->x < 20)
			{
				if (data->decState != SPIN_DISABLED) data->decState = SPIN_PRESSED;
			}
			else
			{
				if (data->incState != SPIN_DISABLED) data->incState = SPIN_PRESSED;
			};
			redrawSpinner(spin);
		};
		return GWM_EVSTATUS_OK;
	case GWM_EVENT_UP:
		if (ev->keycode == GWM_KC_MOUSE_LEFT)
		{
			if (ev->x < 20)
			{
				if (data->decState != SPIN_DISABLED)
				{
					data->decState = SPIN_HOVER;
					
					GWMSpinEvent spinev;
					memset(&spinev, 0, sizeof(GWMSpinEvent));
					spinev.header.type = GWM_EVENT_SPIN_DECR;
					
					gwmPostEvent((GWMEvent*) &spinev, spin);
				};
			}
			else
			{
				if (data->incState != SPIN_DISABLED)
				{
					data->incState = SPIN_HOVER;
					
					GWMSpinEvent spinev;
					memset(&spinev, 0, sizeof(GWMSpinEvent));
					spinev.header.type = GWM_EVENT_SPIN_INCR;
					
					gwmPostEvent((GWMEvent*) &spinev, spin);
				};
			};
			redrawSpinner(spin);
		};
		return GWM_EVSTATUS_OK;
	case GWM_EVENT_COMMAND:
	case GWM_EVENT_EDIT_END:
		{
			GWMSpinEvent spinev;
			memset(&spinev, 0, sizeof(GWMSpinEvent));
			spinev.header.type = GWM_EVENT_SPIN_SET;
			spinev.text = gwmReadTextField(data->field);
			gwmPostEvent((GWMEvent*) &spinev, spin);
		};
		return GWM_EVSTATUS_OK;
	default:
		return GWM_EVSTATUS_CONT;
	};
};

GWMSpinner* gwmNewSpinner(GWMWindow *parent)
{
	GWMSpinner *spin = gwmCreateWindow(parent, "GWMSpinner", 0, 0, 0, 0, 0);
	if (spin == NULL) return NULL;
	
	SpinnerData *data = (SpinnerData*) malloc(sizeof(SpinnerData));
	data->layout = gwmCreateBoxLayout(GWM_BOX_HORIZONTAL);
	gwmSetWindowLayout(spin, data->layout);
	
	data->field = gwmNewTextField(spin);
	gwmBoxLayoutAddWindow(data->layout, data->field, 1, 20, GWM_BOX_LEFT | GWM_BOX_RIGHT | GWM_BOX_FILL);
	gwmSetTextFieldAlignment(data->field, DDI_ALIGN_CENTER);
	
	data->decState = data->incState = SPIN_NORMAL;
	
	gwmPushEventHandler(spin, spinHandler, data);
	return spin;
};

void gwmDestroySpinner(GWMSpinner *spin)
{
	SpinnerData *data = (SpinnerData*) gwmGetData(spin, spinHandler);
	gwmSetWindowLayout(spin, NULL);
	gwmDestroyBoxLayout(data->layout);
	gwmDestroyTextField(data->field);
	gwmDestroyWindow(spin);
	free(data);
};

void gwmSetSpinnerLabel(GWMSpinner *spin, const char *label)
{
	SpinnerData *data = (SpinnerData*) gwmGetData(spin, spinHandler);
	gwmWriteTextField(data->field, label);
	gwmSetWindowFlags(data->field, GWM_WINDOW_MKFOCUSED);
};

static int intHandler(GWMEvent *ev, GWMIntSpinner *ispin, void *context)
{
	IntSpinnerData *data = (IntSpinnerData*) context;
	GWMSpinEvent *spinev = (GWMSpinEvent*) ev;
	int newval;
	
	switch (ev->type)
	{
	case GWM_EVENT_SPIN_INCR:
		newval = data->value + data->step;
		if (newval <= data->maxval) gwmSetIntSpinnerValue(ispin, newval);
		return GWM_EVSTATUS_CONT;
	case GWM_EVENT_SPIN_DECR:
		newval = data->value - data->step;
		if (newval >= data->minval) gwmSetIntSpinnerValue(ispin, newval);
		return GWM_EVSTATUS_CONT;
	case GWM_EVENT_SPIN_SET:
		sscanf(spinev->text, data->format, &newval);
		if (newval == data->value) return GWM_EVSTATUS_CONT;
		if (newval < data->minval) newval = data->minval;
		if (newval > data->maxval) newval = data->maxval;
		gwmSetIntSpinnerValue(ispin, newval);
		return GWM_EVSTATUS_CONT;
	default:
		return GWM_EVSTATUS_CONT;
	};
};

GWMIntSpinner* gwmNewIntSpinner(GWMWindow *parent)
{
	GWMIntSpinner *ispin = gwmNewSpinner(parent);
	if (ispin == NULL) return NULL;
	
	IntSpinnerData *data = (IntSpinnerData*) malloc(sizeof(IntSpinnerData));
	data->value = 0;
	data->minval = 0;
	data->maxval = 100;
	data->step = 1;
	data->format = strdup("%d");
	
	gwmSetSpinnerLabel(ispin, "0");
	gwmPushEventHandler(ispin, intHandler, data);
	return ispin;
};

void gwmDestroyIntSpinner(GWMIntSpinner *ispin)
{
	IntSpinnerData *data = (IntSpinnerData*) gwmGetData(ispin, intHandler);
	free(data->format);
	free(data);
	gwmDestroySpinner(ispin);
};

void gwmSetIntSpinnerValue(GWMIntSpinner *ispin, int value)
{
	IntSpinnerData *data = (IntSpinnerData*) gwmGetData(ispin, intHandler);
	if (value < data->minval) value = data->minval;
	if (value > data->maxval) value = data->maxval;
	data->value = value;
	
	char *label;
	asprintf(&label, data->format, value);
	gwmSetSpinnerLabel(ispin, label);
	free(label);
};

void gwmSetIntSpinnerFormat(GWMIntSpinner *ispin, const char *format)
{
	IntSpinnerData *data = (IntSpinnerData*) gwmGetData(ispin, intHandler);
	free(data->format);
	data->format = strdup(format);
	gwmSetIntSpinnerValue(ispin, data->value);
};

int gwmGetIntSpinnerValue(GWMIntSpinner *ispin)
{
	IntSpinnerData *data = (IntSpinnerData*) gwmGetData(ispin, intHandler);
	return data->value;
};

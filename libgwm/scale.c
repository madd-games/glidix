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
	float				value;
	int				symbol;
} ScaleData;

static int scaleHandler(GWMEvent *ev, GWMWindow *scale, void *context)
{
	return GWM_EVSTATUS_CONT;
};

GWMWindow* gwmNewScale(GWMWindow *parent)
{
	GWMWindow *scale = gwmCreateWindow(parent, "GWMScale", 0, 0, 0, 0, 0);
	if (scale == NULL) return NULL;
	
	ScaleData *data = (ScaleData*) malloc(sizeof(ScaleData));
	data->value = 0.0;
	data->symbol = 0;
	
	gwmPushEventHandler(scale, scaleHandler, data);
	return scale;
};

void gwmDestroyScale(GWMWindow *scale)
{
	ScaleData *data = (ScaleData*) gwmGetData(scale, scaleHandler);
	free(data);
	gwmDestroyWindow(scale);
};

void gwmSetScaleSymbol(GWMWindow *scale, int symbol)
{
	ScaleData *data = (ScaleData*) gwmGetData(scale, scaleHandler);
	data->symbol = symbol;
};

void gwmSetScaleValue(GWMWindow *scale, float value)
{
	ScaleData *data = (ScaleData*) gwmGetData(scale, scaleHandler);
	if (value < 0.0) value = 0.0;
	if (value > 1.0) value = 1.0;
	data->value = value;
	
	GWMCommandEvent cmdev;
	memset(&cmdev, 0, sizeof(GWMCommandEvent));
	cmdev.header.type = GWM_EVENT_VALUE_CHANGED;
	cmdev.symbol = data->symbol;
	
	gwmPostEvent((GWMEvent*) &cmdev, scale);
};

float gwmGetScaleValue(GWMWindow *scale)
{
	ScaleData *data = (ScaleData*) gwmGetData(scale, scaleHandler);
	return data->value;
};

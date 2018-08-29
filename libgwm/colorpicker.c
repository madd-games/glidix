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
	 * "Old" color.
	 */
	DDIColor			oldColor;
	
	/**
	 * "New" color (currently being selected).
	 */
	DDIColor			newColor;
	
	/**
	 * Widgets.
	 */
	GWMSlider*			slRed;
	GWMTextField*			txtRed;
	GWMSlider*			slGreen;
	GWMTextField*			txtGreen;
	GWMSlider*			slBlue;
	GWMTextField*			txtBlue;
	GWMTextField*			txtSpec;
	GWMWindow*			preview;
	
	/**
	 * Answer.
	 */
	int				answer;
} ColorPickerData;

static void pickerUpdateSpec(ColorPickerData *data)
{
	char buf[DDI_COLOR_STRING_SIZE];
	ddiColorToString(&data->newColor, buf);
	gwmWriteTextField(data->txtSpec, buf);
	gwmPostUpdate(data->preview);
};

static int previewHandler(GWMEvent *ev, GWMWindow *win, void *context)
{
	ColorPickerData *data = (ColorPickerData*) context;
	
	switch (ev->type)
	{
	case GWM_EVENT_RETHEME:
	case GWM_EVENT_UPDATE:
		{
			DDISurface *canvas = gwmGetWindowCanvas(win);
			ddiFillRect(canvas, 0, 0, 60, 30, &data->oldColor);
			ddiFillRect(canvas, 0, 30, 60, 30, &data->newColor);
			gwmPostDirty(win);
		};
		return GWM_EVSTATUS_OK;
	default:
		return GWM_EVSTATUS_CONT;
	};
};

static int pickerHandler(GWMEvent *ev, GWMWindow *win, void *context)
{
	ColorPickerData *data = (ColorPickerData*) context;
	char buf[16];
	
	switch (ev->type)
	{
	case GWM_EVENT_VALUE_CHANGED:
		if (ev->win == data->slRed->id)
		{
			uint8_t redval = gwmGetSliderValue(data->slRed) * 0xFF;
			sprintf(buf, "%hhu", redval);
			gwmWriteTextField(data->txtRed, buf);
			data->newColor.red = redval;
			pickerUpdateSpec(data);
		}
		else if (ev->win == data->slGreen->id)
		{
			uint8_t greenval = gwmGetSliderValue(data->slGreen) * 0xFF;
			sprintf(buf, "%hhu", greenval);
			gwmWriteTextField(data->txtGreen, buf);
			data->newColor.green = greenval;
			pickerUpdateSpec(data);
		}
		else if (ev->win == data->slBlue->id)
		{
			uint8_t blueval = gwmGetSliderValue(data->slBlue) * 0xFF;
			sprintf(buf, "%hhu", blueval);
			gwmWriteTextField(data->txtBlue, buf);
			data->newColor.blue = blueval;
			pickerUpdateSpec(data);
		};
		return GWM_EVSTATUS_CONT;
	case GWM_EVENT_EDIT_END:
		if (ev->win == data->txtRed->id)
		{
			const char *redspec = gwmReadTextField(data->txtRed);
			uint8_t redval = data->newColor.red;
			sscanf(redspec, "%hhu", &redval);
			
			// triggers GWM_EVENT_VALUE_CHANGED which then updates whatever is needed
			gwmSetSliderValue(data->slRed, (float) redval / 255.0);
		}
		else if (ev->win == data->txtGreen->id)
		{
			const char *greenspec = gwmReadTextField(data->txtGreen);
			uint8_t greenval = data->newColor.green;
			sscanf(greenspec, "%hhu", &greenval);
			
			// triggers GWM_EVENT_VALUE_CHANGED which then updates whatever is needed
			gwmSetSliderValue(data->slGreen, (float) greenval / 255.0);
		}
		else if (ev->win == data->txtBlue->id)
		{
			const char *bluespec = gwmReadTextField(data->txtBlue);
			uint8_t blueval = data->newColor.blue;
			sscanf(bluespec, "%hhu", &blueval);
			
			// triggers GWM_EVENT_VALUE_CHANGED which then updates whatever is needed
			gwmSetSliderValue(data->slBlue, (float) blueval / 255.0);
		}
		else if (ev->win == data->txtSpec->id)
		{
			const char *spec = gwmReadTextField(data->txtSpec);
			ddiParseColor(spec, &data->newColor);
			
			gwmSetSliderValue(data->slRed, (float) data->newColor.red / 255.0);
			gwmSetSliderValue(data->slGreen, (float) data->newColor.green / 255.0);
			gwmSetSliderValue(data->slBlue, (float) data->newColor.blue / 255.0);
		};
		return GWM_EVSTATUS_CONT;
	case GWM_EVENT_COMMAND:
		data->answer = ((GWMCommandEvent*)ev)->symbol;
		return GWM_EVSTATUS_BREAK;
	default:
		return GWM_EVSTATUS_CONT;
	};
};

int gwmPickColor(GWMWindow *parent, const char *caption, DDIColor *color)
{
	ColorPickerData *data = (ColorPickerData*) malloc(sizeof(ColorPickerData));
	memcpy(&data->oldColor, color, sizeof(DDIColor));
	memcpy(&data->newColor, color, sizeof(DDIColor));
	
	GWMWindow *picker = gwmCreateModal(caption, GWM_POS_UNSPEC, GWM_POS_UNSPEC, 0, 0);
	
	GWMLayout *mainBox = gwmCreateBoxLayout(GWM_BOX_VERTICAL);
	gwmSetWindowLayout(picker, mainBox);
	
	GWMLayout *topBox = gwmCreateBoxLayout(GWM_BOX_HORIZONTAL);
	gwmBoxLayoutAddLayout(mainBox, topBox, 1, 5, GWM_BOX_ALL | GWM_BOX_FILL);
	
	GWMFrame *frCustom = gwmNewFrame(picker);
	gwmBoxLayoutAddWindow(topBox, frCustom, 1, 0, GWM_BOX_FILL);
	gwmSetFrameCaption(frCustom, "Custom");
	
	GWMWindow *pnlCustom = gwmGetFramePanel(frCustom);
	GWMLayout *gridCustom = gwmCreateGridLayout(4);
	gwmSetWindowLayout(pnlCustom, gridCustom);
	
	GWMLabel *lblRed = gwmNewLabel(pnlCustom);
	gwmGridLayoutAddWindow(gridCustom, lblRed, 1, 1, GWM_GRID_FILL, GWM_GRID_CENTER);
	gwmSetLabelText(lblRed, "Red:");
	
	GWMSlider *slRed = gwmNewSlider(pnlCustom);
	gwmGridLayoutAddWindow(gridCustom, slRed, 2, 1, GWM_GRID_FILL, GWM_GRID_CENTER);
	gwmSetSliderFlags(slRed, GWM_SLIDER_HORIZ);
	gwmSetSliderValue(slRed, (float) color->red / 255.0);
	
	char buf[16];
	
	GWMTextField *txtRed = gwmNewTextField(pnlCustom);
	gwmGridLayoutAddWindow(gridCustom, txtRed, 1, 1, GWM_GRID_FILL, GWM_GRID_CENTER);
	sprintf(buf, "%hhu", color->red);
	gwmWriteTextField(txtRed, buf);

	GWMLabel *lblGreen = gwmNewLabel(pnlCustom);
	gwmGridLayoutAddWindow(gridCustom, lblGreen, 1, 1, GWM_GRID_FILL, GWM_GRID_CENTER);
	gwmSetLabelText(lblGreen, "Green:");
	
	GWMSlider *slGreen = gwmNewSlider(pnlCustom);
	gwmGridLayoutAddWindow(gridCustom, slGreen, 2, 1, GWM_GRID_FILL, GWM_GRID_CENTER);
	gwmSetSliderFlags(slGreen, GWM_SLIDER_HORIZ);
	gwmSetSliderValue(slGreen, (float) color->green / 255.0);
	
	GWMTextField *txtGreen = gwmNewTextField(pnlCustom);
	gwmGridLayoutAddWindow(gridCustom, txtGreen, 1, 1, GWM_GRID_FILL, GWM_GRID_CENTER);
	sprintf(buf, "%hhu", color->green);
	gwmWriteTextField(txtGreen, buf);

	GWMLabel *lblBlue = gwmNewLabel(pnlCustom);
	gwmGridLayoutAddWindow(gridCustom, lblBlue, 1, 1, GWM_GRID_FILL, GWM_GRID_CENTER);
	gwmSetLabelText(lblBlue, "Blue:");
	
	GWMSlider *slBlue = gwmNewSlider(pnlCustom);
	gwmGridLayoutAddWindow(gridCustom, slBlue, 2, 1, GWM_GRID_FILL, GWM_GRID_CENTER);
	gwmSetSliderFlags(slBlue, GWM_SLIDER_HORIZ);
	gwmSetSliderValue(slBlue, (float) color->blue / 255.0);
	
	GWMTextField *txtBlue = gwmNewTextField(pnlCustom);
	gwmGridLayoutAddWindow(gridCustom, txtBlue, 1, 1, GWM_GRID_FILL, GWM_GRID_CENTER);
	sprintf(buf, "%hhu", color->blue);
	gwmWriteTextField(txtBlue, buf);

	GWMLabel* lblSpec = gwmNewLabel(pnlCustom);
	gwmGridLayoutAddWindow(gridCustom, lblSpec, 1, 1, GWM_GRID_FILL, GWM_GRID_CENTER);
	gwmSetLabelText(lblSpec, "Color spec:");
	
	GWMTextField* txtSpec = gwmNewTextField(pnlCustom);
	gwmGridLayoutAddWindow(gridCustom, txtSpec, 3, 1, GWM_GRID_FILL, GWM_GRID_CENTER);
	ddiColorToString(color, buf);
	gwmWriteTextField(txtSpec, buf);
	
	GWMFrame *frPreview = gwmNewFrame(picker);
	gwmBoxLayoutAddWindow(topBox, frPreview, 0, 0, 0);
	gwmSetFrameCaption(frPreview, "Preview");
	
	GWMWindow *pnlPreview = gwmGetFramePanel(frPreview);
	GWMLayout *prevBox = gwmCreateBoxLayout(GWM_BOX_VERTICAL);
	gwmSetWindowLayout(pnlPreview, prevBox);
	
	GWMLabel *lblOld = gwmNewLabel(pnlPreview);
	gwmBoxLayoutAddWindow(prevBox, lblOld, 0, 0, 0);
	gwmSetLabelText(lblOld, "Old");
	
	GWMWindow *preview = gwmCreateWindow(pnlPreview, "Preview", GWM_POS_UNSPEC, GWM_POS_UNSPEC, 0, 0, 0);
	GWMLayout *stl = gwmCreateStaticLayout(60, 60, 60, 60);
	gwmSetWindowLayout(preview, stl);
	gwmBoxLayoutAddWindow(prevBox, preview, 0, 0, 0);
	
	GWMLabel *lblNew = gwmNewLabel(pnlPreview);
	gwmBoxLayoutAddWindow(prevBox, lblNew, 0, 0, 0);
	gwmSetLabelText(lblNew, "New");
	
	GWMLayout *btnBox = gwmCreateBoxLayout(GWM_BOX_HORIZONTAL);
	gwmBoxLayoutAddLayout(mainBox, btnBox, 0, 5, GWM_BOX_DOWN | GWM_BOX_FILL);
	
	gwmBoxLayoutAddSpacer(btnBox, 1, 0, 0);
	
	GWMButton *btnOK = gwmCreateStockButton(picker, GWM_SYM_OK);
	gwmBoxLayoutAddWindow(btnBox, btnOK, 0, 5, GWM_BOX_RIGHT);
	
	GWMButton *btnCancel = gwmCreateStockButton(picker, GWM_SYM_CANCEL);
	gwmBoxLayoutAddWindow(btnBox, btnCancel, 0, 5, GWM_BOX_RIGHT);
	
	data->slRed = slRed;
	data->txtRed = txtRed;
	data->slGreen = slGreen;
	data->txtGreen = txtGreen;
	data->slBlue = slBlue;
	data->txtBlue = txtBlue;
	data->txtSpec = txtSpec;
	data->preview = preview;
	
	gwmFit(picker);
	gwmPushEventHandler(preview, previewHandler, data);
	gwmPostUpdate(preview);
	gwmPushEventHandler(picker, pickerHandler, data);
	gwmRunModal(picker, GWM_WINDOW_NOSYSMENU | GWM_WINDOW_NOTASKBAR | GWM_WINDOW_MKFOCUSED);
	
	gwmSetWindowFlags(picker, GWM_WINDOW_HIDDEN | GWM_WINDOW_NOTASKBAR);
	
	gwmDestroyButton(btnCancel);
	gwmDestroyButton(btnOK);
	gwmDestroyBoxLayout(btnBox);
	gwmDestroyLabel(lblNew);
	gwmDestroyWindow(preview);
	gwmDestroyLabel(lblOld);
	gwmDestroyBoxLayout(prevBox);
	gwmDestroyFrame(frPreview);
	gwmDestroyTextField(txtSpec);
	gwmDestroyLabel(lblSpec);
	gwmDestroyTextField(txtBlue);
	gwmDestroySlider(slBlue);
	gwmDestroyLabel(lblBlue);
	gwmDestroyTextField(txtGreen);
	gwmDestroySlider(slGreen);
	gwmDestroyLabel(lblGreen);
	gwmDestroyTextField(txtRed);
	gwmDestroySlider(slRed);
	gwmDestroyLabel(lblRed);
	gwmDestroyGridLayout(gridCustom);
	gwmDestroyFrame(frCustom);
	gwmDestroyBoxLayout(topBox);
	gwmDestroyBoxLayout(mainBox);
	
	int answer = data->answer;
	if (answer == GWM_SYM_OK)
	{
		memcpy(color, &data->newColor, sizeof(DDIColor));
	};
	
	free(data);
	gwmDestroyWindow(picker);
	return answer;
};

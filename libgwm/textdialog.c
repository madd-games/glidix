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
	 * Layouts.
	 */
	GWMLayout*			mainBox;
	GWMLayout*			btnBox;
	
	/**
	 * Widgets.
	 */
	GWMTextField*			txt;
	GWMButton*			btnClose;
} TextData;

static int txtHandler(GWMEvent *ev, GWMWindow *win, void *context)
{
	return GWM_EVSTATUS_CONT;
};

GWMTextDialog* gwmNewTextDialog(GWMWindow *parent)
{
	GWMTextDialog *txt = gwmCreateModal("Text", GWM_POS_UNSPEC, GWM_POS_UNSPEC, 0, 0);
	if (txt == NULL) return NULL;
	
	TextData *data = (TextData*) malloc(sizeof(TextData));
	
	data->mainBox = gwmCreateBoxLayout(GWM_BOX_VERTICAL);
	gwmSetWindowLayout(txt, data->mainBox);
	
	data->txt = gwmNewTextField(txt);
	gwmBoxLayoutAddWindow(data->mainBox, data->txt, 1, 5, GWM_BOX_ALL | GWM_BOX_FILL);
	gwmSetTextFieldFlags(data->txt, GWM_TXT_MULTILINE | GWM_TXT_DISABLED);
	
	data->btnBox = gwmCreateBoxLayout(GWM_BOX_HORIZONTAL);
	gwmBoxLayoutAddLayout(data->mainBox, data->btnBox, 0, 5, GWM_BOX_DOWN | GWM_BOX_FILL);
	
	gwmBoxLayoutAddSpacer(data->btnBox, 1, 0, 0);
	
	data->btnClose = gwmCreateStockButton(txt, GWM_SYM_CLOSE);
	gwmBoxLayoutAddWindow(data->btnBox, data->btnClose, 0, 5, GWM_BOX_LEFT | GWM_BOX_RIGHT);
	
	gwmPushEventHandler(txt, txtHandler, data);
	return txt;
};

void gwmSetTextDialogCaption(GWMTextDialog *txt, const char *caption)
{
	gwmSetWindowCaption(txt, caption);
};

void gwmSetTextDialog(GWMTextDialog *txt, const char *text)
{
	TextData *data = (TextData*) gwmGetData(txt, txtHandler);
	gwmWriteTextField(data->txt, text);
};

void gwmRunTextDialog(GWMTextDialog *txt)
{
	TextData *data = (TextData*) gwmGetData(txt, txtHandler);

	gwmLayout(txt, 600, 700);
	gwmFocus(data->btnClose);
	gwmRunModal(txt, GWM_WINDOW_NOTASKBAR | GWM_WINDOW_NOSYSMENU | GWM_WINDOW_RESIZEABLE);
	gwmSetWindowFlags(txt, GWM_WINDOW_HIDDEN | GWM_WINDOW_NOTASKBAR);
	
	gwmDestroyButton(data->btnClose);
	gwmDestroyTextField(data->txt);
	
	gwmDestroyBoxLayout(data->btnBox);
	gwmDestroyBoxLayout(data->mainBox);
	
	gwmDestroyWindow(txt);
};

void gwmTextDialog(GWMWindow *parent, const char *caption, const char *text)
{
	GWMTextDialog *txt = gwmNewTextDialog(parent);
	gwmSetTextDialogCaption(txt, caption);
	gwmSetTextDialog(txt, text);
	gwmRunTextDialog(txt);
};

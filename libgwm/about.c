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

#define	SYM_CREDITS			1000
#define	SYM_LICENSE			1001

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
	GWMImage*			img;
	GWMLabel*			lblCaption;
	GWMLabel*			lblDesc;
	GWMButton*			btnCredits;
	GWMButton*			btnLicense;
	GWMButton*			btnClose;
	
	/**
	 * License and credits (or NULL).
	 */
	char*				credits;
	char*				license;
} AboutData;

static DDIFont* fntCaption;

static int aboutCommand(GWMCommandEvent *ev, GWMWindow *win, AboutData *data)
{
	switch (ev->symbol)
	{
	case SYM_CREDITS:
		gwmTextDialog(win, "Credits", data->credits);
		return GWM_EVSTATUS_OK;
	case SYM_LICENSE:
		gwmTextDialog(win, "License", data->license);
		return GWM_EVSTATUS_OK;
	default:
		return GWM_EVSTATUS_CONT;
	};
};

static int aboutHandler(GWMEvent *ev, GWMWindow *win, void *context)
{
	AboutData *data = (AboutData*) context;
	
	switch (ev->type)
	{
	case GWM_EVENT_COMMAND:
		return aboutCommand((GWMCommandEvent*) ev, win, data);
	default:
		return GWM_EVSTATUS_CONT;
	};
};

GWMAboutDialog* gwmNewAboutDialog(GWMWindow *parent)
{
	GWMAboutDialog *about = gwmCreateModal("About", GWM_POS_UNSPEC, GWM_POS_UNSPEC, 0, 0);
	if (about == NULL) return NULL;
	
	AboutData *data = (AboutData*) malloc(sizeof(AboutData));
	
	data->mainBox = gwmCreateBoxLayout(GWM_BOX_VERTICAL);
	gwmSetWindowLayout(about, data->mainBox);
	
	if (fntCaption == NULL)
	{
		fntCaption = ddiLoadFont("DejaVu Sans", 25, DDI_STYLE_BOLD, NULL);
		if (fntCaption == NULL)
		{
			fntCaption = gwmGetDefaultFont();
		};
	};
	
	data->img = gwmNewImage(about);
	gwmBoxLayoutAddWindow(data->mainBox, data->img, 0, 5, GWM_BOX_UP);
	
	data->lblCaption = gwmNewLabel(about);
	gwmBoxLayoutAddWindow(data->mainBox, data->lblCaption, 0, 5, GWM_BOX_ALL | GWM_BOX_FILL);
	gwmSetLabelFont(data->lblCaption, fntCaption);
	gwmSetLabelAlignment(data->lblCaption, DDI_ALIGN_CENTER);
	
	data->lblDesc = gwmNewLabel(about);
	gwmBoxLayoutAddWindow(data->mainBox, data->lblDesc, 0, 5, GWM_BOX_LEFT | GWM_BOX_RIGHT | GWM_BOX_FILL);
	gwmSetLabelAlignment(data->lblDesc, DDI_ALIGN_CENTER);
	gwmSetLabelWidth(data->lblDesc, 400);
	
	data->btnBox = gwmCreateBoxLayout(GWM_BOX_HORIZONTAL);
	gwmBoxLayoutAddLayout(data->mainBox, data->btnBox, 0, 5, GWM_BOX_ALL | GWM_BOX_FILL);
	
	data->btnCredits = gwmCreateButtonWithLabel(about, SYM_CREDITS, "Credits");
	gwmBoxLayoutAddWindow(data->btnBox, data->btnCredits, 0, 5, GWM_BOX_RIGHT);
	gwmSetButtonFlags(data->btnCredits, GWM_BUTTON_DISABLED);
	
	data->btnLicense = gwmCreateButtonWithLabel(about, SYM_LICENSE, "License");
	gwmBoxLayoutAddWindow(data->btnBox, data->btnLicense, 0, 0, 0);
	gwmSetButtonFlags(data->btnLicense, GWM_BUTTON_DISABLED);
	
	gwmBoxLayoutAddSpacer(data->btnBox, 1, 0, 0);
	
	data->btnClose = gwmCreateStockButton(about, GWM_SYM_CLOSE);
	gwmBoxLayoutAddWindow(data->btnBox, data->btnClose, 0, 0, 0);
	
	data->credits = NULL;
	data->license = NULL;
	
	gwmPushEventHandler(about, aboutHandler, data);
	return about;
};

void gwmSetAboutIcon(GWMAboutDialog *about, DDISurface *icon, int x, int y, int width, int height)
{
	AboutData *data = (AboutData*) gwmGetData(about, aboutHandler);
	gwmSetImageSurface(data->img, icon);
	gwmSetImageViewport(data->img, x, y, width, height);
};

void gwmSetAboutCaption(GWMAboutDialog *about, const char *caption)
{
	AboutData *data = (AboutData*) gwmGetData(about, aboutHandler);
	
	char *wincap;
	asprintf(&wincap, "About %s", caption);
	gwmSetWindowCaption(about, wincap);
	free(wincap);
	
	gwmSetLabelText(data->lblCaption, caption);
};

void gwmSetAboutDesc(GWMAboutDialog *about, const char *desc)
{
	AboutData *data = (AboutData*) gwmGetData(about, aboutHandler);
	gwmSetLabelText(data->lblDesc, desc);
};

void gwmSetAboutCredits(GWMAboutDialog *about, const char *credits)
{
	AboutData *data = (AboutData*) gwmGetData(about, aboutHandler);
	free(data->credits);
	data->credits = strdup(credits);
	
	gwmSetButtonFlags(data->btnCredits, 0);
};

void gwmSetAboutLicense(GWMAboutDialog *about, const char *license)
{
	AboutData *data = (AboutData*) gwmGetData(about, aboutHandler);
	free(data->license);
	data->license = strdup(license);
	
	gwmSetButtonFlags(data->btnLicense, 0);
};

void gwmRunAbout(GWMAboutDialog *about)
{
	AboutData *data = (AboutData*) gwmGetData(about, aboutHandler);
	
	gwmFit(about);
	gwmRunModal(about, GWM_WINDOW_NOTASKBAR | GWM_WINDOW_NOSYSMENU | GWM_WINDOW_MKFOCUSED);
	gwmSetWindowFlags(about, GWM_WINDOW_HIDDEN | GWM_WINDOW_NOTASKBAR);
	
	gwmDestroyImage(data->img);
	gwmDestroyButton(data->btnCredits);
	gwmDestroyButton(data->btnLicense);
	gwmDestroyButton(data->btnClose);
	gwmDestroyLabel(data->lblDesc);
	gwmDestroyLabel(data->lblCaption);
	
	gwmDestroyBoxLayout(data->mainBox);
	gwmDestroyBoxLayout(data->btnBox);
	
	free(data->credits);
	free(data->license);
	
	gwmDestroyWindow(about);
};

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

#include <libddi.h>
#include <libgwm.h>
#include <stdio.h>
#include <assert.h>

enum
{
	SYM_BUTTON1 = GWM_SYM_USER,
	SYM_BUTTON2,
	SYM_BUTTON3,
	
	SYM_CHECKBOX,
	SYM_CB_MASK,
	
	SYM_BUTTON_FRAME1,
	SYM_BUTTON_FRAME2,
};

GWMWindow *txtfield;
GWMWindow *cbMask;

int myCommandHandler(GWMCommandEvent *ev)
{
	switch (ev->symbol)
	{
	case SYM_BUTTON2:
		gwmMessageBox(NULL, "Example", "You clicked button 2!", GWM_MBUT_OK);
		return GWM_EVSTATUS_CONT;
	case SYM_CHECKBOX:
		if (gwmMessageBox(NULL, "Example", "Are you sure you want to flip this checkbox?", GWM_MBUT_YESNO | GWM_MBICON_QUEST) == GWM_SYM_YES)
		{
			return GWM_EVSTATUS_CONT;
		};
		return GWM_EVSTATUS_OK;
	case SYM_BUTTON_FRAME2:
		gwmMessageBox(NULL, "Example", gwmReadTextField(txtfield), GWM_MBUT_OK | GWM_MBICON_WARN);
		return GWM_EVSTATUS_CONT;
	default:
		return GWM_EVSTATUS_CONT;
	};
};

int myToggledHandler(GWMCommandEvent *ev)
{
	switch (ev->symbol)
	{
	case SYM_CB_MASK:
		if (gwmGetCheckboxState(cbMask))
		{
			gwmSetTextFieldFlags(txtfield, GWM_TXT_MASKED);
		}
		else
		{
			gwmSetTextFieldFlags(txtfield, 0);
		};
		return GWM_EVSTATUS_CONT;
	default:
		return GWM_EVSTATUS_CONT;
	};
};

int myHandler(GWMEvent *ev, GWMWindow *win, void *context)
{
	switch (ev->type)
	{
	case GWM_EVENT_COMMAND:
		return myCommandHandler((GWMCommandEvent*) ev);
	case GWM_EVENT_TOGGLED:
		return myToggledHandler((GWMCommandEvent*) ev);
	default:
		return GWM_EVSTATUS_CONT;
	};
};

int main()
{
	if (gwmInit() != 0)
	{
		fprintf(stderr, "Failed to initialize GWM!\n");
		return 1;
	};
	
	GWMWindow *topWindow = gwmCreateWindow(
		NULL,						// window parent (none)
		"Tutorial Part 3",				// window caption
		GWM_POS_UNSPEC, GWM_POS_UNSPEC,			// window coordinates unspecified
		0, 0,						// window size (will be set later by gwmFit() )
		GWM_WINDOW_HIDDEN | GWM_WINDOW_NOTASKBAR	// hidden, no taskbar icon
	);
	assert(topWindow != NULL);
	
	gwmPushEventHandler(topWindow, myHandler, NULL);
	
	GWMLayout *boxLayout = gwmCreateBoxLayout(GWM_BOX_VERTICAL);
	gwmSetWindowLayout(topWindow, boxLayout);
	
	GWMWindow *notebook = gwmNewNotebook(topWindow);
	gwmBoxLayoutAddWindow(boxLayout, notebook, 1, 0, GWM_BOX_FILL);

	GWMWindow *tab1 = gwmNewTab(notebook);
	GWMWindow *tab2 = gwmNewTab(notebook);
	gwmNewTab(notebook);
	
	DDISurface *canvas = gwmGetWindowCanvas(topWindow);
	DDISurface *icon = ddiLoadAndConvertPNG(&canvas->format, "/usr/share/images/calc.png", NULL);
	assert(icon != NULL);
	
	gwmSetWindowCaption(tab2, "This is a really nice tab");
	gwmSetWindowIcon(tab2, icon);
	
	GWMLayout *tabLayout1 = gwmCreateBoxLayout(GWM_BOX_VERTICAL);
	gwmSetWindowLayout(tab1, tabLayout1);
	
	gwmBoxLayoutAddWindow(tabLayout1, gwmCreateButtonWithLabel(tab1, SYM_BUTTON1, "Button 1"), 0, 0, 0);
	gwmBoxLayoutAddWindow(tabLayout1, gwmCreateButtonWithLabel(tab1, SYM_BUTTON2, "Button 2"), 0, 0, GWM_BOX_FILL);
	
	gwmFit(topWindow);
	gwmSetWindowFlags(topWindow, GWM_WINDOW_MKFOCUSED | GWM_WINDOW_RESIZEABLE);

	gwmMainLoop();
	gwmQuit();
	return 0;
};

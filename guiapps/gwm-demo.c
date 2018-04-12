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
	
	SYM_FILE_DEMO,
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
	case SYM_FILE_DEMO:
		gwmMessageBox(NULL, "Example", "You clicked the demo entry in the menu", GWM_MBUT_OK | GWM_MBICON_INFO);
		return GWM_EVSTATUS_CONT;
	case GWM_SYM_EXIT:
		return GWM_EVSTATUS_BREAK;
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
		"GWM Demo",					// window caption
		GWM_POS_UNSPEC, GWM_POS_UNSPEC,			// window coordinates unspecified
		0, 0,						// window size (will be set later by gwmFit() )
		GWM_WINDOW_HIDDEN | GWM_WINDOW_NOTASKBAR	// hidden, no taskbar icon
	);
	assert(topWindow != NULL);
	
	gwmPushEventHandler(topWindow, myHandler, NULL);
	
	GWMLayout *boxLayout = gwmCreateBoxLayout(GWM_BOX_VERTICAL);
	
	GWMWindowTemplate wt;
	wt.wtComps = GWM_WTC_MENUBAR;
	wt.wtWindow = topWindow;
	wt.wtBody = boxLayout;
	gwmCreateTemplate(&wt);
	
	GWMWindow *menubar = wt.wtMenubar;
	
	GWMMenu *menuFile = gwmCreateMenu();
	gwmMenubarAdd(menubar, "File", menuFile);
	
	gwmMenuAddCommand(menuFile, SYM_FILE_DEMO, "Demo", NULL);
	gwmMenuAddCommand(menuFile, GWM_SYM_EXIT, NULL, NULL);
	
	GWMWindow *button1 = gwmCreateButtonWithLabel(
		topWindow,			// the parent window
		SYM_BUTTON1,			// the symbol
		"Button 1"			// and finally the label
	);

	gwmBoxLayoutAddWindow(
		boxLayout,			// the layout to add the button to
		button1,			// the button
		0,				// the proportion (use minimum size)
		0,				// the border width
		0				// flags
	);

	GWMWindow *button2, *button3;
	gwmBoxLayoutAddWindow(boxLayout, button2 = gwmCreateButtonWithLabel(topWindow, SYM_BUTTON2, "Button 2"), 0, 0, 0);
	gwmBoxLayoutAddWindow(boxLayout, button3 = gwmCreateButtonWithLabel(topWindow, SYM_BUTTON3, "Button 3"), 0, 0, 0);
	
	GWMWindow *optmenu = gwmNewOptionMenu(topWindow);
	gwmSetOptionMenu(optmenu, 0, "Select something...");
	gwmAddOptionMenu(optmenu, 1, "Here's one option");
	gwmAddOptionMenu(optmenu, 2, "...and another!");
	gwmAddOptionMenu(optmenu, 3, "And a final one!!!");
	gwmBoxLayoutAddWindow(boxLayout, optmenu, 0, 0, GWM_BOX_FILL);
	
	GWMWindow *sliderHoriz = gwmNewSlider(topWindow);
	gwmSetSliderFlags(sliderHoriz, GWM_SLIDER_HORIZ);
	gwmSetSliderValue(sliderHoriz, 0.3);
	gwmBoxLayoutAddWindow(boxLayout, sliderHoriz, 0, 0, GWM_BOX_FILL);

	GWMWindow *sliderDisHoriz = gwmNewSlider(topWindow);
	gwmSetSliderFlags(sliderDisHoriz, GWM_SLIDER_HORIZ | GWM_SLIDER_DISABLED);
	gwmSetSliderValue(sliderDisHoriz, 0.2);
	gwmBoxLayoutAddWindow(boxLayout, sliderDisHoriz, 0, 0, GWM_BOX_FILL);
	
	txtfield = gwmNewTextField(topWindow);
	gwmWriteTextField(txtfield, "кипeть злoбой");
	gwmBoxLayoutAddWindow(boxLayout, txtfield, 0, 0, GWM_BOX_FILL);
	
	GWMWindow *checkbox = gwmNewCheckbox(topWindow);
	gwmSetCheckboxLabel(checkbox, "Play with me");
	gwmSetCheckboxSymbol(checkbox, SYM_CHECKBOX);
	gwmBoxLayoutAddWindow(boxLayout, checkbox, 0, 0, 0);
	
	cbMask = gwmNewCheckbox(topWindow);
	gwmSetCheckboxLabel(cbMask, "Mask the text");
	gwmSetCheckboxSymbol(cbMask, SYM_CB_MASK);
	gwmBoxLayoutAddWindow(boxLayout, cbMask, 0, 0, 0);
	
	GWMWindow *frame = gwmNewFrame(topWindow);
	gwmSetFrameCaption(frame, "Example frame");
	gwmBoxLayoutAddWindow(boxLayout, frame, 0, 0, 0);
	
	GWMWindow *panel = gwmGetFramePanel(frame);
	GWMLayout *panelLayout = gwmCreateBoxLayout(GWM_BOX_HORIZONTAL);
	gwmSetWindowLayout(panel, panelLayout);
	
	gwmBoxLayoutAddWindow(panelLayout, gwmCreateButtonWithLabel(panel, SYM_BUTTON_FRAME1, "Frame button 1"), 0, 0, 0);
	gwmBoxLayoutAddWindow(panelLayout, gwmCreateButtonWithLabel(panel, SYM_BUTTON_FRAME2, "Frame button 2"), 0, 0, 0);
	
	GWMWindow *sliderVert = gwmNewSlider(panel);
	gwmSetSliderValue(sliderVert, 0.5);
	gwmBoxLayoutAddWindow(panelLayout, sliderVert, 0, 0, GWM_BOX_FILL);
	
	GWMWindow *sliderDisVert = gwmNewSlider(panel);
	gwmSetSliderFlags(sliderDisVert, GWM_SLIDER_DISABLED);
	gwmSetSliderValue(sliderDisVert, 0.3);
	gwmBoxLayoutAddWindow(panelLayout, sliderDisVert, 0, 0, GWM_BOX_FILL);
	
	GWMWindow *frameA = gwmNewFrame(panel);
	gwmSetFrameCaption(frameA, "Radio group A");
	gwmBoxLayoutAddWindow(panelLayout, frameA, 0, 0, 0);
	
	GWMWindow *panelA = gwmGetFramePanel(frameA);
	GWMLayout *panelLayoutA = gwmCreateBoxLayout(GWM_BOX_VERTICAL);
	gwmSetWindowLayout(panelA, panelLayoutA);
	
	GWMRadioGroup *groupA = gwmCreateRadioGroup(0);
	
	GWMWindow *radioA1 = gwmNewRadioButton(panelA, groupA);
	gwmSetRadioButtonValue(radioA1, 0);
	gwmSetRadioButtonLabel(radioA1, "Radio button A1");
	gwmBoxLayoutAddWindow(panelLayoutA, radioA1, 0, 0, 0);

	GWMWindow *radioA2 = gwmNewRadioButton(panelA, groupA);
	gwmSetRadioButtonValue(radioA2, 1);
	gwmSetRadioButtonLabel(radioA2, "Radio button A2");
	gwmBoxLayoutAddWindow(panelLayoutA, radioA2, 0, 0, 0);

	GWMWindow *radioA3 = gwmNewRadioButton(panelA, groupA);
	gwmSetRadioButtonValue(radioA3, 2);
	gwmSetRadioButtonLabel(radioA3, "Radio button A3");
	gwmBoxLayoutAddWindow(panelLayoutA, radioA3, 0, 0, 0);

	GWMWindow *frameB = gwmNewFrame(panel);
	gwmSetFrameCaption(frameB, "Radio group B");
	gwmBoxLayoutAddWindow(panelLayout, frameB, 0, 0, 0);
	
	GWMWindow *panelB = gwmGetFramePanel(frameB);
	GWMLayout *panelLayoutB = gwmCreateBoxLayout(GWM_BOX_VERTICAL);
	gwmSetWindowLayout(panelB, panelLayoutB);
	
	GWMRadioGroup *groupB = gwmCreateRadioGroup(1);
	
	GWMWindow *radioB1 = gwmNewRadioButton(panelB, groupB);
	gwmSetRadioButtonValue(radioB1, 0);
	gwmSetRadioButtonLabel(radioB1, "Radio button B1");
	gwmBoxLayoutAddWindow(panelLayoutB, radioB1, 0, 0, 0);

	GWMWindow *radioB2 = gwmNewRadioButton(panelB, groupB);
	gwmSetRadioButtonValue(radioB2, 1);
	gwmSetRadioButtonLabel(radioB2, "Radio button B2");
	gwmBoxLayoutAddWindow(panelLayoutB, radioB2, 0, 0, 0);

	gwmFit(topWindow);
	gwmSetWindowFlags(topWindow, GWM_WINDOW_MKFOCUSED | GWM_WINDOW_RESIZEABLE);

	gwmMainLoop();
	gwmDestroyButton(button1);
	gwmDestroyButton(button2);
	gwmDestroyButton(button3);
	gwmDestroyCheckbox(checkbox);
	gwmDestroyBoxLayout(boxLayout);
	gwmDestroyWindow(topWindow);
	gwmQuit();
	return 0;
};

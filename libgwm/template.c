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

int gwmCreateTemplate(GWMWindowTemplate *wt)
{
	GWMLayout *layout = gwmCreateBoxLayout(GWM_BOX_VERTICAL);
	gwmSetWindowLayout(wt->wtWindow, layout);
	
	if (wt->wtComps & GWM_WTC_MENUBAR)
	{
		GWMWindow *menubar = gwmNewMenubar(wt->wtWindow);
		gwmBoxLayoutAddWindow(layout, menubar, 0, 0, GWM_BOX_FILL);
		wt->wtMenubar = menubar;
	};
	
	if (wt->wtComps & GWM_WTC_TOOLBAR)
	{
		GWMLayout *toolbar = gwmCreateBoxLayout(GWM_BOX_HORIZONTAL);
		gwmBoxLayoutAddLayout(layout, toolbar, 0, 0, GWM_BOX_FILL);
		wt->wtToolbar = toolbar;
	};
	
	gwmBoxLayoutAddLayout(layout, wt->wtBody, 1, 0, GWM_BOX_FILL);
	
	if (wt->wtComps & GWM_WTC_STATUSBAR)
	{
		GWMWindow *statbar = gwmCreateWindow(wt->wtWindow, "GWMStatusBar", 0, 0, 0, 0, 0);
		gwmBoxLayoutAddWindow(layout, statbar, 0, 2, GWM_BOX_ALL | GWM_BOX_FILL);
		
		GWMLayout *statLayout = gwmCreateBoxLayout(GWM_BOX_HORIZONTAL);
		gwmSetWindowLayout(statbar, statLayout);
		
		GWMLabel *label = gwmCreateLabel(statbar, "Ready.", 0);
		gwmBoxLayoutAddWindow(statLayout, label, 0, 2, GWM_BOX_RIGHT);
		gwmBoxLayoutAddSpacer(statLayout, 1, 0, 0);
		
		wt->wtStatusLabel = label;
		wt->wtStatusBar = statbar;
	};
	
	return 0;
};

int gwmDestroyTemplate(GWMWindowTemplate *wt)
{
	if (wt->wtComps & GWM_WTC_MENUBAR)
	{
		gwmDestroyMenubar(wt->wtMenubar);
	};
	
	if (wt->wtComps & GWM_WTC_TOOLBAR)
	{
		gwmDestroyBoxLayout(wt->wtToolbar);
	};
	
	gwmDestroyBoxLayout(wt->wtWindow->layout);
	gwmSetWindowLayout(wt->wtWindow, NULL);
	
	return 0;
};

void gwmAddStatusBarWindow(GWMWindow *statbar, GWMWindow *child)
{
	gwmBoxLayoutAddWindow(statbar->layout, child, 0, 2, GWM_BOX_LEFT);
};

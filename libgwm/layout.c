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

GWMLayout* gwmCreateAbstractLayout()
{
	GWMLayout *layout = (GWMLayout*) malloc(sizeof(GWMLayout));
	memset(layout, 0, sizeof(GWMLayout));
	return layout;
};

void gwmDestroyAbstractLayout(GWMLayout *layout)
{
	free(layout);
};

void gwmFit(GWMWindow *win)
{
	if (win->layout == NULL) return;
	
	int width, height;
	win->layout->getPrefSize(win->layout, &width, &height);
	
	gwmResizeWindow(win, width, height);
	win->layout->run(win->layout, 0, 0, width, height);
};

void gwmLayout(GWMWindow *win, int width, int height)
{
	if (win->layout == NULL) return;
	
	int minWidth, minHeight;
	win->layout->getMinSize(win->layout, &minWidth, &minHeight);
	
	if (width < minWidth) width = minWidth;
	if (height < minHeight) height = minHeight;
	
	gwmResizeWindow(win, width, height);
	win->layout->run(win->layout, 0, 0, width, height);
};

void gwmSetWindowLayout(GWMWindow *win, GWMLayout *layout)
{
	win->layout = layout;
};
